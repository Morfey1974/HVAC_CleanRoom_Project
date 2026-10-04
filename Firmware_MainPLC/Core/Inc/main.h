/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define QSPI_IO2_Pin GPIO_PIN_2
#define QSPI_IO2_GPIO_Port GPIOE
#define MCU_MODUL_ID_OUT_AI_Pin GPIO_PIN_3
#define MCU_MODUL_ID_OUT_AI_GPIO_Port GPIOE
#define MCU_MODUL_ID_OUT_DISPLAY_Pin GPIO_PIN_4
#define MCU_MODUL_ID_OUT_DISPLAY_GPIO_Port GPIOE
#define I2C2_SDA_EEPROM_Pin GPIO_PIN_0
#define I2C2_SDA_EEPROM_GPIO_Port GPIOF
#define I2C2_SCL_EEPROM_Pin GPIO_PIN_1
#define I2C2_SCL_EEPROM_GPIO_Port GPIOF
#define QSPI_IO3_Pin GPIO_PIN_6
#define QSPI_IO3_GPIO_Port GPIOF
#define QSPI_IO0_Pin GPIO_PIN_8
#define QSPI_IO0_GPIO_Port GPIOF
#define QSPI_IO1_Pin GPIO_PIN_9
#define QSPI_IO1_GPIO_Port GPIOF
#define QSPI_CLK_Pin GPIO_PIN_10
#define QSPI_CLK_GPIO_Port GPIOF
#define ETH_NRST_Pin GPIO_PIN_0
#define ETH_NRST_GPIO_Port GPIOC
#define RMII_MDC_Pin GPIO_PIN_1
#define RMII_MDC_GPIO_Port GPIOC
#define RMII_REF_CLK_Pin GPIO_PIN_1
#define RMII_REF_CLK_GPIO_Port GPIOA
#define RMII_MDIO_Pin GPIO_PIN_2
#define RMII_MDIO_GPIO_Port GPIOA
#define RMII_CRS_DV_Pin GPIO_PIN_7
#define RMII_CRS_DV_GPIO_Port GPIOA
#define RMII_RXD0_Pin GPIO_PIN_4
#define RMII_RXD0_GPIO_Port GPIOC
#define RMII_RXD1_Pin GPIO_PIN_5
#define RMII_RXD1_GPIO_Port GPIOC
#define QSPI_NCS_Pin GPIO_PIN_11
#define QSPI_NCS_GPIO_Port GPIOE
#define RMII_TXD1_Pin GPIO_PIN_13
#define RMII_TXD1_GPIO_Port GPIOB
#define USART3_TX_Pin GPIO_PIN_8
#define USART3_TX_GPIO_Port GPIOD
#define USART3_RX_Pin GPIO_PIN_9
#define USART3_RX_GPIO_Port GPIOD
#define LED_STATUS_Pin GPIO_PIN_2
#define LED_STATUS_GPIO_Port GPIOG
#define SWDIO_Pin GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SWCLK_Pin GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA
#define RMII_TX_EN_Pin GPIO_PIN_11
#define RMII_TX_EN_GPIO_Port GPIOG
#define RMII_TXD0_Pin GPIO_PIN_13
#define RMII_TXD0_GPIO_Port GPIOG
#define MODUL_ID_OUT_BUS_Pin GPIO_PIN_1
#define MODUL_ID_OUT_BUS_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
