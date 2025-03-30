#pragma once
#include <stdint.h>

typedef struct {
  uint16_t bitmapOffset; ///< Pointer into GFXfont->bitmap
  uint8_t width;         ///< Bitmap dimensions in pixels
  uint8_t height;        ///< Bitmap dimensions in pixels
  uint8_t xAdvance;      ///< Distance to advance cursor (x axis)
  int8_t xOffset;        ///< X dist from cursor pos to UL corner
  int8_t yOffset;        ///< Y dist from cursor pos to UL corner
} GFXglyph;

/// Data stored for FONT AS A WHOLE
typedef struct {
  uint8_t *bitmap;  ///< Glyph bitmaps, concatenated
  GFXglyph *glyph;  ///< Glyph array
  uint16_t first;   ///< ASCII extents (first char)
  uint16_t last;    ///< ASCII extents (last char)
  uint8_t yAdvance; ///< Newline distance (y axis)
} GFXfont;

const uint8_t CG_pixel_3x5Bitmaps[] = {	0x00,				/* 0x20 space */
	0xE8,				/* 0x21 exclam */
	0xB4,				/* 0x22 quotedbl */
	0xBE, 0xFA,			/* 0x23 numbersign */
	0x79, 0x3C,			/* 0x24 dollar */
	0x85, 0x42,			/* 0x25 percent */
	0x71, 0xD6,			/* 0x26 ampersand */
	0xC0,				/* 0x27 quotesingle */
	0x6A, 0x40,			/* 0x28 parenleft */
	0x95, 0x80,			/* 0x29 parenright */
	0xAB, 0xAA,			/* 0x2A asterisk */
	0x5D, 0x00,			/* 0x2B plus */
	0x60,				/* 0x2C comma */
	0xE0,				/* 0x2D hyphen */
	0x80,				/* 0x2E period */
	0x25, 0x48,			/* 0x2F slash */
	0xF6, 0xDE,			/* 0x30 zero */
	0x75, 0x40,			/* 0x31 one */
	0xC5, 0x4E,			/* 0x32 two */
	0xC5, 0x1C,			/* 0x33 three */
	0xB7, 0x92,			/* 0x34 four */
	0xF3, 0x1C,			/* 0x35 five */
	0x73, 0x54,			/* 0x36 six */
	0xE5, 0x24,			/* 0x37 seven */
	0x55, 0x54,			/* 0x38 eight */
	0x55, 0x9C,			/* 0x39 nine */
	0xA0,				/* 0x3A colon */
	0x46,				/* 0x3B semicolon */
	0x2A, 0x22,			/* 0x3C less */
	0xE3, 0x80,			/* 0x3D equal */
	0x88, 0xA8,			/* 0x3E greater */
	0xC5, 0x04,			/* 0x3F question */
	0x57, 0xC6,			/* 0x40 at */
	0x57, 0xDA,			/* 0x41 A */
	0xD7, 0x5C,			/* 0x42 B */
	0x56, 0x54,			/* 0x43 C */
	0xD6, 0xDC,			/* 0x44 D */
	0xF3, 0xCE,			/* 0x45 E */
	0xF3, 0xC8,			/* 0x46 F */
	0x72, 0x56,			/* 0x47 G */
	0xB7, 0xDA,			/* 0x48 H */
	0xF8,				/* 0x49 I */
	0x24, 0xD4,			/* 0x4A J */
	0xB7, 0x5A,			/* 0x4B K */
	0x92, 0x4E,			/* 0x4C L */
	0xBE, 0xDA,			/* 0x4D M */
	0xD6, 0xDA,			/* 0x4E N */
	0x56, 0xD4,			/* 0x4F O */
	0xD7, 0x48,			/* 0x50 P */
	0x56, 0xF6,			/* 0x51 Q */
	0xD7, 0x5A,			/* 0x52 R */
	0x71, 0x1C,			/* 0x53 S */
	0xE9, 0x24,			/* 0x54 T */
	0xB6, 0xDE,			/* 0x55 U */
	0xB6, 0xF4,			/* 0x56 V */
	0xB6, 0xFA,			/* 0x57 W */
	0xB5, 0x5A,			/* 0x58 X */
	0xB7, 0xA4,			/* 0x59 Y */
	0xE5, 0x4E,			/* 0x5A Z */
	0xEA, 0xC0,			/* 0x5B bracketleft */
	0x91, 0x12,			/* 0x5C backslash */
	0xD5, 0xC0,			/* 0x5D bracketright */
	0x54,				/* 0x5E asciicircum */
	0xE0,				/* 0x5F underscore */
	0x90,				/* 0x60 grave */
	0x57, 0xDA,			/* 0x61 a */
	0xD7, 0x5C,			/* 0x62 b */
	0x56, 0x54,			/* 0x63 c */
	0xD6, 0xDC,			/* 0x64 d */
	0xF3, 0xCE,			/* 0x65 e */
	0xF3, 0xC8,			/* 0x66 f */
	0x72, 0x56,			/* 0x67 g */
	0xB7, 0xDA,			/* 0x68 h */
	0xF8,				/* 0x69 i */
	0x24, 0xD4,			/* 0x6A j */
	0xB7, 0x5A,			/* 0x6B k */
	0x92, 0x4E,			/* 0x6C l */
	0xBE, 0xDA,			/* 0x6D m */
	0xD6, 0xDA,			/* 0x6E n */
	0x56, 0xD4,			/* 0x6F o */
	0xD7, 0x48,			/* 0x70 p */
	0x56, 0xF6,			/* 0x71 q */
	0xD7, 0x5A,			/* 0x72 r */
	0x71, 0x1C,			/* 0x73 s */
	0xE9, 0x24,			/* 0x74 t */
	0xB6, 0xDE,			/* 0x75 u */
	0xB6, 0xF4,			/* 0x76 v */
	0xB6, 0xFA,			/* 0x77 w */
	0xB5, 0x5A,			/* 0x78 x */
	0xB7, 0xA4,			/* 0x79 y */
	0xE5, 0x4E,			/* 0x7A z */
	0x6B, 0x26,			/* 0x7B braceleft */
	0xD8,				/* 0x7C bar */
	0xC9, 0xAC,			/* 0x7D braceright */
	0x78,				/* 0x7E asciitilde */
	0xFF, 0xFF, 0xFF, 0x80,	/* 0x-1 .notdef */
	0x00,				/* 0x-1 glyph1 */
	0x00,				/* 0x-1 glyph2 */
};

const GFXglyph CG_pixel_3x5Glyphs[] = {
  	{ 0, 1, 1, 2, 0, -1 }, /* 0x20 space */
	{ 1, 1, 5, 2, 0, -5 }, /* 0x21 exclam */
	{ 2, 3, 2, 4, 0, -5 }, /* 0x22 quotedbl */
	{ 3, 3, 5, 4, 0, -5 }, /* 0x23 numbersign */
	{ 5, 3, 5, 4, 0, -5 }, /* 0x24 dollar */
	{ 7, 3, 5, 4, 0, -5 }, /* 0x25 percent */
	{ 9, 3, 5, 4, 0, -5 }, /* 0x26 ampersand */
	{ 11, 1, 2, 2, 0, -5 }, /* 0x27 quotesingle */
	{ 12, 2, 5, 3, 0, -5 }, /* 0x28 parenleft */
	{ 14, 2, 5, 3, 0, -5 }, /* 0x29 parenright */
	{ 16, 3, 5, 4, 0, -5 }, /* 0x2A asterisk */
	{ 18, 3, 3, 4, 0, -4 }, /* 0x2B plus */
	{ 20, 2, 2, 3, 0, -2 }, /* 0x2C comma */
	{ 21, 3, 1, 4, 0, -3 }, /* 0x2D hyphen */
	{ 22, 1, 1, 2, 0, -1 }, /* 0x2E period */
	{ 23, 3, 5, 4, 0, -5 }, /* 0x2F slash */
	{ 25, 3, 5, 4, 0, -5 }, /* 0x30 zero */
	{ 27, 2, 5, 3, 0, -5 }, /* 0x31 one */
	{ 29, 3, 5, 4, 0, -5 }, /* 0x32 two */
	{ 31, 3, 5, 4, 0, -5 }, /* 0x33 three */
	{ 33, 3, 5, 4, 0, -5 }, /* 0x34 four */
	{ 35, 3, 5, 4, 0, -5 }, /* 0x35 five */
	{ 37, 3, 5, 4, 0, -5 }, /* 0x36 six */
	{ 39, 3, 5, 4, 0, -5 }, /* 0x37 seven */
	{ 41, 3, 5, 4, 0, -5 }, /* 0x38 eight */
	{ 43, 3, 5, 4, 0, -5 }, /* 0x39 nine */
	{ 45, 1, 3, 2, 0, -4 }, /* 0x3A colon */
	{ 46, 2, 4, 3, 0, -4 }, /* 0x3B semicolon */
	{ 47, 3, 5, 4, 0, -5 }, /* 0x3C less */
	{ 49, 3, 3, 4, 0, -4 }, /* 0x3D equal */
	{ 51, 3, 5, 4, 0, -5 }, /* 0x3E greater */
	{ 53, 3, 5, 4, 0, -5 }, /* 0x3F question */
	{ 55, 3, 5, 4, 0, -5 }, /* 0x40 at */
	{ 57, 3, 5, 4, 0, -5 }, /* 0x41 A */
	{ 59, 3, 5, 4, 0, -5 }, /* 0x42 B */
	{ 61, 3, 5, 4, 0, -5 }, /* 0x43 C */
	{ 63, 3, 5, 4, 0, -5 }, /* 0x44 D */
	{ 65, 3, 5, 4, 0, -5 }, /* 0x45 E */
	{ 67, 3, 5, 4, 0, -5 }, /* 0x46 F */
	{ 69, 3, 5, 4, 0, -5 }, /* 0x47 G */
	{ 71, 3, 5, 4, 0, -5 }, /* 0x48 H */
	{ 73, 1, 5, 2, 0, -5 }, /* 0x49 I */
	{ 74, 3, 5, 4, 0, -5 }, /* 0x4A J */
	{ 76, 3, 5, 4, 0, -5 }, /* 0x4B K */
	{ 78, 3, 5, 4, 0, -5 }, /* 0x4C L */
	{ 80, 3, 5, 4, 0, -5 }, /* 0x4D M */
	{ 82, 3, 5, 4, 0, -5 }, /* 0x4E N */
	{ 84, 3, 5, 4, 0, -5 }, /* 0x4F O */
	{ 86, 3, 5, 4, 0, -5 }, /* 0x50 P */
	{ 88, 3, 5, 4, 0, -5 }, /* 0x51 Q */
	{ 90, 3, 5, 4, 0, -5 }, /* 0x52 R */
	{ 92, 3, 5, 4, 0, -5 }, /* 0x53 S */
	{ 94, 3, 5, 4, 0, -5 }, /* 0x54 T */
	{ 96, 3, 5, 4, 0, -5 }, /* 0x55 U */
	{ 98, 3, 5, 4, 0, -5 }, /* 0x56 V */
	{ 100, 3, 5, 4, 0, -5 }, /* 0x57 W */
	{ 102, 3, 5, 4, 0, -5 }, /* 0x58 X */
	{ 104, 3, 5, 4, 0, -5 }, /* 0x59 Y */
	{ 106, 3, 5, 4, 0, -5 }, /* 0x5A Z */
	{ 108, 2, 5, 3, 0, -5 }, /* 0x5B bracketleft */
	{ 110, 3, 5, 4, 0, -5 }, /* 0x5C backslash */
	{ 112, 2, 5, 3, 0, -5 }, /* 0x5D bracketright */
	{ 114, 3, 2, 4, 0, -5 }, /* 0x5E asciicircum */
	{ 115, 3, 1, 4, 0, -1 }, /* 0x5F underscore */
	{ 116, 2, 2, 3, 0, -5 }, /* 0x60 grave */
	{ 117, 3, 5, 4, 0, -5 }, /* 0x61 a */
	{ 119, 3, 5, 4, 0, -5 }, /* 0x62 b */
	{ 121, 3, 5, 4, 0, -5 }, /* 0x63 c */
	{ 123, 3, 5, 4, 0, -5 }, /* 0x64 d */
	{ 125, 3, 5, 4, 0, -5 }, /* 0x65 e */
	{ 127, 3, 5, 4, 0, -5 }, /* 0x66 f */
	{ 129, 3, 5, 4, 0, -5 }, /* 0x67 g */
	{ 131, 3, 5, 4, 0, -5 }, /* 0x68 h */
	{ 133, 1, 5, 2, 0, -5 }, /* 0x69 i */
	{ 134, 3, 5, 4, 0, -5 }, /* 0x6A j */
	{ 136, 3, 5, 4, 0, -5 }, /* 0x6B k */
	{ 138, 3, 5, 4, 0, -5 }, /* 0x6C l */
	{ 140, 3, 5, 4, 0, -5 }, /* 0x6D m */
	{ 142, 3, 5, 4, 0, -5 }, /* 0x6E n */
	{ 144, 3, 5, 4, 0, -5 }, /* 0x6F o */
	{ 146, 3, 5, 4, 0, -5 }, /* 0x70 p */
	{ 148, 3, 5, 4, 0, -5 }, /* 0x71 q */
	{ 150, 3, 5, 4, 0, -5 }, /* 0x72 r */
	{ 152, 3, 5, 4, 0, -5 }, /* 0x73 s */
	{ 154, 3, 5, 4, 0, -5 }, /* 0x74 t */
	{ 156, 3, 5, 4, 0, -5 }, /* 0x75 u */
	{ 158, 3, 5, 4, 0, -5 }, /* 0x76 v */
	{ 160, 3, 5, 4, 0, -5 }, /* 0x77 w */
	{ 162, 3, 5, 4, 0, -5 }, /* 0x78 x */
	{ 164, 3, 5, 4, 0, -5 }, /* 0x79 y */
	{ 166, 3, 5, 4, 0, -5 }, /* 0x7A z */
	{ 168, 3, 5, 4, 0, -5 }, /* 0x7B braceleft */
	{ 170, 1, 5, 2, 0, -5 }, /* 0x7C bar */
	{ 171, 3, 5, 4, 0, -5 }, /* 0x7D braceright */
	{ 173, 3, 2, 4, 0, -4 }, /* 0x7E asciitilde */
	{ 174, 5, 5, 6, 0, -5 }, /* 0x-1 .notdef */
	{ 178, 1, 1, 2, 0, -1 }, /* 0x-1 glyph1 */
	{ 179, 1, 1, 2, 0, -1 }, /* 0x-1 glyph2 */
};

const GFXfont CG_pixel_3x52 = {
  (uint8_t  *)CG_pixel_3x5Bitmaps,
  (GFXglyph *)CG_pixel_3x5Glyphs,
  0x20, 0x7E, 6 };
