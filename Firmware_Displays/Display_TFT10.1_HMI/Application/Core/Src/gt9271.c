#include "gt9271.h"
#include "i2c.h"
#include "main.h"

#define GT9271_ADDR_5D   (0x5DU << 1)
#define GT9271_ADDR_14   (0x14U << 1)
#define GT9271_REG_CFG   0x8047U
#define GT9271_REG_GSTID 0x814EU
#define GT9271_I2C_TO    50U
#define GT9271_CFG_TO    200U

/* EastRising ER-TFTM101-1 GT9271 table from BuyDisplay TP.h */
static const uint8_t GT9271_CFG[] = {
    0x63U, 0x00U, 0x04U, 0x58U, 0x02U, 0x0AU, 0x3DU, 0x00U,
    0x01U, 0x08U, 0x28U, 0x0FU, 0x50U, 0x32U, 0x03U, 0x05U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x17U,
    0x19U, 0x1DU, 0x14U, 0x90U, 0x2FU, 0x89U, 0x23U, 0x25U,
    0xD3U, 0x07U, 0x00U, 0x00U, 0x00U, 0x02U, 0x03U, 0x1DU,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x19U, 0x32U, 0x94U, 0xD5U, 0x02U,
    0x07U, 0x00U, 0x00U, 0x04U, 0xA2U, 0x1AU, 0x00U, 0x90U,
    0x1EU, 0x00U, 0x80U, 0x23U, 0x00U, 0x73U, 0x28U, 0x00U,
    0x68U, 0x2EU, 0x00U, 0x68U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x16U, 0x15U, 0x14U, 0x11U, 0x10U, 0x0FU, 0x0EU, 0x0DU,
    0x0CU, 0x09U, 0x08U, 0x07U, 0x06U, 0x05U, 0x04U, 0x01U,
    0x00U, 0xFFU, 0xFFU, 0xFFU, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x29U, 0x28U,
    0x27U, 0x26U, 0x25U, 0x24U, 0x23U, 0x22U, 0x21U, 0x20U,
    0x1FU, 0x1EU, 0x1CU, 0x1BU, 0x19U, 0x14U, 0x13U, 0x12U,
    0x11U, 0x10U, 0x0FU, 0x0EU, 0x0DU, 0x0CU, 0x0AU, 0x08U,
    0x07U, 0x06U, 0x04U, 0x02U, 0x00U, 0xFFU, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x71U, 0x01U
};

static uint16_t s_addr = GT9271_ADDR_5D;
static uint8_t s_ok;
static uint8_t s_down;
static uint16_t s_last_x;
static uint16_t s_last_y;

static void GT9271_IntOutput(GPIO_PinState level)
{
  GPIO_InitTypeDef g = {0};

  g.Pin = CTP_INT_Pin;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CTP_INT_GPIO_Port, &g);
  HAL_GPIO_WritePin(CTP_INT_GPIO_Port, CTP_INT_Pin, level);
}

static void GT9271_IntInput(void)
{
  GPIO_InitTypeDef g = {0};

  g.Pin = CTP_INT_Pin;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(CTP_INT_GPIO_Port, &g);
}

static void GT9271_Reset(void)
{
  HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
  GT9271_IntOutput(GPIO_PIN_RESET);
  HAL_GPIO_WritePin(CTP_RST_GPIO_Port, CTP_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(20);
  HAL_GPIO_WritePin(CTP_RST_GPIO_Port, CTP_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(500);
  GT9271_IntInput();
  HAL_Delay(200);
}

static HAL_StatusTypeDef GT9271_ReadRegs(uint16_t reg, uint8_t *buf, uint16_t n)
{
  return HAL_I2C_Mem_Read(&hi2c1, s_addr, reg, I2C_MEMADD_SIZE_16BIT, buf, n, GT9271_I2C_TO);
}

static HAL_StatusTypeDef GT9271_WriteRegs(uint16_t reg, uint8_t *buf, uint16_t n)
{
  return HAL_I2C_Mem_Write(&hi2c1, s_addr, reg, I2C_MEMADD_SIZE_16BIT, buf, n, GT9271_CFG_TO);
}

static uint8_t GT9271_SendCfg(void)
{
  uint8_t cfg[sizeof(GT9271_CFG)];
  uint8_t retry;

  for (retry = 0U; retry < (uint8_t)sizeof(GT9271_CFG); retry++)
  {
    cfg[retry] = GT9271_CFG[retry];
  }
  cfg[sizeof(GT9271_CFG) - 1U] = 0x01U;

  for (retry = 0U; retry < 5U; retry++)
  {
    if (GT9271_WriteRegs(GT9271_REG_CFG, cfg, (uint16_t)sizeof(cfg)) == HAL_OK)
    {
      HAL_Delay(20);
      return 1U;
    }
    HAL_Delay(10);
  }
  return 0U;
}

static void GT9271_I2c100k(void)
{
  HAL_I2C_DeInit(&hi2c1);
  hi2c1.Init.Timing = 0xF0831F27U;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    return;
  }
  (void)HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);
  (void)HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0);
}

uint8_t GT9271_Init(void)
{
  s_ok = 0U;
  s_down = 0U;

  GT9271_I2c100k();
  GT9271_Reset();
  s_addr = GT9271_ADDR_5D;
  if (GT9271_SendCfg() == 0U)
  {
    s_addr = GT9271_ADDR_14;
    GT9271_Reset();
    if (GT9271_SendCfg() == 0U)
    {
      return 0U;
    }
  }

  s_ok = 1U;
  return 1U;
}

uint8_t GT9271_Poll(uint16_t *x, uint16_t *y)
{
  uint8_t buf[8];
  uint8_t zero = 0U;
  uint8_t n;
  uint16_t rx;
  uint16_t ry;

  if ((s_ok == 0U) || (x == 0) || (y == 0))
  {
    return 0U;
  }

  if (GT9271_ReadRegs(GT9271_REG_GSTID, buf, 8U) != HAL_OK)
  {
    return s_down;
  }

  if ((buf[0] & 0x80U) == 0U)
  {
    if (s_down != 0U)
    {
      *x = s_last_x;
      *y = s_last_y;
    }
    return s_down;
  }

  n = (uint8_t)(buf[0] & 0x0FU);
  (void)GT9271_WriteRegs(GT9271_REG_GSTID, &zero, 1U);

  if (n == 0U)
  {
    s_down = 0U;
    return 0U;
  }

  rx = (uint16_t)((uint16_t)buf[2] | ((uint16_t)buf[3] << 8));
  ry = (uint16_t)((uint16_t)buf[4] | ((uint16_t)buf[5] << 8));
  if (rx > 1023U)
  {
    rx = 1023U;
  }
  if (ry > 599U)
  {
    ry = 599U;
  }
  /* BuyDisplay demo: Draw_Big_Point(1024 - x, 600 - y) */
  s_last_x = (uint16_t)(1023U - rx);
  s_last_y = (uint16_t)(599U - ry);
  s_down = 1U;
  *x = s_last_x;
  *y = s_last_y;
  return 1U;
}
