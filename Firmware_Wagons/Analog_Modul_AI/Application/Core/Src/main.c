/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fdcan.h"
#include "spi.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ads1220.h"
#include "hvac_can.h"
#include "hvac_cfg.h"
#include "hvac_id.h"
#include "fwupd/fwupd_app.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AI_FW_VERSION       0x0103u /* major << 8 | minor */
#define AI_BOARD_REV        1u
#define AI_APP_START        0x08004000u /* after the 16K bootloader, see linker script */

/* ADS1220 inputs: 0/4-20 mA over 250 Ohm (up to 5 V), scaled x0.2 by INA159 */
#define AI_CHANNELS         2u
#define AI_MUX_CH1          0x0Au   /* AIN2, terminal H3 */
#define AI_MUX_CH2          0x09u   /* AIN1, terminal H4 */
#define AI_FRONTEND_GAIN    5.0f    /* ADC volts -> volts on the 250 Ohm shunt */
#define AI_SHUNT_OHM        250.0f

#define AI_LOOP_MIN_MA      3.6f    /* 4-20 mA below: open loop */
#define AI_LOOP_MAX_MA      21.0f   /* above: short / overrange */

/* The measurement frame carries 0.01 units in int16. */
#define AI_VALUE_LIMIT      327.0f

/* Until the Main PLC sends the configuration: bench transmitter Rotronic HF5,
 * output 1 = humidity 0..100 %RH, output 2 = temperature 0..50 C. */
#define AI_DEF_CH1_MIN      0.0f
#define AI_DEF_CH1_MAX      100.0f
#define AI_DEF_CH2_MIN      0.0f
#define AI_DEF_CH2_MAX      50.0f

#define AI_SEND_PERIOD_MS   500u
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
typedef struct
{
  uint8_t signal;  /* HVAC_SIG_* */
  float   lo;      /* value at the bottom of the signal */
  float   hi;      /* value at the top; lo == hi: report the signal itself */
} AiChannelCfg;

static AiChannelCfg s_ch[AI_CHANNELS] = {
  { HVAC_SIG_4_20MA, AI_DEF_CH1_MIN, AI_DEF_CH1_MAX },
  { HVAC_SIG_4_20MA, AI_DEF_CH2_MIN, AI_DEF_CH2_MAX },
};
static const uint8_t s_mux[AI_CHANNELS] = { AI_MUX_CH1, AI_MUX_CH2 };

static uint8_t s_cfg_gen;      /* generation of the last CFG_SET, 0 = defaults */
static uint8_t s_cfg_ok_mask;
static uint8_t s_cfg_bad_mask;
static uint8_t s_cfg_last_err;

FWUPD_APP_HEADER(FWUPD_TYPE_AI, AI_BOARD_REV, AI_FW_VERSION);
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Converts one channel to engineering units by its configuration; returns 0 if the loop is faulty or off. */
static uint8_t AI_ReadChannel(uint8_t ch, float *value, uint8_t *adc_fault)
{
  const AiChannelCfg *c = &s_ch[ch];
  float base = (c->signal == HVAC_SIG_4_20MA) ? 4.0f : 0.0f;
  float v_adc;

  *value = 0.0f;
  if (c->signal == HVAC_SIG_OFF) return 0u;
  if (ADS1220_ReadVoltage(s_mux[ch], &v_adc) != HAL_OK)
  {
    *adc_fault = 1u;
    return 0u;
  }
  float ma = v_adc * AI_FRONTEND_GAIN / AI_SHUNT_OHM * 1000.0f;
  if (ma > AI_LOOP_MAX_MA) return 0u;
  if (c->signal == HVAC_SIG_4_20MA && ma < AI_LOOP_MIN_MA) return 0u;
  if (ma < 0.0f) ma = 0.0f;

  if (c->lo == c->hi)
  {
    *value = ma;
    return 1u;
  }
  float v = c->lo + (ma - base) * (c->hi - c->lo) / (20.0f - base);
  if (v < c->lo) v = c->lo;
  if (v > c->hi) v = c->hi;
  *value = v;
  return 1u;
}

static void AI_Send(uint32_t id, uint8_t is_ext, const uint8_t d[8])
{
  FDCAN_TxHeaderTypeDef h = {0};

  h.Identifier = id;
  h.IdType = is_ext ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
  h.TxFrameType = FDCAN_DATA_FRAME;
  h.DataLength = FDCAN_DLC_BYTES_8;
  h.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  h.BitRateSwitch = FDCAN_BRS_OFF;
  h.FDFormat = FDCAN_CLASSIC_CAN;
  h.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &h, (uint8_t *)d);
}

static void AI_SendFwupd(uint32_t ext_id, const uint8_t data[8])
{
  AI_Send(ext_id, 1u, data);
}

static void AI_SendStd(uint32_t std_id, const uint8_t data[8])
{
  AI_Send(std_id, 0u, data);
}

/* Optocoupler input with pull-up: line raised -> pin low. */
static uint8_t AI_IdInputActive(void)
{
  return (HAL_GPIO_ReadPin(IN_ID_AI_GPIO_Port, IN_ID_AI_Pin) == GPIO_PIN_RESET) ? 1u : 0u;
}

static void AI_IdSetOutput(uint8_t out)
{
  HAL_GPIO_WritePin(OUT_ID_AI_GPIO_Port, OUT_ID_AI_Pin,
                    (out == HVAC_ID_OUT_CHAIN) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static const HvacIdNodeCfg s_id_cfg = {
  .cat = HVAC_CAT_AI,
  .board_rev = AI_BOARD_REV,
  .input_active = AI_IdInputActive,
  .set_output = AI_IdSetOutput,
  .send = AI_SendStd,
};

static void AI_SendCfgStatus(void)
{
  uint8_t d[8];
  uint8_t line, rail, place;

  if (!HvacId_Get(&line, &rail, &place)) rail = place = HVAC_CFG_PLACE_ANY;
  d[0] = FWUPD_TYPE_AI;
  d[1] = place;
  d[2] = rail;
  d[3] = s_cfg_gen;
  d[4] = s_cfg_ok_mask;
  d[5] = s_cfg_bad_mask;
  d[6] = s_cfg_last_err;
  d[7] = 0u;
  AI_SendStd(HVAC_CAN_ID_CFG_STATUS, d);
}

/* Applies one CFG_SET addressed to this module; the channel keeps its previous setting if rejected. */
static void AI_ApplyCfg(const uint8_t d[8])
{
  uint8_t ch = (uint8_t)(d[2] >> 4);
  uint8_t sig = (uint8_t)(d[2] & 0x0Fu);
  float lo = (float)hvac_cfg_get16(&d[4]) / 10.0f;
  float hi = (float)hvac_cfg_get16(&d[6]) / 10.0f;
  uint8_t err = HVAC_CFG_E_OK;
  uint8_t line, rail, place;

  if (!HvacId_Get(&line, &rail, &place) || d[0] != place || d[1] != rail) return;

  if (ch < 1u || ch > AI_CHANNELS) err = HVAC_CFG_E_CHANNEL;
  else if (sig != HVAC_SIG_OFF && sig != HVAC_SIG_4_20MA && sig != HVAC_SIG_0_20MA) err = HVAC_CFG_E_SIGNAL;
  else if (sig != HVAC_SIG_OFF && !(lo == 0.0f && hi == 0.0f) &&
           (lo >= hi || lo < -AI_VALUE_LIMIT || hi > AI_VALUE_LIMIT)) err = HVAC_CFG_E_RANGE;

  /* A new generation starts clean; within one generation the first rejection is kept. */
  if (d[3] != s_cfg_gen)
  {
    s_cfg_gen = d[3];
    s_cfg_ok_mask = 0u;
    s_cfg_bad_mask = 0u;
    s_cfg_last_err = HVAC_CFG_E_OK;
  }
  if (err != HVAC_CFG_E_OK && s_cfg_last_err == HVAC_CFG_E_OK) s_cfg_last_err = err;
  if (ch >= 1u && ch <= AI_CHANNELS)
  {
    uint8_t bit = (uint8_t)(1u << (ch - 1u));
    if (err == HVAC_CFG_E_OK)
    {
      s_ch[ch - 1u].signal = sig;
      s_ch[ch - 1u].lo = lo;
      s_ch[ch - 1u].hi = hi;
      s_cfg_ok_mask |= bit;
      s_cfg_bad_mask &= (uint8_t)~bit;
    }
    else
    {
      s_cfg_ok_mask &= (uint8_t)~bit;
      s_cfg_bad_mask |= bit;
    }
  }
}

static void AI_PollRx(void)
{
  FDCAN_RxHeaderTypeDef rh;
  uint8_t d[8];

  while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan2, FDCAN_RX_FIFO0) > 0u)
  {
    if (HAL_FDCAN_GetRxMessage(&hfdcan2, FDCAN_RX_FIFO0, &rh, d) != HAL_OK) break;
    if (rh.DataLength != FDCAN_DLC_BYTES_8) continue;
    if (rh.IdType == FDCAN_EXTENDED_ID)
    {
      FwupdApp_OnRx(rh.Identifier, 1u, d, 8u);
    }
    else if (rh.Identifier == HVAC_CAN_ID_ID_CMD)
    {
      HvacId_OnCmd(d);
    }
    else if (rh.Identifier == HVAC_CAN_ID_CFG_SET)
    {
      uint8_t line, rail, place;
      AI_ApplyCfg(d);
      if (HvacId_Get(&line, &rail, &place) && d[0] == place && d[1] == rail) AI_SendCfgStatus();
    }
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  SCB->VTOR = AI_APP_START;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FDCAN2_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  ADS1220_Init();

  FDCAN_TxHeaderTypeDef TxHeader;
  TxHeader.Identifier = HVAC_CAN_ID_AI_MEAS;
  TxHeader.IdType = FDCAN_STANDARD_ID;
  TxHeader.TxFrameType = FDCAN_DATA_FRAME;
  TxHeader.DataLength = FDCAN_DLC_BYTES_8;
  TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
  TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
  TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  TxHeader.MessageMarker = 0;

  /* Standard IDs: configuration and identification; extended IDs: firmware update. */
  HAL_FDCAN_ConfigGlobalFilter(&hfdcan2, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0,
                               FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
  HAL_FDCAN_Start(&hfdcan2);

  {
    const uint32_t uid[3] = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };
    FwupdApp_Init(AI_SendFwupd, FWUPD_TYPE_AI, AI_BOARD_REV, AI_FW_VERSION);
    HvacId_Init(&s_id_cfg, fwupd_node_tag(uid));
  }

  HvacAiMeas meas = {0};
  uint32_t last_send = HAL_GetTick() - AI_SEND_PERIOD_MS;
  uint32_t last_status = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (hfdcan2.Instance->PSR & FDCAN_PSR_BO) {
        HAL_FDCAN_Stop(&hfdcan2);
        HAL_FDCAN_Start(&hfdcan2);
    }

    AI_PollRx();
    FwupdApp_Poll();
    HvacId_Poll();

    if (HAL_GetTick() - last_status >= HVAC_CFG_STATUS_MS)
    {
      last_status = HAL_GetTick();
      AI_SendCfgStatus();
    }

    if (HAL_GetTick() - last_send >= AI_SEND_PERIOD_MS)
    {
      last_send = HAL_GetTick();

      /* Frame fields: humidity = channel 1, temperature = channel 2 (bench layout). */
      uint8_t adc_fault = 0u;
      meas.status = 0u;
      if (!AI_ReadChannel(0u, &meas.humidity_pct, &adc_fault))
        meas.status |= HVAC_AI_ST_HUM_FAULT;
      if (!AI_ReadChannel(1u, &meas.temperature_c, &adc_fault))
        meas.status |= HVAC_AI_ST_TEMP_FAULT;
      if (adc_fault) meas.status |= HVAC_AI_ST_ADC_FAULT;
      meas.counter++;

      uint8_t tx_data[8];
      hvac_can_ai_encode(&meas, tx_data);
      HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &TxHeader, tx_data);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
