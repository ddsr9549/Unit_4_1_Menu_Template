**
 * @file PWM.c
 * @brief General-purpose PWM driver — adapted for STM32F4 (F446RE)
 * Change: HAL include updated to stm32l4xx_hal.h
 * To migrate back to L476RG: change include to stm32l4xx_hal.h
 */

#include "PWM.h"
#include "stm32l4xx_hal.h"

static inline uint32_t clamp_u32(uint32_t x, uint32_t lo, uint32_t hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

void PWM_Init(PWM_cfg_t* cfg)
{
    if (!cfg->setup_done) {
        cfg->pwm_started = 0;
        cfg->last_duty   = 0;
        cfg->setup_done  = 1;
    }
}

uint8_t PWM_IsRunning(PWM_cfg_t* cfg)
{
    return cfg->pwm_started ? 1u : 0u;
}

void PWM_Off(PWM_cfg_t* cfg)
{
    if (cfg->pwm_started) {
        __HAL_TIM_SET_COMPARE(cfg->htim, cfg->channel, 0);
        HAL_TIM_PWM_Stop(cfg->htim, cfg->channel);
        cfg->pwm_started = 0;
    }
    cfg->last_duty = 0;
}

static void apply_duty_at_current_frequency(PWM_cfg_t* cfg, uint8_t duty_percent)
{
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(cfg->htim);
    uint32_t top = arr + 1u;
    duty_percent = (uint8_t)clamp_u32(duty_percent, 0u, 100u);
    uint32_t ccr = (top * (uint32_t)duty_percent) / 100u;
    if (ccr > arr) ccr = arr;
    __HAL_TIM_SET_COMPARE(cfg->htim, cfg->channel, ccr);
}

void PWM_SetFreq(PWM_cfg_t* cfg, uint32_t freq_hz)
{
    if (!cfg->setup_done) PWM_Init(cfg);
    freq_hz = clamp_u32(freq_hz, cfg->min_freq_hz, cfg->max_freq_hz);
    uint32_t arr = (cfg->tick_freq_hz / freq_hz) - 1u;
    arr = clamp_u32(arr, 1u, 65535u);
    __HAL_TIM_SET_AUTORELOAD(cfg->htim, arr);
    __HAL_TIM_SET_COUNTER(cfg->htim, 0);
    HAL_TIM_GenerateEvent(cfg->htim, TIM_EVENTSOURCE_UPDATE);
    if (cfg->pwm_started && cfg->last_duty > 0)
        apply_duty_at_current_frequency(cfg, cfg->last_duty);
}

void PWM_SetDuty(PWM_cfg_t* cfg, uint8_t duty_percent)
{
    if (!cfg->setup_done) PWM_Init(cfg);
    if (duty_percent == 0) { PWM_Off(cfg); return; }
    if (!cfg->pwm_started) {
        HAL_TIM_PWM_Start(cfg->htim, cfg->channel);
        cfg->pwm_started = 1;
    }
    cfg->last_duty = duty_percent;
    apply_duty_at_current_frequency(cfg, duty_percent);
}

void PWM_Set(PWM_cfg_t* cfg, uint32_t freq_hz, uint8_t duty_percent)
{
    if (duty_percent == 0 || freq_hz == 0) { PWM_Off(cfg); return; }
    PWM_SetFreq(cfg, freq_hz);
    PWM_SetDuty(cfg, duty_percent);
}

void PWM_SetTicks(PWM_cfg_t* cfg, uint32_t on_ticks, uint32_t off_ticks)
{
    if (!cfg->setup_done) PWM_Init(cfg);
    if (on_ticks == 0) { PWM_Off(cfg); return; }
    on_ticks  = clamp_u32(on_ticks,  1u, 65535u);
    off_ticks = clamp_u32(off_ticks, 1u, 65535u);
    uint32_t arr = clamp_u32(on_ticks + off_ticks - 1u, 1u, 65535u);
    __HAL_TIM_SET_AUTORELOAD(cfg->htim, arr);
    __HAL_TIM_SET_COUNTER(cfg->htim, 0);
    if (!cfg->pwm_started) {
        HAL_TIM_PWM_Start(cfg->htim, cfg->channel);
        cfg->pwm_started = 1;
    }
    uint32_t ccr = clamp_u32(on_ticks - 1u, 0u, arr);
    __HAL_TIM_SET_COMPARE(cfg->htim, cfg->channel, ccr);
    cfg->last_duty = (uint8_t)clamp_u32((100u * on_ticks) / (on_ticks + off_ticks), 0u, 100u);
    HAL_TIM_GenerateEvent(cfg->htim, TIM_EVENTSOURCE_UPDATE);
}
