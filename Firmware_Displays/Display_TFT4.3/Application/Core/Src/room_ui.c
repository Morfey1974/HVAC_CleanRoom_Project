#include "room_ui.h"
#include "ssd1963.h"
#include "logo_ad.h"
#include "ui_font_aa.h"

#include <stdio.h>

#define ROOM_UI_HEADER_H    150U
#define ROOM_UI_SEP_W       2U
#define ROOM_UI_LOGO_X      12U
#define ROOM_UI_BORDER_W    3U
#define ROOM_UI_UNIT_GAP    4U

/* CMYK: header K20; T orange; H 79/45/37/24; P 75/0/75/0 */
#define ROOM_UI_HEADER_BG   SSD1963_RGB565(204, 204, 204)
#define ROOM_UI_COL_T       SSD1963_RGB565(255, 140, 0)
#define ROOM_UI_COL_H       SSD1963_RGB565(41, 107, 122)
#define ROOM_UI_COL_P       SSD1963_RGB565(64, 255, 64)
#define ROOM_UI_ALARM_BG    SSD1963_COLOR_RED
#define ROOM_UI_SEP         SSD1963_COLOR_WHITE
#define ROOM_UI_TEXT        SSD1963_COLOR_WHITE

/* Doors block: header right of the title, below the "ID:" text drawn by main.c at y 10..41. */
#define ROOM_UI_DOORS_N       5U
#define ROOM_UI_DOORS_X       600U
#define ROOM_UI_DOORS_CELL_W  36U
#define ROOM_UI_DOORS_GAP     4U
#define ROOM_UI_DOORS_W       (ROOM_UI_DOORS_N * ROOM_UI_DOORS_CELL_W + (ROOM_UI_DOORS_N - 1U) * ROOM_UI_DOORS_GAP)
#define ROOM_UI_DOORS_LABEL_Y 50U
#define ROOM_UI_DOORS_CELL_Y  70U
#define ROOM_UI_DOORS_CELL_H  40U
#define ROOM_UI_DOORS_LINK_Y  118U
#define ROOM_UI_DOOR_CLOSED   SSD1963_RGB565(0, 150, 60)
#define ROOM_UI_DOOR_OPEN     SSD1963_COLOR_RED
#define ROOM_UI_DOOR_UNKNOWN  SSD1963_RGB565(110, 110, 110)
#define ROOM_UI_DOORS_TEXT    SSD1963_COLOR_BLACK

typedef struct
{
  uint16_t x;
  uint16_t w;
  uint16_t color;
  const uint8_t *letter;
  uint16_t letter_w;
  uint16_t letter_h;
  const char *unit;   /* "°C" / "%" / "Pa" */
  uint16_t letter_x;
  uint16_t letter_y;
  uint16_t value_y;
  uint16_t value_left;
  uint16_t value_right;
} RoomUi_Column;

static RoomUi_Column s_cols[3];
static uint8_t s_inited;
static uint8_t s_have_last;
static int32_t s_last_t;
static int32_t s_last_h;
static int32_t s_last_p;
static uint8_t s_last_fault;
static uint8_t s_doors_drawn;
static uint8_t s_last_doors_mask;
static uint8_t s_last_doors_link;

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

static void RoomUi_FormatDigits(char *buf, uint32_t buflen, float value)
{
  int32_t v = (int32_t)((value >= 0.0f) ? (value + 0.5f) : (value - 0.5f));
  (void)snprintf(buf, buflen, "%ld", (long)v);
}

static uint16_t RoomUi_MidY(void)
{
  uint16_t body_y0 = ROOM_UI_HEADER_H;
  uint16_t body_y1 = (uint16_t)(SSD1963_HEIGHT - 1U);
  return (uint16_t)(body_y0 + ((body_y1 - body_y0) / 2U));
}

static void RoomUi_ColumnFillBounds(const RoomUi_Column *col,
                                    uint16_t *x0, uint16_t *x1)
{
  *x0 = col->x;
  *x1 = (uint16_t)(col->x + col->w - 1U);
  if (*x0 < ROOM_UI_BORDER_W)
  {
    *x0 = ROOM_UI_BORDER_W;
  }
  if (*x1 > (uint16_t)(SSD1963_WIDTH - 1U - ROOM_UI_BORDER_W))
  {
    *x1 = (uint16_t)(SSD1963_WIDTH - 1U - ROOM_UI_BORDER_W);
  }
}

static void RoomUi_SetupValueLayout(RoomUi_Column *col)
{
  uint16_t mid_y = RoomUi_MidY();
  uint16_t body_y0 = ROOM_UI_HEADER_H;
  uint16_t body_y1 = (uint16_t)(SSD1963_HEIGHT - 1U);
  uint16_t x0;
  uint16_t x1;

  RoomUi_ColumnFillBounds(col, &x0, &x1);
  col->value_y = (uint16_t)(mid_y + ((body_y1 - mid_y - UI_VALUE_FONT_H) / 2U));
  col->value_left = x0;
  col->value_right = x1;
  col->letter_x = (uint16_t)(col->x + ((col->w - col->letter_w) / 2U));
  col->letter_y = (uint16_t)(body_y0 + ((mid_y - body_y0 - col->letter_h) / 2U));
}

/* Фон колонки отдельно от текста */
static void RoomUi_FillColumn(const RoomUi_Column *col, uint16_t bg)
{
  uint16_t x0;
  uint16_t x1;

  RoomUi_ColumnFillBounds(col, &x0, &x1);
  SSD1963_Fill(x0, ROOM_UI_HEADER_H, x1, (uint16_t)(SSD1963_HEIGHT - 1U), bg);
}

static void RoomUi_DrawLetter(const RoomUi_Column *col, uint16_t bg)
{
  SSD1963_DrawAlpha(col->letter_x, col->letter_y, col->letter_w, col->letter_h,
                    col->letter, ROOM_UI_TEXT, bg);
}

/* Стереть старое число цветом колонки, поверх нарисовать новое */
static void RoomUi_DrawValue(const RoomUi_Column *col, const char *digits,
                             uint16_t bg)
{
  uint16_t tw = SSD1963_TextWidthAA(digits);
  uint16_t uw = SSD1963_TextWidthAA(col->unit);
  uint16_t total = (uint16_t)(tw + ROOM_UI_UNIT_GAP + uw);
  uint16_t avail = (uint16_t)(col->value_right - col->value_left + 1U);
  uint16_t start;
  uint16_t unit_x;

  if (total >= avail)
  {
    start = col->value_left;
  }
  else
  {
    start = (uint16_t)(col->value_left + ((avail - total) / 2U));
  }
  unit_x = (uint16_t)(start + tw + ROOM_UI_UNIT_GAP);

  SSD1963_Fill(col->value_left, col->value_y, col->value_right,
               (uint16_t)(col->value_y + UI_VALUE_FONT_H - 1U), bg);
  SSD1963_DrawStringAA(start, col->value_y, digits, ROOM_UI_TEXT, bg);
  SSD1963_DrawStringAA(unit_x, col->value_y, col->unit, ROOM_UI_TEXT, bg);
}

static void RoomUi_DrawStaticColumn(RoomUi_Column *col)
{
  RoomUi_SetupValueLayout(col);
  RoomUi_FillColumn(col, col->color);
  RoomUi_DrawLetter(col, col->color);
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

void RoomUi_Init(const char *room_name)
{
  const uint16_t usable = (uint16_t)(SSD1963_WIDTH - (2U * ROOM_UI_SEP_W));
  const uint16_t col_w = (uint16_t)(usable / 3U);
  uint16_t title_x;
  uint16_t logo_y;
  uint16_t title_y;

  (void)room_name;
  s_inited = 1U;
  s_have_last = 0U;
  s_doors_drawn = 0U;

  s_cols[0].x = 0U;
  s_cols[0].w = col_w;
  s_cols[0].color = ROOM_UI_COL_T;
  s_cols[0].letter = UiLetter_T;
  s_cols[0].letter_w = UI_LETTER_T_W;
  s_cols[0].letter_h = UI_LETTER_T_H;
  s_cols[0].unit = "°C";

  s_cols[1].x = (uint16_t)(col_w + ROOM_UI_SEP_W);
  s_cols[1].w = col_w;
  s_cols[1].color = ROOM_UI_COL_H;
  s_cols[1].letter = UiLetter_H;
  s_cols[1].letter_w = UI_LETTER_H_W;
  s_cols[1].letter_h = UI_LETTER_H_H;
  s_cols[1].unit = "%";

  s_cols[2].x = (uint16_t)(2U * (col_w + ROOM_UI_SEP_W));
  s_cols[2].w = (uint16_t)(SSD1963_WIDTH - s_cols[2].x);
  s_cols[2].color = ROOM_UI_COL_P;
  s_cols[2].letter = UiLetter_P;
  s_cols[2].letter_w = UI_LETTER_P_W;
  s_cols[2].letter_h = UI_LETTER_P_H;
  s_cols[2].unit = "Pa";

  SSD1963_FillScreen(SSD1963_COLOR_BLACK);
  SSD1963_Fill(0, 0, SSD1963_WIDTH - 1U, (uint16_t)(ROOM_UI_HEADER_H - 1U), ROOM_UI_HEADER_BG);

  logo_y = (uint16_t)((ROOM_UI_HEADER_H - LOGO_AD_HEIGHT) / 2U);
  SSD1963_DrawImage(ROOM_UI_LOGO_X, logo_y, LOGO_AD_WIDTH, LOGO_AD_HEIGHT, LogoAd_RGB565);

  title_x = (uint16_t)((SSD1963_WIDTH - UI_TITLE_W) / 2U);
  title_y = (uint16_t)((ROOM_UI_HEADER_H - UI_TITLE_H) / 2U);
  SSD1963_DrawImage(title_x, title_y, UI_TITLE_W, UI_TITLE_H, UiTitle_CleanRoom);

  RoomUi_DrawStaticColumn(&s_cols[0]);
  RoomUi_DrawStaticColumn(&s_cols[1]);
  RoomUi_DrawStaticColumn(&s_cols[2]);

  SSD1963_Fill(col_w, ROOM_UI_HEADER_H,
               (uint16_t)(col_w + ROOM_UI_SEP_W - 1U),
               (uint16_t)(SSD1963_HEIGHT - 1U), ROOM_UI_SEP);
  SSD1963_Fill((uint16_t)(col_w + ROOM_UI_SEP_W + col_w), ROOM_UI_HEADER_H,
               (uint16_t)(col_w + ROOM_UI_SEP_W + col_w + ROOM_UI_SEP_W - 1U),
               (uint16_t)(SSD1963_HEIGHT - 1U), ROOM_UI_SEP);

  RoomUi_DrawBorder();
}

void RoomUi_Update(const RoomUi_Values *values)
{
  char buf[16];
  int32_t t;
  int32_t h;
  int32_t p;

  if ((s_inited == 0U) || (values == 0))
  {
    return;
  }

  t = (int32_t)((values->temperature_c >= 0.0f) ? (values->temperature_c + 0.5f)
                                                : (values->temperature_c - 0.5f));
  h = (int32_t)((values->humidity_pct >= 0.0f) ? (values->humidity_pct + 0.5f)
                                               : (values->humidity_pct - 0.5f));
  p = (int32_t)((values->pressure_pa >= 0.0f) ? (values->pressure_pa + 0.5f)
                                              : (values->pressure_pa - 0.5f));

  const int32_t vals[3] = { t, h, p };
  const int32_t lasts[3] = { s_last_t, s_last_h, s_last_p };
  const uint8_t bits[3] = { ROOM_UI_FAULT_T, ROOM_UI_FAULT_H, ROOM_UI_FAULT_P };

  for (uint32_t i = 0U; i < 3U; i++)
  {
    uint8_t fault = (uint8_t)(values->fault_mask & bits[i]);
    uint8_t fault_changed = (uint8_t)(fault != (s_last_fault & bits[i]));

    if ((s_have_last == 0U) || fault_changed || ((fault == 0U) && (vals[i] != lasts[i])))
    {
      if (fault != 0U)
      {
        RoomUi_DrawValue(&s_cols[i], "--", ROOM_UI_ALARM_BG);
      }
      else
      {
        RoomUi_FormatDigits(buf, sizeof(buf), (float)vals[i]);
        RoomUi_DrawValue(&s_cols[i], buf, s_cols[i].color);
      }
    }
  }

  s_have_last = 1U;
  s_last_t = t;
  s_last_h = h;
  s_last_p = p;
  s_last_fault = values->fault_mask;
}

static void RoomUi_DrawCentered8x16(uint16_t y, const char *text, uint16_t chars,
                                    uint16_t fg, uint16_t bg)
{
  uint16_t w = (uint16_t)(chars * 8U);
  uint16_t x = (uint16_t)(ROOM_UI_DOORS_X + ((ROOM_UI_DOORS_W - w) / 2U));

  SSD1963_Fill(ROOM_UI_DOORS_X, y, (uint16_t)(ROOM_UI_DOORS_X + ROOM_UI_DOORS_W - 1U),
               (uint16_t)(y + 15U), bg);
  SSD1963_DrawString(x, y, text, fg, bg, 1U);
}

void RoomUi_UpdateDoors(uint8_t closed_mask, uint8_t link)
{
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
    RoomUi_DrawCentered8x16(ROOM_UI_DOORS_LABEL_Y, "ДВЕРИ", 5U, ROOM_UI_DOORS_TEXT, ROOM_UI_HEADER_BG);
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
    SSD1963_DrawString((uint16_t)(x0 + ((ROOM_UI_DOORS_CELL_W - 16U) / 2U)),
                       (uint16_t)(ROOM_UI_DOORS_CELL_Y + ((ROOM_UI_DOORS_CELL_H - 32U) / 2U)),
                       digit, ROOM_UI_TEXT, bg, 2U);
  }

  if ((s_doors_drawn == 0U) || (link != s_last_doors_link))
  {
    if (link != 0U)
    {
      RoomUi_DrawCentered8x16(ROOM_UI_DOORS_LINK_Y, "СВЯЗЬ ЕСТЬ", 10U, ROOM_UI_DOOR_CLOSED, ROOM_UI_HEADER_BG);
    }
    else
    {
      RoomUi_DrawCentered8x16(ROOM_UI_DOORS_LINK_Y, "НЕТ СВЯЗИ", 9U, ROOM_UI_DOOR_OPEN, ROOM_UI_HEADER_BG);
    }
  }

  s_doors_drawn = 1U;
  s_last_doors_mask = closed_mask;
  s_last_doors_link = link;
}

void RoomUi_SimulateStep(RoomUi_Values *values, uint32_t tick_ms)
{
  int16_t s1;
  int16_t s2;
  int16_t s3;

  if (values == 0)
  {
    return;
  }

  s1 = RoomUi_Sin1000(tick_ms / 40U);
  s2 = RoomUi_Sin1000((tick_ms / 35U) + 120U);
  s3 = RoomUi_Sin1000((tick_ms / 45U) + 240U);

  values->temperature_c = 24.0f + ((float)s1 * 0.003f);
  values->humidity_pct = 50.0f + ((float)s2 * 0.008f);
  values->pressure_pa = 20.0f + ((float)s3 * 0.004f);
}
