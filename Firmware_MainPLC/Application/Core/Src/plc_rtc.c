/*
 * plc_rtc.c — FRTC8900S / RX8900 real-time clock on I2C1 (address 0x32), see plc_rtc.h.
 *
 * Registers: 0x00..0x06 sec, min, hour, week (one-hot, bit 0 = Sunday), day, month, year (BCD),
 * 0x0D extension, 0x0E flags (bit 1 VLF: backup voltage was lost, time is not valid), 0x0F control.
 */
#include "plc_rtc.h"

#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "i2c.h"

#define RTC_ADDR          (0x32u << 1)
#define RTC_REG_SEC       0x00u
#define RTC_REG_EXT       0x0Du
#define RTC_REG_FLAG      0x0Eu
#define RTC_REG_CTRL      0x0Fu
#define RTC_FLAG_VLF      0x02u
#define RTC_CTRL_DEFAULT  0x40u  /* CSEL = 01: temperature compensation every 2 s */
#define RTC_I2C_TMO_MS    50u
#define RTC_PERIOD_MS     500u
#define RTC_START_MS      1000u  /* oscillation start after power-up */
#define RTC_FAILS_REINIT  3u

static const osThreadAttr_t s_task_attr = {
  .name = "rtcTask",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

static PlcRtcSnapshot s_snap;
static PlcRtcTime s_set;
static volatile uint8_t s_set_req;

static uint8_t Bcd(uint8_t v)   { return (uint8_t)(((v / 10u) << 4) | (v % 10u)); }
static uint8_t UnBcd(uint8_t v) { return (uint8_t)((v >> 4) * 10u + (v & 0x0Fu)); }

static uint8_t Rtc_DaysIn(uint16_t y, uint8_t m)
{
  static const uint8_t d[12] = {31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u};
  if (m == 2u && (y % 4u) == 0u) return 29u;
  return d[m - 1u];
}

static uint8_t Rtc_TimeOk(const PlcRtcTime *t)
{
  return t->year >= 2000u && t->year <= 2099u && t->month >= 1u && t->month <= 12u &&
         t->day >= 1u && t->day <= Rtc_DaysIn(t->year, t->month) &&
         t->hour <= 23u && t->min <= 59u && t->sec <= 59u;
}

/* 0 = Sunday (Sakamoto). */
static uint8_t Rtc_Weekday(uint16_t y, uint8_t m, uint8_t d)
{
  static const uint8_t k[12] = {0u, 3u, 2u, 5u, 0u, 3u, 5u, 1u, 4u, 6u, 2u, 4u};
  if (m < 3u) y--;
  return (uint8_t)((y + y / 4u - y / 100u + y / 400u + k[m - 1u] + d) % 7u);
}

static uint8_t Rtc_Read(uint8_t reg, uint8_t *buf, uint16_t n)
{
  return HAL_I2C_Mem_Read(&hi2c1, RTC_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, n, RTC_I2C_TMO_MS) == HAL_OK;
}

static uint8_t Rtc_Write(uint8_t reg, const uint8_t *buf, uint16_t n)
{
  return HAL_I2C_Mem_Write(&hi2c1, RTC_ADDR, reg, I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, n, RTC_I2C_TMO_MS) == HAL_OK;
}

static uint8_t Rtc_DoSet(const PlcRtcTime *t, uint8_t full_init)
{
  uint8_t r[7];
  uint8_t ext = 0x00u, ctrl = RTC_CTRL_DEFAULT, flag = 0x00u;

  r[0] = Bcd(t->sec);
  r[1] = Bcd(t->min);
  r[2] = Bcd(t->hour);
  r[3] = (uint8_t)(1u << Rtc_Weekday(t->year, t->month, t->day));
  r[4] = Bcd(t->day);
  r[5] = Bcd(t->month);
  r[6] = Bcd((uint8_t)(t->year - 2000u));

  if (full_init && (!Rtc_Write(RTC_REG_EXT, &ext, 1u) || !Rtc_Write(RTC_REG_CTRL, &ctrl, 1u))) return 0u;
  if (!Rtc_Write(RTC_REG_SEC, r, sizeof(r))) return 0u;
  return Rtc_Write(RTC_REG_FLAG, &flag, 1u);
}

static void PlcRtc_Task(void *argument)
{
  uint8_t fails = 0u;

  (void)argument;
  osDelay(RTC_START_MS);
  for (uint8_t a = 0x08u; a < 0x78u; a++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(a << 1), 1u, 5u) != HAL_OK) continue;
    if (s_snap.scan_n++ == 0u) s_snap.scan_first = a;
  }
  for (;;)
  {
    uint8_t r[16];
    PlcRtcSnapshot s;
    uint8_t ok = Rtc_Read(RTC_REG_SEC, r, sizeof(r));

    taskENTER_CRITICAL();
    s = s_snap;
    taskEXIT_CRITICAL();

    if (ok && s_set_req)
    {
      PlcRtcTime t;

      taskENTER_CRITICAL();
      t = s_set;
      s_set_req = 0u;
      taskEXIT_CRITICAL();
      if (Rtc_DoSet(&t, (r[RTC_REG_FLAG] & RTC_FLAG_VLF) != 0u) && Rtc_Read(RTC_REG_SEC, r, sizeof(r))) s.sets++;
      else ok = 0u;
    }

    s.present = ok;
    if (ok)
    {
      fails = 0u;
      s.t.sec = UnBcd(r[0] & 0x7Fu);
      s.t.min = UnBcd(r[1] & 0x7Fu);
      s.t.hour = UnBcd(r[2] & 0x3Fu);
      s.t.day = UnBcd(r[4] & 0x3Fu);
      s.t.month = UnBcd(r[5] & 0x1Fu);
      s.t.year = (uint16_t)(2000u + UnBcd(r[6]));
      s.valid = ((r[RTC_REG_FLAG] & RTC_FLAG_VLF) == 0u) && Rtc_TimeOk(&s.t);
    }
    else
    {
      s.valid = 0u;
      s.errors++;
      s.last_err = HAL_I2C_GetError(&hi2c1);
      if (++fails >= RTC_FAILS_REINIT)
      {
        fails = 0u;
        (void)HAL_I2C_DeInit(&hi2c1);
        (void)HAL_I2C_Init(&hi2c1);
      }
    }

    taskENTER_CRITICAL();
    s_snap = s;
    taskEXIT_CRITICAL();
    osDelay(RTC_PERIOD_MS);
  }
}

void PlcRtc_Start(void)
{
  memset(&s_snap, 0, sizeof(s_snap));
  osThreadNew(PlcRtc_Task, NULL, &s_task_attr);
}

void PlcRtc_Get(PlcRtcSnapshot *out)
{
  taskENTER_CRITICAL();
  *out = s_snap;
  taskEXIT_CRITICAL();
}

uint8_t PlcRtc_RequestSet(const PlcRtcTime *t)
{
  if (!Rtc_TimeOk(t)) return 0u;
  taskENTER_CRITICAL();
  s_set = *t;
  s_set_req = 1u;
  taskEXIT_CRITICAL();
  return 1u;
}
