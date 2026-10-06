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
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "fmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1963.h"
#include "room_ui.h"
#include "hvac_can.h"
#include "hvac_cfg.h"
#include "hvac_id.h"
#include "fwupd/fwupd_app.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Выбор платы дисплея: раскомментируйте нужную пару перед прошивкой! */

/*Первая плата с дисплеем*/
#define NODE_INDEX 0
#define NODE_UID   789456

/*Вторая плата, без дисплея*/
//#define NODE_INDEX 1
//#define NODE_UID   123456

/*Третья плата, без дисплея*/
//#define NODE_INDEX 2
//#define NODE_UID   858585

/*******/

#define DEMO_TEMPERATURE_C   22.4f
#define DEMO_HUMIDITY_PCT    45.0f
#define DEMO_PRESSURE_PA     20.0f

#define DISP_BOARD_REV       1u
#define DISP_FW_VERSION      0x0103u /* major << 8 | minor */
#define DISP_ID_ENABLED      0u /* board rev 1 has no ID input from the HUB; 1 on boards with it */
#define DISP_APP_VTOR        0x08020400u /* vector table, see linker script */
#define DISP_CHAIN_TIMEOUT_MS 3000u
#define DISP_ID_X            640u
#define DISP_CHAIN_X         160u
#define DISP_CHAIN_Y         132u
#define DISP_CHAIN_CHARS     52u
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* USER CODE BEGIN PV */
static RoomUi_Values g_room_values;

extern uint32_t _estack;
void Reset_Handler(void);

/* The H7 vector table is longer than FWUPD_HEADER_OFFSET: the bootloader takes SP and reset handler
 * from here, the full table follows at +0x400 (DISP_APP_VTOR). */
__attribute__((section(".boot_vec"), used)) const void *const g_boot_vec[2] = { &_estack, (void *)Reset_Handler };

FWUPD_APP_HEADER(FWUPD_TYPE_DISPLAY_TFT43, DISP_BOARD_REV, DISP_FW_VERSION);

/* Last chain summary from the Main PLC (HVAC_CAN_ID_CHAIN), written by the FDCAN interrupt. */
static volatile uint8_t s_chain[8];
static volatile uint32_t s_chain_rx_tick;
static volatile uint8_t s_chain_received;
extern FDCAN_HandleTypeDef hfdcan1;
FDCAN_RxHeaderTypeDef RxHeader;
uint8_t RxData[8];

uint32_t remote_nodes[3] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
uint16_t remote_errors[3] = {0, 0, 0};
uint32_t loco_tx_count = 0;
uint32_t loco_ecr = 0;

/* Written by the FDCAN interrupt, drawn by the main loop (FMC drawing is not reentrant). */
static volatile HvacAiMeas s_ai_meas;
static volatile uint32_t s_ai_rx_tick;
static volatile uint8_t s_ai_received;
static volatile uint8_t s_doors_mask;
static volatile uint8_t s_doors_status;
static volatile uint32_t s_doors_rx_tick;
static volatile uint8_t s_doors_received;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Queues one 8-byte frame; called from the main loop and from the FDCAN interrupt. */
static void Disp_Send(uint32_t id, uint8_t is_ext, const uint8_t d[8])
{
  FDCAN_TxHeaderTypeDef h = {0};
  uint32_t primask = __get_PRIMASK();

  h.Identifier = id;
  h.IdType = is_ext ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
  h.TxFrameType = FDCAN_DATA_FRAME;
  h.DataLength = FDCAN_DLC_BYTES_8;
  h.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  h.BitRateSwitch = FDCAN_BRS_OFF;
  h.FDFormat = FDCAN_CLASSIC_CAN;
  h.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  __disable_irq();
  HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &h, (uint8_t *)d);
  __set_PRIMASK(primask);
}

static void Disp_SendFwupd(uint32_t ext_id, const uint8_t d[8])
{
  Disp_Send(ext_id, 1u, d);
}

static void Disp_SendId(uint32_t std_id, const uint8_t d[8])
{
  Disp_Send(std_id, 0u, d);
}

/* Optocoupler input with pull-up: line raised -> pin low. */
static uint8_t Disp_IdInputActive(void)
{
  return (HAL_GPIO_ReadPin(Plata_ID_GPIO_Port, Plata_ID_Pin) == GPIO_PIN_RESET) ? 1u : 0u;
}

static void Disp_IdSetOutput(uint8_t out)
{
  (void)out; /* the display has no ID output */
}

static const HvacIdNodeCfg s_id_cfg = {
  .cat = HVAC_CAT_TFT43,
  .board_rev = DISP_BOARD_REV,
  .input_active = Disp_IdInputActive,
  .set_output = Disp_IdSetOutput,
  .send = Disp_SendId,
};

static const char *Disp_TypeName(uint8_t cat)
{
  switch (cat)
  {
    case HVAC_CAT_LOCOMOTIVE:   return "LOC";
    case HVAC_CAT_AI:           return "AI";
    case HVAC_CAT_AO:           return "AO";
    case HVAC_CAT_DI:           return "DI";
    case HVAC_CAT_DO:           return "DO";
    case HVAC_CAT_RELAY:        return "RL";
    case HVAC_CAT_HUB_DISPLAYS: return "HUBD";
    case HVAC_CAT_TFT43:        return "TFT";
    default:                    return "?";
  }
}

/* Own place ID in the top right corner: system name "TFT-RR.MM". */
static void Disp_DrawId(void)
{
  char s[16];
  uint8_t line, rail, place;

  if (HvacId_Get(&line, &rail, &place)) snprintf(s, sizeof(s), "TFT-%02u.%02u", rail, place);
  else                                  snprintf(s, sizeof(s), "TFT-?    ");
  SSD1963_DrawString(DISP_ID_X, 10, s, 0xFFE0, 0x0000, 2);
}

/* First problem of the module check from the Main PLC, under the title. */
static void Disp_DrawChain(void)
{
  static char last[128];
  char msg[96] = "";
  char s[128];
  uint8_t c[8];
  uint8_t fresh;
  uint16_t bg = SSD1963_RGB565(204, 204, 204);
  uint16_t fg = SSD1963_COLOR_BLACK;
  uint32_t n;

  __disable_irq();
  for (uint32_t i = 0u; i < 8u; i++) c[i] = s_chain[i];
  fresh = (s_chain_received && (HAL_GetTick() - s_chain_rx_tick) < DISP_CHAIN_TIMEOUT_MS) ? 1u : 0u;
  __enable_irq();

  if (fresh && c[0] == HVAC_CHAIN_ST_BUSY)
  {
    snprintf(msg, sizeof(msg), "Модули: проверка...");
  }
  else if (fresh && c[0] == HVAC_CHAIN_ST_ERROR)
  {
    char place[32];

    if (c[4] == HVAC_ID_NONE) snprintf(place, sizeof(place), "вне цепочки");
    else                      snprintf(place, sizeof(place), "%u.%02u.%02u", c[3], c[4], c[5]);
    switch (c[2])
    {
      case HVAC_CHAIN_P_MISSING:
        snprintf(msg, sizeof(msg), "Нет модуля %s %s", Disp_TypeName(c[6]), place);
        break;
      case HVAC_CHAIN_P_WRONG_TYPE:
        snprintf(msg, sizeof(msg), "%s: нужен %s, стоит %s", place, Disp_TypeName(c[6]), Disp_TypeName(c[7]));
        break;
      case HVAC_CHAIN_P_EXTRA:
        snprintf(msg, sizeof(msg), "Лишний модуль %s %s", Disp_TypeName(c[7]), place);
        break;
      default:
        snprintf(msg, sizeof(msg), "Ошибка настройки %s %s", Disp_TypeName(c[6]), place);
        break;
    }
    if (c[1] > 1u)
    {
      size_t len = strlen(msg);
      snprintf(&msg[len], sizeof(msg) - len, " (+%u)", (unsigned)(c[1] - 1u));
    }
    bg = SSD1963_COLOR_RED;
    fg = SSD1963_COLOR_WHITE;
  }

  /* Pad with spaces (counted in characters, Cyrillic letters are 2 bytes) to erase the old text. */
  n = 0u;
  for (const char *p = msg; *p; p++) if (((uint8_t)*p & 0xC0u) != 0x80u) n++;
  snprintf(s, sizeof(s), "%s", msg);
  for (size_t len = strlen(s); n < DISP_CHAIN_CHARS && len + 1u < sizeof(s); n++, len++) { s[len] = ' '; s[len + 1u] = '\0'; }
  if (strcmp(s, last) == 0) return;
  strcpy(last, s);
  SSD1963_DrawString(DISP_CHAIN_X, DISP_CHAIN_Y, s, fg, bg, 1U);
}

#if NODE_INDEX == 0
/* Applies the latest AI frame (or its absence) to the values shown on screen. */
static void Display_ApplyAiMeas(RoomUi_Values *v)
{
  HvacAiMeas m;
  uint32_t rx_tick;
  uint8_t received;

  __disable_irq();
  m.humidity_pct = s_ai_meas.humidity_pct;
  m.temperature_c = s_ai_meas.temperature_c;
  m.status = s_ai_meas.status;
  rx_tick = s_ai_rx_tick;
  received = s_ai_received;
  __enable_irq();

  uint8_t fault = (uint8_t)(v->fault_mask & ROOM_UI_FAULT_P);
  if (!received || (HAL_GetTick() - rx_tick) > HVAC_DISPLAY_TIMEOUT_MS ||
      (m.status & (HVAC_AI_ST_NO_LINK | HVAC_AI_ST_ADC_FAULT)))
  {
    fault |= ROOM_UI_FAULT_T | ROOM_UI_FAULT_H;
  }
  else
  {
    if (m.status & HVAC_AI_ST_HUM_FAULT) fault |= ROOM_UI_FAULT_H;
    else v->humidity_pct = m.humidity_pct;
    if (m.status & HVAC_AI_ST_TEMP_FAULT) fault |= ROOM_UI_FAULT_T;
    else v->temperature_c = m.temperature_c;
    v->pressure_pa = 0.0f; /* no pressure sensor yet */
  }
  v->fault_mask = fault;
}

/* Applies the latest doors frame (or its absence) to the doors block. */
static void Display_ApplyDoors(void)
{
  uint8_t mask;
  uint8_t status;
  uint32_t rx_tick;
  uint8_t received;
  uint8_t link;

  __disable_irq();
  mask = s_doors_mask;
  status = s_doors_status;
  rx_tick = s_doors_rx_tick;
  received = s_doors_received;
  __enable_irq();

  link = (received && (HAL_GetTick() - rx_tick) <= HVAC_DOORS_TIMEOUT_MS &&
          (status & HVAC_DOORS_ST_NO_LINK) == 0U) ? 1U : 0U;
  RoomUi_UpdateDoors((uint8_t)(mask & HVAC_DOORS_MASK), link);
}
#endif
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  SCB->VTOR = DISP_APP_VTOR;
  __DSB();
  SCB_EnableICache();
  SCB_EnableDCache();
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* FMC Bank1 @0x60000000: open MPU (Region0 otherwise blocks it) */
  {
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    HAL_MPU_Disable();

    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x0;
    MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
    MPU_InitStruct.SubRegionDisable = 0x8F;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    MPU_InitStruct.Number = MPU_REGION_NUMBER1;
    MPU_InitStruct.BaseAddress = 0x60000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_64MB;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
  }
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART3_UART_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_FMC_Init();
  MX_FDCAN1_Init();
  /* USER CODE BEGIN 2 */
  SSD1963_Init();

  g_room_values.temperature_c = DEMO_TEMPERATURE_C;
  g_room_values.humidity_pct   = DEMO_HUMIDITY_PCT;
  g_room_values.pressure_pa    = DEMO_PRESSURE_PA;
#if NODE_INDEX == 0
  g_room_values.fault_mask     = ROOM_UI_FAULT_T | ROOM_UI_FAULT_H;
#endif
  RoomUi_Init("Clean Room");
  RoomUi_Update(&g_room_values);
#if NODE_INDEX == 0
  RoomUi_UpdateDoors(0U, 0U);
#endif

  /* Configure FDCAN Filter to accept everything */
  FDCAN_FilterTypeDef sFilterConfig;
  sFilterConfig.IdType = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex = 0;
  sFilterConfig.FilterType = FDCAN_FILTER_MASK;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1 = 0x000;
  sFilterConfig.FilterID2 = 0x000;
  HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig);
  /* Extended IDs = firmware update protocol. */
  HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0,
                               FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
  HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);

  /* Start CAN for both roles */
  HAL_FDCAN_Start(&hfdcan1);

  FwupdApp_Init(Disp_SendFwupd, FWUPD_TYPE_DISPLAY_TFT43, DISP_BOARD_REV, DISP_FW_VERSION);
  {
    const uint32_t uid[3] = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };
    if (DISP_ID_ENABLED) HvacId_Init(&s_id_cfg, fwupd_node_tag(uid));
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    char buf[128];
    uint32_t can_errors = hfdcan1.Instance->ECR;
    uint32_t can_psr = hfdcan1.Instance->PSR;
    uint32_t can_txfqs = hfdcan1.Instance->TXFQS;
    sprintf(buf, "CAN STAT: PSR=0x%08lX, ECR=0x%08lX, TXFQS=0x%08lX\r\n", can_psr, can_errors, can_txfqs);
    extern UART_HandleTypeDef huart3;
    HAL_UART_Transmit(&huart3, (uint8_t*)buf, strlen(buf), 100);

    // Auto-recovery from Bus-Off
    if (can_psr & FDCAN_PSR_BO) {
        HAL_FDCAN_Stop(&hfdcan1);
        HAL_FDCAN_Start(&hfdcan1);
    }
#if NODE_INDEX == 0
    Display_ApplyAiMeas(&g_room_values);
    Display_ApplyDoors();
#endif
    RoomUi_Update(&g_room_values);
    if (DISP_ID_ENABLED) Disp_DrawId();
    Disp_DrawChain();

    for (uint32_t t0 = HAL_GetTick(); (HAL_GetTick() - t0) < 500u; )
    {
      FwupdApp_Poll();
      __disable_irq();
      HvacId_Poll(); /* also called from the FDCAN interrupt */
      __enable_irq();
      HAL_Delay(10);
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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = 64;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 31;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 2048;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET)
  {
    while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0u &&
           HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
    {
      uint8_t len8 = (RxHeader.DataLength == FDCAN_DLC_BYTES_8) ? 1u : 0u;

      if (RxHeader.IdType == FDCAN_EXTENDED_ID)
      {
        FwupdApp_OnRx(RxHeader.Identifier, 1u, RxData, len8 ? 8u : 0u);
      }
      else if (RxHeader.Identifier == HVAC_CAN_ID_ID_CMD && len8)
      {
        /* Answer at once: the main loop is slow while it draws, the PLC waits HVAC_ID_ANSWER_MS. */
        HvacId_OnCmd(RxData);
        HvacId_Poll();
      }
      else if (RxHeader.Identifier == HVAC_CAN_ID_CHAIN && len8)
      {
        for (uint32_t i = 0u; i < 8u; i++) s_chain[i] = RxData[i];
        s_chain_rx_tick = HAL_GetTick();
        s_chain_received = 1u;
      }
      else if (RxHeader.Identifier == (0x201 + NODE_INDEX))
      {
        // Локомотив запрашивает наш статус. Отвечаем.
        FDCAN_TxHeaderTypeDef TxHeader;
        TxHeader.Identifier = 0x211 + NODE_INDEX;
        TxHeader.IdType = FDCAN_STANDARD_ID;
        TxHeader.TxFrameType = FDCAN_DATA_FRAME;
        TxHeader.DataLength = FDCAN_DLC_BYTES_4;
        TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
        TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
        TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        TxHeader.MessageMarker = 0;

        uint8_t tx_data[4];
        tx_data[0] = NODE_UID & 0xFF;
        tx_data[1] = (NODE_UID >> 8) & 0xFF;
        tx_data[2] = (NODE_UID >> 16) & 0xFF;
        tx_data[3] = (NODE_UID >> 24) & 0xFF;
        HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &TxHeader, tx_data);
      }
#if NODE_INDEX == 0
      else if (RxHeader.Identifier == HVAC_CAN_ID_AI_MEAS)
      {
        HvacAiMeas m;
        hvac_can_ai_decode(RxData, &m);
        s_ai_meas.humidity_pct = m.humidity_pct;
        s_ai_meas.temperature_c = m.temperature_c;
        s_ai_meas.status = m.status;
        s_ai_meas.counter = m.counter;
        s_ai_rx_tick = HAL_GetTick();
        s_ai_received = 1U;
      }
      else if (RxHeader.Identifier == HVAC_CAN_ID_DOORS && RxHeader.DataLength == FDCAN_DLC_BYTES_4)
      {
        s_doors_mask = RxData[0];
        s_doors_status = RxData[1];
        s_doors_rx_tick = HAL_GetTick();
        s_doors_received = 1U;
      }
      else if (RxHeader.Identifier == 0x302 || RxHeader.Identifier == 0x303)
      {
        uint8_t idx = RxHeader.Identifier - 0x301;
        uint32_t uid = RxData[0] | (RxData[1] << 8) | (RxData[2] << 16) | (RxData[3] << 24);
        remote_nodes[idx] = uid;
      }
      else if (RxHeader.Identifier == 0x304)
      {
        loco_tx_count = RxData[0] | (RxData[1] << 8) | (RxData[2] << 16) | (RxData[3] << 24);
        loco_ecr      = RxData[4] | (RxData[5] << 8) | (RxData[6] << 16) | (RxData[7] << 24);
      }
      else if (RxHeader.Identifier == 0x305)
      {
        remote_errors[0] = RxData[0] | (RxData[1] << 8);
        remote_errors[1] = RxData[2] | (RxData[3] << 8);
        remote_errors[2] = RxData[4] | (RxData[5] << 8);
      }
#endif
    }
  }
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x8F;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x60000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_64B;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

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
