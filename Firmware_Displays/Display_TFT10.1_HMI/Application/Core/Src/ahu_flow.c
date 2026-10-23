#include "ahu_snap.h"
#include "ahu_motion.h"
#include "lt7683.h"

#define FLOW_AW     68
#define FLOW_AH     28
#define FLOW_SHAFT  40
#define FLOW_HALF   14
#define FLOW_PERIOD 1400U
#if ((FAN_S * FAN_S) > (FLOW_L_W * FLOW_L_H))
#define FLOW_BUF_N  (FAN_S * FAN_S)
#else
#define FLOW_BUF_N  (FLOW_L_W * FLOW_L_H)
#endif

#define COL_DARK    LT7683_RGB565(30, 41, 55)
#define COL_BLADE   LT7683_RGB565(216, 221, 228)
#define COL_HUB     LT7683_RGB565(139, 148, 158)
#define COL_RING    LT7683_RGB565(226, 232, 240)

static uint16_t s_buf[FLOW_BUF_N];
static uint16_t s_last_step = 0xFFFFU;

static const int16_t kSin1000[91] = {
    0, 17, 35, 52, 70, 87, 105, 122, 139, 156, 174, 191, 208, 225, 242, 259,
    276, 292, 309, 326, 342, 358, 375, 391, 407, 423, 438, 454, 469, 485, 500,
    515, 530, 545, 559, 574, 588, 602, 616, 629, 643, 656, 669, 682, 695, 707,
    719, 731, 743, 755, 766, 777, 788, 799, 809, 819, 829, 839, 848, 857, 866,
    875, 883, 891, 899, 906, 914, 921, 927, 934, 940, 946, 951, 956, 961, 966,
    970, 974, 978, 982, 985, 988, 990, 993, 995, 996, 998, 999, 999, 1000, 1000
};

static int16_t Sin1000(int32_t deg)
{
  uint32_t d = (uint32_t)((deg % 360) + 360) % 360U;
  if (d <= 90U) return kSin1000[d];
  if (d <= 180U) return kSin1000[180U - d];
  if (d <= 270U) return (int16_t)(-kSin1000[d - 180U]);
  return (int16_t)(-kSin1000[360U - d]);
}

static int16_t Cos1000(int32_t deg)
{
  return Sin1000(deg + 90);
}

static uint16_t Mix(uint16_t bg, uint16_t fg, uint8_t a)
{
  uint32_t br = (bg >> 11) & 0x1FU;
  uint32_t bg6 = (bg >> 5) & 0x3FU;
  uint32_t bb = bg & 0x1FU;
  uint32_t fr = (fg >> 11) & 0x1FU;
  uint32_t fg6 = (fg >> 5) & 0x3FU;
  uint32_t fb = fg & 0x1FU;
  uint32_t r = (br * (uint32_t)(255U - a) + fr * (uint32_t)a) / 255U;
  uint32_t g = (bg6 * (uint32_t)(255U - a) + fg6 * (uint32_t)a) / 255U;
  uint32_t b = (bb * (uint32_t)(255U - a) + fb * (uint32_t)a) / 255U;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static uint8_t Env(uint32_t t)
{
  if (t < 308U) return (uint8_t)((t * 255U) / 308U);
  if (t < 1092U) return 255U;
  return (uint8_t)(((FLOW_PERIOD - t) * 255U) / 308U);
}

static int16_t Xoff(uint32_t t)
{
  uint32_t u = (t * 128U) / FLOW_PERIOD;
  uint32_t e = ((3U * u * u * 128U) - (2U * u * u * u)) / (128U * 128U);
  return (int16_t)(-6 + (int32_t)((e * 18U) / 128U));
}

static uint16_t Tone(uint8_t cool, uint8_t u)
{
  if (cool != 0U)
  {
    return LT7683_RGB565((uint16_t)(70U - u / 8U),
                         (uint16_t)(198U - u / 10U),
                         (uint16_t)(242U - u / 12U));
  }
  return LT7683_RGB565(255U, (uint16_t)(122U - u / 8U), (uint16_t)(20U - u / 20U));
}

static uint8_t GradA(uint8_t u)
{
  if (u < 70U) return (uint8_t)((u * 200U) / 70U);
  return 255U;
}

static void StampArrow(uint16_t bw, uint16_t bh, int32_t ax, int32_t ay,
                       uint8_t alpha, uint8_t cool)
{
  int32_t py;
  int32_t px;
  uint32_t head_w = (uint32_t)(FLOW_AW - FLOW_SHAFT);

  if (alpha < 12U)
  {
    return;
  }

  for (py = 0; py < FLOW_AH; py++)
  {
    int32_t ly = py - (FLOW_AH / 2);
    int32_t y = ay + py;
    uint32_t half;

    if ((y < 0) || (y >= (int32_t)bh))
    {
      continue;
    }
    if (ly < 0)
    {
      ly = -ly;
    }
    if (ly <= FLOW_HALF)
    {
      half = ((uint32_t)FLOW_HALF * head_w - ((uint32_t)ly * head_w)) / (uint32_t)FLOW_HALF;
    }
    else
    {
      continue;
    }

    for (px = 0; px < FLOW_AW; px++)
    {
      int32_t x = ax + px;
      uint8_t inside = 0U;
      uint8_t u;
      uint8_t a;
      uint32_t i;

      if ((x < 0) || (x >= (int32_t)bw))
      {
        continue;
      }
      if ((px <= FLOW_SHAFT) && (ly <= 7))
      {
        inside = 1U;
      }
      else if ((px >= FLOW_SHAFT) && ((uint32_t)(px - FLOW_SHAFT) <= half))
      {
        inside = 1U;
      }
      if (inside == 0U)
      {
        continue;
      }
      u = (uint8_t)((px * 255) / (FLOW_AW - 1));
      a = (uint8_t)(((uint32_t)GradA(u) * (uint32_t)alpha) / 255U);
      i = ((uint32_t)y * (uint32_t)bw) + (uint32_t)x;
      s_buf[i] = Mix(s_buf[i], Tone(cool, u), a);
    }
  }
}

static void PaintArrows(uint16_t dx, uint16_t dy, uint16_t bw, uint16_t bh,
                        const uint16_t *bg, uint32_t t0, uint32_t t1, uint8_t cool, uint8_t on)
{
  uint32_t n = (uint32_t)bw * (uint32_t)bh;
  uint32_t i;
  int16_t base = 8;

  for (i = 0U; i < n; i++)
  {
    s_buf[i] = bg[i];
  }
  if (on != 0U)
  {
    StampArrow(bw, bh, base + Xoff(t0), 10, Env(t0), cool);
    StampArrow(bw, bh, base + Xoff(t1), 10, Env(t1), cool);
  }
  else
  {
    StampArrow(bw, bh, base, 10, 70U, cool);
  }
  LT7683_DrawImage(dx, dy, bw, bh, s_buf);
}

static void Put(int32_t x, int32_t y, uint16_t color)
{
  if ((x < 0) || (y < 0) || (x >= (int32_t)FAN_S) || (y >= (int32_t)FAN_S))
  {
    return;
  }
  s_buf[(uint32_t)y * FAN_S + (uint32_t)x] = color;
}

static void PaintFan(uint16_t phase, uint8_t on)
{
  int32_t y;
  int32_t x;
  int32_t r2 = FAN_R * FAN_R;
  uint32_t i;
  uint8_t b;

  for (i = 0U; i < FAN_N; i++)
  {
    s_buf[i] = AhuFan_Bg[i];
  }

  for (y = -FAN_R; y <= FAN_R; y++)
  {
    for (x = -FAN_R; x <= FAN_R; x++)
    {
      if ((x * x + y * y) <= r2)
      {
        Put(FAN_CX + x, FAN_CY + y, COL_DARK);
      }
    }
  }

  if (on != 0U)
  {
    for (b = 0U; b < 12U; b++)
    {
      int32_t a0 = (int32_t)phase + (int32_t)b * 30 - 7;
      int32_t a1 = a0 + 14;
      int32_t rr;
      int32_t aa;
      for (rr = 8; rr <= FAN_R - 2; rr++)
      {
        for (aa = a0; aa <= a1; aa++)
        {
          int32_t px = FAN_CX + (int32_t)((rr * Cos1000(aa)) / 1000);
          int32_t py = FAN_CY + (int32_t)((rr * Sin1000(aa)) / 1000);
          Put(px, py, COL_BLADE);
        }
      }
    }
  }

  for (y = -8; y <= 8; y++)
  {
    for (x = -8; x <= 8; x++)
    {
      if ((x * x + y * y) <= 64)
      {
        Put(FAN_CX + x, FAN_CY + y, COL_HUB);
      }
    }
  }
  for (y = -12; y <= 12; y++)
  {
    for (x = -12; x <= 12; x++)
    {
      int32_t d2 = x * x + y * y;
      if ((d2 >= 121) && (d2 <= 169))
      {
        Put(FAN_CX + x, FAN_CY + y, COL_RING);
      }
    }
  }

  LT7683_DrawImage(FAN_X, FAN_Y, FAN_S, FAN_S, s_buf);
}

void AhuSnap_Tick(uint32_t tick_ms, uint8_t on)
{
  uint16_t step = (uint16_t)((tick_ms / 40U) % 256U);
  uint32_t t0;
  uint32_t t1;
  uint16_t phase;

  if (step == s_last_step)
  {
    return;
  }
  s_last_step = step;

  t0 = tick_ms % FLOW_PERIOD;
  t1 = (tick_ms + (FLOW_PERIOD / 2U)) % FLOW_PERIOD;
  PaintArrows(FLOW_L_X, FLOW_L_Y, FLOW_L_W, FLOW_L_H, AhuFlow_LeftBg, t0, t1, 0U, on);
  PaintArrows(FLOW_R_X, FLOW_R_Y, FLOW_R_W, FLOW_R_H, AhuFlow_RightBg, t0, t1, 1U, on);

  phase = (uint16_t)((tick_ms / 8U) % 360U);
  PaintFan(phase, on);
}
