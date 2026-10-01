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
extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;
volatile uint32_t last_can2_rx_tick = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Queues a frame from thread context; the RX interrupt also writes to the TX FIFO. */
static void CAN_SendFromMain(FDCAN_HandleTypeDef *hfdcan, FDCAN_TxHeaderTypeDef *hdr, uint8_t *data)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, hdr, data);
  __set_PRIMASK(primask);
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
  MX_FDCAN1_Init();
  MX_FDCAN2_Init();
  /* USER CODE BEGIN 2 */
  FDCAN_FilterTypeDef sFilterConfig;
  sFilterConfig.IdType = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex = 0;
  sFilterConfig.FilterType = FDCAN_FILTER_MASK;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1 = 0x000;
  sFilterConfig.FilterID2 = 0x000;

  HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig);
  HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
  HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
  HAL_FDCAN_Start(&hfdcan1);

  HAL_FDCAN_ConfigFilter(&hfdcan2, &sFilterConfig);
  HAL_FDCAN_ConfigGlobalFilter(&hfdcan2, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
  HAL_FDCAN_ActivateNotification(&hfdcan2, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
  HAL_FDCAN_Start(&hfdcan2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (hfdcan1.Instance->PSR & FDCAN_PSR_BO) {
        HAL_FDCAN_Stop(&hfdcan1);
        HAL_FDCAN_Start(&hfdcan1);
    }
    if (hfdcan2.Instance->PSR & FDCAN_PSR_BO) {
        HAL_FDCAN_Stop(&hfdcan2);
        HAL_FDCAN_Start(&hfdcan2);
    }

    static uint32_t last_id_poll = 0;
    uint32_t current_tick = HAL_GetTick();

    // Опрос слотов и назначение ID раз в секунду
    if (current_tick - last_id_poll >= 1000) {
        last_id_poll = current_tick;
        
        uint16_t id_pins[9] = {
            MCU_DISPLAY_ID_1_Pin, MCU_DISPLAY_ID_2_Pin, MCU_DISPLAY_ID_3_Pin,
            MCU_DISPLAY_ID_4_Pin, MCU_DISPLAY_ID_5_Pin, MCU_DISPLAY_ID_6_Pin,
            MCU_DISPLAY_ID_7_Pin, MCU_DISPLAY_ID_8_Pin, MCU_DISPLAY_ID_9_Pin
        };
        GPIO_TypeDef* id_ports[9] = {
            MCU_DISPLAY_ID_1_GPIO_Port, MCU_DISPLAY_ID_2_GPIO_Port, MCU_DISPLAY_ID_3_GPIO_Port,
            MCU_DISPLAY_ID_4_GPIO_Port, MCU_DISPLAY_ID_5_GPIO_Port, MCU_DISPLAY_ID_6_GPIO_Port,
            MCU_DISPLAY_ID_7_GPIO_Port, MCU_DISPLAY_ID_8_GPIO_Port, MCU_DISPLAY_ID_9_GPIO_Port
        };

        for (int i = 0; i < 9; i++) {
            HAL_GPIO_WritePin(id_ports[i], id_pins[i], GPIO_PIN_SET);
            HAL_Delay(5); // Ждем пока сигнал дойдет до дисплея
            
            FDCAN_TxHeaderTypeDef idHeader;
            idHeader.Identifier = 0x400;
            idHeader.IdType = FDCAN_STANDARD_ID;
            idHeader.TxFrameType = FDCAN_DATA_FRAME;
            idHeader.DataLength = FDCAN_DLC_BYTES_4;
            idHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
            idHeader.BitRateSwitch = FDCAN_BRS_OFF;
            idHeader.FDFormat = FDCAN_CLASSIC_CAN;
            idHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
            idHeader.MessageMarker = 0;
            
            uint32_t new_id = (i + 1) * 11111;
            uint8_t payload[4];
            payload[0] = (uint8_t)(new_id & 0xFF);
            payload[1] = (uint8_t)((new_id >> 8) & 0xFF);
            payload[2] = (uint8_t)((new_id >> 16) & 0xFF);
            payload[3] = (uint8_t)((new_id >> 24) & 0xFF);
            
            CAN_SendFromMain(&hfdcan1, &idHeader, payload);
            HAL_Delay(5);
            
            HAL_GPIO_WritePin(id_ports[i], id_pins[i], GPIO_PIN_RESET);
            HAL_Delay(5);
        }
    }
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
    uint8_t RxData[64];
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
    {
      if (hfdcan == &hfdcan2)
      {
        last_can2_rx_tick = HAL_GetTick();
        FDCAN_TxHeaderTypeDef fwdHeader;
        fwdHeader.Identifier = RxHeader.Identifier;
        fwdHeader.IdType = RxHeader.IdType;
        fwdHeader.TxFrameType = RxHeader.RxFrameType;
        fwdHeader.DataLength = RxHeader.DataLength;
        fwdHeader.ErrorStateIndicator = RxHeader.ErrorStateIndicator;
        fwdHeader.BitRateSwitch = RxHeader.BitRateSwitch;
        fwdHeader.FDFormat = RxHeader.FDFormat;
        fwdHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        fwdHeader.MessageMarker = 0;
        
        HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &fwdHeader, RxData);
      }
      else if (hfdcan == &hfdcan1)
      {
        FDCAN_TxHeaderTypeDef fwdHeader;
        fwdHeader.Identifier = RxHeader.Identifier;
        fwdHeader.IdType = RxHeader.IdType;
        fwdHeader.TxFrameType = RxHeader.RxFrameType;
        fwdHeader.DataLength = RxHeader.DataLength;
        fwdHeader.ErrorStateIndicator = RxHeader.ErrorStateIndicator;
        fwdHeader.BitRateSwitch = RxHeader.BitRateSwitch;
        fwdHeader.FDFormat = RxHeader.FDFormat;
        fwdHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        fwdHeader.MessageMarker = 0;
        
        HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &fwdHeader, RxData);
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
