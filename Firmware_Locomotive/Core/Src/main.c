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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern FDCAN_HandleTypeDef hfdcan2;
FDCAN_TxHeaderTypeDef TxHeader;
uint8_t TxData[8];

uint32_t node_ids[3] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}; // 0xFFFFFFFF = Lost/Error
uint32_t node_last_seen[3] = {0, 0, 0};
uint32_t loco_tx_count = 0;
uint16_t node_errors[3] = {0, 0, 0};
uint32_t loco_hw_errors = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_FDCAN1_Init();
  MX_FDCAN2_Init();
  /* USER CODE BEGIN 2 */
  TxHeader.Identifier = 0x111;
  TxHeader.IdType = FDCAN_STANDARD_ID;
  TxHeader.TxFrameType = FDCAN_DATA_FRAME;
  TxHeader.DataLength = FDCAN_DLC_BYTES_3;
  TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
  TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
  TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  TxHeader.MessageMarker = 0;

  FDCAN_FilterTypeDef sFilterConfig;
  sFilterConfig.IdType = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex = 0;
  sFilterConfig.FilterType = FDCAN_FILTER_MASK;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1 = 0x000;
  sFilterConfig.FilterID2 = 0x000;
  HAL_FDCAN_ConfigFilter(&hfdcan2, &sFilterConfig);
  HAL_FDCAN_ActivateNotification(&hfdcan2, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);

  HAL_FDCAN_Start(&hfdcan2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (hfdcan2.Instance->PSR & FDCAN_PSR_BO) {
        HAL_FDCAN_Stop(&hfdcan2);
        HAL_FDCAN_Start(&hfdcan2);
    }

    uint32_t now = HAL_GetTick();

    // 1. Опрос 3-х дисплеев
    for (int i = 0; i < 3; i++) {
        if (now - node_last_seen[i] > 3000) {
            node_ids[i] = 0xFFFFFFFF; // Error / Lost
            if (node_errors[i] < 65535) node_errors[i]++;
        }
        
        TxHeader.Identifier = 0x201 + i;
        TxHeader.DataLength = FDCAN_DLC_BYTES_0;
        if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &TxHeader, TxData) == HAL_OK) loco_tx_count++;
        HAL_Delay(10);
    }

    // 2. Отправка результатов опроса главному дисплею
    for (int i = 0; i < 3; i++) {
        TxHeader.Identifier = 0x301 + i;
        TxHeader.DataLength = FDCAN_DLC_BYTES_4;
        TxData[0] = node_ids[i] & 0xFF;
        TxData[1] = (node_ids[i] >> 8) & 0xFF;
        TxData[2] = (node_ids[i] >> 16) & 0xFF;
        TxData[3] = (node_ids[i] >> 24) & 0xFF;
        if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &TxHeader, TxData) == HAL_OK) loco_tx_count++;
        HAL_Delay(10);
    }

    // 3. Отправка случайных данных по комнате
    TxHeader.Identifier = 0x111;
    TxHeader.DataLength = FDCAN_DLC_BYTES_3;
    TxData[0] = (uint8_t)((rand() % 16) + 15); // Температура: 15..30
    TxData[1] = (uint8_t)((rand() % 31) + 30); // Влажность: 30..60
    TxData[2] = (uint8_t)((rand() % 51) + 50); // Давление: 50..100
    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &TxHeader, TxData) == HAL_OK) loco_tx_count++;
    
    // 4. Отправка метрик (статистики) на дисплей
    TxHeader.Identifier = 0x304;
    TxHeader.DataLength = FDCAN_DLC_BYTES_8;
    TxData[0] = loco_tx_count & 0xFF;
    TxData[1] = (loco_tx_count >> 8) & 0xFF;
    TxData[2] = (loco_tx_count >> 16) & 0xFF;
    TxData[3] = (loco_tx_count >> 24) & 0xFF;
    uint32_t ecr = hfdcan2.Instance->ECR;
    loco_hw_errors += ((ecr >> 16) & 0xFF); // Накапливаем ошибки CEL (сбрасываются при чтении)
    TxData[4] = ecr & 0xFF; // TEC
    TxData[5] = (ecr >> 8) & 0xFF; // REC
    TxData[6] = loco_hw_errors & 0xFF;
    TxData[7] = (loco_hw_errors >> 8) & 0xFF;
    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &TxHeader, TxData) == HAL_OK) loco_tx_count++;

    // 5. Отправка счетчиков программных ошибок по каждому узлу
    TxHeader.Identifier = 0x305;
    TxHeader.DataLength = FDCAN_DLC_BYTES_6;
    TxData[0] = node_errors[0] & 0xFF;
    TxData[1] = (node_errors[0] >> 8) & 0xFF;
    TxData[2] = node_errors[1] & 0xFF;
    TxData[3] = (node_errors[1] >> 8) & 0xFF;
    TxData[4] = node_errors[2] & 0xFF;
    TxData[5] = (node_errors[2] >> 8) & 0xFF;
    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &TxHeader, TxData) == HAL_OK) loco_tx_count++;

    HAL_Delay(1000); // Цикл раз в 1 секунду
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
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET)
  {
    FDCAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
    {
      if (RxHeader.Identifier >= 0x211 && RxHeader.Identifier <= 0x213)
      {
        uint8_t index = RxHeader.Identifier - 0x211;
        uint32_t uid = RxData[0] | (RxData[1] << 8) | (RxData[2] << 16) | (RxData[3] << 24);
        node_ids[index] = uid;
        node_last_seen[index] = HAL_GetTick();
      }
    }
  }
}
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
