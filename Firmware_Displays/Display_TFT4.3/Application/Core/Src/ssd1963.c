#include "ssd1963.h"
#include "main.h"
#include "font8x16.h"
#include "ui_font_aa.h"

/* Bank1 NE1, A0 -> RS: command @0x60000000, data @0x60000002 (16-bit) */
#define SSD1963_REG (*((__IO uint16_t *)0x60000000U))
#define SSD1963_RAM (*((__IO uint16_t *)0x60000002U))

static void SSD1963_WriteCmd(uint16_t cmd)
{
  SSD1963_REG = cmd;
}

static void SSD1963_WriteData(uint16_t data)
{
  SSD1963_RAM = data;
}

static void SSD1963_WriteColor(uint16_t color)
{
  SSD1963_WriteData(color);
}

#if defined(__GNUC__)
__attribute__((optimize("O3")))
#endif
static void SSD1963_BurstSolid(uint32_t count, uint16_t color)
{
  volatile uint16_t *ram = &SSD1963_RAM;

  while (count >= 16U)
  {
    *ram = color; *ram = color; *ram = color; *ram = color;
    *ram = color; *ram = color; *ram = color; *ram = color;
    *ram = color; *ram = color; *ram = color; *ram = color;
    *ram = color; *ram = color; *ram = color; *ram = color;
    count -= 16U;
  }
  while (count-- > 0U)
  {
    *ram = color;
  }
}

#if defined(__GNUC__)
__attribute__((optimize("O3")))
#endif
static void SSD1963_BurstPixels(uint32_t count, const uint16_t *src)
{
  volatile uint16_t *ram = &SSD1963_RAM;

  while (count >= 8U)
  {
    *ram = src[0];
    *ram = src[1];
    *ram = src[2];
    *ram = src[3];
    *ram = src[4];
    *ram = src[5];
    *ram = src[6];
    *ram = src[7];
    src += 8;
    count -= 8U;
  }
  while (count-- > 0U)
  {
    *ram = *src++;
  }
}

static void SSD1963_HwReset(void)
{
  HAL_GPIO_WritePin(LCD_REST_GPIO_Port, LCD_REST_Pin, GPIO_PIN_RESET);
  HAL_Delay(20);
  HAL_GPIO_WritePin(LCD_REST_GPIO_Port, LCD_REST_Pin, GPIO_PIN_SET);
  HAL_Delay(20);
}

void SSD1963_SetBacklight(uint8_t on)
{
  HAL_GPIO_WritePin(LCD_LED_GPIO_Port, LCD_LED_Pin,
                    on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void SSD1963_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  SSD1963_WriteCmd(0x2A);
  SSD1963_WriteData(x0 >> 8);
  SSD1963_WriteData(x0 & 0xFF);
  SSD1963_WriteData(x1 >> 8);
  SSD1963_WriteData(x1 & 0xFF);

  SSD1963_WriteCmd(0x2B);
  SSD1963_WriteData(y0 >> 8);
  SSD1963_WriteData(y0 & 0xFF);
  SSD1963_WriteData(y1 >> 8);
  SSD1963_WriteData(y1 & 0xFF);

  SSD1963_WriteCmd(0x2C);
}

void SSD1963_WritePixel(uint16_t x, uint16_t y, uint16_t color)
{
  if ((x >= SSD1963_WIDTH) || (y >= SSD1963_HEIGHT))
  {
    return;
  }

  SSD1963_SetWindow(x, y, x, y);
  SSD1963_WriteColor(color);
}

void SSD1963_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
  uint32_t count;

  if (x0 >= SSD1963_WIDTH)
  {
    x0 = SSD1963_WIDTH - 1U;
  }
  if (y0 >= SSD1963_HEIGHT)
  {
    y0 = SSD1963_HEIGHT - 1U;
  }
  if (x1 >= SSD1963_WIDTH)
  {
    x1 = SSD1963_WIDTH - 1U;
  }
  if (y1 >= SSD1963_HEIGHT)
  {
    y1 = SSD1963_HEIGHT - 1U;
  }
  if ((x0 > x1) || (y0 > y1))
  {
    return;
  }

  count = ((uint32_t)(x1 - x0) + 1U) * ((uint32_t)(y1 - y0) + 1U);
  SSD1963_SetWindow(x0, y0, x1, y1);
  SSD1963_BurstSolid(count, color);
}

void SSD1963_FillScreen(uint16_t color)
{
  SSD1963_Fill(0, 0, SSD1963_WIDTH - 1U, SSD1963_HEIGHT - 1U, color);
}

uint32_t SSD1963_FillScreenTimed(uint16_t color)
{
  uint32_t t0 = HAL_GetTick();
  SSD1963_FillScreen(color);
  return (HAL_GetTick() - t0);
}

void SSD1963_Init(void)
{
  SSD1963_HwReset();
  SSD1963_SetBacklight(1);

  /* PLL ~120 MHz, 10 MHz crystal (BuyDisplay / ER-TFTM043) */
  SSD1963_WriteCmd(0xE2);
  SSD1963_WriteData(0x23);
  SSD1963_WriteData(0x02);
  SSD1963_WriteData(0x54);

  SSD1963_WriteCmd(0xE0);
  SSD1963_WriteData(0x01);
  HAL_Delay(10);

  SSD1963_WriteCmd(0xE0);
  SSD1963_WriteData(0x03);
  HAL_Delay(10);

  SSD1963_WriteCmd(0x01);
  HAL_Delay(100);

  /* PCLK ниже — убирает рябь на 4.3" 800x480 BuyDisplay */
  SSD1963_WriteCmd(0xE6);
  SSD1963_WriteData(0x03);
  SSD1963_WriteData(0x33);
  SSD1963_WriteData(0x33);

  /* 24-bit TFT panel, 800x480 */
  SSD1963_WriteCmd(0xB0);
  SSD1963_WriteData(0x20);
  SSD1963_WriteData(0x00);
  SSD1963_WriteData(0x03);
  SSD1963_WriteData(0x1F);
  SSD1963_WriteData(0x01);
  SSD1963_WriteData(0xDF);
  SSD1963_WriteData(0x00);

  /* HSYNC (BuyDisplay 800BD) */
  SSD1963_WriteCmd(0xB4);
  SSD1963_WriteData(0x04);
  SSD1963_WriteData(0x1F);
  SSD1963_WriteData(0x00);
  SSD1963_WriteData(0xD2);
  SSD1963_WriteData(0x00);
  SSD1963_WriteData(0x00);
  SSD1963_WriteData(0x00);
  SSD1963_WriteData(0x00);

  /* VSYNC (BuyDisplay 800BD) */
  SSD1963_WriteCmd(0xB6);
  SSD1963_WriteData(0x02);
  SSD1963_WriteData(0x0C);
  SSD1963_WriteData(0x00);
  SSD1963_WriteData(0x22);
  SSD1963_WriteData(0x00);
  SSD1963_WriteData(0x00);
  SSD1963_WriteData(0x00);

  /* GPIO0 = LCD ON */
  SSD1963_WriteCmd(0xB8);
  SSD1963_WriteData(0x0F);
  SSD1963_WriteData(0x01);

  SSD1963_WriteCmd(0xBA);
  SSD1963_WriteData(0x01);

  /* BGR: на BuyDisplay 4.3" иначе R/B меняются местами */
  SSD1963_WriteCmd(0x36);
  SSD1963_WriteData(0x08);

  /* 16-bit 565 host interface */
  SSD1963_WriteCmd(0xF0);
  SSD1963_WriteData(0x03);

  SSD1963_WriteCmd(0xBC);
  SSD1963_WriteData(0x58); /* contrast (было 0x40) */
  SSD1963_WriteData(0x80); /* brightness */
  SSD1963_WriteData(0x50); /* saturation (было 0x40) */
  SSD1963_WriteData(0x01);
  HAL_Delay(10);

  SSD1963_SetWindow(0, 0, SSD1963_WIDTH - 1U, SSD1963_HEIGHT - 1U);

  SSD1963_WriteCmd(0x29);

  SSD1963_WriteCmd(0xBE);
  SSD1963_WriteData(0x06);
  SSD1963_WriteData(0x80);
  SSD1963_WriteData(0x01);
  SSD1963_WriteData(0xF0);
  SSD1963_WriteData(0x00);
  SSD1963_WriteData(0x00);

  SSD1963_WriteCmd(0xD0);
  SSD1963_WriteData(0x0D);
}

static uint32_t SSD1963_Utf8Next(const char **text)
{
  const uint8_t *p = (const uint8_t *)(*text);
  uint32_t cp;

  if (p[0] == 0U)
  {
    return 0U;
  }

  if (p[0] < 0x80U)
  {
    cp = p[0];
    p += 1U;
  }
  else if ((p[0] & 0xE0U) == 0xC0U)
  {
    cp = ((uint32_t)(p[0] & 0x1FU) << 6) | (uint32_t)(p[1] & 0x3FU);
    p += 2U;
  }
  else if ((p[0] & 0xF0U) == 0xE0U)
  {
    cp = ((uint32_t)(p[0] & 0x0FU) << 12) |
         ((uint32_t)(p[1] & 0x3FU) << 6) |
         (uint32_t)(p[2] & 0x3FU);
    p += 3U;
  }
  else
  {
    cp = (uint32_t)'?';
    p += 1U;
  }

  *text = (const char *)p;
  return cp;
}

void SSD1963_DrawString(uint16_t x, uint16_t y, const char *utf8,
                        uint16_t fg, uint16_t bg, uint8_t scale)
{
  uint16_t cursor_x = x;

  if ((utf8 == 0) || (scale == 0U))
  {
    return;
  }

  while (*utf8 != '\0')
  {
    uint32_t cp = SSD1963_Utf8Next(&utf8);
    const uint8_t *glyph;
    uint8_t row;
    uint8_t col;
    uint8_t sy;
    uint8_t sx;

    if (cp == 0U)
    {
      break;
    }

    glyph = Font8x16_GetGlyph(cp);

    for (row = 0U; row < FONT8X16_HEIGHT; row++)
    {
      uint8_t bits = glyph[row];
      for (col = 0U; col < FONT8X16_WIDTH; col++)
      {
        uint16_t color = ((bits & (uint8_t)(0x80U >> col)) != 0U) ? fg : bg;
        uint16_t px = (uint16_t)(cursor_x + ((uint16_t)col * (uint16_t)scale));
        uint16_t py = (uint16_t)(y + ((uint16_t)row * (uint16_t)scale));

        if (scale == 1U)
        {
          if ((px < SSD1963_WIDTH) && (py < SSD1963_HEIGHT))
          {
            SSD1963_WritePixel(px, py, color);
          }
        }
        else
        {
          for (sy = 0U; sy < scale; sy++)
          {
            for (sx = 0U; sx < scale; sx++)
            {
              uint16_t dx = (uint16_t)(px + sx);
              uint16_t dy = (uint16_t)(py + sy);
              if ((dx < SSD1963_WIDTH) && (dy < SSD1963_HEIGHT))
              {
                SSD1963_WritePixel(dx, dy, color);
              }
            }
          }
        }
      }
    }

    cursor_x = (uint16_t)(cursor_x + ((uint16_t)FONT8X16_WIDTH * (uint16_t)scale));
    if (cursor_x >= SSD1963_WIDTH)
    {
      break;
    }
  }
}

uint16_t SSD1963_TextWidth(const char *utf8, uint8_t scale)
{
  uint16_t width = 0U;
  const char *p = utf8;

  if ((utf8 == 0) || (scale == 0U))
  {
    return 0U;
  }

  while (*p != '\0')
  {
    uint32_t cp = SSD1963_Utf8Next(&p);
    if (cp == 0U)
    {
      break;
    }
    width = (uint16_t)(width + ((uint16_t)FONT8X16_WIDTH * (uint16_t)scale));
  }

  return width;
}

void SSD1963_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       const uint16_t *rgb565)
{
  uint32_t count;

  if ((rgb565 == 0) || (w == 0U) || (h == 0U))
  {
    return;
  }
  if ((x >= SSD1963_WIDTH) || (y >= SSD1963_HEIGHT))
  {
    return;
  }
  if ((x + w) > SSD1963_WIDTH)
  {
    w = (uint16_t)(SSD1963_WIDTH - x);
  }
  if ((y + h) > SSD1963_HEIGHT)
  {
    h = (uint16_t)(SSD1963_HEIGHT - y);
  }

  count = (uint32_t)w * (uint32_t)h;
  SSD1963_SetWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
  SSD1963_BurstPixels(count, rgb565);
}

uint32_t SSD1963_DrawImageTimed(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                const uint16_t *rgb565)
{
  uint32_t t0 = HAL_GetTick();
  SSD1963_DrawImage(x, y, w, h, rgb565);
  return (HAL_GetTick() - t0);
}

/* Быстрый blend RGB565: каналы упакованы, alpha 0..255 */
static uint16_t SSD1963_Blend565(uint16_t fg, uint16_t bg, uint8_t alpha)
{
  uint32_t a;
  uint32_t fg32;
  uint32_t bg32;
  uint32_t out;

  if (alpha == 0U)
  {
    return bg;
  }
  if (alpha >= 255U)
  {
    return fg;
  }

  a = (uint32_t)alpha + 1U; /* 1..256 */
  fg32 = (((uint32_t)fg | ((uint32_t)fg << 16)) & 0x07E0F81FUL);
  bg32 = (((uint32_t)bg | ((uint32_t)bg << 16)) & 0x07E0F81FUL);
  out = (((((fg32 - bg32) * a) >> 8) + bg32) & 0x07E0F81FUL);
  return (uint16_t)((out & 0xFFFFU) | (out >> 16));
}

/*
 * Раньше каждый пиксель = SetWindow + 1 запись (очень медленно).
 * Теперь одно окно на глиф и поток пикселей в RAM.
 */
#if defined(__GNUC__)
__attribute__((optimize("O3")))
#endif
void SSD1963_DrawAlpha(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       const uint8_t *alpha, uint16_t fg, uint16_t bg)
{
  uint16_t draw_w;
  uint16_t draw_h;
  uint16_t row;
  uint16_t col;
  volatile uint16_t *ram = &SSD1963_RAM;

  if ((alpha == 0) || (w == 0U) || (h == 0U))
  {
    return;
  }
  if ((x >= SSD1963_WIDTH) || (y >= SSD1963_HEIGHT))
  {
    return;
  }

  draw_w = w;
  draw_h = h;
  if ((uint32_t)x + (uint32_t)draw_w > SSD1963_WIDTH)
  {
    draw_w = (uint16_t)(SSD1963_WIDTH - x);
  }
  if ((uint32_t)y + (uint32_t)draw_h > SSD1963_HEIGHT)
  {
    draw_h = (uint16_t)(SSD1963_HEIGHT - y);
  }

  SSD1963_SetWindow(x, y,
                    (uint16_t)(x + draw_w - 1U),
                    (uint16_t)(y + draw_h - 1U));

  for (row = 0U; row < draw_h; row++)
  {
    const uint8_t *line = &alpha[(uint32_t)row * (uint32_t)w];

    for (col = 0U; col < draw_w; col++)
    {
      uint8_t a = line[col];
      uint16_t c;

      if (a == 0U)
      {
        c = bg;
      }
      else if (a >= 255U)
      {
        c = fg;
      }
      else
      {
        c = SSD1963_Blend565(fg, bg, a);
      }
      *ram = c;
    }
  }
}

void SSD1963_DrawStringAA(uint16_t x, uint16_t y, const char *utf8,
                          uint16_t fg, uint16_t bg)
{
  uint16_t cursor_x = x;
  const char *p = utf8;

  if (utf8 == 0)
  {
    return;
  }

  while (*p != '\0')
  {
    uint32_t cp = SSD1963_Utf8Next(&p);
    const UiFontAa_Glyph *glyph;

    if (cp == 0U)
    {
      break;
    }

    glyph = UiFontAa_Find(cp);
    if (glyph != 0)
    {
      SSD1963_DrawAlpha(cursor_x, y, glyph->w, glyph->h, glyph->alpha, fg, bg);
      cursor_x = (uint16_t)(cursor_x + glyph->w);
    }
    else
    {
      cursor_x = (uint16_t)(cursor_x + (UI_VALUE_FONT_H / 2U));
    }

    if (cursor_x >= SSD1963_WIDTH)
    {
      break;
    }
  }
}

uint16_t SSD1963_TextWidthAA(const char *utf8)
{
  return UiFontAa_TextWidth(utf8);
}
