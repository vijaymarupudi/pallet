#include "pallet/constants.hpp"

#ifdef PALLET_CONSTANTS_PLATFORM_IS_POSIX

#include "pallet/PosixGraphicsInterface.hpp"

#include <unistd.h>
#include <optional>
#include <atomic>

namespace pallet {

static const size_t MAX_BATCH_LEN = 32;

enum class GraphicsUserEventType : int32_t {
    Render
};

void SDLHardwareInterface::init()  {

  // Can't do this, dbus_shutdown() will call _exit();
  // SDL_SetHint(SDL_HINT_SHUTDOWN_DBUS_ON_QUIT, "1");

  SDL_Init(SDL_INIT_VIDEO);
   this->sdlInited = true;
  this->window = SDL_CreateWindow("Testing", 128 * this->scaleFactor, 64 * this->scaleFactor, 0);
  auto surface = SDL_GetWindowSurface(this->window);
  this->renderer = SDL_CreateSoftwareRenderer(surface);
  this->userEventType = SDL_RegisterEvents(1);

}

void SDLHardwareInterface::clear() {
  SDL_SetRenderDrawColor(this->renderer, 0, 0, 0, 255);
  SDL_RenderClear(this->renderer);
}

void SDLHardwareInterface::render() {
  SDL_RenderPresent(this->renderer);
  SDL_UpdateWindowSurface(this->window);
}

void SDLHardwareInterface::rect(float x, float y, float w, float h, int c) {
  SDL_FRect rect {
    x * scaleFactor,
    y * scaleFactor,
    w * scaleFactor,
    h * scaleFactor
  };

  // c will be between 0-15
  int v = (c << 4) | c;
  SDL_SetRenderDrawColor(this->renderer, v, v, v, 255);
  int ret = SDL_RenderFillRect(this->renderer, &rect);
  if (!ret) {
    printf("%s\n", SDL_GetError());
  }
}

void SDLHardwareInterface::point(float x, float y, int c) {
  this->rect(x, y, 1, 1, c);
}


// thread safe
void SDLHardwareInterface::stopLoop() {
  *(this->shouldRunLoop) = false;
}

void SDLHardwareInterface::loop() {
  SDL_Event events[MAX_BATCH_LEN];
  memset(&events[0], 0, sizeof(SDL_Event) * MAX_BATCH_LEN);
  while (*(this->shouldRunLoop) && SDL_WaitEvent(events)) {
    size_t len = 1;
    for (; len < MAX_BATCH_LEN; len++) {
      if (!SDL_PollEvent(&events[len])) {
        break;
      }
    }
    if (this->onEventsCallback) {
      this->onEventsCallback(events, len, this->onEventsUserData);
    }
  }
}

// SDLHardwareInterface::SDLHardwareInterface(SDLHardwareInterface&& other)
//   : sdlInited(other.sdlInited),
//     window(other.window),
//     renderer(other.renderer) {
//   other.sdlInited = false;
//   other.window = nullptr;
//   other.renderer = nullptr;
// }

void SDLHardwareInterface::cleanup() {
  if (renderer) {
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
  }
  if (window) {
    SDL_DestroyWindow(window);
    window = nullptr;
  }

  if (this->sdlInited) {
    this->sdlInited = false;
    SDL_Quit();
  }
}

Result<PosixGraphicsInterface> PosixGraphicsInterface::create(PosixPlatform& platform) {
  return pallet::Pipe::create().and_then([&](auto&& pipe) {
    return Result<PosixGraphicsInterface>(std::in_place, platform, std::move(pipe));
  });
}

PosixGraphicsInterface::PosixGraphicsInterface(PosixPlatform& platform, pallet::Pipe&& ipipe) :
  platform(&platform), pipeFdManager(platform),
  sdlHardwareInterfaceStopper(&sdlHardwareInterface),
  operationsBuffer(new std::vector<Operation>), pipe(std::move(ipipe))

{
  pipeFdManager.setFd(this->pipe.getReadFd());
  pipeFdManager.startReading([](int fd, void* data, size_t len, void* ud) {
    (void)fd;
    static_cast<PosixGraphicsInterface*>(ud)->uponPipeIn(data, len);
  }, this);

  this->sdlHardwareInterface.onEventsUserData = this;
  this->sdlHardwareInterface.onEventsCallback = [](SDL_Event* e, size_t len,
                                                   void* u) {
    static_cast<PosixGraphicsInterface*>(u)->uponEventsGThread(e, len);
  };

  std::atomic<bool> sdlInterfaceInited = false;

  this->thrd = std::jthread([this, &sdlInterfaceInited](){
    this->sdlHardwareInterface.init();
    sdlInterfaceInited = true;
    sdlInterfaceInited.notify_one();
    this->sdlHardwareInterface.loop();
    this->sdlHardwareInterface.cleanup();
  });

  bool old = sdlInterfaceInited.load();
  if (!old) {
    sdlInterfaceInited.wait(old);
  }

  this->hardwareInterfaceRunning = true;
}

PosixGraphicsInterface::PosixGraphicsInterface(PosixGraphicsInterface&& iface)
  : platform{iface.platform},
    pipeFdManager{std::move(iface.pipeFdManager)},
    sdlHardwareInterface{std::move(iface.sdlHardwareInterface)},
    thrd{std::move(iface.thrd)},
    operationsBuffer{std::move(iface.operationsBuffer)},
    operationVectorStack{std::move(iface.operationVectorStack)},
    pipe{std::move(iface.pipe)}
{
  pipeFdManager.setReadWriteUserData(this, nullptr);
}


void PosixGraphicsInterface::uponPipeIn(void* datain, size_t len) {
  unsigned char* data = static_cast<unsigned char*>(datain);
  auto scaleFactor = this->sdlHardwareInterface.scaleFactor;
  for (size_t i = 0; i < len; i += sizeof(SDL_Event)) {
    std::optional<GraphicsEvent> event;
    SDL_Event sdlEvent;
    memcpy(&sdlEvent, data + i, sizeof(SDL_Event));
    if (sdlEvent.type == SDL_EVENT_MOUSE_MOTION) {
      event = GraphicsEventMouseMove {
        sdlEvent.motion.x / scaleFactor,
        sdlEvent.motion.y / scaleFactor
      };
    }
    else if (sdlEvent.type == SDL_EVENT_KEY_DOWN || sdlEvent.type == SDL_EVENT_KEY_UP) {
      bool state = sdlEvent.type == SDL_EVENT_KEY_DOWN ? true : false;
      event = GraphicsEventKey {
        state,
        sdlEvent.key.repeat != 0,
        static_cast<uint32_t>(sdlEvent.key.key),
        sdlEvent.key.scancode,
        sdlEvent.key.mod
      };
    } else if (sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN || sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_UP) {
      bool state = sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? true : false;
      int button = sdlEvent.button.button;
      event = GraphicsEventMouseButton {
        sdlEvent.button.x / scaleFactor,
        sdlEvent.button.y / scaleFactor,
        state,
        button
      };
    } else if (sdlEvent.type == SDL_EVENT_QUIT) {
      event = GraphicsEventQuit{};
    }
    if (event) {
      if (this->onEventCallback) {
        this->onEventCallback(std::move(*event), this->onEventCallbackUserData);
      }
    }
  }
}

void PosixGraphicsInterface::render() {

  if (!this->hardwareInterfaceRunning) {
    // It's a no-op if the graphics aren't hardwareInterfaceRunning!
    this->operationsBuffer->clear();
    return;
  }

  auto old = std::move(this->operationsBuffer);
  auto previous_vector = this->operationVectorStack.pop();
  if (previous_vector) {
    this->operationsBuffer = std::move(*previous_vector);
  } else {
    this->operationsBuffer.reset(new std::vector<Operation>);
  }

  SDL_Event event;
  SDL_zero(event);
  event.type = sdlHardwareInterface.userEventType;
  event.user.code = static_cast<int32_t>(GraphicsUserEventType::Render);
  event.user.data1 = old.release();
  event.user.timestamp = SDL_GetTicks();
  int ret = SDL_PushEvent(&event);
  if (ret < 0) {
    printf("%s\n", SDL_GetError());
  }
}

void PosixGraphicsInterface::quit() {
  if (this->hardwareInterfaceRunning) {
    this->sdlHardwareInterface.stopLoop();
    this->hardwareInterfaceRunning = false;
  }
}

void PosixGraphicsInterface::renderOperations(std::vector<Operation>& operations) {
  // This is called in the SDL thread

  struct OperationActions {
    SDLHardwareInterface& sdlHardwareInterface;
    void operator()(OperationRect& op) {
      this->sdlHardwareInterface.rect(op.x, op.y, op.w, op.h, op.c);
    }

    void operator()(OperationPoint& op) {
      this->sdlHardwareInterface.point(op.x, op.y, op.c);
    }

    void operator()(OperationText& op) {
      auto opx = op.x;
      auto opy = op.y;
      auto text = op.text;
      auto align = op.align;
      auto baseline = op.baseline;

      if (align != GraphicsPosition::Default || baseline != GraphicsPosition::Default) {
        auto measurement = this->sdlHardwareInterface.measureText(text.data(), text.size());
        switch (align) {
        case GraphicsPosition::Center:
          opx -= measurement.width / 2;
          break;
        case GraphicsPosition::Default:
          break;
        default:
          break;
        }

        switch (baseline) {
        case GraphicsPosition::Center:
          opy = opy - measurement.ymin - (measurement.ymax - measurement.ymin) / 2;
          break;
        case GraphicsPosition::Bottom:
          opy = opy - measurement.ymax;
        case GraphicsPosition::Default:
          break;
        }
      }

      // printf("(%f, %f) %d %d text: %s\n", opx, opy, op.fc, op.bc, text.data());

      this->sdlHardwareInterface.text(opx, opy,
                                      text.data(),
                                      text.size(),
                                      op.fc,
                                      op.bc);
    }

    void operator()(OperationClear& clear) {
      (void)clear;
      this->sdlHardwareInterface.clear();
    }
  };

  auto actions = OperationActions{this->sdlHardwareInterface};

  for (auto& op : operations) {
    pallet::visit(actions, op);
  }

  this->sdlHardwareInterface.render();
}

void PosixGraphicsInterface::uponEventsGThread(SDL_Event* events, size_t len) {
  SDL_Event eventsToSend[MAX_BATCH_LEN];
  size_t nEvents = 0;
  for (size_t i = 0; i < len; i++) {
    SDL_Event *event = &events[i];
    if (event->type == this->sdlHardwareInterface.userEventType) {
      this->uponUserEventGThread(event);
    } else {
      eventsToSend[nEvents] = *event;
      nEvents++;
    }
  }
  if (nEvents > 0) {
    this->pipe.write(eventsToSend, sizeof(SDL_Event) * nEvents);
  }
}

void PosixGraphicsInterface::uponUserEventGThread(SDL_Event* event) {
  auto userEventType = static_cast<GraphicsUserEventType>(event->user.code);
  switch (userEventType) {
  case GraphicsUserEventType::Render:
    {
      auto data1 = static_cast<std::vector<Operation>*>(event->user.data1);
      std::unique_ptr<std::vector<Operation>> operations (data1);
      this->renderOperations(*operations);
      operations->clear();
      this->operationVectorStack.push(std::move(operations));
    }
    break;
  }
}

void PosixGraphicsInterface::addOperation(auto&& op) {
  this->operationsBuffer->push_back(std::forward<decltype(op)>(op));
}

void PosixGraphicsInterface::rect(float x, float y, float w, float h, int c) {
  this->addOperation(OperationRect {x, y, w, h, c});
}

void PosixGraphicsInterface::clear() {
  this->addOperation(OperationClear{});
}

void PosixGraphicsInterface::point(float x, float y, int c) {
  this->addOperation(OperationPoint {x, y, c});
}

void PosixGraphicsInterface::text(float x, float y, std::string_view str, int fc, int bc,
                                  GraphicsPosition align, GraphicsPosition baseline) {
  this->addOperation(OperationText {
      x, y, std::string(str.data(),
                        str.size()),
      fc, bc,
      align, baseline
    });
}

GraphicsTextMeasurement PosixGraphicsInterface::measureText(std::string_view str) {
  return this->sdlHardwareInterface.measureText(str.data(), str.size());
}

}

#endif
