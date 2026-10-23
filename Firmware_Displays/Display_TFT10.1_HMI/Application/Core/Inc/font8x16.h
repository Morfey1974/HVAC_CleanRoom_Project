#ifndef FONT8X16_H
#define FONT8X16_H

#include <stdint.h>

#define FONT8X16_WIDTH  8U
#define FONT8X16_HEIGHT 16U

const uint8_t *Font8x16_GetGlyph(uint32_t codepoint);

#endif
