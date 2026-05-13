/**
 * @file SevenSeg.h
 * @brief Multiplexed dual 7-segment display driver
 *
 * Displays a 2-digit number (00-99) on two multiplexed
 * common-cathode 7-segment displays using TIM6 interrupt.
 *
 * Usage:
 *   SevenSeg_Init();
 *   SevenSeg_SetNumber(42);   // shows "42"
 *   SevenSeg_SetNumber(7);    // shows "07"
 *   SevenSeg_Clear();         // blanks display
 */

#ifndef SEVENSEG_H
#define SEVENSEG_H

#include <stdint.h>
#include "hw_config.h"

/* ============================================================
 *  PUBLIC API
 * ============================================================ */

/**
 * @brief Initialize 7-segment GPIO pins
 *        Call once in main before using display
 */
void SevenSeg_Init(void);

/**
 * @brief Display a 2-digit number (0-99)
 * @param number Value to display (clamped to 0-99)
 */
void SevenSeg_SetNumber(uint8_t number);

/**
 * @brief Turn off all segments (blank display)
 */
void SevenSeg_Clear(void);

/**
 * @brief Multiplexing ISR handler
 *        Call this from TIM6 interrupt (HAL_TIM_PeriodElapsedCallback)
 *        Alternates between tens and units digit at 2kHz
 */
void SevenSeg_TimerCallback(void);

#endif /* SEVENSEG_H */
