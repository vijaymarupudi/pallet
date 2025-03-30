#include <cstdint>
#include "GraphicsInterface.hpp"

namespace pallet {

void GraphicsHardwareInterface::renderCompressedBitmap(float x, float y, int w, int h, uint8_t* data, int fc, int bc) {
  for (int ypos = 0; ypos < h; ypos++) {
    for (int xpos = 0; xpos < w; xpos++) {
      auto arrayIndex = ypos * w + xpos;
      auto byteIndex = arrayIndex / 8;
      auto bitIndex = (arrayIndex % 8);
      auto reading = (0x80 >> bitIndex) & data[byteIndex];
      auto xloc = x + xpos;
      auto yloc = y + ypos;
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
void GraphicsHardwareInterface::text(float x, float y, const char* str, size_t strLen, int fc, int bc, const GFXfont* font) {
  float xpos = x;
  float ypos = y;
  for (size_t i = 0; i < strLen; i++) {
    char c = str[i];
    if (c == '\n') {
      ypos += font->yAdvance;
      xpos = x;
      continue;
    }
    auto glyph = &font->glyph[c - font->first];
    uint8_t* data = &font->bitmap[glyph->bitmapOffset];
    float xrenderpos = xpos + glyph->xOffset;
    float yrenderpos = ypos + glyph->yOffset;
    this->renderCompressedBitmap(xrenderpos, yrenderpos, glyph->width,
                                 glyph->height, data, fc, bc);
    xpos += glyph->xAdvance;
  }
}

GraphicsTextMeasurement GraphicsHardwareInterface::measureText(const char* str, size_t strLen, const GFXfont* font) {

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

}
