#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l4xx_hal.h"

extern volatile uint32_t g_tim6_ticks;
extern volatile uint32_t g_tim7_ticks;

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */