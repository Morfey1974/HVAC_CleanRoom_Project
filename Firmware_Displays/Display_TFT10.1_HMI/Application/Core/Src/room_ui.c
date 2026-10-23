#include "room_ui.h"
#include "ahu_ui.h"
#include "ssd1963.h"
#include "logo_ad.h"
#include "ui_font_aa.h"
#include "ui_font_menu.h"
#include "main.h"

#include <stdio.h>

#define ROOM_UI_HEADER_H    150U
#define ROOM_UI_LOGO_X      12U
#define ROOM_UI_BORDER_W    3U

#define ROOM_UI_HEADER_BG   SSD1963_RGB565(204, 204, 204)
#define ROOM_UI_SEP         SSD1963_COLOR_WHITE
#define ROOM_UI_TEXT        SSD1963_COLOR_WHITE

#define ROOM_UI_DOORS_N       5U
#define ROOM_UI_DOORS_CELL_W  36U
#define ROOM_UI_DOORS_GAP     4U
#define ROOM_UI_DOORS_W       (ROOM_UI_DOORS_N * ROOM_UI_DOORS_CELL_W + (ROOM_UI_DOORS_N - 1U) * ROOM_UI_DOORS_GAP)
#define ROOM_UI_DOORS_X       (SSD1963_WIDTH - ROOM_UI_DOORS_W - 16U)
#define ROOM_UI_DOORS_LABEL_Y 22U
#define ROOM_UI_DOORS_CELL_Y  54U
#define ROOM_UI_DOORS_CELL_H  40U
#define ROOM_UI_DOORS_LINK_Y  102U
#define ROOM_UI_DOOR_CLOSED   SSD1963_RGB565(0, 140, 40)
#define ROOM_UI_DOOR_OPEN     SSD1963_RGB565(200, 0, 0)
#define ROOM_UI_DOOR_UNKNOWN  SSD1963_RGB565(90, 90, 90)
#define ROOM_UI_DOORS_TEXT    SSD1963_COLOR_BLACK
#define ROOM_UI_DOORS_OK      SSD1963_RGB565(0, 70, 20)
#define ROOM_UI_STATUS_Y      118U

static uint8_t s_inited;
static uint8_t s_have_last;
static uint8_t s_ahu_drawn;
#if !AHU_UI_USE_SNAP
static int32_t s_last_t;
static int32_t s_last_h;
static int32_t s_last_p;
static int32_t s_last_fan;
#endif
static uint8_t s_doors_drawn;
#if !AHU_UI_USE_SNAP
static uint8_t s_last_doors_mask;
static uint8_t s_last_doors_link;
#endif

static int16_t RoomUi_Sin1000(uint32_t deg)
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
  int16_t s;

  if (d <= 90U)
  {
    s = table[d];
  }
  else if (d <= 180U)
  {
    s = table[180U - d];
  }
  else if (d <= 270U)
  {
    s = (int16_t)(-table[d - 180U]);
  }
  else
  {
    s = (int16_t)(-table[360U - d]);
  }

  return s;
}

static void RoomUi_DrawBorder(void)
{
  const uint16_t x1 = (uint16_t)(SSD1963_WIDTH - 1U);
  const uint16_t y1 = (uint16_t)(SSD1963_HEIGHT - 1U);
  const uint16_t b = (uint16_t)(ROOM_UI_BORDER_W - 1U);

  SSD1963_Fill(0, 0, x1, b, ROOM_UI_SEP);
  SSD1963_Fill(0, (uint16_t)(y1 - b), x1, y1, ROOM_UI_SEP);
  SSD1963_Fill(0, 0, b, y1, ROOM_UI_SEP);
  SSD1963_Fill((uint16_t)(x1 - b), 0, x1, y1, ROOM_UI_SEP);
}

#if !AHU_UI_USE_SNAP
static void RoomUi_DrawCenteredMenu(uint16_t y, const char *text,
                                    uint16_t fg, uint16_t bg)
{
  uint16_t tw = SSD1963_TextWidthMenuAA(text);
  uint16_t x;

  if (tw >= ROOM_UI_DOORS_W)
  {
    x = ROOM_UI_DOORS_X;
  }
  else
  {
    x = (uint16_t)(ROOM_UI_DOORS_X + ((ROOM_UI_DOORS_W - tw) / 2U));
  }
  SSD1963_Fill(ROOM_UI_DOORS_X, y, (uint16_t)(ROOM_UI_DOORS_X + ROOM_UI_DOORS_W - 1U),
               (uint16_t)(y + UI_MENU_FONT_H - 1U), bg);
  SSD1963_DrawStringMenuAA(x, y, text, fg, bg);
}

static void RoomUi_DrawStatus(const RoomUi_Values *values)
{
  char buf[64];
  uint16_t x = (uint16_t)(ROOM_UI_LOGO_X + LOGO_AD_WIDTH + 16U);
  uint16_t x1 = (uint16_t)(ROOM_UI_DOORS_X - 12U);
  int t;
  int h;
  int p;
  int f;

  if ((values == 0) || (x1 <= x))
  {
    return;
  }

  t = (int)(values->temperature_c + ((values->temperature_c >= 0.0f) ? 0.5f : -0.5f));
  h = (int)(values->humidity_pct + ((values->humidity_pct >= 0.0f) ? 0.5f : -0.5f));
  p = (int)(values->pressure_pa + ((values->pressure_pa >= 0.0f) ? 0.5f : -0.5f));
  f = (int)(values->fan_speed + 0.5f);

  if ((s_have_last != 0U) && (t == s_last_t) && (h == s_last_h) &&
      (p == s_last_p) && (f == s_last_fan))
  {
    return;
  }

  SSD1963_Fill(x, ROOM_UI_STATUS_Y, x1,
               (uint16_t)(ROOM_UI_STATUS_Y + UI_MENU_FONT_H - 1U), ROOM_UI_HEADER_BG);
  (void)snprintf(buf, sizeof(buf), "Зал %d C   %d %%   %d Pa   вент. %d %%", t, h, p, f);
  SSD1963_DrawStringMenuAA(x, ROOM_UI_STATUS_Y, buf, ROOM_UI_DOORS_TEXT, ROOM_UI_HEADER_BG);

  s_last_t = t;
  s_last_h = h;
  s_last_p = p;
  s_last_fan = f;
  s_have_last = 1U;
}
#endif

void RoomUi_Init(const char *room_name)
{
  uint16_t title_x;
  uint16_t logo_y;
  uint16_t title_y;

  (void)room_name;
  s_inited = 1U;
  s_have_last = 0U;
  s_ahu_drawn = 0U;
  s_doors_drawn = 0U;

  SSD1963_FillScreen(SSD1963_COLOR_BLACK);
  SSD1963_Fill(0, 0, SSD1963_WIDTH - 1U, (uint16_t)(ROOM_UI_HEADER_H - 1U), ROOM_UI_HEADER_BG);

  logo_y = (uint16_t)((ROOM_UI_HEADER_H - LOGO_AD_HEIGHT) / 2U);
  SSD1963_DrawImage(ROOM_UI_LOGO_X, logo_y, LOGO_AD_WIDTH, LOGO_AD_HEIGHT, LogoAd_RGB565);

  title_x = (uint16_t)((SSD1963_WIDTH - UI_TITLE_W) / 2U);
  title_y = 10U;
  SSD1963_DrawImage(title_x, title_y, UI_TITLE_W, UI_TITLE_H, UiTitle_CleanRoom);

  RoomUi_DrawBorder();
}

void RoomUi_Update(const RoomUi_Values *values)
{
  if ((s_inited == 0U) || (values == 0))
  {
    return;
  }

#if AHU_UI_USE_SNAP
  if (s_ahu_drawn == 0U)
  {
    uint16_t logo_y = (uint16_t)((ROOM_UI_HEADER_H - LOGO_AD_HEIGHT) / 2U);

    AhuUi_Draw(values, HAL_GetTick());
    SSD1963_DrawImage(ROOM_UI_LOGO_X, logo_y, LOGO_AD_WIDTH, LOGO_AD_HEIGHT, LogoAd_RGB565);
    s_ahu_drawn = 1U;
  }
  AhuUi_Update(values, HAL_GetTick());
#else
  RoomUi_DrawStatus(values);
  if (s_ahu_drawn == 0U)
  {
    AhuUi_Draw(values, HAL_GetTick());
    s_ahu_drawn = 1U;
  }
  else
  {
    AhuUi_Update(values, HAL_GetTick());
  }
#endif
}

void RoomUi_UpdateDoors(uint8_t closed_mask, uint8_t link)
{
#if AHU_UI_USE_SNAP
  (void)closed_mask;
  (void)link;
  return;
#else
  link = (link != 0U) ? 1U : 0U;
  if (link == 0U)
  {
    closed_mask = 0U;
  }

  if ((s_inited == 0U) ||
      ((s_doors_drawn != 0U) && (closed_mask == s_last_doors_mask) && (link == s_last_doors_link)))
  {
    return;
  }

  if (s_doors_drawn == 0U)
  {
    RoomUi_DrawCenteredMenu(ROOM_UI_DOORS_LABEL_Y, "ДВЕРИ", ROOM_UI_DOORS_TEXT, ROOM_UI_HEADER_BG);
  }

  for (uint16_t i = 0U; i < ROOM_UI_DOORS_N; i++)
  {
    uint16_t x0 = (uint16_t)(ROOM_UI_DOORS_X + i * (ROOM_UI_DOORS_CELL_W + ROOM_UI_DOORS_GAP));
    uint16_t bg;
    char digit[2];

    if (link == 0U)
    {
      bg = ROOM_UI_DOOR_UNKNOWN;
    }
    else
    {
      bg = ((closed_mask & (1U << i)) != 0U) ? ROOM_UI_DOOR_CLOSED : ROOM_UI_DOOR_OPEN;
    }

    digit[0] = (char)('1' + i);
    digit[1] = '\0';
    SSD1963_Fill(x0, ROOM_UI_DOORS_CELL_Y, (uint16_t)(x0 + ROOM_UI_DOORS_CELL_W - 1U),
                 (uint16_t)(ROOM_UI_DOORS_CELL_Y + ROOM_UI_DOORS_CELL_H - 1U), bg);
    {
      uint16_t dw = SSD1963_TextWidthMenuAA(digit);
      uint16_t dx = (dw < ROOM_UI_DOORS_CELL_W)
                        ? (uint16_t)(x0 + ((ROOM_UI_DOORS_CELL_W - dw) / 2U))
                        : x0;
      uint16_t dy = (uint16_t)(ROOM_UI_DOORS_CELL_Y +
                               ((ROOM_UI_DOORS_CELL_H - UI_MENU_FONT_H) / 2U));
      SSD1963_DrawStringMenuAA(dx, dy, digit, ROOM_UI_TEXT, bg);
    }
  }

  if ((s_doors_drawn == 0U) || (link != s_last_doors_link))
  {
    if (link != 0U)
    {
      RoomUi_DrawCenteredMenu(ROOM_UI_DOORS_LINK_Y, "СВЯЗЬ ЕСТЬ", ROOM_UI_DOORS_OK, ROOM_UI_HEADER_BG);
    }
    else
    {
      RoomUi_DrawCenteredMenu(ROOM_UI_DOORS_LINK_Y, "НЕТ СВЯЗИ", ROOM_UI_DOOR_OPEN, ROOM_UI_HEADER_BG);
    }
  }

  s_doors_drawn = 1U;
  s_last_doors_mask = closed_mask;
  s_last_doors_link = link;
#endif
}

void RoomUi_LogoRect(uint16_t *x0, uint16_t *y0, uint16_t *x1, uint16_t *y1)
{
  uint16_t logo_y = (uint16_t)((ROOM_UI_HEADER_H - LOGO_AD_HEIGHT) / 2U);

  if ((x0 == 0) || (y0 == 0) || (x1 == 0) || (y1 == 0))
  {
    return;
  }
  *x0 = ROOM_UI_LOGO_X;
  *y0 = logo_y;
  *x1 = (uint16_t)(ROOM_UI_LOGO_X + LOGO_AD_WIDTH - 1U);
  *y1 = (uint16_t)(logo_y + LOGO_AD_HEIGHT - 1U);
}

void RoomUi_Invalidate(void)
{
  s_have_last = 0U;
  s_ahu_drawn = 0U;
}

void RoomUi_RedrawAll(const RoomUi_Values *values, uint8_t closed_mask, uint8_t link)
{
  RoomUi_Init("Clean Room");
  RoomUi_Update(values);
  RoomUi_UpdateDoors(closed_mask, link);
}

void RoomUi_SimulateStep(RoomUi_Values *values, uint32_t tick_ms)
{
  int16_t s1;
  int16_t s2;
  float k;

  if (values == 0)
  {
    return;
  }

  s1 = RoomUi_Sin1000(tick_ms / 40U);
  s2 = RoomUi_Sin1000((tick_ms / 35U) + 120U);
  k = values->fan_speed * 0.01f;
  if (k < 0.0f)
  {
    k = 0.0f;
  }
  if (k > 1.0f)
  {
    k = 1.0f;
  }

  values->temperature_c += (values->t_set - values->temperature_c) * 0.05f;
  values->temperature_c += ((float)s1 * 0.0005f) * k;
  values->humidity_pct = 48.0f + ((float)s2 * 0.006f);
  values->pressure_pa = 6.0f + (values->fan_speed * 0.2f);
}