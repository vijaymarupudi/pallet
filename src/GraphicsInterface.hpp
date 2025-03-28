#pragma once

#include <inttypes.h>
#include <thread>
#include <memory>
#include <string_view>
#include <vector>
#include <array>

#include "CGPixel.h"
#include "constants.hpp"
#include "containers/ThreadSafeStack.hpp"
#include "Platform.hpp"
#include "error.hpp"
#include "memory.hpp"
#include "variant.hpp"

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

using Operation = Variant<OperationRect,
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

struct GraphicsEventQuit {};


using GraphicsEvent = Variant<GraphicsEventMouseButton,
                              GraphicsEventMouseMove,
                              GraphicsEventKey,
                              GraphicsEventQuit>;


template <class T>
consteval const char* getGraphicsEventName() {
  if constexpr (std::is_same_v<T, GraphicsEventMouseButton>) {
    return "MouseButton";
  } else if constexpr (std::is_same_v<T, GraphicsEventMouseMove>) {
    return "MouseMove";
  } else if constexpr (std::is_same_v<T, GraphicsEventKey>) {
    return "Key";
  } else if constexpr (std::is_same_v<T, GraphicsEventQuit>) {
    return "Quit";
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
  // fc = foreground color
  // bc = background color

  virtual void renderCompressedBitmap(float x, float y, int w, int h, uint8_t* data, int fc, int bc = 0);
  void text(float x, float y, const char* str, size_t strLen, int fc, int bc, const GFXfont* font = &CG_pixel_3x52);
  GraphicsTextMeasurement measureText(const char* str, size_t strLen, const GFXfont* font = &CG_pixel_3x52);
  
};

#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX

class SDLHardwareInterface final : public GraphicsHardwareInterface {

  struct Data {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool inited = false;
  };

  struct DataDestroyer {
    void operator() (Data& data) noexcept {
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

  void init() override;
  void clear() override;
  void render() override;
  void rect(float x, float y, float w, float h, int c) override;
  void point(float x, float y, int c) override;
  void loop();
  void close();
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
  virtual void quit() = 0;
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
    void operator()(std::array<int, 2>& pipes) noexcept {
      if (!(pipes[0] < 0)) {
        close(pipes[0]);
      }

      if (!(pipes[1] < 0)) {
        close(pipes[1]);
      }
    }
  };
}

class LinuxGraphicsInterface final : public GraphicsInterface {

  LinuxPlatform* platform;
  FdManager pipeFdManager;
  SDLHardwareInterface sdlHardwareInterface;
  std::thread thrd;
  std::unique_ptr<std::vector<Operation>> operationsBuffer = nullptr;
  containers::ThreadSafeStack<std::unique_ptr<std::vector<Operation>>> operationVectorStack;
  UniqueResource<std::array<int, 2>, detail::PipeDataDestroyer> pipes;

public:

  static Result<LinuxGraphicsInterface> create(LinuxPlatform& platform);

  LinuxGraphicsInterface(LinuxPlatform& platform);
  LinuxGraphicsInterface(LinuxGraphicsInterface&& iface);

  void uponPipeIn(void* datain, size_t len);
  void renderOperations(std::vector<Operation>& operations);
  void uponEvents(SDL_Event* events, size_t len);
  void uponUserEvent(SDL_Event* event);
  void addOperation(auto&& op);

  virtual void render() override;
  virtual void quit() override;
  virtual void clear() override;
  virtual void rect(float x, float y, float w, float h, int c) override;
  virtual void point(float x, float y, int c) override;
  virtual void text(float x, float y, std::string_view str,
                    int fc = 15, int bc = 0,
                    GraphicsPosition align = GraphicsPosition::Default,
                    GraphicsPosition baseline = GraphicsPosition::Default) override;
  virtual GraphicsTextMeasurement measureText(std::string_view str) override;
  
};

  static_assert(std::is_move_constructible_v<LinuxGraphicsInterface>);

#endif
}
