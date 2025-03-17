#pragma once

#include "SDL2/SDL.h"
#include <inttypes.h>
#include "CGPixel.h"
#include <thread>

namespace pallet {

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
  char* text;
  size_t textLen;  
};

enum class OperationType : uint8_t {
  Rect,
  Point,
  Text,
  Clear
};

struct Operation {
  OperationType type;
  union {
    OperationPoint point;
    OperationRect rect;
    OperationText text;
  };

  Operation(OperationPoint point) {
    this->type = OperationType::Point;
    this->point = point;
  }

  Operation(OperationRect rect) {
    this->type = OperationType::Rect;
    this->rect = rect;
  }

  Operation(OperationText text) {
    this->type = OperationType::Text;
    this->text = text;
  }

  Operation(OperationType type) {
    this->type = type;
  }
};
}

using namespace detail;

class GraphicsInterface {
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

  void renderOperations(std::vector<Operation>& operations) {
    for (auto& op : operations) {
      switch (op.type) {
      case OperationType::Rect:
        this->rect(op.rect.x, op.rect.y, op.rect.w, op.rect.h, op.rect.c);
        break;
      case OperationType::Point:
        this->point(op.point.x, op.point.y, op.point.c);
        break;
      case OperationType::Text:
        this->text(op.text.x, op.text.y, op.text.text, op.text.textLen);
        free(op.text.text);
        break;
      case OperationType::Clear:
        this->clear();
        break;
      }
    }

    this->render();
  }

  void text(int x, int y, const char* str, size_t strLen, const GFXfont* font = &CG_pixel_3x52) {
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
                                   glyph->height, data, 15);
      xpos += glyph->xAdvance;
    }
  }
};

class SDLInterface : public GraphicsInterface {
  SDL_Window* window;
  SDL_Surface* surface;
  SDL_Renderer* renderer;
  int scaleFactor = 9;

public:

  unsigned int userEventType;

  void config(int scaleFactor = 9) {
    this->scaleFactor = scaleFactor;
  }
  
  void init() override {
    SDL_Init(SDL_INIT_VIDEO);
    this->window = SDL_CreateWindow("Testing", SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  1024, 600, SDL_WINDOW_SHOWN);
    this->surface = SDL_GetWindowSurface(window);
    this->renderer = SDL_CreateSoftwareRenderer(surface);
    this->userEventType = SDL_RegisterEvents(1);
  }

  void clear() override {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
  }

  void render() override {
    // SDL_RenderPresent(renderer);
    // SDL_UpdateWindowSurface(window);
  }

  void rect(int x, int y, int w, int h, int c) override {
    SDL_Rect rect {x * scaleFactor,
                   y * scaleFactor,
                   w * scaleFactor,
                   h * scaleFactor};

    int v = ((c + 1) << 4) - 1;
    printf("c: %d\n", v);
    SDL_SetRenderDrawColor(renderer, v, v, v, 255);
    SDL_RenderFillRect(renderer, &rect);
  }

  void point(int x, int y, int c) override {
    int v = ((c + 1) << 4) - 1;
    SDL_SetRenderDrawColor(renderer, v, v, v, 255);
    this->rect(x, y, 1, 1, c);
  }

  void loop() {

    this->clear();
    this->rect(0, 0, 100, 100, 15);
    this->render();

    
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
      printf("Got event: %u\n", event.type);
      if (event.type == SDL_QUIT) {
        this->close();
      }
      if (event.type == this->userEventType) {
        auto operations = reinterpret_cast<std::vector<Operation>*>(event.user.data1);
        this->renderOperations(*operations);
        delete operations;
      }
    }
  }

  void close() {
    SDL_DestroyWindow(window);
    SDL_Quit();
  }
};

class SDLThreadedInterface {

  LinuxPlatform& platform;
  std::vector<Operation>* operationsBuffer = nullptr;
  SDLInterface sdlInterface;
  std::thread thrd;

public:

  SDLThreadedInterface(LinuxPlatform& platform) : platform(platform),
                                                  operationsBuffer(new std::vector<Operation>) {
    this->sdlInterface.init();
    thrd = std::thread([this](){
      this->sdlInterface.loop();
    });
  }

  void render() {
    auto old = this->operationsBuffer;
    this->operationsBuffer = new std::vector<Operation>;
    SDL_Event event;
    SDL_zero(event);
    event.type = sdlInterface.userEventType;
    event.user.code = 1;
    event.user.data1 = old;
    event.user.timestamp = SDL_GetTicks();
    int ret = SDL_PushEvent((SDL_Event*)&event);
    if (ret < 0) {
      printf("%s\n", SDL_GetError());
    }
  }

  void addOperation(auto&& op) {
    this->operationsBuffer->push_back(std::forward<decltype(op)>(op));
  }

  void rect(int x, int y, int w, int h, int c) {
    this->addOperation(OperationRect {x, y, w, h, c});
  }

  void clear() {
    this->addOperation(Operation(OperationType::Clear));
  }

  void point(int x, int y, int c) {
    this->addOperation(OperationPoint {x, y, c});
  }

  void text(int x, int y, const char* str, size_t strLen) {
    auto allocStr = (char*)malloc(strLen);
    memcpy(allocStr, str, strLen);
    this->addOperation(OperationText {x, y, allocStr, strLen});
  }
};
}
