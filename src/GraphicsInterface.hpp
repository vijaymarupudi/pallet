#pragma once

#include "SDL2/SDL.h"
#include <inttypes.h>
#include "CGPixel.h"
#include <thread>
#include <memory>
#include <atomic>
#include <variant>
#include <string_view>
#include <optional>
#include <unistd.h>

#include "constants.hpp"
#include "containers/ThreadSafeStack.hpp"
#include "Platform.hpp"

namespace pallet {

enum class GraphicsPosition {
  Default,
  Center,
  Bottom
};

namespace detail {

struct OperationRect {
  int x;
  int y;
  int w;
  int h;
  int c;
};

struct OperationPoint {
  int x;
  int y;
  int c;
};

struct OperationText {
  int x;
  int y;
  std::string text;
  int fc;
  int bc;
  GraphicsPosition align;
  GraphicsPosition baseline;
};

struct OperationClear {};

using Operation = std::variant<OperationRect,
                               OperationPoint,
                               OperationText,
                               OperationClear>;

}

struct GraphicsEventMouseButton {
  int x;
  int y;
  bool state;
  int button;
};

struct GraphicsEventMouseMove {
  int x;
  int y;
};

struct GraphicsEventKey {
  bool state;
  bool repeat;
  uint32_t keycode;
  uint32_t scancode;
  uint32_t mod;
};


using GraphicsEvent = std::variant<GraphicsEventMouseButton,
                                   GraphicsEventMouseMove,
                                   GraphicsEventKey>;

struct GraphicsTextMeasurement {
  int width;
  int ymin;
  int ymax;
};

using namespace detail;

class GraphicsHardwareInterface {
public:
  virtual void init() = 0;
  virtual void clear() = 0;
  virtual void render() = 0;
  virtual void rect(int, int, int, int, int) = 0;
  virtual void point(int, int, int) = 0;
  virtual void renderCompressedBitmap(int x, int y, int w, int h, uint8_t* data, int fc, int bc = 0) {
    for (int ypos = 0; ypos < h; ypos++) {
      for (int xpos = 0; xpos < w; xpos++) {
        auto arrayIndex = ypos * w + xpos;
        auto byteIndex = arrayIndex / 8;
        auto bitIndex = (arrayIndex % 8);
        auto xloc = x + xpos;
        auto yloc = y + ypos;
        auto reading = (0x80 >> bitIndex) & data[byteIndex];
        if (reading) {
          this->point(xloc, yloc, fc);
        } else if (bc != -1) {
          this->point(xloc, yloc, bc);
        }
      }
    }
  }

  // fc = foreground color
  // bc = background color
  void text(int x, int y, const char* str, size_t strLen, int fc, int bc, const GFXfont* font = &CG_pixel_3x52) {
    int xpos = x;
    int ypos = y;
    for (size_t i = 0; i < strLen; i++) {
      char c = str[i];
      if (c == '\n') {
        ypos += font->yAdvance;
        xpos = x;
        continue;
      }
      auto glyph = &font->glyph[c - font->first];
      uint8_t* data = &font->bitmap[glyph->bitmapOffset];
      int xrenderpos = xpos + glyph->xOffset;
      int yrenderpos = ypos + glyph->yOffset;
      this->renderCompressedBitmap(xrenderpos, yrenderpos, glyph->width,
                                   glyph->height, data, fc, bc);
      xpos += glyph->xAdvance;
    }
  }

  GraphicsTextMeasurement measureText(const char* str, size_t strLen, const GFXfont* font = &CG_pixel_3x52) {

    auto min = [](auto x, auto y) {
      if (x < y) { return x;}
      return y;
    };

    auto max = [](auto x, auto y) {
      if (x > y) { return x;}
      return y;
    };

    int ymax = 0;
    int ymin = 0;
    int xpos = 0;
    int ypos = 0;

    for (size_t i = 0; i < strLen; i++) {
      char c = str[i];
      if (c == '\n') {
        ypos += font->yAdvance;
        xpos = 0;
        continue;
      }
      auto glyph = &font->glyph[c - font->first];
      int xrenderpos = xpos + glyph->xOffset;
      (void)xrenderpos;
      int yrenderpos = ypos + glyph->yOffset;
      ymin = min(ymin, yrenderpos);
      ymax = max(ymax, yrenderpos + glyph->height);
      xpos += glyph->xAdvance;
    }

    auto lastGlyph = &font->glyph[str[strLen - 1] - font->first];
    xpos -= lastGlyph->xAdvance;
    xpos += lastGlyph->xOffset + lastGlyph->width;

    return GraphicsTextMeasurement {xpos, ymin, ymax};
  }
};

#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX



const size_t MAX_BATCH_LEN = 30;

class SDLHardwareInterface : public GraphicsHardwareInterface {
  SDL_Window* window;
  SDL_Surface* surface;
  SDL_Renderer* renderer;
public:

  int scaleFactor = 9;

  void(*onEventsCallback)(SDL_Event* events, size_t len, void* ud) = nullptr;
  void* onEventsUserData = nullptr;

  unsigned int userEventType;

  void config(int scaleFactor = 9) {
    this->scaleFactor = scaleFactor;
  }

  void init() override {
    SDL_Init(SDL_INIT_VIDEO);
    this->window = SDL_CreateWindow("Testing", SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                    128 * this->scaleFactor, 64 * this->scaleFactor, SDL_WINDOW_SHOWN);
    this->surface = SDL_GetWindowSurface(window);
    this->renderer = SDL_CreateSoftwareRenderer(surface);
    this->userEventType = SDL_RegisterEvents(1);
  }

  void clear() override {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
  }

  void render() override {
    SDL_UpdateWindowSurface(window);
  }

  void rect(int x, int y, int w, int h, int c) override {
    SDL_Rect rect {x * scaleFactor,
                   y * scaleFactor,
                   w * scaleFactor,
                   h * scaleFactor};

    int v = ((c + 1) << 4) - 1;
    SDL_SetRenderDrawColor(renderer, v, v, v, 255);
    int ret = SDL_RenderFillRect(renderer, &rect);
    if (ret) {
      printf("%s\n", SDL_GetError());
    }
  }

  void point(int x, int y, int c) override {
    this->rect(x, y, 1, 1, c);
  }

  void loop() {
    SDL_Event events[MAX_BATCH_LEN];
    while (SDL_WaitEvent(events)) {
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

  void close() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
  }
};

class LinuxGraphicsInterface {

  LinuxPlatform& platform;
  FdManager pipeFdManager;
  std::unique_ptr<std::vector<Operation>> operationsBuffer = nullptr;

  SDLHardwareInterface sdlHardwareInterface;

  std::atomic<bool> sdlInterfaceInited = false;
  std::thread thrd;
  containers::ThreadSafeStack<std::unique_ptr<std::vector<Operation>>>
  operationVectorStack;

  int pipes[2];
  void(*onEventCallback) (GraphicsEvent event, void* ud);
  void* onEventCallbackUserData = nullptr;

public:

  void setOnEvent(void(*onEventCallback) (GraphicsEvent event, void* ud), void* ud = nullptr) {
    this->onEventCallback = onEventCallback;
    this->onEventCallbackUserData = ud;
  }

  LinuxGraphicsInterface(LinuxPlatform& platform) :
    platform(platform), pipeFdManager(platform), operationsBuffer(new std::vector<Operation>) {

    pipe(this->pipes);

    pipeFdManager.setFd(pipes[0]);

    pipeFdManager.startReading([](int fd, void* data, size_t len, void* ud) {
      (void)fd;
      reinterpret_cast<LinuxGraphicsInterface*>(ud)->uponPipeIn(data, len);
    }, this);

    this->sdlHardwareInterface.onEventsUserData = this;
    this->sdlHardwareInterface.onEventsCallback = [](SDL_Event* e, size_t len,
                                             void* u) {
      reinterpret_cast<LinuxGraphicsInterface*>(u)->uponEvents(e, len);
    };

    this->thrd = std::thread([this](){
      this->sdlHardwareInterface.init();
      this->sdlInterfaceInited = true;
      this->sdlInterfaceInited.notify_one();
      this->sdlHardwareInterface.loop();
    });

    bool old = this->sdlInterfaceInited;
    if (!old) {
      this->sdlInterfaceInited.wait(old);
    }
  }

  void uponPipeIn(void* datain, size_t len) {
    unsigned char* data = reinterpret_cast<unsigned char*>(datain);
    auto scaleFactor = this->sdlHardwareInterface.scaleFactor;
    for (size_t i = 0; i < len; i += sizeof(SDL_Event)) {
      std::optional<GraphicsEvent> event;
      SDL_Event sdlEvent;
      memcpy(&sdlEvent, data + i, sizeof(SDL_Event));
      if (sdlEvent.type == SDL_MOUSEMOTION) {
        event = GraphicsEventMouseMove {
          sdlEvent.motion.x / scaleFactor,
          sdlEvent.motion.y / scaleFactor
        };
      }
      else if (sdlEvent.type == SDL_KEYDOWN || sdlEvent.type == SDL_KEYUP) {
        bool state = sdlEvent.type == SDL_KEYDOWN ? true : false;
        event = GraphicsEventKey {
          state,
          sdlEvent.key.repeat != 0,
          static_cast<uint32_t>(sdlEvent.key.keysym.sym),
          sdlEvent.key.keysym.scancode,
          sdlEvent.key.keysym.mod
        };
      } else if (sdlEvent.type == SDL_MOUSEBUTTONDOWN || sdlEvent.type == SDL_MOUSEBUTTONUP) {
        bool state = sdlEvent.type == SDL_MOUSEBUTTONDOWN ? true : false;
        int button = sdlEvent.button.button;
        event = GraphicsEventMouseButton {
          sdlEvent.button.x / scaleFactor,
          sdlEvent.button.y / scaleFactor,
          state,
          button
        };
      }

      if (event) {
        if (this->onEventCallback) {
          this->onEventCallback(std::move(*event), this->onEventCallbackUserData);
        }
      }
    }
  }

  void render() {
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
    event.user.code = 1;
    event.user.data1 = old.release();
    event.user.timestamp = SDL_GetTicks();
    int ret = SDL_PushEvent(&event);
    if (ret < 0) {
      printf("%s\n", SDL_GetError());
    }
  }

  void renderOperations(std::vector<Operation>& operations) {
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

        int opx = op.x;
        int opy = op.y;
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
      std::visit(actions, op);
    }

    this->sdlHardwareInterface.render();
  }

  void uponEvents(SDL_Event* events, size_t len) {
    SDL_Event eventsToSend[MAX_BATCH_LEN];
    size_t nEvents = 0;
    for (size_t i = 0; i < len; i++) {
      SDL_Event *event = &events[i];
      if (event->type == SDL_QUIT) {
        this->sdlHardwareInterface.close();
      } else if (event->type == this->sdlHardwareInterface.userEventType) {
        this->uponUserEvent(event);
      } else {
        eventsToSend[nEvents] = *event;
        nEvents++;
      }
    }
    write(this->pipes[1], eventsToSend, sizeof(SDL_Event) * nEvents);
  }

  void uponUserEvent(SDL_Event* event) {
    auto data1 = reinterpret_cast<std::vector<Operation>*>(event->user.data1);
    std::unique_ptr<std::vector<Operation>> operations (data1);
    this->renderOperations(*operations);
    operations->clear();
    this->operationVectorStack.push(std::move(operations));
  }

  void addOperation(auto&& op) {
    this->operationsBuffer->push_back(std::forward<decltype(op)>(op));
  }

  void rect(int x, int y, int w, int h, int c) {
    this->addOperation(OperationRect {x, y, w, h, c});
  }

  void strokeRect(int x, int y, int w, int h, int c) {
    this->addOperation(OperationRect {x, y, 1, h, c});
    this->addOperation(OperationRect {x, y, w, 1, c});
    this->addOperation(OperationRect {x + w - 1, y, 1, h, c});
    this->addOperation(OperationRect {x, y + h - 1, w, 1, c});
  }

  void clear() {
    this->addOperation(OperationClear{});
  }

  void point(int x, int y, int c) {
    this->addOperation(OperationPoint {x, y, c});
  }

  void text(int x, int y, std::string_view str, int fc = 15, int bc = 0,
            GraphicsPosition align = GraphicsPosition::Default, GraphicsPosition baseline = GraphicsPosition::Default) {
    this->addOperation(OperationText {
        x, y, std::string(str.data(),
                          str.size()),
        fc, bc,
        align, baseline
      });
  }

  GraphicsTextMeasurement measureText(std::string_view str) {
    return this->sdlHardwareInterface.measureText(str.data(), str.size());
  }

  ~LinuxGraphicsInterface() {
    close(pipes[0]);
    close(pipes[1]);
  }
};
}
#endif
