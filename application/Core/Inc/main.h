/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "stm32f1xx_hal.h"

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
void app_main(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define OUT_RELAY_Pin GPIO_PIN_0
#define OUT_RELAY_GPIO_Port GPIOA
#define TIM2_CH2_BUZZ_Pin GPIO_PIN_1
#define TIM2_CH2_BUZZ_GPIO_Port GPIOA
#define USART2_TX_ETC_Pin GPIO_PIN_2
#define USART2_TX_ETC_GPIO_Port GPIOA
#define USART2_RX_ETC_Pin GPIO_PIN_3
#define USART2_RX_ETC_GPIO_Port GPIOA
#define EXTI_SAFE_KET_Pin GPIO_PIN_4
#define EXTI_SAFE_KET_GPIO_Port GPIOA
#define EXTI_SAFE_KET_EXTI_IRQn EXTI4_IRQn
#define ADC2_IN5_RTemp_Pin GPIO_PIN_5
#define ADC2_IN5_RTemp_GPIO_Port GPIOA
#define ADC2_IN6_LTemp_Pin GPIO_PIN_6
#define ADC2_IN6_LTemp_GPIO_Port GPIOA
#define ADC1_IN7_CP_FB_Pin GPIO_PIN_7
#define ADC1_IN7_CP_FB_GPIO_Port GPIOA
#define OUT_CC_Pin GPIO_PIN_0
#define OUT_CC_GPIO_Port GPIOB
#define OUT_WT_CLK_Pin GPIO_PIN_2
#define OUT_WT_CLK_GPIO_Port GPIOB
#define EXTI_YPPE_AB_Pin GPIO_PIN_12
#define EXTI_YPPE_AB_GPIO_Port GPIOB
#define EXTI_YPPE_AB_EXTI_IRQn EXTI15_10_IRQn
#define L_REALY_S_Pin GPIO_PIN_13
#define L_REALY_S_GPIO_Port GPIOB
#define N_REALY_S_Pin GPIO_PIN_14
#define N_REALY_S_GPIO_Port GPIOB
#define OUT_4G_RESET_Pin GPIO_PIN_8
#define OUT_4G_RESET_GPIO_Port GPIOA
#define USART1_TX_4G_Pin GPIO_PIN_9
#define USART1_TX_4G_GPIO_Port GPIOA
#define USART1_RX_4G_Pin GPIO_PIN_10
#define USART1_RX_4G_GPIO_Port GPIOA
#define OUT_4G_POWER_EN_Pin GPIO_PIN_11
#define OUT_4G_POWER_EN_GPIO_Port GPIOA
#define RC522_CS_Pin GPIO_PIN_15
#define RC522_CS_GPIO_Port GPIOA
#define OUT_WS_LED_Pin GPIO_PIN_6
#define OUT_WS_LED_GPIO_Port GPIOB
#define TIM4_CH3_PWM_1K_Pin GPIO_PIN_8
#define TIM4_CH3_PWM_1K_GPIO_Port GPIOB
#define OUT_WT_DATA_Pin GPIO_PIN_9
#define OUT_WT_DATA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define BIT0 ((uint32_t) 1)
#define BIT(x) (BIT0 << x)

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
