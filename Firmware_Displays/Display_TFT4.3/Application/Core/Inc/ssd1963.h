#ifndef SSD1963_H
#define SSD1963_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SSD1963_WIDTH   800U
#define SSD1963_HEIGHT  480U

#define SSD1963_COLOR_BLACK   0x0000U
#define SSD1963_COLOR_WHITE   0xFFFFU
#define SSD1963_COLOR_RED     0xF800U
#define SSD1963_COLOR_GREEN   0x07E0U
#define SSD1963_COLOR_BLUE    0x001FU
#define SSD1963_COLOR_YELLOW  0xFFE0U
#define SSD1963_COLOR_CYAN    0x07FFU
#define SSD1963_COLOR_MAGENTA 0xF81FU
/* Салатовый ≈ RGB(173, 255, 47) */
#define SSD1963_COLOR_LIME    0xAFE5U

#define SSD1963_RGB565(r, g, b) \
  ((uint16_t)((((r) & 0xF8U) << 8) | (((g) & 0xFCU) << 3) | (((b) & 0xF8U) >> 3)))

void SSD1963_Init(void);
void SSD1963_SetBacklight(uint8_t on);
void SSD1963_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void SSD1963_WritePixel(uint16_t x, uint16_t y, uint16_t color);
void SSD1963_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void SSD1963_FillScreen(uint16_t color);
void SSD1963_DrawString(uint16_t x, uint16_t y, const char *utf8,
                        uint16_t fg, uint16_t bg, uint8_t scale);
void SSD1963_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       const uint16_t *rgb565);
/* Возвращает длительность отрисовки в миллисекундах (HAL_GetTick). */
uint32_t SSD1963_FillScreenTimed(uint16_t color);
uint32_t SSD1963_DrawImageTimed(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                const uint16_t *rgb565);
void SSD1963_DrawAlpha(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       const uint8_t *alpha, uint16_t fg, uint16_t bg);
void SSD1963_DrawStringAA(uint16_t x, uint16_t y, const char *utf8,
                          uint16_t fg, uint16_t bg);
uint16_t SSD1963_TextWidth(const char *utf8, uint8_t scale);
uint16_t SSD1963_TextWidthAA(const char *utf8);

#ifdef __cplusplus
}
#endif

#endif /* SSD1963_H */
