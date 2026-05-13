**
 * @file PWM.h
 * @brief General-purpose PWM driver — adapted for STM32F4 (F446RE)
 * Change: HAL include updated to stm32l4xx_hal.h
 * To migrate back to L476RG: change include to stm32l4xx_hal.h
 */

#pragma once
#include <stdint.h>
#include "stm32l4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    TIM_HandleTypeDef* htim;
    uint32_t channel;
    uint32_t tick_freq_hz;
    uint32_t min_freq_hz;
    uint32_t max_freq_hz;
    uint8_t  setup_done;
    uint8_t  pwm_started;
    uint8_t  last_duty;
} PWM_cfg_t;

void    PWM_Init    (PWM_cfg_t* cfg);
void    PWM_SetFreq (PWM_cfg_t* cfg, uint32_t freq_hz);
void    PWM_SetDuty (PWM_cfg_t* cfg, uint8_t duty_percent);
void    PWM_Set     (PWM_cfg_t* cfg, uint32_t freq_hz, uint8_t duty_percent);
void    PWM_SetTicks(PWM_cfg_t* cfg, uint32_t on_ticks, uint32_t off_ticks);
void    PWM_Off     (PWM_cfg_t* cfg);
uint8_t PWM_IsRunning(PWM_cfg_t* cfg);

#ifdef __cplusplus
}
#endif
