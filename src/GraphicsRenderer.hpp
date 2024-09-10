#pragma once

#include "SDL2/SDL.h"
#include <inttypes.h>
#include "CGPixel.h"


class GraphicsRenderer {
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
  void renderText(int x, int y, const char* str, const GFXfont* font = &CG_pixel_3x52) {
    int xpos = x;
    int ypos = y;
    for (int i = 0; str[i] != 0; i++) {
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


class SDLRenderer : public GraphicsRenderer {
  SDL_Window* window;
  SDL_Surface* surface;
  SDL_Renderer* renderer;
  int scaleFactor;
public:

  void config(int scaleFactor = 9) {
    this->scaleFactor = scaleFactor;
  }
  
  void init() override {
    
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Testing", SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  1024, 600, SDL_WINDOW_SHOWN);
    surface = SDL_GetWindowSurface(window);
    renderer = SDL_CreateSoftwareRenderer(surface);
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
    SDL_RenderFillRect(renderer, &rect);
  }

  void point(int x, int y, int c) override {
    int v = ((c + 1) << 4) - 1;
    SDL_SetRenderDrawColor(renderer, v, v, v, 255);
    this->rect(x, y, 1, 1, c);
  }
};
