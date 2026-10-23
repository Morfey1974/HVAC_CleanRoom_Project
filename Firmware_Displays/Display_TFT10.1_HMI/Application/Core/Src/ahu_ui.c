#include "ahu_ui.h"
#include "ssd1963.h"
#include "ui_font_menu.h"
#if AHU_UI_USE_SNAP
#include "ahu_snap.h"
#endif

#include <stdio.h>

#define ISO_DX     28
#define ISO_DY     18
#define CASE_X     78U
#define CASE_Y     232U
#define CASE_H     214U
#define CASE_W     760U
#define INFO_Y     548U
#define INFO_H     48U
#define CHIP_H     28U

#define COL_BG     SSD1963_RGB565(232, 238, 245)
#define COL_CASE   SSD1963_RGB565(226, 232, 240)
#define COL_TOP    SSD1963_RGB565(241, 245, 249)
#define COL_SIDE   SSD1963_RGB565(203, 213, 225)
#define COL_LINE   SSD1963_RGB565(148, 163, 184)
#define COL_FLOOR  SSD1963_RGB565(203, 213, 225)
#define COL_BASE   SSD1963_RGB565(100, 116, 139)
#define COL_FILT   SSD1963_RGB565(34, 197, 94)
#define COL_FILTT  SSD1963_RGB565(74, 222, 128)
#define COL_FAN    SSD1963_RGB565(37, 99, 235)
#define COL_FANT   SSD1963_RGB565(59, 130, 246)
#define COL_FANS   SSD1963_RGB565(29, 78, 216)
#define COL_HUB    SSD1963_RGB565(15, 23, 42)
#define COL_BLADE  SSD1963_RGB565(226, 232, 240)
#define COL_MOT    SSD1963_RGB565(249, 115, 22)
#define COL_COIL   SSD1963_RGB565(203, 213, 225)
#define COL_PIPE   SSD1963_RGB565(180, 83, 9)
#define COL_DX     SSD1963_RGB565(29, 78, 216)
#define COL_HOOD   SSD1963_RGB565(226, 232, 240)
#define COL_INK    SSD1963_RGB565(51, 65, 85)
#define COL_CHIP   SSD1963_COLOR_WHITE
#define COL_CHIPL  SSD1963_RGB565(203, 213, 225)
#define COL_SEL    SSD1963_RGB565(8, 145, 178)
#define COL_INFO   SSD1963_RGB565(248, 250, 252)
#define COL_HINT   SSD1963_RGB565(100, 116, 139)

typedef struct
{
  uint16_t x;
  uint16_t w;
  const char *short_name;
  const char *full_name;
} AhuSec;

static const AhuSec s_sec[AHU_SEC_COUNT] = {
    { 0U, 72U, "Вход", "Вход с улицы" },
    { 72U, 108U, "Фильтр", "Фильтр грубой очистки" },
    { 180U, 108U, "Фильтр", "Фильтр тонкой очистки" },
    { 288U, 168U, "Вентилятор EC", "Вентилятор EC" },
    { 456U, 112U, "Охладитель", "Охладитель DX" },
    { 568U, 108U, "Фильтр 95", "Фильтр 95%" },
    { 676U, 84U, "Выход", "Подача в зал" },
};

static uint8_t s_drawn;
static uint8_t s_selected = AHU_SEC_NONE;
static uint8_t s_last_sel = 0xFEU;
static int16_t s_last_speed = -1;
static int16_t s_last_t = -1000;
static uint16_t s_last_phase = 0xFFFFU;
static uint16_t s_fan_cx;
static uint16_t s_fan_cy;
static uint16_t s_fan_r;

static int16_t Ahu_Sin1000(uint32_t deg)
{
  static const int16_t table[91] = {
      0, 17, 35, 52, 70, 87, 105, 122, 139, 156, 174, 191, 208, 225, 242, 259,
      276, 292, 309, 326, 342, 358, 375, 391, 407, 423, 438, 454, 469, 485, 500,
      515, 530, 545, 559, 574, 588, 602, 616, 629, 643, 656, 669, 682, 695, 707,
      719, 731, 743, 755, 766, 777, 788, 799, 809, 819, 829, 839, 848, 857, 866,
      875, 883, 891, 899, 906, 914, 921, 927, 934, 940, 946, 951, 956, 961, 966,
      970, 974, 978, 982, 985, 988, 990, 993, 995, 996, 998, 999, 999, 1000, 1000
  };
  uint32_t d = deg % 360U;
  if (d <= 90U)
  {
    return table[d];
  }
  if (d <= 180U)
  {
    return table[180U - d];
  }
  if (d <= 270U)
  {
    return (int16_t)(-table[d - 180U]);
  }
  return (int16_t)(-table[360U - d]);
}

static uint16_t Ahu_ISqrt(uint32_t n)
{
  uint32_t x = n;
  uint32_t y = 0U;
  uint32_t b = 1UL << 30;

  while (b > n)
  {
    b >>= 2;
  }
  while (b != 0U)
  {
    if (x >= (y + b))
    {
      x -= (y + b);
      y = (y >> 1) + b;
    }
    else
    {
      y >>= 1;
    }
    b >>= 2;
  }
  return (uint16_t)y;
}

static void Ahu_Fill(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
  if (x0 > x1)
  {
    int32_t t = x0;
    x0 = x1;
    x1 = t;
  }
  if (y0 > y1)
  {
    int32_t t = y0;
    y0 = y1;
    y1 = t;
  }
  if ((x1 < 0) || (y1 < 0) || (x0 >= (int32_t)SSD1963_WIDTH) || (y0 >= (int32_t)SSD1963_HEIGHT))
  {
    return;
  }
  if (x0 < 0)
  {
    x0 = 0;
  }
  if (y0 < 0)
  {
    y0 = 0;
  }
  if (x1 > (int32_t)(SSD1963_WIDTH - 1U))
  {
    x1 = (int32_t)(SSD1963_WIDTH - 1U);
  }
  if (y1 > (int32_t)(SSD1963_HEIGHT - 1U))
  {
    y1 = (int32_t)(SSD1963_HEIGHT - 1U);
  }
  SSD1963_Fill((uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1, c);
}

static void Ahu_IsoBox(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                       uint16_t front, uint16_t top, uint16_t side)
{
  uint16_t i;

  Ahu_Fill((int32_t)x0, (int32_t)y0, (int32_t)x1, (int32_t)y1, front);
  for (i = 0U; i <= (uint16_t)ISO_DY; i++)
  {
    uint16_t ox = (uint16_t)((i * (uint16_t)ISO_DX) / (uint16_t)ISO_DY);
    Ahu_Fill((int32_t)(x0 + ox), (int32_t)y0 - (int32_t)i,
             (int32_t)(x1 + ox), (int32_t)y0 - (int32_t)i, top);
  }
  for (i = 0U; i <= (uint16_t)ISO_DX; i++)
  {
    uint16_t oy = (uint16_t)((i * (uint16_t)ISO_DY) / (uint16_t)ISO_DX);
    Ahu_Fill((int32_t)(x1 + i), (int32_t)y0 - (int32_t)oy,
             (int32_t)(x1 + i), (int32_t)y1 - (int32_t)oy, side);
  }
}

static void Ahu_Circle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t c)
{
  int16_t y;
  int16_t rr = (int16_t)r;

  for (y = -rr; y <= rr; y++)
  {
    uint16_t x = Ahu_ISqrt((uint32_t)((int32_t)rr * (int32_t)rr - (int32_t)y * (int32_t)y));
    Ahu_Fill((int32_t)cx - (int32_t)x, (int32_t)cy + (int32_t)y,
             (int32_t)cx + (int32_t)x, (int32_t)cy + (int32_t)y, c);
  }
}

static void Ahu_TextC(uint16_t x, uint16_t y, uint16_t w, const char *s,
                      uint16_t fg, uint16_t bg)
{
  uint16_t tw;
  uint16_t tx;

  if (s == 0)
  {
    return;
  }
  tw = SSD1963_TextWidthMenuAA(s);
  tx = (tw < w) ? (uint16_t)(x + ((w - tw) / 2U)) : x;
  SSD1963_DrawStringMenuAA(tx, y, s, fg, bg);
}

static void Ahu_Chip(uint16_t cx, uint16_t y, const char *s)
{
  uint16_t tw = SSD1963_TextWidthMenuAA(s);
  uint16_t w = (uint16_t)(tw + 16U);
  uint16_t x = (cx > (w / 2U)) ? (uint16_t)(cx - (w / 2U)) : 0U;

  Ahu_Fill((int32_t)x, (int32_t)y, (int32_t)(x + w), (int32_t)(y + CHIP_H), COL_CHIP);
  Ahu_Fill((int32_t)x, (int32_t)y, (int32_t)(x + w), (int32_t)y, COL_CHIPL);
  Ahu_Fill((int32_t)x, (int32_t)(y + CHIP_H), (int32_t)(x + w), (int32_t)(y + CHIP_H), COL_CHIPL);
  SSD1963_DrawStringMenuAA((uint16_t)(x + 8U), (uint16_t)(y - 1U), s, COL_INK, COL_CHIP);
}

static uint16_t Ahu_SecX(uint8_t i)
{
  return (uint16_t)(CASE_X + s_sec[i].x);
}

static void Ahu_DrawFilter(uint16_t x, uint16_t w, const char *pct)
{
  uint16_t bx = (uint16_t)(x + (w / 3U));
  uint16_t bw = (uint16_t)(w / 3U);
  uint16_t y0 = (uint16_t)(CASE_Y + 14U);
  uint16_t y1 = (uint16_t)(CASE_Y + CASE_H - 10U);
  uint16_t i;

  Ahu_IsoBox(bx, y0, (uint16_t)(bx + bw), y1, COL_FILT, COL_FILTT, COL_CHIP);
  for (i = 0U; i < 8U; i++)
  {
    uint16_t xx = (uint16_t)(bx + bw + 4U + i);
    Ahu_Fill((int32_t)xx, (int32_t)(y0 + 6U), (int32_t)xx, (int32_t)(y1 - 6U),
             ((i & 1U) != 0U) ? COL_CHIPL : COL_TOP);
  }
  Ahu_Chip((uint16_t)(x + (w / 2U)), (uint16_t)(CASE_Y + CASE_H + 10U), pct);
}

static void Ahu_DrawFanStatic(void)
{
  uint16_t x = Ahu_SecX(AHU_SEC_FAN);
  uint16_t w = s_sec[AHU_SEC_FAN].w;
  uint16_t x0 = (uint16_t)(x + 16U);
  uint16_t x1 = (uint16_t)(x + w - 28U);
  uint16_t y0 = (uint16_t)(CASE_Y + 22U);
  uint16_t y1 = (uint16_t)(CASE_Y + CASE_H - 18U);

  Ahu_IsoBox(x0, (uint16_t)(y1 + 4U), x1, (uint16_t)(CASE_Y + CASE_H - 6U),
             SSD1963_RGB565(71, 85, 105), SSD1963_RGB565(100, 116, 139),
             SSD1963_RGB565(51, 65, 85));
  Ahu_IsoBox(x0, y0, x1, y1, COL_FAN, COL_FANT, COL_FANS);
  Ahu_IsoBox((uint16_t)(x1 + 2U), (uint16_t)(((uint32_t)y0 + y1) / 2U - 12U),
             (uint16_t)(x + w - 6U), (uint16_t)(((uint32_t)y0 + y1) / 2U + 12U),
             COL_MOT, SSD1963_RGB565(251, 146, 60), SSD1963_RGB565(234, 88, 12));

  s_fan_cx = (uint16_t)((x0 + x1) / 2U);
  s_fan_cy = (uint16_t)((y0 + y1) / 2U);
  s_fan_r = 52U;
}

static void Ahu_DrawFanBlades(uint16_t phase)
{
  uint16_t i;
  uint16_t t;
  uint16_t r = s_fan_r;

  Ahu_Circle(s_fan_cx, s_fan_cy, r, COL_HUB);
  Ahu_Circle(s_fan_cx, s_fan_cy, (uint16_t)(r - 3U), SSD1963_RGB565(30, 41, 59));

  for (i = 0U; i < 8U; i++)
  {
    uint32_t a = (uint32_t)phase + (uint32_t)i * 45U;
    int16_t cs = Ahu_Sin1000(a + 90U);
    int16_t sn = Ahu_Sin1000(a);
    for (t = 8U; t < (uint16_t)(r - 6U); t++)
    {
      int32_t px = (int32_t)s_fan_cx + (((int32_t)cs * (int32_t)t) / 1000);
      int32_t py = (int32_t)s_fan_cy + (((int32_t)sn * (int32_t)t) / 1000);
      Ahu_Fill(px - 2, py - 2, px + 2, py + 2, COL_BLADE);
    }
  }
  Ahu_Circle(s_fan_cx, s_fan_cy, 10U, SSD1963_RGB565(148, 163, 184));
}

static void Ahu_DrawCoil(void)
{
  uint16_t x = Ahu_SecX(AHU_SEC_COIL);
  uint16_t w = s_sec[AHU_SEC_COIL].w;
  uint16_t x0 = (uint16_t)(x + 28U);
  uint16_t x1 = (uint16_t)(x + w - 22U);
  uint16_t y0 = (uint16_t)(CASE_Y + 16U);
  uint16_t y1 = (uint16_t)(CASE_Y + CASE_H - 10U);
  uint16_t i;

  Ahu_IsoBox(x0, y0, x1, y1, COL_COIL, COL_TOP, COL_SIDE);
  for (i = 0U; i < 6U; i++)
  {
    uint16_t yy = (uint16_t)(y0 + 18U + (i * 28U));
    Ahu_Fill((int32_t)(x0 + 6U), (int32_t)yy, (int32_t)(x1 - 4U), (int32_t)(yy + 6U), COL_PIPE);
  }
  Ahu_Fill((int32_t)(x0 + 8U), (int32_t)(y0 + 8U), (int32_t)(x0 + 14U), (int32_t)(y1 - 8U), COL_PIPE);
  Ahu_Chip((uint16_t)(x + (w / 2U)), (uint16_t)(CASE_Y + CASE_H + 10U), "DX");
}

static void Ahu_DrawInlet(void)
{
  uint16_t x = Ahu_SecX(AHU_SEC_INLET);
  uint16_t i;
  uint16_t hx = (uint16_t)(CASE_X - 40U);

  Ahu_IsoBox(hx, (uint16_t)(CASE_Y + 16U), CASE_X, (uint16_t)(CASE_Y + CASE_H - 16U),
             COL_HOOD, COL_TOP, COL_SIDE);
  for (i = 0U; i < 6U; i++)
  {
    uint16_t y = (uint16_t)(CASE_Y + 28U + (i * 28U));
    Ahu_Fill((int32_t)(hx + 4U), (int32_t)y, (int32_t)(CASE_X - 4U), (int32_t)(y + 3U), COL_LINE);
  }
  Ahu_Fill((int32_t)(x + 10U), (int32_t)(CASE_Y + (CASE_H / 2U) - 8U),
           (int32_t)(x + s_sec[AHU_SEC_INLET].w - 8U),
           (int32_t)(CASE_Y + (CASE_H / 2U) + 8U), SSD1963_RGB565(125, 211, 252));
}

static void Ahu_DrawOutlet(void)
{
  uint16_t x1 = (uint16_t)(CASE_X + CASE_W);
  uint16_t y0 = (uint16_t)(CASE_Y + 58U);
  uint16_t y1 = (uint16_t)(CASE_Y + CASE_H - 58U);
  uint16_t bx = (uint16_t)(x1 + 10U);
  uint16_t by = (uint16_t)(((uint32_t)y0 + y1) / 2U - 18U);

  Ahu_IsoBox(x1, y0, (uint16_t)(x1 + 48U), y1, COL_SIDE, COL_TOP, COL_CHIPL);
  Ahu_Fill((int32_t)bx, (int32_t)by, (int32_t)(bx + 52U), (int32_t)(by + 36U), COL_DX);
  SSD1963_DrawString(bx + 6U, (uint16_t)(by + 4U), "SUPPLY", SSD1963_COLOR_WHITE, COL_DX, 1U);
  SSD1963_DrawString(bx + 14U, (uint16_t)(by + 20U), "AIR", SSD1963_COLOR_WHITE, COL_DX, 1U);
}

static void Ahu_DrawNames(void)
{
  uint8_t i;
  uint16_t y = (uint16_t)(CASE_Y - ISO_DY - 34U);

  for (i = 0U; i < AHU_SEC_COUNT; i++)
  {
    Ahu_TextC(Ahu_SecX(i), y, s_sec[i].w, s_sec[i].short_name, COL_INK, COL_BG);
  }
}

static void Ahu_DrawSelect(uint8_t sec, uint16_t color)
{
  uint16_t x;
  uint16_t x1;
  uint16_t y0 = CASE_Y;
  uint16_t y1 = (uint16_t)(CASE_Y + CASE_H);

  if (sec >= AHU_SEC_COUNT)
  {
    return;
  }
  x = Ahu_SecX(sec);
  x1 = (uint16_t)(x + s_sec[sec].w);
  Ahu_Fill((int32_t)x, (int32_t)y0, (int32_t)x1, (int32_t)(y0 + 3U), color);
  Ahu_Fill((int32_t)x, (int32_t)(y1 - 3U), (int32_t)x1, (int32_t)y1, color);
  Ahu_Fill((int32_t)x, (int32_t)y0, (int32_t)(x + 3U), (int32_t)y1, color);
  Ahu_Fill((int32_t)(x1 - 3U), (int32_t)y0, (int32_t)x1, (int32_t)y1, color);
}

static void Ahu_DrawInfo(const RoomUi_Values *values)
{
  char line[48];
  uint16_t y = (uint16_t)(INFO_Y + 8U);

  Ahu_Fill(12, (int32_t)INFO_Y, (int32_t)(SSD1963_WIDTH - 13U),
           (int32_t)(INFO_Y + INFO_H - 1U), COL_INFO);

  if ((s_selected >= AHU_SEC_COUNT) || (values == 0))
  {
    Ahu_TextC(12U, y, (uint16_t)(SSD1963_WIDTH - 24U),
              "Нажмите на оборудование", COL_HINT, COL_INFO);
    return;
  }

  switch (s_selected)
  {
    case AHU_SEC_FAN:
      (void)snprintf(line, sizeof(line), "%s   скорость %d %%",
                     s_sec[s_selected].full_name, (int)(values->fan_speed + 0.5f));
      break;
    case AHU_SEC_INLET:
    case AHU_SEC_OUTLET:
      (void)snprintf(line, sizeof(line), "%s   зал %d °C",
                     s_sec[s_selected].full_name, (int)(values->temperature_c + 0.5f));
      break;
    case AHU_SEC_COIL:
      (void)snprintf(line, sizeof(line), "%s   охлаждение", s_sec[s_selected].full_name);
      break;
    case AHU_SEC_F1:
      (void)snprintf(line, sizeof(line), "%s   42 %%", s_sec[s_selected].full_name);
      break;
    case AHU_SEC_F2:
      (void)snprintf(line, sizeof(line), "%s   80 %%", s_sec[s_selected].full_name);
      break;
    case AHU_SEC_F95:
      (void)snprintf(line, sizeof(line), "%s   95 %%", s_sec[s_selected].full_name);
      break;
    default:
      (void)snprintf(line, sizeof(line), "%s", s_sec[s_selected].full_name);
      break;
  }
  Ahu_TextC(12U, y, (uint16_t)(SSD1963_WIDTH - 24U), line, COL_INK, COL_INFO);
}

static void Ahu_DrawCase(void)
{
  uint8_t i;
  uint16_t x1 = (uint16_t)(CASE_X + CASE_W);
  uint16_t y1 = (uint16_t)(CASE_Y + CASE_H);

  Ahu_Fill(3, 150, (int32_t)(SSD1963_WIDTH - 4U), (int32_t)(SSD1963_HEIGHT - 4U), COL_BG);

  Ahu_Fill((int32_t)(CASE_X - 24U), (int32_t)(y1 + 4U),
           (int32_t)(x1 + 24U + ISO_DX), (int32_t)(y1 + 14U), COL_FLOOR);
  Ahu_IsoBox(CASE_X, CASE_Y, x1, y1, COL_CASE, COL_TOP, COL_SIDE);
  Ahu_Fill((int32_t)CASE_X, (int32_t)y1, (int32_t)x1, (int32_t)(y1 + 10U), COL_BASE);

  for (i = 1U; i < AHU_SEC_COUNT; i++)
  {
    uint16_t x = Ahu_SecX(i);
    Ahu_Fill((int32_t)x, (int32_t)CASE_Y, (int32_t)x, (int32_t)y1, COL_LINE);
  }

  Ahu_DrawInlet();
  Ahu_DrawFilter(Ahu_SecX(AHU_SEC_F1), s_sec[AHU_SEC_F1].w, "42%");
  Ahu_DrawFilter(Ahu_SecX(AHU_SEC_F2), s_sec[AHU_SEC_F2].w, "80%");
  Ahu_DrawFanStatic();
  Ahu_DrawCoil();
  Ahu_DrawFilter(Ahu_SecX(AHU_SEC_F95), s_sec[AHU_SEC_F95].w, "95%");
  Ahu_DrawOutlet();
  Ahu_DrawNames();
}

void AhuUi_Draw(const RoomUi_Values *values, uint32_t tick_ms)
{
  s_drawn = 0U;
  s_last_sel = 0xFEU;
  s_last_speed = -1;
  s_last_t = -1000;
  s_last_phase = 0xFFFFU;
#if AHU_UI_USE_SNAP
  AhuSnap_Draw();
  s_drawn = 1U;
  AhuSnap_Tick(tick_ms, (values != 0) && (values->fan_speed > 5.0f) ? 1U : 0U);
#else
  Ahu_DrawCase();
  s_drawn = 1U;
  AhuUi_Update(values, tick_ms);
#endif
}

void AhuUi_Update(const RoomUi_Values *values, uint32_t tick_ms)
{
#if AHU_UI_USE_SNAP
  if (s_drawn != 0U)
  {
    AhuSnap_Tick(tick_ms, (values != 0) && (values->fan_speed > 5.0f) ? 1U : 0U);
  }
  return;
#else
  uint16_t phase;
  int16_t speed;
  int16_t t;
  char buf[16];

  if ((s_drawn == 0U) || (values == 0))
  {
    return;
  }

  speed = (int16_t)(values->fan_speed + 0.5f);
  if (speed < 0)
  {
    speed = 0;
  }
  if (speed > 100)
  {
    speed = 100;
  }

  if (speed > 0)
  {
    phase = (uint16_t)(((tick_ms / 8U) * (uint32_t)speed) / 20U) % 360U;
  }
  else
  {
    phase = 0U;
  }

  if ((phase != s_last_phase) || (speed != s_last_speed))
  {
    Ahu_DrawFanBlades(phase);
    s_last_phase = phase;
  }

  if (speed != s_last_speed)
  {
    (void)snprintf(buf, sizeof(buf), "%d%%", (int)speed);
    Ahu_Chip((uint16_t)(Ahu_SecX(AHU_SEC_FAN) + (s_sec[AHU_SEC_FAN].w / 2U)),
             (uint16_t)(CASE_Y + CASE_H + 10U), buf);
    if (s_selected == AHU_SEC_FAN)
    {
      Ahu_DrawInfo(values);
    }
    s_last_speed = speed;
  }

  t = (int16_t)(values->temperature_c + 0.5f);
  if (t != s_last_t)
  {
    (void)snprintf(buf, sizeof(buf), "%d C", (int)t);
    Ahu_Chip((uint16_t)(Ahu_SecX(AHU_SEC_OUTLET) + (s_sec[AHU_SEC_OUTLET].w / 2U)),
             (uint16_t)(CASE_Y + CASE_H + 10U), buf);
    if (s_selected < AHU_SEC_COUNT)
    {
      Ahu_DrawInfo(values);
    }
    s_last_t = t;
  }

  if ((s_selected != s_last_sel) || (s_last_sel == 0xFEU))
  {
    if ((s_last_sel < AHU_SEC_COUNT) && (s_selected != s_last_sel))
    {
      Ahu_DrawSelect(s_last_sel, COL_LINE);
    }
    if (s_selected < AHU_SEC_COUNT)
    {
      Ahu_DrawSelect(s_selected, COL_SEL);
    }
    Ahu_DrawInfo(values);
    s_last_sel = s_selected;
  }
#endif
}

uint8_t AhuUi_Hit(uint16_t x, uint16_t y)
{
#if AHU_UI_USE_SNAP
  (void)x;
  (void)y;
  return AHU_SEC_NONE;
#else
  uint8_t i;

  if ((y < CASE_Y) || (y > (uint16_t)(CASE_Y + CASE_H)))
  {
    return AHU_SEC_NONE;
  }
  for (i = 0U; i < AHU_SEC_COUNT; i++)
  {
    uint16_t x0 = Ahu_SecX(i);
    uint16_t x1 = (uint16_t)(x0 + s_sec[i].w);
    if ((x >= x0) && (x < x1))
    {
      return i;
    }
  }
  return AHU_SEC_NONE;
#endif
}

void AhuUi_Select(uint8_t sec, const RoomUi_Values *values)
{
#if AHU_UI_USE_SNAP
  (void)sec;
  (void)values;
  return;
#else
  if (sec == s_selected)
  {
    s_selected = AHU_SEC_NONE;
  }
  else
  {
    s_selected = sec;
  }
  if (s_drawn != 0U)
  {
    if (s_last_sel < AHU_SEC_COUNT)
    {
      Ahu_DrawSelect(s_last_sel, COL_LINE);
    }
    if (s_selected < AHU_SEC_COUNT)
    {
      Ahu_DrawSelect(s_selected, COL_SEL);
    }
    Ahu_DrawInfo(values);
    s_last_sel = s_selected;
  }
#endif
}
