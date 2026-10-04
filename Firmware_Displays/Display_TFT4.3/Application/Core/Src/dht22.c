#include "dht22.h"
#include "main.h"
#include "tim.h"

/*
 * Разбор кадра DHT22/AM2302 (после успешной контрольной суммы):
 *   RH  = ((b0 << 8) | b1) * 0.1 %
 *   T   = ((b2 << 8) | b3) * 0.1 °C  (бит15 — знак)
 * Пример raw 00 0A 01 11 1C → RH=1.0 %, T=27.3 °C — так шлёт датчик.
 */

static void Dht22_TimStartUs(void)
{
  /* TIM2: PSC=95 при ~96 МГц таймера → 1 мкс/тик (см. MX) */
  __HAL_TIM_SET_AUTORELOAD(&htim2, 0xFFFFFFFFU);
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  (void)HAL_TIM_Base_Start(&htim2);
}

static void Dht22_DelayUs(uint32_t us)
{
  uint32_t start = __HAL_TIM_GET_COUNTER(&htim2);
  while ((__HAL_TIM_GET_COUNTER(&htim2) - start) < us)
  {
  }
}

static void Dht22_PinOut(void)
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = DHT22_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(DHT22_GPIO_Port, &gpio);
}

static void Dht22_PinIn(void)
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = DHT22_Pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DHT22_GPIO_Port, &gpio);
}

static uint8_t Dht22_ReadLevel(void)
{
  return ((DHT22_GPIO_Port->IDR & DHT22_Pin) != 0U) ? 1U : 0U;
}

static uint8_t Dht22_Expect(uint8_t level, uint32_t timeout_us)
{
  uint32_t start = __HAL_TIM_GET_COUNTER(&htim2);
  while (Dht22_ReadLevel() != level)
  {
    if ((__HAL_TIM_GET_COUNTER(&htim2) - start) > timeout_us)
    {
      return 1U;
    }
  }
  return 0U;
}

void Dht22_Init(void)
{
  __HAL_RCC_GPIOB_CLK_ENABLE();
  Dht22_TimStartUs();

  HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_SET);
  Dht22_PinOut();
  HAL_Delay(2000);
}

uint8_t Dht22_Read(Dht22_Data *out)
{
  uint8_t data[5] = {0};
  uint8_t i;
  uint32_t primask;

  if (out == 0)
  {
    return 1U;
  }

  out->raw[0] = out->raw[1] = out->raw[2] = out->raw[3] = out->raw[4] = 0U;

  primask = __get_PRIMASK();
  __disable_irq();

  Dht22_PinOut();
  HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_RESET);
  Dht22_DelayUs(1200);
  HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_SET);
  Dht22_DelayUs(30);
  Dht22_PinIn();

  if (Dht22_Expect(0, 100) != 0U)
  {
    __set_PRIMASK(primask);
    return 2U;
  }
  if (Dht22_Expect(1, 100) != 0U)
  {
    __set_PRIMASK(primask);
    return 3U;
  }
  if (Dht22_Expect(0, 100) != 0U)
  {
    __set_PRIMASK(primask);
    return 4U;
  }

  /* Длина высокого импульса: ~26 мкс = 0, ~70 мкс = 1 */
  for (i = 0U; i < 40U; i++)
  {
    uint32_t t0;
    uint32_t high_us;

    if (Dht22_Expect(1, 80) != 0U)
    {
      __set_PRIMASK(primask);
      return 5U;
    }
    t0 = __HAL_TIM_GET_COUNTER(&htim2);
    if (Dht22_Expect(0, 120) != 0U)
    {
      __set_PRIMASK(primask);
      return 6U;
    }
    high_us = __HAL_TIM_GET_COUNTER(&htim2) - t0;
    data[i / 8U] =
        (uint8_t)((data[i / 8U] << 1) | ((high_us > 40U) ? 1U : 0U));
  }

  __set_PRIMASK(primask);
  Dht22_PinOut();
  HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_SET);

  out->raw[0] = data[0];
  out->raw[1] = data[1];
  out->raw[2] = data[2];
  out->raw[3] = data[3];
  out->raw[4] = data[4];

  if (((uint8_t)(data[0] + data[1] + data[2] + data[3])) != data[4])
  {
    return 7U;
  }

  {
    uint16_t hum_raw = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    uint16_t temp_raw = (uint16_t)(((uint16_t)data[2] << 8) | data[3]);
    float hum = (float)hum_raw * 0.1f;
    float temp;

    if ((temp_raw & 0x8000U) != 0U)
    {
      temp_raw = (uint16_t)(temp_raw & 0x7FFFU);
      temp = -((float)temp_raw * 0.1f);
    }
    else
    {
      temp = (float)temp_raw * 0.1f;
    }

    if ((hum > 100.0f) || (temp < -40.0f) || (temp > 80.0f))
    {
      return 8U;
    }

    out->humidity_pct = hum;
    out->temperature_c = temp;
  }

  return 0U;
}
