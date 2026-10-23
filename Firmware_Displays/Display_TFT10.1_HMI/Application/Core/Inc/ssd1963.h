#ifndef SSD1963_H
#define SSD1963_H

#include "lt7683.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1963_WIDTH   LT7683_WIDTH
#define SSD1963_HEIGHT  LT7683_HEIGHT

#define SSD1963_COLOR_BLACK   LT7683_COLOR_BLACK
#define SSD1963_COLOR_WHITE   LT7683_COLOR_WHITE
#define SSD1963_COLOR_RED     LT7683_COLOR_RED
#define SSD1963_COLOR_GREEN   LT7683_COLOR_GREEN
#define SSD1963_COLOR_BLUE    LT7683_COLOR_BLUE
#define SSD1963_COLOR_YELLOW  LT7683_COLOR_YELLOW
#define SSD1963_COLOR_CYAN    LT7683_COLOR_CYAN
#define SSD1963_COLOR_MAGENTA LT7683_COLOR_MAGENTA
#define SSD1963_COLOR_LIME    LT7683_COLOR_LIME

#define SSD1963_RGB565  LT7683_RGB565

#define SSD1963_Init            LT7683_Init
#define SSD1963_SetBacklight    LT7683_SetBacklight
#define SSD1963_SetWindow       LT7683_SetWindow
#define SSD1963_WritePixel      LT7683_WritePixel
#define SSD1963_Fill            LT7683_Fill
#define SSD1963_FillScreen      LT7683_FillScreen
#define SSD1963_DrawString      LT7683_DrawString
#define SSD1963_DrawImage       LT7683_DrawImage
#define SSD1963_FillScreenTimed LT7683_FillScreenTimed
#define SSD1963_DrawImageTimed  LT7683_DrawImageTimed
#define SSD1963_DrawAlpha       LT7683_DrawAlpha
#define SSD1963_DrawStringAA      LT7683_DrawStringAA
#define SSD1963_DrawStringMenuAA  LT7683_DrawStringMenuAA
#define SSD1963_TextWidth         LT7683_TextWidth
#define SSD1963_TextWidthAA       LT7683_TextWidthAA
#define SSD1963_TextWidthMenuAA   LT7683_TextWidthMenuAA

#ifdef __cplusplus
}
#endif

#endif /* SSD1963_H */
