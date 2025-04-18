#pragma once

#include <cstdint>
#include <string_view>
#include <string>

#include "pallet/constants.hpp"
#include "pallet/CGPixel.h"

#include "pallet/LightVariant.hpp"


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

using Operation = LightVariant<OperationRect,
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


using GraphicsEvent = LightVariant<GraphicsEventMouseButton,
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

}
