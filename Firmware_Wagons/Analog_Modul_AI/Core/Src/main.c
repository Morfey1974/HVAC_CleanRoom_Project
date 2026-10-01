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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ADS1220 inputs: 4-20 mA over 250 Ohm (1-5 V), scaled x0.2 by INA159 */
#define AI_MUX_HUMIDITY     0x0Au   /* AIN2, terminal H3, transmitter output 1 */
#define AI_MUX_TEMPERATURE  0x09u   /* AIN1, terminal H4, transmitter output 2 */
#define AI_FRONTEND_GAIN    5.0f    /* ADC volts -> volts on the 250 Ohm shunt */
#define AI_SHUNT_OHM        250.0f

#define AI_LOOP_MIN_MA      3.6f    /* below: open loop */
#define AI_LOOP_MAX_MA      21.0f   /* above: short / overrange */

/* Rotronic HF5: humidity is always 0..100 %RH; the temperature scale depends on
 * the order code on the label (digit after the probe code): 1 = 0..50 C,
 * 2 = 10..40 C, 3 = -40..60 C, 4 = -30..70 C, 5 = -40..85 C. */
#define AI_HUM_MIN          0.0f
#define AI_HUM_MAX          100.0f
#define AI_TEMP_MIN         0.0f
#define AI_TEMP_MAX         50.0f

#define AI_SEND_PERIOD_MS   500u
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Converts one 4-20 mA channel to engineering units; returns 0 if the loop is faulty. */
static uint8_t AI_ReadLoop(uint8_t mux, float lo, float hi, float *value, uint8_t *adc_fault)
{
  float v_adc;
  if (ADS1220_ReadVoltage(mux, &v_adc) != HAL_OK)
  {
    *adc_fault = 1u;
    return 0u;
  }
  float ma = v_adc * AI_FRONTEND_GAIN / AI_SHUNT_OHM * 1000.0f;
  if (ma < AI_LOOP_MIN_MA || ma > AI_LOOP_MAX_MA) return 0u;

  float v = lo + (ma - 4.0f) * (hi - lo) / 16.0f;
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  *value = v;
  return 1u;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

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
  
  HAL_FDCAN_Start(&hfdcan2);

  HvacAiMeas meas = {0};
  uint32_t last_send = HAL_GetTick() - AI_SEND_PERIOD_MS;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (hfdcan2.Instance->PSR & FDCAN_PSR_BO) {
        HAL_FDCAN_Stop(&hfdcan2);
        HAL_FDCAN_Start(&hfdcan2);
    }

    if (HAL_GetTick() - last_send >= AI_SEND_PERIOD_MS)
    {
      last_send = HAL_GetTick();

      uint8_t adc_fault = 0u;
      meas.status = 0u;
      if (!AI_ReadLoop(AI_MUX_HUMIDITY, AI_HUM_MIN, AI_HUM_MAX, &meas.humidity_pct, &adc_fault))
        meas.status |= HVAC_AI_ST_HUM_FAULT;
      if (!AI_ReadLoop(AI_MUX_TEMPERATURE, AI_TEMP_MIN, AI_TEMP_MAX, &meas.temperature_c, &adc_fault))
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
