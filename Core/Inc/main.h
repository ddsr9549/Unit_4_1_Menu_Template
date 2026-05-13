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
#include "stm32l4xx_hal.h"
 
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
 
/* NUCLEO on-board LED (PA5) and USER button (PC13) */
#define LD2_Pin           GPIO_PIN_5
#define LD2_GPIO_Port     GPIOA
 
#define B1_Pin            GPIO_PIN_13
#define B1_GPIO_Port      GPIOC
 
/* Push buttons on GPIOC
 * BTN2 = PC2  (RESET  button — EXTI2)
 * BTN3 = PC3  (spare  — EXTI3)
 * BTN8 = PC10 (spare  — EXTI15_10)
 * BTN9 = PC11 (spare  — EXTI15_10)
 */
#define BTN2_Pin          GPIO_PIN_2
#define BTN3_Pin          GPIO_PIN_3
#define BTN8_Pin          GPIO_PIN_10
#define BTN9_Pin          GPIO_PIN_11
 
/* Push buttons on GPIOA
 * BTN4 = PA4  (spare  — EXTI4)
 * BTN5 = PA6  (spare  — EXTI9_5)
 * BTN6 = PA7  (spare  — EXTI9_5)
 * BTN7 = PA8  (spare  — EXTI9_5)
 */
#define BTN4_Pin          GPIO_PIN_4
#define BTN5_Pin          GPIO_PIN_6
#define BTN6_Pin          GPIO_PIN_7
#define BTN7_Pin          GPIO_PIN_8
 
/* USER CODE BEGIN Private defines */
 
/* Volatile tick counters incremented by TIM6 / TIM7 ISRs */
extern volatile uint32_t g_tim6_ticks;
extern volatile uint32_t g_tim7_ticks;
 
/* USER CODE END Private defines */
 
#ifdef __cplusplus
}
#endif
 
#endif /* __MAIN_H */
