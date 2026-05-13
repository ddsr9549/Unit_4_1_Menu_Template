**
 * @file SevenSeg.c
 * @brief Multiplexed dual 7-segment display driver
 */

#include "SevenSeg.h"
#include "tim.h"
#include "stm32l4xx_hal.h"

static const uint8_t seg_digits[10] = {
    0b00111111, 0b00000110, 0b01011011, 0b01001111,
    0b01100110, 0b01101101, 0b01111101, 0b00000111,
    0b01111111, 0b01101111
};

static uint8_t g_tens   = 0;
static uint8_t g_units  = 0;
static uint8_t g_active = 0;
static uint8_t g_blank  = 1;

static void write_segments(uint8_t pattern)
{
    HAL_GPIO_WritePin(SEG_A_PORT, SEG_A_PIN, (pattern & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_B_PORT, SEG_B_PIN, (pattern & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_C_PORT, SEG_C_PIN, (pattern & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_D_PORT, SEG_D_PIN, (pattern & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_E_PORT, SEG_E_PIN, (pattern & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_F_PORT, SEG_F_PIN, (pattern & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_G_PORT, SEG_G_PIN, (pattern & 0x40) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void all_digits_off(void)
{
    HAL_GPIO_WritePin(DIG_1_PORT, DIG_1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DIG_2_PORT, DIG_2_PIN, GPIO_PIN_RESET);
}

void SevenSeg_Init(void)
{
    g_tens = 0; g_units = 0; g_blank = 1; g_active = 0;
    write_segments(0x00);
    all_digits_off();
    HAL_TIM_Base_Start_IT(&htim6);
}

void SevenSeg_SetNumber(uint8_t number)
{
    if (number > 99) number = 99;
    g_tens  = number / 10;
    g_units = number % 10;
    g_blank = 0;
}

void SevenSeg_Clear(void)
{
    g_blank = 1;
    write_segments(0x00);
    all_digits_off();
}

void SevenSeg_TimerCallback(void)
{
    if (g_blank) { write_segments(0x00); all_digits_off(); return; }
    all_digits_off();
    if (g_active == 0)
    {
        write_segments(seg_digits[g_tens]);
        HAL_GPIO_WritePin(DIG_1_PORT, DIG_1_PIN, GPIO_PIN_SET);
        g_active = 1;
    }
    else
    {
        write_segments(seg_digits[g_units]);
        HAL_GPIO_WritePin(DIG_2_PORT, DIG_2_PIN, GPIO_PIN_SET);
        g_active = 0;
    }
}
