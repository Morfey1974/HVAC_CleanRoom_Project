#ifndef LT7683_H
#define LT7683_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define LT7683_WIDTH   1024U
#define LT7683_HEIGHT  600U

#define LT7683_COLOR_BLACK   0x0000U
#define LT7683_COLOR_WHITE   0xFFFFU
#define LT7683_COLOR_RED     0xF800U
#define LT7683_COLOR_GREEN   0x07E0U
#define LT7683_COLOR_BLUE    0x001FU
#define LT7683_COLOR_YELLOW  0xFFE0U
#define LT7683_COLOR_CYAN    0x07FFU
#define LT7683_COLOR_MAGENTA 0xF81FU
#define LT7683_COLOR_LIME    0xAFE5U

#define LT7683_RGB565(r, g, b) \
  ((uint16_t)((((r) & 0xF8U) << 8) | (((g) & 0xFCU) << 3) | (((b) & 0xF8U) >> 3)))

void LT7683_Init(void);
void LT7683_SetBacklight(uint8_t on);
void LT7683_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LT7683_WritePixel(uint16_t x, uint16_t y, uint16_t color);
void LT7683_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void LT7683_FillScreen(uint16_t color);
void LT7683_DrawString(uint16_t x, uint16_t y, const char *utf8,
                       uint16_t fg, uint16_t bg, uint8_t scale);
void LT7683_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      const uint16_t *rgb565);
void LT7683_DrawRle8(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     const uint8_t *data, uint32_t nbytes);
uint32_t LT7683_FillScreenTimed(uint16_t color);
uint32_t LT7683_DrawImageTimed(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                               const uint16_t *rgb565);
void LT7683_DrawAlpha(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      const uint8_t *alpha, uint16_t fg, uint16_t bg);
void LT7683_DrawStringAA(uint16_t x, uint16_t y, const char *utf8,
                         uint16_t fg, uint16_t bg);
void LT7683_DrawStringMenuAA(uint16_t x, uint16_t y, const char *utf8,
                             uint16_t fg, uint16_t bg);
uint16_t LT7683_TextWidth(const char *utf8, uint8_t scale);
uint16_t LT7683_TextWidthAA(const char *utf8);
uint16_t LT7683_TextWidthMenuAA(const char *utf8);

#ifdef __cplusplus
}
#endif

#endif /* LT7683_H */
