/***
 * @file main.c
 * @brief MultiGame Console â€" Main Entry Point
 *
 * Hardware: STM32 NUCLEO-L476RG
 * Display:  PC via UART â€" Python tkinter window (LCD damaged)
 *
 * Timer usage:
 *   TIM3 CH3 â†’ Buzzer PWM  (PB0)
 *   TIM4 CH1 â†’ LED PWM     (PB6)
 *   TIM6     â†’ 7-seg multiplex (500us ISR)
 *   TIM7     â†’ 1 second countdown ISR
 */

#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "hw_config.h"
#include "Buzzer.h"
#include "PWM.h"
#include "LCD.h"
#include "Joystick.h"
#include "SevenSeg.h"
#include "Menu.h"
#include "InputHandler.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void SystemClock_Config(void);

/* â"€â"€ Game forward declarations â"€â"€ */
MenuState Game1_Run(void);
MenuState Game2_Run(void);
MenuState Game3_Run(void);

/* â"€â"€ Global hardware structs â"€â"€ */
ST7789V2_cfg_t lcd_cfg;
Buzzer_cfg_t   buzzer_cfg;
PWM_cfg_t      pwm_cfg;
Joystick_cfg_t joystick_cfg;
Joystick_t     joystick_data;
MenuSystem     menu;

/* â"€â"€ Global tick counters â"€â"€ */
volatile uint32_t g_tim6_ticks = 0;
volatile uint32_t g_tim7_ticks = 0;

/* â"€â"€ UART printf redirect â"€â"€ */
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

/* â"€â"€ Timer ISR â"€â"€ */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        g_tim6_ticks++;
        SevenSeg_TimerCallback();
    }
    else if (htim->Instance == TIM7)
    {
        g_tim7_ticks++;
    }
}

/* â"€â"€ Main â"€â"€ */
int main(void)
{
  /* BLINK TEST — PA5 = LD2 on NUCLEO */
  RCC->AHB2ENR |= (1<<0);
  GPIOA->MODER = (GPIOA->MODER & ~(3<<10)) | (1<<10);
  for(int i=0;i<5;i++){GPIOA->ODR^=(1<<5);for(volatile int d=0;d<500000;d++);}

    HAL_Init();
    SystemClock_Config();

    /* Peripheral init â€" NO SPI (LCD not used) */
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_ADC1_Init();
    // MX_ADC2_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    MX_TIM6_Init();
    MX_TIM7_Init();
    /* NO MX_SPI2_Init -- LCD driver handles SPI internally */

    /* ── LCD ── */
    memset(&lcd_cfg, 0, sizeof(lcd_cfg));
    lcd_cfg.spi        = LCD_SPI;
    lcd_cfg.RST.port   = LCD_RST_PORT;  lcd_cfg.RST.pin  = LCD_RST_PIN;
    lcd_cfg.BL.port    = LCD_BL_PORT;   lcd_cfg.BL.pin   = LCD_BL_PIN;
    lcd_cfg.DC.port    = LCD_DC_PORT;   lcd_cfg.DC.pin   = LCD_DC_PIN;
    lcd_cfg.CS.port    = LCD_CS_PORT;   lcd_cfg.CS.pin   = LCD_CS_PIN;
    lcd_cfg.MOSI.port  = LCD_MOSI_PORT; lcd_cfg.MOSI.pin = LCD_MOSI_PIN;
    lcd_cfg.SCLK.port  = LCD_SCLK_PORT; lcd_cfg.SCLK.pin = LCD_SCLK_PIN;
    LCD_init(&lcd_cfg);

    /* â"€â"€ Buzzer â"€â"€ */
    memset(&buzzer_cfg, 0, sizeof(buzzer_cfg));
    buzzer_cfg.htim         = &htim3;
    buzzer_cfg.channel      = BUZZER_CHANNEL;
    buzzer_cfg.tick_freq_hz = BUZZER_TICK_FREQ;
    buzzer_cfg.min_freq_hz  = BUZZER_MIN_FREQ;
    buzzer_cfg.max_freq_hz  = BUZZER_MAX_FREQ;
    // buzzer_init(&buzzer_cfg);

    /* â"€â"€ LED PWM â"€â"€ */
    memset(&pwm_cfg, 0, sizeof(pwm_cfg));
    pwm_cfg.htim         = &htim4;
    pwm_cfg.channel      = LED_CHANNEL;
    pwm_cfg.tick_freq_hz = LED_TICK_FREQ;
    pwm_cfg.min_freq_hz  = 10;
    pwm_cfg.max_freq_hz  = 50000;
    // PWM_Init(&pwm_cfg);
    // PWM_SetFreq(&pwm_cfg, LED_PWM_FREQ);
    // PWM_SetDuty(&pwm_cfg, LED_BRIGHTNESS_MENU);

    /* â"€â"€ Joystick â"€â"€ */
    memset(&joystick_cfg, 0, sizeof(joystick_cfg));
    joystick_cfg.adc           = &hadc1;
    joystick_cfg.x_channel     = JOYSTICK_X_CHANNEL;
    joystick_cfg.y_channel     = JOYSTICK_Y_CHANNEL;
    joystick_cfg.sampling_time = JOYSTICK_SAMPLE_TIME;
    joystick_cfg.center_x      = JOYSTICK_CENTER_X;
    joystick_cfg.center_y      = JOYSTICK_CENTER_Y;
    joystick_cfg.deadzone      = JOYSTICK_DEADZONE;
    // Joystick_Init(&joystick_cfg);

    /* â"€â"€ 7-Segment (starts TIM6 ISR internally) â"€â"€ */
    // SevenSeg_Init(); // TEMP

    /* â"€â"€ Buttons â"€â"€ */
    // Input_Init();

    /* â"€â"€ Menu â"€â"€ */
    Menu_Init(&menu);

    /* ── Startup splash on LCD ── */
    LCD_Fill_Buffer(0);
    LCD_printString("MULTI", 70, 70, 6, 3);
    LCD_printString("GAME", 78, 110, 6, 3);
    LCD_printString("CONSOLE", 50, 155, 1, 2);
    LCD_Refresh(&lcd_cfg);

    SevenSeg_SetNumber(88);
    // PWM_SetDuty(&pwm_cfg, 80);

    /* Startup beep sequence */
    RGB_SET_EASY();
    buzzer_tone(&buzzer_cfg, 523, 30); HAL_Delay(150); buzzer_off(&buzzer_cfg);
    RGB_SET_MEDIUM();
    HAL_Delay(80);
    buzzer_tone(&buzzer_cfg, 659, 30); HAL_Delay(150); buzzer_off(&buzzer_cfg);
    RGB_SET_HARD();
    HAL_Delay(80);
    buzzer_tone(&buzzer_cfg, 784, 30); HAL_Delay(200); buzzer_off(&buzzer_cfg);
    RGB_OFF();

    HAL_Delay(500);
    SevenSeg_Clear();
    // PWM_SetDuty(&pwm_cfg, LED_BRIGHTNESS_MENU);

    printf("MultiGame L476RG Ready.\r\n");

    /* â"€â"€ Main game loop â"€â"€ */
    MenuState current_state = MENU_STATE_HOME;

    while (1)
    {
        switch (current_state)
        {
            case MENU_STATE_HOME:
                // PWM_SetDuty(&pwm_cfg, LED_BRIGHTNESS_MENU);
                SevenSeg_Clear();
                current_state = Menu_Run(&menu);
                break;

            case MENU_STATE_GAME_1:
                current_state = Game1_Run();
                break;

            case MENU_STATE_GAME_2:
                current_state = Game2_Run();
                break;

            case MENU_STATE_GAME_3:
                current_state = Game3_Run();
                break;

            default:
                current_state = MENU_STATE_HOME;
                break;
        }
    }

    return 0;
}

/* â"€â"€ Clock 80MHz (NUCLEO-L476RG, HSI 16MHz â†’ PLL â†’ 80MHz) â"€â"€ */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef o = {0};
    RCC_ClkInitTypeDef c = {0};

    /* Voltage range 1 required for 80MHz operation */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
        Error_Handler();

    /* HSI 16MHz â†’ PLL â†’ 80MHz:
       SYSCLK = HSI(16) / PLLM(1) * PLLN(10) / PLLR(2) = 80MHz */
    o.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    o.HSIState            = RCC_HSI_ON;
    o.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    o.PLL.PLLState        = RCC_PLL_ON;
    o.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    o.PLL.PLLM            = 1;
    o.PLL.PLLN            = 10;
    o.PLL.PLLP            = RCC_PLLP_DIV7;
    o.PLL.PLLQ            = RCC_PLLQ_DIV2;
    o.PLL.PLLR            = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&o) != HAL_OK)
        Error_Handler();

    /* SYSCLK=80MHz, AHB=80MHz, APB1=80MHz, APB2=80MHz */
    c.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    c.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV1;
    c.APB2CLKDivider = RCC_HCLK_DIV1;
    /* Flash latency 4 wait-states for 80MHz in voltage range 1 */
    if (HAL_RCC_ClockConfig(&c, FLASH_LATENCY_4) != HAL_OK)
        Error_Handler();
}

void Error_Handler(void) { __disable_irq(); while(1){} }

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    printf("Assert failed: file %s, line %lu\r\n", file, (unsigned long)line);
}
#endif