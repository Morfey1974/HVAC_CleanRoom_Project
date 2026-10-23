#include "hmi_menu.h"
#include "ssd1963.h"
#include "ui_menu_labels.h"
#include "author_photo.h"
#include "ahu_ui.h"
#include "main.h"

#define MENU_W     640U
#define MENU_H     360U
#define MENU_X     ((SSD1963_WIDTH - MENU_W) / 2U)
#define MENU_Y     80U
#define MENU_PAD   20U

#define COL_PANEL  SSD1963_RGB565(204, 204, 204)
#define COL_SHADOW SSD1963_RGB565(120, 120, 120)
#define COL_BTN    SSD1963_RGB565(230, 230, 230)
#define COL_T      SSD1963_RGB565(255, 96, 0)
#define COL_H      SSD1963_RGB565(0, 92, 160)
#define COL_P      SSD1963_RGB565(0, 210, 32)
#define COL_OK     SSD1963_RGB565(0, 140, 40)
#define COL_CLOSE  SSD1963_RGB565(200, 0, 0)
#define COL_TEXT   SSD1963_COLOR_WHITE
#define COL_INK    SSD1963_COLOR_BLACK
#define COL_MINUS  SSD1963_RGB565(255, 96, 0)
#define COL_PLUS   SSD1963_RGB565(0, 210, 32)

#define PAGE_NONE   0U
#define PAGE_MAIN   1U
#define PAGE_SP     2U
#define PAGE_BRIGHT 3U
#define PAGE_SIM    4U
#define PAGE_ABOUT  5U

#define ID_SP       1U
#define ID_BRIGHT   2U
#define ID_SIM      3U
#define ID_ABOUT    4U
#define ID_CLOSE    5U
#define ID_BACK     6U
#define ID_T_M      10U
#define ID_T_P      11U
#define ID_H_M      12U
#define ID_H_P      13U
#define ID_P_M      14U
#define ID_P_P      15U
#define ID_B_M      16U
#define ID_B_P      17U
#define ID_SIM_TOG  20U
#define ID_LINK_TOG 21U
#define ID_FAN_M    22U
#define ID_FAN_P    23U
#define ID_DOOR1    30U

typedef struct
{
  uint16_t x0;
  uint16_t y0;
  uint16_t x1;
  uint16_t y1;
  uint8_t id;
} HmiHit;

static HmiMenu_Ctx s_ctx;
static uint8_t s_page;
static uint8_t s_brightness = 100U;
static volatile uint8_t s_pwm_duty = 100U;
static volatile uint8_t s_pwm_cnt;
static HmiHit s_hits[16];
static uint8_t s_nhits;
static uint8_t s_was_down;
static uint8_t s_hold_id;
static uint32_t s_hold_t0;
static uint32_t s_hold_rep;
static uint8_t s_need_full;

static uint8_t Hmi_InRect(uint16_t x, uint16_t y, uint16_t x0, uint16_t y0,
                          uint16_t x1, uint16_t y1)
{
  return ((x >= x0) && (x <= x1) && (y >= y0) && (y <= y1)) ? 1U : 0U;
}

static void Hmi_SetBrightness(uint8_t pct)
{
  if (pct > 100U)
  {
    pct = 100U;
  }
  if (pct < 10U)
  {
    pct = 10U;
  }
  s_brightness = pct;
  s_pwm_duty = pct;
}

static void Hmi_BrightnessInit(void)
{
  GPIO_InitTypeDef gpio = {0};

  gpio.Pin = LCD_LED_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_LED_GPIO_Port, &gpio);
  HAL_GPIO_WritePin(LCD_LED_GPIO_Port, LCD_LED_Pin, GPIO_PIN_SET);

  s_pwm_duty = 100U;
  s_pwm_cnt = 0U;

  __HAL_RCC_TIM6_CLK_ENABLE();
  TIM6->CR1 = 0U;
  TIM6->PSC = 0U;
  TIM6->ARR = 1249U;
  TIM6->EGR = TIM_EGR_UG;
  TIM6->SR = 0U;
  TIM6->DIER = TIM_DIER_UIE;
  TIM6->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
  HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

void TIM6_DAC_IRQHandler(void)
{
  uint8_t c;

  TIM6->SR = 0U;
  c = (uint8_t)(s_pwm_cnt + 1U);
  if (c >= 100U)
  {
    c = 0U;
  }
  s_pwm_cnt = c;
  if (c < s_pwm_duty)
  {
    LCD_LED_GPIO_Port->BSRR = LCD_LED_Pin;
  }
  else
  {
    LCD_LED_GPIO_Port->BSRR = ((uint32_t)LCD_LED_Pin << 16);
  }
}

static void Hmi_ClearHits(void)
{
  s_nhits = 0U;
}

static void Hmi_AddHit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t id)
{
  if (s_nhits >= (uint8_t)(sizeof(s_hits) / sizeof(s_hits[0])))
  {
    return;
  }
  s_hits[s_nhits].x0 = x;
  s_hits[s_nhits].y0 = y;
  s_hits[s_nhits].x1 = (uint16_t)(x + w - 1U);
  s_hits[s_nhits].y1 = (uint16_t)(y + h - 1U);
  s_hits[s_nhits].id = id;
  s_nhits++;
}

static uint8_t Hmi_HitId(uint16_t x, uint16_t y)
{
  uint8_t i;
  for (i = 0U; i < s_nhits; i++)
  {
    if (Hmi_InRect(x, y, s_hits[i].x0, s_hits[i].y0, s_hits[i].x1, s_hits[i].y1) != 0U)
    {
      return s_hits[i].id;
    }
  }
  return 0U;
}

static uint16_t Hmi_Tint(uint16_t c, uint8_t light)
{
  uint16_t r = (uint16_t)(((c >> 11) & 0x1FU) << 3);
  uint16_t g = (uint16_t)(((c >> 5) & 0x3FU) << 2);
  uint16_t b = (uint16_t)((c & 0x1FU) << 3);

  if (light != 0U)
  {
    r = (uint16_t)(r + ((255U - r) / 2U));
    g = (uint16_t)(g + ((255U - g) / 2U));
    b = (uint16_t)(b + ((255U - b) / 2U));
  }
  else
  {
    r = (uint16_t)(r / 2U);
    g = (uint16_t)(g / 2U);
    b = (uint16_t)(b / 2U);
  }
  return SSD1963_RGB565((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

static void Hmi_DrawRaised(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t face)
{
  uint16_t x1 = (uint16_t)(x + w - 1U);
  uint16_t y1 = (uint16_t)(y + h - 1U);
  uint16_t hi = Hmi_Tint(face, 1U);
  uint16_t lo = Hmi_Tint(face, 0U);

  SSD1963_Fill(x, y, x1, y1, face);
  SSD1963_Fill(x, y, x1, (uint16_t)(y + 2U), hi);
  SSD1963_Fill(x, y, (uint16_t)(x + 2U), y1, hi);
  SSD1963_Fill(x, (uint16_t)(y1 - 2U), x1, y1, lo);
  SSD1963_Fill((uint16_t)(x1 - 2U), y, x1, y1, lo);
}

static void Hmi_Blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     const HmiLbl *lbl)
{
  uint16_t tx;
  uint16_t ty;

  if ((lbl == 0) || (lbl->rgb == 0) || (lbl->w == 0U) || (lbl->h == 0U))
  {
    return;
  }
  tx = x;
  ty = y;
  if (lbl->w < w)
  {
    tx = (uint16_t)(x + ((w - lbl->w) / 2U));
  }
  if (lbl->h < h)
  {
    ty = (uint16_t)(y + ((h - lbl->h) / 2U));
  }
  SSD1963_DrawImage(tx, ty, lbl->w, lbl->h, lbl->rgb);
}

static void Hmi_DrawBtn(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        const HmiLbl *lbl, uint16_t bg, uint8_t id)
{
  Hmi_DrawRaised(x, y, w, h, bg);
  Hmi_Blit(x, y, w, h, lbl);
  Hmi_AddHit(x, y, w, h, id);
}

static void Hmi_DrawPanel(const HmiLbl *title)
{
  uint16_t stripe = (uint16_t)(MENU_W / 3U);

  SSD1963_Fill((uint16_t)(MENU_X + 6U), (uint16_t)(MENU_Y + 6U),
               (uint16_t)(MENU_X + MENU_W + 5U),
               (uint16_t)(MENU_Y + MENU_H + 5U), COL_SHADOW);
  Hmi_DrawRaised(MENU_X, MENU_Y, MENU_W, MENU_H, COL_PANEL);
  SSD1963_Fill(MENU_X, MENU_Y, (uint16_t)(MENU_X + stripe - 1U),
               (uint16_t)(MENU_Y + 7U), COL_T);
  SSD1963_Fill((uint16_t)(MENU_X + stripe), MENU_Y,
               (uint16_t)(MENU_X + (2U * stripe) - 1U),
               (uint16_t)(MENU_Y + 7U), COL_H);
  SSD1963_Fill((uint16_t)(MENU_X + (2U * stripe)), MENU_Y,
               (uint16_t)(MENU_X + MENU_W - 1U),
               (uint16_t)(MENU_Y + 7U), COL_P);
  Hmi_ClearHits();
  Hmi_Blit((uint16_t)(MENU_X + MENU_PAD), (uint16_t)(MENU_Y + 8U),
           (uint16_t)(MENU_W - (2U * MENU_PAD)), 48U, title);
}

static void Hmi_DrawNumber(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           int value, const HmiLbl *unit)
{
  uint8_t d[4];
  uint8_t n = 0U;
  unsigned u;
  uint16_t tw;
  uint16_t tx;
  uint16_t ty;
  uint8_t i;

  if (value < 0)
  {
    value = 0;
  }
  u = (unsigned)value;
  do
  {
    d[n] = (uint8_t)(u % 10U);
    n++;
    u /= 10U;
  } while ((u != 0U) && (n < 4U));

  tw = (uint16_t)((uint16_t)n * HMI_DIG_W);
  if (unit != 0)
  {
    tw = (uint16_t)(tw + unit->w);
  }
  tx = (tw < w) ? (uint16_t)(x + ((w - tw) / 2U)) : x;
  ty = (HMI_DIG_H < h) ? (uint16_t)(y + ((h - HMI_DIG_H) / 2U)) : y;
  i = n;
  while (i != 0U)
  {
    i--;
    SSD1963_DrawImage(tx, ty, HMI_DIG_W, HMI_DIG_H, HmiDig_Btn[d[i]]);
    tx = (uint16_t)(tx + HMI_DIG_W);
  }
  if (unit != 0)
  {
    SSD1963_DrawImage(tx, ty, unit->w, unit->h, unit->rgb);
  }
}

static void Hmi_DrawMain(void)
{
  const uint16_t x = (uint16_t)(MENU_X + MENU_PAD);
  const uint16_t w = (uint16_t)(MENU_W - (2U * MENU_PAD));
  const uint16_t h = 48U;
  uint16_t y = (uint16_t)(MENU_Y + 58U);

  Hmi_DrawPanel(&HmiLbl_TitleMenu);
  Hmi_DrawBtn(x, y, w, h, &HmiLbl_BtnSp, COL_T, ID_SP);
  y = (uint16_t)(y + h + 10U);
  Hmi_DrawBtn(x, y, w, h, &HmiLbl_BtnBright, COL_H, ID_BRIGHT);
  y = (uint16_t)(y + h + 10U);
  Hmi_DrawBtn(x, y, w, h, &HmiLbl_BtnSim, COL_P, ID_SIM);
  y = (uint16_t)(y + h + 10U);
  Hmi_DrawBtn(x, y, w, h, &HmiLbl_BtnAbout, COL_BTN, ID_ABOUT);
  y = (uint16_t)(y + h + 10U);
  Hmi_DrawBtn(x, y, w, h, &HmiLbl_BtnClose, COL_CLOSE, ID_CLOSE);
}

static void Hmi_DrawStepper(uint16_t y, const HmiLbl *name, int value,
                            const HmiLbl *unit, uint8_t id_m, uint8_t id_p)
{
  const uint16_t x = (uint16_t)(MENU_X + MENU_PAD);
  const uint16_t bw = 72U;
  const uint16_t h = 52U;
  const uint16_t vw = (uint16_t)(MENU_W - (2U * MENU_PAD) - (2U * bw) - 160U);
  uint16_t vx;

  if (name != 0)
  {
    Hmi_Blit(x, y, 80U, h, name);
  }
  Hmi_DrawBtn((uint16_t)(x + 88U), y, bw, h, &HmiLbl_BtnMinus, COL_MINUS, id_m);
  vx = (uint16_t)(x + 88U + bw + 8U);
  Hmi_DrawRaised(vx, y, vw, h, COL_BTN);
  Hmi_DrawNumber(vx, y, vw, h, value, unit);
  Hmi_DrawBtn((uint16_t)(vx + vw + 8U), y, bw, h, &HmiLbl_BtnPlus, COL_PLUS, id_p);
}

static void Hmi_DrawSp(void)
{
  uint16_t y = (uint16_t)(MENU_Y + 50U);
  int t = 0;
  int h = 0;
  int p = 0;
  int fan = 0;

  if (s_ctx.values != 0)
  {
    t = (int)(s_ctx.values->t_set + ((s_ctx.values->t_set >= 0.0f) ? 0.5f : -0.5f));
    h = (int)(s_ctx.values->humidity_pct + ((s_ctx.values->humidity_pct >= 0.0f) ? 0.5f : -0.5f));
    p = (int)(s_ctx.values->pressure_pa + ((s_ctx.values->pressure_pa >= 0.0f) ? 0.5f : -0.5f));
    fan = (int)(s_ctx.values->fan_speed + 0.5f);
  }

  Hmi_DrawPanel(&HmiLbl_TitleSp);
  Hmi_DrawStepper(y, &HmiLbl_NameT, t, &HmiLbl_UnitC, ID_T_M, ID_T_P);
  y = (uint16_t)(y + 52U);
  Hmi_DrawStepper(y, &HmiLbl_NameH, h, &HmiLbl_UnitPct, ID_H_M, ID_H_P);
  y = (uint16_t)(y + 52U);
  Hmi_DrawStepper(y, &HmiLbl_NameP, p, &HmiLbl_UnitPa, ID_P_M, ID_P_P);
  y = (uint16_t)(y + 52U);
  Hmi_DrawStepper(y, 0, fan, &HmiLbl_UnitPct, ID_FAN_M, ID_FAN_P);
  SSD1963_DrawStringMenuAA((uint16_t)(MENU_X + MENU_PAD), (uint16_t)(y + 8U),
                           "ВЕНТ", COL_INK, COL_PANEL);
  y = (uint16_t)(y + 56U);
  Hmi_DrawBtn((uint16_t)(MENU_X + MENU_PAD), y,
              (uint16_t)(MENU_W - (2U * MENU_PAD)), 40U, &HmiLbl_BtnBack, COL_BTN, ID_BACK);
}

static void Hmi_DrawBright(void)
{
  uint16_t y = (uint16_t)(MENU_Y + 90U);

  Hmi_DrawPanel(&HmiLbl_TitleBright);
  Hmi_DrawStepper(y, 0, (int)s_brightness, &HmiLbl_UnitPct, ID_B_M, ID_B_P);
  y = (uint16_t)(y + 90U);
  Hmi_DrawBtn((uint16_t)(MENU_X + MENU_PAD), y,
              (uint16_t)(MENU_W - (2U * MENU_PAD)), 48U, &HmiLbl_BtnBack, COL_BTN, ID_BACK);
}

static void Hmi_DrawSim(void)
{
  const uint16_t x = (uint16_t)(MENU_X + MENU_PAD);
  const uint16_t w = (uint16_t)(MENU_W - (2U * MENU_PAD));
  uint16_t y = (uint16_t)(MENU_Y + 58U);
  uint16_t i;
  uint16_t dw;
  uint8_t sim_on = 0U;
  uint8_t link = 0U;
  uint8_t doors = 0U;

  if (s_ctx.sim_on != 0)
  {
    sim_on = *(s_ctx.sim_on);
  }
  if (s_ctx.link != 0)
  {
    link = *(s_ctx.link);
  }
  if (s_ctx.doors_mask != 0)
  {
    doors = *(s_ctx.doors_mask);
  }

  Hmi_DrawPanel(&HmiLbl_TitleSim);
  Hmi_DrawBtn(x, y, w, 48U,
              (sim_on != 0U) ? &HmiLbl_SimOn : &HmiLbl_SimOff,
              (sim_on != 0U) ? COL_OK : COL_BTN, ID_SIM_TOG);
  y = (uint16_t)(y + 58U);
  Hmi_DrawBtn(x, y, w, 48U,
              (link != 0U) ? &HmiLbl_LinkOn : &HmiLbl_LinkOff,
              (link != 0U) ? COL_OK : COL_CLOSE, ID_LINK_TOG);
  y = (uint16_t)(y + 58U);
  dw = 56U;
  for (i = 0U; i < 5U; i++)
  {
    uint16_t bx = (uint16_t)(x + (i * (dw + 12U)));
    uint8_t on = (uint8_t)((doors & (1U << i)) != 0U);
    Hmi_DrawBtn(bx, y, dw, 52U,
                on ? HmiLbl_DoorOk[i] : HmiLbl_DoorNo[i],
                on ? COL_OK : COL_CLOSE, (uint8_t)(ID_DOOR1 + i));
  }
  y = (uint16_t)(y + 70U);
  Hmi_DrawBtn(x, y, w, 48U, &HmiLbl_BtnBack, COL_BTN, ID_BACK);
}

static void Hmi_DrawAbout(void)
{
  const uint16_t x = (uint16_t)(MENU_X + MENU_PAD);
  const uint16_t px = (uint16_t)(x + 4U);
  const uint16_t py = (uint16_t)(MENU_Y + 54U);
  const uint16_t tx = (uint16_t)(px + AUTHOR_PHOTO_W + 20U);
  uint16_t ty = py;

  Hmi_DrawPanel(&HmiLbl_TitleAbout);
  Hmi_DrawRaised((uint16_t)(px - 4U), (uint16_t)(py - 4U),
                 (uint16_t)(AUTHOR_PHOTO_W + 8U),
                 (uint16_t)(AUTHOR_PHOTO_H + 8U), COL_BTN);
  SSD1963_DrawImage(px, py, AUTHOR_PHOTO_W, AUTHOR_PHOTO_H, AuthorPhoto_RGB565);
  SSD1963_DrawImage(tx, ty, HmiLbl_AboutHvac.w, HmiLbl_AboutHvac.h, HmiLbl_AboutHvac.rgb);
  ty = (uint16_t)(ty + HMI_LBL_H + 8U);
  SSD1963_DrawImage(tx, ty, HmiLbl_AboutPanel.w, HmiLbl_AboutPanel.h, HmiLbl_AboutPanel.rgb);
  ty = (uint16_t)(ty + HMI_LBL_H + 20U);
  SSD1963_DrawImage(tx, ty, HmiLbl_AboutAuthor.w, HmiLbl_AboutAuthor.h, HmiLbl_AboutAuthor.rgb);
  ty = (uint16_t)(ty + HMI_LBL_H + 4U);
  SSD1963_DrawImage(tx, ty, HmiLbl_AboutName.w, HmiLbl_AboutName.h, HmiLbl_AboutName.rgb);
  Hmi_DrawBtn(x, (uint16_t)(MENU_Y + MENU_H - 68U),
              (uint16_t)(MENU_W - (2U * MENU_PAD)), 48U,
              &HmiLbl_BtnBack, COL_BTN, ID_BACK);
}

static void Hmi_DrawPage(void)
{
  switch (s_page)
  {
    case PAGE_MAIN:
      Hmi_DrawMain();
      break;
    case PAGE_SP:
      Hmi_DrawSp();
      break;
    case PAGE_BRIGHT:
      Hmi_DrawBright();
      break;
    case PAGE_SIM:
      Hmi_DrawSim();
      break;
    case PAGE_ABOUT:
      Hmi_DrawAbout();
      break;
    default:
      break;
  }
}

static void Hmi_Close(void)
{
  s_page = PAGE_NONE;
  s_need_full = 1U;
  Hmi_ClearHits();
}

static void Hmi_OpenMain(void)
{
  s_page = PAGE_MAIN;
  Hmi_DrawPage();
}

static void Hmi_ApplyValues(void)
{
  if (s_ctx.values != 0)
  {
    s_ctx.values->fault_mask = 0U;
  }
}

static void Hmi_ApplyDoors(void)
{
  if ((s_ctx.doors_mask != 0) && (s_ctx.link != 0))
  {
    RoomUi_UpdateDoors(*(s_ctx.doors_mask), *(s_ctx.link));
  }
}

static void Hmi_StepSp(uint8_t id)
{
  float *v;
  float step;
  float lo;
  float hi;
  uint8_t is_fan = 0U;

  if (s_ctx.values == 0)
  {
    return;
  }

  if ((id == ID_T_M) || (id == ID_T_P))
  {
    v = &s_ctx.values->t_set;
    step = 1.0f;
    lo = 0.0f;
    hi = 50.0f;
  }
  else if ((id == ID_H_M) || (id == ID_H_P))
  {
    v = &s_ctx.values->humidity_pct;
    step = 1.0f;
    lo = 0.0f;
    hi = 100.0f;
  }
  else if ((id == ID_FAN_M) || (id == ID_FAN_P))
  {
    v = &s_ctx.values->fan_speed;
    step = 5.0f;
    lo = 0.0f;
    hi = 100.0f;
    is_fan = 1U;
  }
  else
  {
    v = &s_ctx.values->pressure_pa;
    step = 1.0f;
    lo = 0.0f;
    hi = 100.0f;
  }

  if ((id == ID_T_M) || (id == ID_H_M) || (id == ID_P_M) || (id == ID_FAN_M))
  {
    *v -= step;
  }
  else
  {
    *v += step;
  }
  if (*v < lo)
  {
    *v = lo;
  }
  if (*v > hi)
  {
    *v = hi;
  }
  if ((is_fan == 0U) && ((id == ID_T_M) || (id == ID_T_P)))
  {
    if ((s_ctx.sim_on == 0) || (*(s_ctx.sim_on) == 0U))
    {
      s_ctx.values->temperature_c = s_ctx.values->t_set;
    }
  }
  Hmi_ApplyValues();
  Hmi_DrawPage();
}

static void Hmi_StepBright(int8_t dir)
{
  int v = (int)s_brightness + ((int)dir * 10);
  if (v < 10)
  {
    v = 10;
  }
  if (v > 100)
  {
    v = 100;
  }
  Hmi_SetBrightness((uint8_t)v);
  Hmi_DrawPage();
}

static uint8_t Hmi_IsHold(uint8_t id)
{
  return ((id == ID_T_M) || (id == ID_T_P) || (id == ID_H_M) || (id == ID_H_P) ||
          (id == ID_P_M) || (id == ID_P_P) || (id == ID_B_M) || (id == ID_B_P) ||
          (id == ID_FAN_M) || (id == ID_FAN_P))
             ? 1U
             : 0U;
}

static void Hmi_DoId(uint8_t id)
{
  if (id == 0U)
  {
    return;
  }
  if (id == ID_SP)
  {
    s_page = PAGE_SP;
    Hmi_DrawPage();
  }
  else if (id == ID_BRIGHT)
  {
    s_page = PAGE_BRIGHT;
    Hmi_DrawPage();
  }
  else if (id == ID_SIM)
  {
    s_page = PAGE_SIM;
    Hmi_DrawPage();
  }
  else if (id == ID_ABOUT)
  {
    s_page = PAGE_ABOUT;
    Hmi_DrawPage();
  }
  else if (id == ID_CLOSE)
  {
    Hmi_Close();
  }
  else if (id == ID_BACK)
  {
    s_page = PAGE_MAIN;
    Hmi_DrawPage();
  }
  else if ((id == ID_T_M) || (id == ID_T_P) || (id == ID_H_M) || (id == ID_H_P) ||
           (id == ID_P_M) || (id == ID_P_P) || (id == ID_FAN_M) || (id == ID_FAN_P))
  {
    Hmi_StepSp(id);
  }
  else if (id == ID_B_M)
  {
    Hmi_StepBright(-1);
  }
  else if (id == ID_B_P)
  {
    Hmi_StepBright(1);
  }
  else if (id == ID_SIM_TOG)
  {
    if (s_ctx.sim_on != 0)
    {
      *(s_ctx.sim_on) = (*(s_ctx.sim_on) != 0U) ? 0U : 1U;
    }
    Hmi_DrawPage();
  }
  else if (id == ID_LINK_TOG)
  {
    if (s_ctx.link != 0)
    {
      *(s_ctx.link) = (*(s_ctx.link) != 0U) ? 0U : 1U;
    }
    Hmi_ApplyDoors();
    Hmi_DrawPage();
  }
  else if ((id >= ID_DOOR1) && (id < (uint8_t)(ID_DOOR1 + 5U)))
  {
    uint8_t bit = (uint8_t)(1U << (id - ID_DOOR1));
    if (s_ctx.doors_mask != 0)
    {
      *(s_ctx.doors_mask) ^= bit;
    }
    Hmi_ApplyDoors();
    Hmi_DrawPage();
  }
}

void HmiMenu_Init(HmiMenu_Ctx *ctx)
{
  if (ctx != 0)
  {
    s_ctx = *ctx;
  }
  s_page = PAGE_NONE;
  s_was_down = 0U;
  s_hold_id = 0U;
  s_need_full = 0U;
  Hmi_BrightnessInit();
}

uint8_t HmiMenu_IsOpen(void)
{
  return (s_page != PAGE_NONE) ? 1U : 0U;
}

void HmiMenu_Poll(uint8_t down, uint16_t x, uint16_t y)
{
  uint32_t now = HAL_GetTick();
  uint8_t id;

  if ((down != 0U) && (s_was_down == 0U))
  {
    if (s_page == PAGE_NONE)
    {
      uint16_t lx0;
      uint16_t ly0;
      uint16_t lx1;
      uint16_t ly1;

      RoomUi_LogoRect(&lx0, &ly0, &lx1, &ly1);
      if (lx0 >= 8U)
      {
        lx0 = (uint16_t)(lx0 - 8U);
      }
      else
      {
        lx0 = 0U;
      }
      if (ly0 >= 8U)
      {
        ly0 = (uint16_t)(ly0 - 8U);
      }
      else
      {
        ly0 = 0U;
      }
      lx1 = (uint16_t)(lx1 + 8U);
      ly1 = (uint16_t)(ly1 + 8U);
      if (Hmi_InRect(x, y, lx0, ly0, lx1, ly1) != 0U)
      {
        Hmi_OpenMain();
      }
      else if (s_ctx.values != 0)
      {
        uint8_t sec = AhuUi_Hit(x, y);
        if (sec != AHU_SEC_NONE)
        {
          AhuUi_Select(sec, s_ctx.values);
        }
      }
    }
    else
    {
      id = Hmi_HitId(x, y);
      if (id != 0U)
      {
        Hmi_DoId(id);
        if (Hmi_IsHold(id) != 0U)
        {
          s_hold_id = id;
          s_hold_t0 = now;
          s_hold_rep = now;
        }
        else
        {
          s_hold_id = 0U;
        }
      }
    }
  }
  else if ((down != 0U) && (s_hold_id != 0U))
  {
    if (((now - s_hold_t0) > 400U) && ((now - s_hold_rep) > 120U))
    {
      Hmi_DoId(s_hold_id);
      s_hold_rep = now;
    }
  }
  else if (down == 0U)
  {
    s_hold_id = 0U;
  }

  s_was_down = down;

  if ((s_need_full != 0U) && (s_page == PAGE_NONE))
  {
    s_need_full = 0U;
    if ((s_ctx.values != 0) && (s_ctx.doors_mask != 0) && (s_ctx.link != 0))
    {
      RoomUi_RedrawAll(s_ctx.values, *(s_ctx.doors_mask), *(s_ctx.link));
    }
  }
}
