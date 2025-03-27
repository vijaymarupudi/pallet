#pragma once

#include <inttypes.h>
#include <thread>
#include <memory>
#include <variant>
#include <string_view>
#include <vector>
#include <array>

#include "CGPixel.h"
#include "constants.hpp"
#include "containers/ThreadSafeStack.hpp"
#include "Platform.hpp"
#include "error.hpp"
#include "memory.hpp"

#include "SDL3/SDL.h"

namespace pallet {

enum class GraphicsPosition {
  Default,
  Center,
  Bottom
};

namespace detail {

struct OperationRect {
  float x;
  float y;
  float w;
  float h;
  int c;
};

struct OperationPoint {
  float x;
  float y;
  int c;
};

struct OperationText {
  float x;
  float y;
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
  float x;
  float y;
  bool state;
  int button;
};

struct GraphicsEventMouseMove {
  float x;
  float y;
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


template <class T>
consteval const char* getGraphicsEventName() {
  if constexpr (std::is_same_v<T, GraphicsEventMouseButton>) {
    return "MouseButton";
  } else if constexpr (std::is_same_v<T, GraphicsEventMouseMove>) {
    return "MouseMove";
  } else if constexpr (std::is_same_v<T, GraphicsEventKey>) {
    return "Key";
  } else {
    static_assert(false, "Cannot find a string name for GraphicsEvent");
  }
}

struct GraphicsTextMeasurement {
  int width;
  int ymin;
  int ymax;
};

class GraphicsHardwareInterface {
public:
  virtual void init() = 0;
  virtual void clear() = 0;
  virtual void render() = 0;
  virtual void rect(float, float, float, float, int) = 0;
  virtual void point(float, float, int) = 0;
  virtual void renderCompressedBitmap(float x, float y, int w, int h, uint8_t* data, int fc, int bc = 0) {
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
  void text(float x, float y, const char* str, size_t strLen, int fc, int bc, const GFXfont* font = &CG_pixel_3x52) {
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

const size_t MAX_BATCH_LEN = 32;

class SDLHardwareInterface : public GraphicsHardwareInterface {

  struct Data {
    bool inited = false;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
  };

  struct DataDestroyer {
    void operator()(Data& data) {
      if (data.renderer) {
          SDL_DestroyRenderer(data.renderer);
          data.renderer = nullptr;
      }
      if (data.window) {
        SDL_DestroyWindow(data.window);
        data.window = nullptr;
      }

      if (data.inited) {
        data.inited = false;
        SDL_Quit();
      }
    }
  };

  UniqueResource<Data, DataDestroyer> data;
  
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
    data->inited = true;
    data->window = SDL_CreateWindow("Testing", 128 * this->scaleFactor, 64 * this->scaleFactor, 0);
    auto surface = SDL_GetWindowSurface(data->window);
    data->renderer = SDL_CreateSoftwareRenderer(surface);
    this->userEventType = SDL_RegisterEvents(1);
  }

  void clear() override {
    SDL_SetRenderDrawColor(data->renderer, 0, 0, 0, 255);
    SDL_RenderClear(data->renderer);
  }

  void render() override {
    SDL_RenderPresent(data->renderer);
    SDL_UpdateWindowSurface(data->window);
  }

  void rect(float x, float y, float w, float h, int c) override {
    SDL_FRect rect {(float)x * scaleFactor,
      (float)y * scaleFactor,
      (float)w * scaleFactor,
      (float)h * scaleFactor};

    // c will be between 0-15
    int v = (c << 4) | c;
    SDL_SetRenderDrawColor(data->renderer, v, v, v, 255);
    int ret = SDL_RenderFillRect(data->renderer, &rect);
    if (ret) {
      printf("%s\n", SDL_GetError());
    }
  }

  void point(float x, float y, int c) override {
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
    data.cleanup();
  }
};

class GraphicsInterface {
  public:

  void(*onEventCallback) (GraphicsEvent event, void* ud);
  void* onEventCallbackUserData = nullptr;
  
  void setOnEvent(void(*onEventCallback) (GraphicsEvent event, void* ud), void* ud = nullptr) {
    this->onEventCallback = onEventCallback;
    this->onEventCallbackUserData = ud;
  }

  virtual void strokeRect(float x, float y, float w, float h, int c) {
    this->rect(x, y, 1, h, c);
    this->rect(x, y, w, 1, c);
    this->rect(x + w - 1, y, 1, h, c);
    this->rect(x, y + h - 1, w, 1, c);
  };

  virtual void render() = 0;
  virtual void rect(float x, float y, float w, float h, int c) = 0;
  virtual void clear() = 0;
  virtual void point(float x, float y, int c) = 0;
  virtual void text(float x, float y, std::string_view str, int fc = 15,
                    int bc = 0,
                    GraphicsPosition align = GraphicsPosition::Default,
                    GraphicsPosition baseline = GraphicsPosition::Default) = 0;
  virtual GraphicsTextMeasurement measureText(std::string_view str) = 0;
};

using namespace detail;

namespace detail {
  struct PipeDataDestroyer {
    void operator()(std::array<int, 2>& pipes) {
      if (!(pipes[0] < 0)) {
        close(pipes[0]);
      }

      if (!(pipes[1] < 0)) {
        close(pipes[1]);
      }
    }
  };
}

class LinuxGraphicsInterface : public GraphicsInterface {

  LinuxPlatform* platform;
  FdManager pipeFdManager;
  std::unique_ptr<std::vector<Operation>> operationsBuffer = nullptr;
  SDLHardwareInterface sdlHardwareInterface;
  std::thread thrd;
  containers::ThreadSafeStack<std::unique_ptr<std::vector<Operation>>> operationVectorStack;
  UniqueResource<std::array<int, 2>, detail::PipeDataDestroyer> pipes;

public:

  static Result<LinuxGraphicsInterface> create(LinuxPlatform& platform);
  LinuxGraphicsInterface(LinuxPlatform& platform);
  
  void uponPipeIn(void* datain, size_t len);
  void renderOperations(std::vector<Operation>& operations);
  void uponEvents(SDL_Event* events, size_t len);
  void uponUserEvent(SDL_Event* event);
  void addOperation(auto&& op);

  virtual void render() override;
  virtual void clear() override;
  virtual void rect(float x, float y, float w, float h, int c) override;
  virtual void point(float x, float y, int c) override;
  virtual void text(float x, float y, std::string_view str,
                    int fc = 15, int bc = 0,
                    GraphicsPosition align = GraphicsPosition::Default,
                    GraphicsPosition baseline = GraphicsPosition::Default) override;
  virtual GraphicsTextMeasurement measureText(std::string_view str) override;
};

static_assert(pallet::isMovable<LinuxGraphicsInterface>);

#endif
}

