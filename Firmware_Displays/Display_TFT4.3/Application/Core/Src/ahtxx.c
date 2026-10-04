#include "ahtxx.h"
#include "i2c.h"

#define AHTXX_ADDR_7BIT   0x38U
#define AHTXX_ADDR        (AHTXX_ADDR_7BIT << 1)
#define AHTXX_CMD_INIT20  0xBEU /* AHT20/AHT30 */
#define AHTXX_CMD_INIT10  0xE1U /* AHT10 */
#define AHTXX_CMD_MEASURE 0xACU
#define AHTXX_CMD_RESET   0xBAU

static uint8_t Ahtxx_Write(const uint8_t *data, uint16_t len)
{
  if (HAL_I2C_Master_Transmit(&hi2c1, AHTXX_ADDR, (uint8_t *)data, len, 200) !=
      HAL_OK)
  {
    return 1U;
  }
  return 0U;
}

static uint8_t Ahtxx_Present(void)
{
  if (HAL_I2C_IsDeviceReady(&hi2c1, AHTXX_ADDR, 5, 200) == HAL_OK)
  {
    return 1U;
  }
  return 0U;
}

uint8_t Ahtxx_Init(void)
{
  uint8_t cmd[3];
  uint8_t status;

  HAL_Delay(100);

  if (Ahtxx_Present() == 0U)
  {
    return 2U;
  }

  cmd[0] = AHTXX_CMD_RESET;
  (void)Ahtxx_Write(cmd, 1);
  HAL_Delay(40);

  /* Сначала AHT20/30, при неудаче — AHT10 */
  cmd[0] = AHTXX_CMD_INIT20;
  cmd[1] = 0x08U;
  cmd[2] = 0x00U;
  if (Ahtxx_Write(cmd, 3) != 0U)
  {
    cmd[0] = AHTXX_CMD_INIT10;
    if (Ahtxx_Write(cmd, 3) != 0U)
    {
      return 3U;
    }
  }
  HAL_Delay(20);

  if (HAL_I2C_Master_Receive(&hi2c1, AHTXX_ADDR, &status, 1, 200) == HAL_OK)
  {
    if ((status & 0x08U) == 0U)
    {
      /* не калиброван — ещё раз init */
      cmd[0] = AHTXX_CMD_INIT20;
      cmd[1] = 0x08U;
      cmd[2] = 0x00U;
      (void)Ahtxx_Write(cmd, 3);
      HAL_Delay(20);
    }
  }

  return (Ahtxx_Present() != 0U) ? 0U : 2U;
}

uint8_t Ahtxx_Read(Ahtxx_Data *out)
{
  uint8_t cmd[3];
  uint8_t data[7];
  uint32_t hum_raw;
  uint32_t temp_raw;
  uint32_t t0;

  if (out == 0)
  {
    return 1U;
  }

  if (Ahtxx_Present() == 0U)
  {
    return 2U;
  }

  cmd[0] = AHTXX_CMD_MEASURE;
  cmd[1] = 0x33U;
  cmd[2] = 0x00U;
  if (Ahtxx_Write(cmd, 3) != 0U)
  {
    return 2U;
  }

  HAL_Delay(100);

  t0 = HAL_GetTick();
  for (;;)
  {
    if (HAL_I2C_Master_Receive(&hi2c1, AHTXX_ADDR, data, 6, 200) != HAL_OK)
    {
      return 3U;
    }
    if ((data[0] & 0x80U) == 0U)
    {
      break;
    }
    if ((HAL_GetTick() - t0) > 1000U)
    {
      return 4U;
    }
    HAL_Delay(20);
  }

  hum_raw = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) |
            ((uint32_t)data[3] >> 4);
  temp_raw = ((uint32_t)(data[3] & 0x0FU) << 16) | ((uint32_t)data[4] << 8) |
             (uint32_t)data[5];

  out->humidity_pct = ((float)hum_raw * 100.0f) / 1048576.0f;
  out->temperature_c = (((float)temp_raw * 200.0f) / 1048576.0f) - 50.0f;

  if ((out->humidity_pct < 0.0f) || (out->humidity_pct > 100.0f) ||
      (out->temperature_c < -40.0f) || (out->temperature_c > 85.0f))
  {
    return 5U;
  }

  return 0U;
}
