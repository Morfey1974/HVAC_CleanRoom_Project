#include "lt7683.h"
#include "main.h"
#include "font8x16.h"
#include "ui_font_aa.h"
#include "ui_font_menu.h"

/* Bank1 NE1, A0 -> RS: command @0x60000000, data @0x60000002 (16-bit) */
#define LT7683_CMD  (*((__IO uint16_t *)0x60000000U))
#define LT7683_DATA (*((__IO uint16_t *)0x60000002U))

#define LT7683_REG_SRR     0x00U
#define LT7683_REG_CCR     0x01U
#define LT7683_REG_MACR    0x02U
#define LT7683_REG_ICR     0x03U
#define LT7683_REG_MRWDP   0x04U
#define LT7683_REG_PPLLC1  0x05U
#define LT7683_REG_PPLLC2  0x06U
#define LT7683_REG_MPLLC1  0x07U
#define LT7683_REG_MPLLC2  0x08U
#define LT7683_REG_SPLLC1  0x09U
#define LT7683_REG_SPLLC2  0x0AU
#define LT7683_REG_MPWCTR  0x10U
#define LT7683_REG_PIPCDEP 0x11U
#define LT7683_REG_DPCR    0x12U
#define LT7683_REG_PCSR    0x13U
#define LT7683_REG_HDWR    0x14U
#define LT7683_REG_HDWFTR  0x15U
#define LT7683_REG_HNDR    0x16U
#define LT7683_REG_HNDFTR  0x17U
#define LT7683_REG_HSTR    0x18U
#define LT7683_REG_HPWR    0x19U
#define LT7683_REG_VDHR0   0x1AU
#define LT7683_REG_VDHR1   0x1BU
#define LT7683_REG_VNDR0   0x1CU
#define LT7683_REG_VNDR1   0x1DU
#define LT7683_REG_VSTR    0x1EU
#define LT7683_REG_VPWR    0x1FU
#define LT7683_REG_MISA0   0x20U
#define LT7683_REG_MIW0    0x24U
#define LT7683_REG_MWULX0  0x26U
#define LT7683_REG_MWULY0  0x28U
#define LT7683_REG_CVSSA0  0x50U
#define LT7683_REG_CVS_IMWTH0 0x54U
#define LT7683_REG_AWUL_X0 0x56U
#define LT7683_REG_AWUL_Y0 0x58U
#define LT7683_REG_AW_WTH0 0x5AU
#define LT7683_REG_AW_HT0  0x5CU
#define LT7683_REG_AW_COLOR 0x5EU
#define LT7683_REG_CURH0   0x5FU
#define LT7683_REG_CURV0   0x61U
#define LT7683_REG_SDRAR   0xE0U
#define LT7683_REG_SDRMD   0xE1U
#define LT7683_REG_SDR_REF0 0xE2U
#define LT7683_REG_SDR_REF1 0xE3U
#define LT7683_REG_SDRCR   0xE4U

static uint8_t LT7683_Status(void)
{
  return (uint8_t)LT7683_CMD;
}

/* WAIT# low = busy. Do not poll STSR on the bus during pixel writes. */
static void LT7683_WaitReady(void)
{
  uint32_t t0 = HAL_GetTick();

  while (HAL_GPIO_ReadPin(LCD_WAIT_GPIO_Port, LCD_WAIT_Pin) == GPIO_PIN_RESET)
  {
    if ((HAL_GetTick() - t0) > 50U)
    {
      break;
    }
  }
}

static void LT7683_WriteReg(uint8_t reg, uint8_t val)
{
  LT7683_WaitReady();
  LT7683_CMD = reg;
  LT7683_DATA = val;
}

static void LT7683_EndRamWrite(void)
{
  volatile uint32_t i;

  __DSB();
  LT7683_WaitReady();
  for (i = 0U; i < 4000U; i++)
  {
    __NOP();
  }
  LT7683_CMD = LT7683_REG_ICR;
  LT7683_DATA = 0x00U;
  LT7683_WaitReady();
}

static void LT7683_WriteReg16(uint8_t reg_lo, uint16_t val)
{
  LT7683_WriteReg(reg_lo, (uint8_t)(val & 0xFFU));
  LT7683_WriteReg((uint8_t)(reg_lo + 1U), (uint8_t)(val >> 8));
}

static void LT7683_WriteRegRaw(uint8_t reg, uint8_t val)
{
  LT7683_CMD = reg;
  LT7683_DATA = val;
}

#if defined(__GNUC__)
__attribute__((optimize("O3")))
#endif
static void LT7683_BurstSolid(uint32_t count, uint16_t color)
{
  volatile uint16_t *ram = &LT7683_DATA;

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
  LT7683_EndRamWrite();
}

#if defined(__GNUC__)
__attribute__((optimize("O3")))
#endif
static void LT7683_BurstPixels(uint32_t count, const uint16_t *src)
{
  volatile uint16_t *ram = &LT7683_DATA;

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
  LT7683_EndRamWrite();
}

static void LT7683_HwReset(void)
{
  HAL_GPIO_WritePin(LCD_REST_GPIO_Port, LCD_REST_Pin, GPIO_PIN_RESET);
  HAL_Delay(50);
  HAL_GPIO_WritePin(LCD_REST_GPIO_Port, LCD_REST_Pin, GPIO_PIN_SET);
  HAL_Delay(50);
}

void LT7683_SetBacklight(uint8_t on)
{
  HAL_GPIO_WritePin(LCD_LED_GPIO_Port, LCD_LED_Pin,
                    on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void LT7683_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  uint16_t w;
  uint16_t h;

  if (x1 < x0)
  {
    uint16_t t = x0;
    x0 = x1;
    x1 = t;
  }
  if (y1 < y0)
  {
    uint16_t t = y0;
    y0 = y1;
    y1 = t;
  }

  w = (uint16_t)(x1 - x0 + 1U);
  h = (uint16_t)(y1 - y0 + 1U);

  LT7683_WriteReg16(LT7683_REG_AWUL_X0, x0);
  LT7683_WriteReg16(LT7683_REG_AWUL_Y0, y0);
  LT7683_WriteReg16(LT7683_REG_AW_WTH0, w);
  LT7683_WriteReg16(LT7683_REG_AW_HT0, h);
  LT7683_WriteReg16(LT7683_REG_CURH0, x0);
  LT7683_WriteReg16(LT7683_REG_CURV0, y0);

  LT7683_WaitReady();
  LT7683_CMD = LT7683_REG_MRWDP;
  __DSB();
}

void LT7683_WritePixel(uint16_t x, uint16_t y, uint16_t color)
{
  if ((x >= LT7683_WIDTH) || (y >= LT7683_HEIGHT))
  {
    return;
  }

  LT7683_SetWindow(x, y, x, y);
  LT7683_DATA = color;
}

void LT7683_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
  uint32_t count;

  if (x0 >= LT7683_WIDTH)
  {
    x0 = LT7683_WIDTH - 1U;
  }
  if (y0 >= LT7683_HEIGHT)
  {
    y0 = LT7683_HEIGHT - 1U;
  }
  if (x1 >= LT7683_WIDTH)
  {
    x1 = LT7683_WIDTH - 1U;
  }
  if (y1 >= LT7683_HEIGHT)
  {
    y1 = LT7683_HEIGHT - 1U;
  }
  if ((x0 > x1) || (y0 > y1))
  {
    return;
  }

  count = ((uint32_t)(x1 - x0) + 1U) * ((uint32_t)(y1 - y0) + 1U);
  LT7683_SetWindow(x0, y0, x1, y1);
  LT7683_BurstSolid(count, color);
}

void LT7683_FillScreen(uint16_t color)
{
  LT7683_Fill(0, 0, LT7683_WIDTH - 1U, LT7683_HEIGHT - 1U, color);
}

uint32_t LT7683_FillScreenTimed(uint16_t color)
{
  uint32_t t0 = HAL_GetTick();
  LT7683_FillScreen(color);
  return (HAL_GetTick() - t0);
}

static void LT7683_PllInit(void)
{
  /* 10 MHz crystal, same sequence that showed a clean logo/title */
  LT7683_WriteRegRaw(LT7683_REG_PPLLC1, 0x04U);
  LT7683_WriteRegRaw(LT7683_REG_PPLLC2, 0x13U);
  LT7683_WriteRegRaw(LT7683_REG_MPLLC1, 0x02U);
  LT7683_WriteRegRaw(LT7683_REG_MPLLC2, 0x17U);
  LT7683_WriteRegRaw(LT7683_REG_SPLLC1, 0x02U);
  LT7683_WriteRegRaw(LT7683_REG_SPLLC2, 0x17U);
  HAL_Delay(1);
  LT7683_WriteRegRaw(LT7683_REG_CCR, 0x00U);
  HAL_Delay(10);
  LT7683_WriteRegRaw(LT7683_REG_CCR, 0x80U);
  HAL_Delay(10);
  LT7683_WriteRegRaw(LT7683_REG_CCR, 0x81U); /* PLL on, 16-bit host, 24-bit TFT */
  HAL_Delay(10);
}

static void LT7683_SdramInit(void)
{
  uint32_t t0;

  LT7683_WriteReg(LT7683_REG_SDRAR, 0x29U);
  LT7683_WriteReg(LT7683_REG_SDRMD, 0x03U);
  LT7683_WriteReg(LT7683_REG_SDR_REF0, 0x53U);
  LT7683_WriteReg(LT7683_REG_SDR_REF1, 0x07U);
  LT7683_WriteReg(LT7683_REG_SDRCR, 0x01U);

  t0 = HAL_GetTick();
  while ((LT7683_Status() & 0x04U) == 0U)
  {
    if ((HAL_GetTick() - t0) > 500U)
    {
      break;
    }
  }
}

static void LT7683_PanelInit(void)
{
  LT7683_WriteReg(LT7683_REG_MACR, 0x00U);
  LT7683_WriteReg(LT7683_REG_ICR, 0x00U);     /* graphic mode */
  LT7683_WriteReg(LT7683_REG_MPWCTR, 0x04U);  /* main 16 bpp (bits 3:2) */
  LT7683_WriteReg(LT7683_REG_PIPCDEP, 0x05U);
  LT7683_WriteReg(LT7683_REG_PCSR, 0xC0U);    /* HSYNC/VSYNC high, DE high */
  LT7683_WriteReg(LT7683_REG_DPCR, 0x00U);    /* display off while timing is set */

  /* 1024 x 600, BuyDisplay ER-TFTM101-1 porches */
  LT7683_WriteReg(LT7683_REG_HDWR, (uint8_t)((LT7683_WIDTH / 8U) - 1U));
  LT7683_WriteReg(LT7683_REG_HDWFTR, 0x00U);
  LT7683_WriteReg(LT7683_REG_HNDR, (uint8_t)((160U / 8U) - 1U));
  LT7683_WriteReg(LT7683_REG_HNDFTR, 0x00U);
  LT7683_WriteReg(LT7683_REG_HSTR, (uint8_t)((160U / 8U) - 1U));
  LT7683_WriteReg(LT7683_REG_HPWR, (uint8_t)((70U / 8U) - 1U));
  LT7683_WriteReg16(LT7683_REG_VDHR0, (uint16_t)(LT7683_HEIGHT - 1U));
  LT7683_WriteReg16(LT7683_REG_VNDR0, (uint16_t)(23U - 1U));
  LT7683_WriteReg(LT7683_REG_VSTR, (uint8_t)(12U - 1U));
  LT7683_WriteReg(LT7683_REG_VPWR, (uint8_t)(10U - 1U));

  LT7683_WriteReg16(LT7683_REG_MISA0, 0U);
  LT7683_WriteReg16(LT7683_REG_MISA0 + 2U, 0U);
  LT7683_WriteReg16(LT7683_REG_MIW0, LT7683_WIDTH);
  LT7683_WriteReg16(LT7683_REG_MWULX0, 0U);
  LT7683_WriteReg16(LT7683_REG_MWULY0, 0U);

  LT7683_WriteReg16(LT7683_REG_CVSSA0, 0U);
  LT7683_WriteReg16(LT7683_REG_CVSSA0 + 2U, 0U);
  LT7683_WriteReg16(LT7683_REG_CVS_IMWTH0, LT7683_WIDTH);
  LT7683_WriteReg16(LT7683_REG_AWUL_X0, 0U);
  LT7683_WriteReg16(LT7683_REG_AWUL_Y0, 0U);
  LT7683_WriteReg16(LT7683_REG_AW_WTH0, LT7683_WIDTH);
  LT7683_WriteReg16(LT7683_REG_AW_HT0, LT7683_HEIGHT);
  LT7683_WriteReg(LT7683_REG_AW_COLOR, 0x01U); /* 16 bpp canvas */
  LT7683_WriteReg16(LT7683_REG_CURH0, 0U);
  LT7683_WriteReg16(LT7683_REG_CURV0, 0U);

  LT7683_WriteReg(LT7683_REG_DPCR, 0xC0U); /* PCLK falling, display on */
}

void LT7683_Init(void)
{
  uint32_t t0;

  LT7683_HwReset();
  LT7683_SetBacklight(1);

  t0 = HAL_GetTick();
  while ((LT7683_Status() & 0x02U) != 0U)
  {
    if ((HAL_GetTick() - t0) > 200U)
    {
      break;
    }
  }

  LT7683_WriteRegRaw(LT7683_REG_SRR, 0x01U);
  HAL_Delay(10);
  LT7683_PllInit();
  LT7683_SdramInit();
  LT7683_PanelInit();
  LT7683_FillScreen(LT7683_COLOR_BLACK);
}

static uint32_t LT7683_Utf8Next(const char **text)
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

void LT7683_DrawString(uint16_t x, uint16_t y, const char *utf8,
                       uint16_t fg, uint16_t bg, uint8_t scale)
{
  uint16_t cursor_x = x;
  uint16_t buf[FONT8X16_WIDTH * FONT8X16_HEIGHT * 4U];

  if ((utf8 == 0) || (scale == 0U) || (scale > 2U))
  {
    return;
  }

  while (*utf8 != '\0')
  {
    uint32_t cp = LT7683_Utf8Next(&utf8);
    const uint8_t *glyph;
    uint16_t gw;
    uint16_t gh;
    uint16_t n;
    uint8_t row;
    uint8_t col;
    uint8_t sy;
    uint8_t sx;

    if (cp == 0U)
    {
      break;
    }

    glyph = Font8x16_GetGlyph(cp);
    gw = (uint16_t)((uint16_t)FONT8X16_WIDTH * (uint16_t)scale);
    gh = (uint16_t)((uint16_t)FONT8X16_HEIGHT * (uint16_t)scale);
    n = 0U;

    for (row = 0U; row < FONT8X16_HEIGHT; row++)
    {
      uint8_t bits = glyph[row];
      for (sy = 0U; sy < scale; sy++)
      {
        for (col = 0U; col < FONT8X16_WIDTH; col++)
        {
          uint16_t color = ((bits & (uint8_t)(0x80U >> col)) != 0U) ? fg : bg;
          for (sx = 0U; sx < scale; sx++)
          {
            buf[n++] = color;
          }
        }
      }
    }

    if (((uint32_t)cursor_x + gw) > LT7683_WIDTH)
    {
      break;
    }
    if (((uint32_t)y + gh) > LT7683_HEIGHT)
    {
      break;
    }

    LT7683_SetWindow(cursor_x, y,
                     (uint16_t)(cursor_x + gw - 1U),
                     (uint16_t)(y + gh - 1U));
    LT7683_BurstPixels((uint32_t)gw * (uint32_t)gh, buf);

    cursor_x = (uint16_t)(cursor_x + gw);
    if (cursor_x >= LT7683_WIDTH)
    {
      break;
    }
  }
}

uint16_t LT7683_TextWidth(const char *utf8, uint8_t scale)
{
  uint16_t width = 0U;
  const char *p = utf8;

  if ((utf8 == 0) || (scale == 0U))
  {
    return 0U;
  }

  while (*p != '\0')
  {
    uint32_t cp = LT7683_Utf8Next(&p);
    if (cp == 0U)
    {
      break;
    }
    width = (uint16_t)(width + ((uint16_t)FONT8X16_WIDTH * (uint16_t)scale));
  }

  return width;
}

void LT7683_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      const uint16_t *rgb565)
{
  uint32_t count;

  if ((rgb565 == 0) || (w == 0U) || (h == 0U))
  {
    return;
  }
  if ((x >= LT7683_WIDTH) || (y >= LT7683_HEIGHT))
  {
    return;
  }
  if ((x + w) > LT7683_WIDTH)
  {
    w = (uint16_t)(LT7683_WIDTH - x);
  }
  if ((y + h) > LT7683_HEIGHT)
  {
    h = (uint16_t)(LT7683_HEIGHT - y);
  }

  count = (uint32_t)w * (uint32_t)h;
  LT7683_SetWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
  LT7683_BurstPixels(count, rgb565);
}

#if defined(__GNUC__)
__attribute__((optimize("O3")))
#endif
void LT7683_DrawRle8(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     const uint8_t *data, uint32_t nbytes)
{
  volatile uint16_t *ram = &LT7683_DATA;
  uint32_t expected;
  uint32_t written = 0U;
  uint32_t i = 0U;

  if ((data == 0) || (w == 0U) || (h == 0U) || (nbytes < 3U))
  {
    return;
  }
  if ((x >= LT7683_WIDTH) || (y >= LT7683_HEIGHT))
  {
    return;
  }
  if ((x + w) > LT7683_WIDTH)
  {
    w = (uint16_t)(LT7683_WIDTH - x);
  }
  if ((y + h) > LT7683_HEIGHT)
  {
    h = (uint16_t)(LT7683_HEIGHT - y);
  }

  expected = (uint32_t)w * (uint32_t)h;
  LT7683_SetWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));

  while ((i + 2U) < nbytes)
  {
    uint32_t n = data[i];
    uint16_t color = (uint16_t)data[i + 1U] | ((uint16_t)data[i + 2U] << 8);
    i += 3U;
    if (n == 0U)
    {
      continue;
    }
    if ((written + n) > expected)
    {
      n = expected - written;
    }
    written += n;
    while (n >= 16U)
    {
      *ram = color; *ram = color; *ram = color; *ram = color;
      *ram = color; *ram = color; *ram = color; *ram = color;
      *ram = color; *ram = color; *ram = color; *ram = color;
      *ram = color; *ram = color; *ram = color; *ram = color;
      n -= 16U;
    }
    while (n-- > 0U)
    {
      *ram = color;
    }
    if (written >= expected)
    {
      break;
    }
  }

  LT7683_EndRamWrite();
}

uint32_t LT7683_DrawImageTimed(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                               const uint16_t *rgb565)
{
  uint32_t t0 = HAL_GetTick();
  LT7683_DrawImage(x, y, w, h, rgb565);
  return (HAL_GetTick() - t0);
}

#if defined(__GNUC__)
__attribute__((optimize("O3")))
#endif
void LT7683_DrawAlpha(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      const uint8_t *alpha, uint16_t fg, uint16_t bg)
{
  uint16_t draw_w;
  uint16_t draw_h;
  uint16_t row;
  uint16_t col;
  volatile uint16_t *ram = &LT7683_DATA;

  if ((alpha == 0) || (w == 0U) || (h == 0U))
  {
    return;
  }
  if ((x >= LT7683_WIDTH) || (y >= LT7683_HEIGHT))
  {
    return;
  }

  draw_w = w;
  draw_h = h;
  if ((uint32_t)x + (uint32_t)draw_w > LT7683_WIDTH)
  {
    draw_w = (uint16_t)(LT7683_WIDTH - x);
  }
  if ((uint32_t)y + (uint32_t)draw_h > LT7683_HEIGHT)
  {
    draw_h = (uint16_t)(LT7683_HEIGHT - y);
  }

  LT7683_SetWindow(x, y,
                   (uint16_t)(x + draw_w - 1U),
                   (uint16_t)(y + draw_h - 1U));

  for (row = 0U; row < draw_h; row++)
  {
    const uint8_t *line = &alpha[(uint32_t)row * (uint32_t)w];

    for (col = 0U; col < draw_w; col++)
    {
      uint8_t a = line[col];
      uint16_t c;

      /* Weak coverage becomes the column color so orange/blue
         edges do not leave dark specks (green hid them). */
      if (a < 128U)
      {
        c = bg;
      }
      else
      {
        c = fg;
      }
      *ram = c;
    }
  }
  LT7683_EndRamWrite();
}

void LT7683_DrawStringAA(uint16_t x, uint16_t y, const char *utf8,
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
    uint32_t cp = LT7683_Utf8Next(&p);
    const UiFontAa_Glyph *glyph;

    if (cp == 0U)
    {
      break;
    }

    glyph = UiFontAa_Find(cp);
    if (glyph != 0)
    {
      LT7683_DrawAlpha(cursor_x, y, glyph->w, glyph->h, glyph->alpha, fg, bg);
      cursor_x = (uint16_t)(cursor_x + glyph->w);
    }
    else
    {
      cursor_x = (uint16_t)(cursor_x + (UI_VALUE_FONT_H / 2U));
    }

    if (cursor_x >= LT7683_WIDTH)
    {
      break;
    }
  }
}

uint16_t LT7683_TextWidthAA(const char *utf8)
{
  return UiFontAa_TextWidth(utf8);
}

void LT7683_DrawStringMenuAA(uint16_t x, uint16_t y, const char *utf8,
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
    uint32_t cp = LT7683_Utf8Next(&p);
    const UiFontAa_Glyph *glyph;

    if (cp == 0U)
    {
      break;
    }

    glyph = UiMenuFont_Find(cp);
    if (glyph != 0)
    {
      LT7683_DrawAlpha(cursor_x, y, glyph->w, glyph->h, glyph->alpha, fg, bg);
      cursor_x = (uint16_t)(cursor_x + glyph->w);
    }
    else
    {
      cursor_x = (uint16_t)(cursor_x + (UI_MENU_FONT_H / 3U));
    }

    if (cursor_x >= LT7683_WIDTH)
    {
      break;
    }
  }
}

uint16_t LT7683_TextWidthMenuAA(const char *utf8)
{
  return UiMenuFont_TextWidth(utf8);
}
