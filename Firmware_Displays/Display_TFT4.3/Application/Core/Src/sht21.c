#include "sht21.h"
#include "i2c.h"

/* GY-21: Si7021 / HTU21D / SHT21, адрес 0x40 */
#define SHT21_ADDR           (0x40U << 1)
#define SHT21_CMD_HUM_NH     0xF5U
#define SHT21_CMD_TEMP_PREV  0xE0U /* температура от последнего замера RH */
#define SHT21_CMD_RESET      0xFEU
#define SHT21_CMD_USER_W     0xE6U
#define SHT21_CMD_USER_R     0xE7U

static uint8_t Sht21_Cmd(uint8_t cmd)
{
  if (HAL_I2C_Master_Transmit(&hi2c1, SHT21_ADDR, &cmd, 1, 200) != HAL_OK)
  {
    return 1U;
  }
  return 0U;
}

uint8_t Sht21_Init(void)
{
  uint8_t buf[2];
  uint8_t user;

  HAL_Delay(50);

  if (HAL_I2C_IsDeviceReady(&hi2c1, SHT21_ADDR, 5, 200) != HAL_OK)
  {
    return 2U;
  }

  if (Sht21_Cmd(SHT21_CMD_RESET) != 0U)
  {
    return 3U;
  }
  HAL_Delay(20);

  /* Выключить нагреватель (бит 2), 12 бит RH / 14 бит T — типичный режим */
  if (Sht21_Cmd(SHT21_CMD_USER_R) != 0U)
  {
    return 4U;
  }
  if (HAL_I2C_Master_Receive(&hi2c1, SHT21_ADDR, &user, 1, 200) != HAL_OK)
  {
    return 4U;
  }
  user = (uint8_t)(user & (uint8_t)~0x04U); /* heater off */
  buf[0] = SHT21_CMD_USER_W;
  buf[1] = user;
  if (HAL_I2C_Master_Transmit(&hi2c1, SHT21_ADDR, buf, 2, 200) != HAL_OK)
  {
    return 4U;
  }

  return 0U;
}

uint8_t Sht21_Read(Sht21_Data *out)
{
  uint8_t cmd;
  uint8_t data[3];
  uint16_t raw_h;
  uint16_t raw_t;
  float rh;
  float tc;

  if (out == 0)
  {
    return 1U;
  }

  if (HAL_I2C_IsDeviceReady(&hi2c1, SHT21_ADDR, 2, 100) != HAL_OK)
  {
    return 2U;
  }

  /* 1) Замер влажности (внутри датчик меряет и температуру) */
  cmd = SHT21_CMD_HUM_NH;
  if (HAL_I2C_Master_Transmit(&hi2c1, SHT21_ADDR, &cmd, 1, 200) != HAL_OK)
  {
    return 2U;
  }
  HAL_Delay(50);
  if (HAL_I2C_Master_Receive(&hi2c1, SHT21_ADDR, data, 3, 200) != HAL_OK)
  {
    return 3U;
  }
  raw_h = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
  raw_h &= 0xFFFCU;

  /* 2) Температура из того же цикла (команда 0xE0) — как в даташите Si7021 */
  cmd = SHT21_CMD_TEMP_PREV;
  if (HAL_I2C_Master_Transmit(&hi2c1, SHT21_ADDR, &cmd, 1, 200) != HAL_OK)
  {
    return 12U;
  }
  if (HAL_I2C_Master_Receive(&hi2c1, SHT21_ADDR, data, 2, 200) != HAL_OK)
  {
    return 13U;
  }
  raw_t = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
  raw_t &= 0xFFFCU;

  /* Формулы datasheet SHT21 / HTU21D / Si7021 */
  rh = -6.0f + (125.0f * ((float)raw_h / 65536.0f));
  tc = -46.85f + (175.72f * ((float)raw_t / 65536.0f));

  /* Температурная компенсация RH (Sensirion SHT2x) */
  rh = rh + (tc - 25.0f) * (0.01f + 0.00008f * (float)raw_h);

  if (rh < 0.0f)
  {
    rh = 0.0f;
  }
  if (rh > 100.0f)
  {
    rh = 100.0f;
  }

  if ((tc < -40.0f) || (tc > 125.0f))
  {
    return 5U;
  }

  out->humidity_pct = rh;
  out->temperature_c = tc;
  out->raw_h = raw_h;
  out->raw_t = raw_t;
  return 0U;
}
