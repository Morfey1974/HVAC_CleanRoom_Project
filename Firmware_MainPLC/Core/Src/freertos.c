/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

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
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for netTask */
osThreadId_t netTaskHandle;
const osThreadAttr_t netTask_attributes = {
  .name = "netTask",
  .stack_size = 768 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for commsTask */
osThreadId_t commsTaskHandle;
const osThreadAttr_t commsTask_attributes = {
  .name = "commsTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for doorsTask */
osThreadId_t doorsTaskHandle;
const osThreadAttr_t doorsTask_attributes = {
  .name = "doorsTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for supervisorTask */
osThreadId_t supervisorTaskHandle;
const osThreadAttr_t supervisorTask_attributes = {
  .name = "supervisorTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for httpTask */
osThreadId_t httpTaskHandle;
const osThreadAttr_t httpTask_attributes = {
  .name = "httpTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for canTask */
osThreadId_t canTaskHandle;
const osThreadAttr_t canTask_attributes = {
  .name = "canTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for rs485Task */
osThreadId_t rs485TaskHandle;
const osThreadAttr_t rs485Task_attributes = {
  .name = "rs485Task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for loggerTask */
osThreadId_t loggerTaskHandle;
const osThreadAttr_t loggerTask_attributes = {
  .name = "loggerTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for watchdogTask */
osThreadId_t watchdogTaskHandle;
const osThreadAttr_t watchdogTask_attributes = {
  .name = "watchdogTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for journalTask */
osThreadId_t journalTaskHandle;
const osThreadAttr_t journalTask_attributes = {
  .name = "journalTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartNetTask(void *argument);
void StartCommsTask(void *argument);
void StartDoorsTask(void *argument);
void StartSupervisorTask(void *argument);
void StartHttpTask(void *argument);
void StartCanTask(void *argument);
void StartRs485Task(void *argument);
void StartLoggerTask(void *argument);
void StartWatchdogTask(void *argument);
void StartJournalTask(void *argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of netTask */
  netTaskHandle = osThreadNew(StartNetTask, NULL, &netTask_attributes);

  /* creation of commsTask */
  commsTaskHandle = osThreadNew(StartCommsTask, NULL, &commsTask_attributes);

  /* creation of doorsTask */
  doorsTaskHandle = osThreadNew(StartDoorsTask, NULL, &doorsTask_attributes);

  /* creation of supervisorTask */
  supervisorTaskHandle = osThreadNew(StartSupervisorTask, NULL, &supervisorTask_attributes);

  /* creation of httpTask */
  httpTaskHandle = osThreadNew(StartHttpTask, NULL, &httpTask_attributes);

  /* creation of canTask */
  canTaskHandle = osThreadNew(StartCanTask, NULL, &canTask_attributes);

  /* creation of rs485Task */
  rs485TaskHandle = osThreadNew(StartRs485Task, NULL, &rs485Task_attributes);

  /* creation of loggerTask */
  loggerTaskHandle = osThreadNew(StartLoggerTask, NULL, &loggerTask_attributes);

  /* creation of watchdogTask */
  watchdogTaskHandle = osThreadNew(StartWatchdogTask, NULL, &watchdogTask_attributes);

  /* creation of journalTask */
  journalTaskHandle = osThreadNew(StartJournalTask, NULL, &journalTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartNetTask */
/**
  * @brief  Function implementing the netTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartNetTask */
void StartNetTask(void *argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN StartNetTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartNetTask */
}

/* USER CODE BEGIN Header_StartCommsTask */
/**
* @brief Function implementing the commsTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCommsTask */
void StartCommsTask(void *argument)
{
  /* USER CODE BEGIN StartCommsTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCommsTask */
}

/* USER CODE BEGIN Header_StartDoorsTask */
/**
* @brief Function implementing the doorsTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDoorsTask */
void StartDoorsTask(void *argument)
{
  /* USER CODE BEGIN StartDoorsTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDoorsTask */
}

/* USER CODE BEGIN Header_StartSupervisorTask */
/**
* @brief Function implementing the supervisorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSupervisorTask */
void StartSupervisorTask(void *argument)
{
  /* USER CODE BEGIN StartSupervisorTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartSupervisorTask */
}

/* USER CODE BEGIN Header_StartHttpTask */
/**
* @brief Function implementing the httpTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartHttpTask */
void StartHttpTask(void *argument)
{
  /* USER CODE BEGIN StartHttpTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartHttpTask */
}

/* USER CODE BEGIN Header_StartCanTask */
/**
* @brief Function implementing the canTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCanTask */
void StartCanTask(void *argument)
{
  /* USER CODE BEGIN StartCanTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCanTask */
}

/* USER CODE BEGIN Header_StartRs485Task */
/**
* @brief Function implementing the rs485Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartRs485Task */
void StartRs485Task(void *argument)
{
  /* USER CODE BEGIN StartRs485Task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartRs485Task */
}

/* USER CODE BEGIN Header_StartLoggerTask */
/**
* @brief Function implementing the loggerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartLoggerTask */
void StartLoggerTask(void *argument)
{
  /* USER CODE BEGIN StartLoggerTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartLoggerTask */
}

/* USER CODE BEGIN Header_StartWatchdogTask */
/**
* @brief Function implementing the watchdogTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartWatchdogTask */
void StartWatchdogTask(void *argument)
{
  /* USER CODE BEGIN StartWatchdogTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartWatchdogTask */
}

/* USER CODE BEGIN Header_StartJournalTask */
/**
* @brief Function implementing the journalTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartJournalTask */
void StartJournalTask(void *argument)
{
  /* USER CODE BEGIN StartJournalTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartJournalTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

