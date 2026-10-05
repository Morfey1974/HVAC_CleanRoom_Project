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
#include <string.h>
#include "hvac_can.h"
#include "hvac_cfg.h"
#include "fwupd/fwupd_app.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LOCO_BOARD_REV    1u
#define LOCO_FW_VERSION   0x0101u /* major << 8 | minor */
#define LOCO_APP_START    0x08004000u /* after the 16K bootloader, see linker script */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;
FDCAN_TxHeaderTypeDef TxHeader;
uint8_t TxData[8];

uint32_t node_ids[3] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}; // 0xFFFFFFFF = Lost/Error
uint32_t node_last_seen[3] = {0, 0, 0};
uint32_t loco_tx_count = 0;
uint16_t node_errors[3] = {0, 0, 0};
uint32_t loco_hw_errors = 0;

FWUPD_APP_HEADER(FWUPD_TYPE_LOCOMOTIVE, LOCO_BOARD_REV, LOCO_FW_VERSION);
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

/* Firmware update frames go to the Main PLC on CAN2. */
static void Fwupd_SendToPlc(uint32_t ext_id, const uint8_t data[8])
{
  FDCAN_TxHeaderTypeDef h = {0};

  h.Identifier = ext_id;
  h.IdType = FDCAN_EXTENDED_ID;
  h.TxFrameType = FDCAN_DATA_FRAME;
  h.DataLength = FDCAN_DLC_BYTES_8;
  h.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  h.BitRateSwitch = FDCAN_BRS_OFF;
  h.FDFormat = FDCAN_CLASSIC_CAN;
  h.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  CAN_SendFromMain(&hfdcan2, &h, (uint8_t *)data);
}

/* From the RX interrupt: passes a standard 8-byte frame unchanged to the other bus. */
static void CAN_Forward(FDCAN_HandleTypeDef *to, uint32_t id, const uint8_t *data)
{
  FDCAN_TxHeaderTypeDef h = {0};

  h.Identifier = id;
  h.IdType = FDCAN_STANDARD_ID;
  h.TxFrameType = FDCAN_DATA_FRAME;
  h.DataLength = FDCAN_DLC_BYTES_8;
  h.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  h.BitRateSwitch = FDCAN_BRS_OFF;
  h.FDFormat = FDCAN_CLASSIC_CAN;
  h.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  HAL_FDCAN_AddMessageToTxFifoQ(to, &h, (uint8_t *)data);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  SCB->VTOR = LOCO_APP_START;
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
  HAL_FDCAN_ConfigGlobalFilter(&hfdcan2, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
  HAL_FDCAN_ActivateNotification(&hfdcan2, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);

  HAL_FDCAN_Start(&hfdcan2);
  
  HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig);
  HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
  HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
  HAL_FDCAN_Start(&hfdcan1);

  FwupdApp_Init(Fwupd_SendToPlc, FWUPD_TYPE_LOCOMOTIVE, LOCO_BOARD_REV, LOCO_FW_VERSION);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  extern volatile uint32_t analog_rx_tick;
  uint32_t last_no_link = 0;
  while (1)
  {
    if (hfdcan2.Instance->PSR & FDCAN_PSR_BO) {
        HAL_FDCAN_Stop(&hfdcan2);
        HAL_FDCAN_Start(&hfdcan2);
    }
    if (hfdcan1.Instance->PSR & FDCAN_PSR_BO) {
        HAL_FDCAN_Stop(&hfdcan1);
        HAL_FDCAN_Start(&hfdcan1);
    }

    uint32_t now = HAL_GetTick();
    if (now - analog_rx_tick > HVAC_AI_LINK_TIMEOUT_MS && now - last_no_link >= HVAC_AI_NO_LINK_PERIOD_MS) {
        last_no_link = now;
        HvacAiMeas lost = { .status = HVAC_AI_ST_NO_LINK };
        uint8_t err_data[8];
        hvac_can_ai_encode(&lost, err_data);

        FDCAN_TxHeaderTypeDef errHeader;
        errHeader.Identifier = HVAC_CAN_ID_AI_MEAS;
        errHeader.IdType = FDCAN_STANDARD_ID;
        errHeader.TxFrameType = FDCAN_DATA_FRAME;
        errHeader.DataLength = FDCAN_DLC_BYTES_8;
        errHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        errHeader.BitRateSwitch = FDCAN_BRS_OFF;
        errHeader.FDFormat = FDCAN_CLASSIC_CAN;
        errHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        errHeader.MessageMarker = 0;

        CAN_SendFromMain(&hfdcan2, &errHeader, err_data);
    }

    FwupdApp_Poll();
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
volatile uint32_t analog_rx_tick = 0;

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET)
  {
    FDCAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[64];
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
    {
      if (hfdcan == &hfdcan2 && RxHeader.IdType == FDCAN_EXTENDED_ID)
      {
        FwupdApp_OnRx(RxHeader.Identifier, 1u, RxData,
                      (RxHeader.DataLength == FDCAN_DLC_BYTES_8) ? 8u : 0u);
      }
      else if (hfdcan == &hfdcan2 && RxHeader.Identifier == HVAC_CAN_ID_CFG_SET &&
               RxHeader.DataLength == FDCAN_DLC_BYTES_8)
      {
        CAN_Forward(&hfdcan1, HVAC_CAN_ID_CFG_SET, RxData);
      }
      else if (hfdcan == &hfdcan1 && RxHeader.Identifier == HVAC_CAN_ID_CFG_STATUS &&
               RxHeader.DataLength == FDCAN_DLC_BYTES_8)
      {
        CAN_Forward(&hfdcan2, HVAC_CAN_ID_CFG_STATUS, RxData);
      }
      else if (hfdcan == &hfdcan1 && RxHeader.Identifier == HVAC_CAN_ID_AI_MEAS)
      {
        analog_rx_tick = HAL_GetTick();

        FDCAN_TxHeaderTypeDef fwdHeader;
        fwdHeader.Identifier = HVAC_CAN_ID_AI_MEAS;
        fwdHeader.IdType = FDCAN_STANDARD_ID;
        fwdHeader.TxFrameType = FDCAN_DATA_FRAME;
        fwdHeader.DataLength = RxHeader.DataLength; // Expecting FDCAN_DLC_BYTES_8
        fwdHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        fwdHeader.BitRateSwitch = FDCAN_BRS_OFF;
        fwdHeader.FDFormat = FDCAN_CLASSIC_CAN;
        fwdHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        fwdHeader.MessageMarker = 0;
        
        HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &fwdHeader, RxData);
      }
      else if (RxHeader.Identifier >= 0x211 && RxHeader.Identifier <= 0x213)
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
