/**
 * @file hw_config.h
 * @brief Central hardware configuration for MultiGame Console
 *
 * LED System:
 *   Green LED (PB6)  — ON while game is running, OFF in menu
 *   RGB LED:
 *     R → PA10  G → PA11  B → PA12
 *     Easy   = GREEN  (R=0 G=1 B=0)
 *     Medium = BLUE   (R=0 G=0 B=1)
 *     Hard   = RED    (R=1 G=0 B=0)
 *
 * LCD wiring per official README:
 *   MOSI→PB15  SCK→PB13  CS→PB12  DC→PB11  RST→PB2  BL→PB1
 */

#ifndef HW_CONFIG_H
#define HW_CONFIG_H

#include "stm32l4xx_hal.h"   /* Change to stm32f4xx_hal.h for F446RE */

/* ============================================================
 *  BOARD SELECTION
 * ============================================================ */
//#define BOARD_F446RE
#define BOARD_L476RG

/* ============================================================
 *  SYSTEM CLOCK
 * ============================================================ */
#ifdef BOARD_F446RE
    #define HW_SYSCLK_HZ        180000000UL
    #define HW_TIM_TICK_FREQ    1000000UL
    #define HW_TIM_PRESCALER    179
#endif
#ifdef BOARD_L476RG
    #define HW_SYSCLK_HZ        80000000UL
    #define HW_TIM_TICK_FREQ    1000000UL
    #define HW_TIM_PRESCALER    79
#endif

/* ============================================================
 *  LCD ST7789V2 — SPI2
 *  DC = PB11 per official README
 * ============================================================ */
#define LCD_SPI             SPI2
#define LCD_CS_PORT         GPIOB
#define LCD_CS_PIN          GPIO_PIN_12
#define LCD_DC_PORT         GPIOB
#define LCD_DC_PIN          GPIO_PIN_10
#define LCD_RST_PORT        GPIOB
#define LCD_RST_PIN         GPIO_PIN_2
#define LCD_BL_PORT         GPIOB
#define LCD_BL_PIN          GPIO_PIN_1
#define LCD_MOSI_PORT       GPIOB
#define LCD_MOSI_PIN        GPIO_PIN_15
#define LCD_SCLK_PORT       GPIOB
#define LCD_SCLK_PIN        GPIO_PIN_13

/* ============================================================
 *  LED PWM — TIM4 CH1 (PB6)
 *  Brightness controlled via PWM duty cycle
 * ============================================================ */
#define LED_TIM              htim4
#define LED_CHANNEL          TIM_CHANNEL_1
#define LED_TICK_FREQ        HW_TIM_TICK_FREQ
#define LED_PWM_FREQ         1000U       /* 1 kHz PWM frequency */
#define LED_BRIGHTNESS_MENU  30U         /* 30% duty in menu     */

/* ============================================================
 *  GREEN LED — PB6 (same pin, driven by TIM4 PWM above)
 *  ON while game running, OFF in menu
 *  Connect: PB6 → 330Ω → LED(+) → LED(-) → GND
 * ============================================================ */
#define GREEN_LED_PORT      GPIOB
#define GREEN_LED_PIN       GPIO_PIN_6

#define GREEN_LED_ON()   HAL_GPIO_WritePin(GREEN_LED_PORT, GREEN_LED_PIN, GPIO_PIN_SET)
#define GREEN_LED_OFF()  HAL_GPIO_WritePin(GREEN_LED_PORT, GREEN_LED_PIN, GPIO_PIN_RESET)

/* ============================================================
 *  RGB LED — Simple GPIO (ON/OFF per colour)
 *  R → PA10  G → PA11  B → PA12
 *
 *  Common Cathode: common pin → GND
 *  Common Anode:   common pin → 3V3
 *                  (invert logic: SET=OFF, RESET=ON)
 *
 *  Wire each pin via 330Ω resistor
 * ============================================================ */
#define RGB_LED_R_PORT      GPIOA
#define RGB_LED_R_PIN       GPIO_PIN_10

#define RGB_LED_G_PORT      GPIOA
#define RGB_LED_G_PIN       GPIO_PIN_11

#define RGB_LED_B_PORT      GPIOA
#define RGB_LED_B_PIN       GPIO_PIN_12

/* Common Cathode macros (HIGH = ON) */
#define RGB_R_ON()   HAL_GPIO_WritePin(RGB_LED_R_PORT, RGB_LED_R_PIN, GPIO_PIN_SET)
#define RGB_R_OFF()  HAL_GPIO_WritePin(RGB_LED_R_PORT, RGB_LED_R_PIN, GPIO_PIN_RESET)
#define RGB_G_ON()   HAL_GPIO_WritePin(RGB_LED_G_PORT, RGB_LED_G_PIN, GPIO_PIN_SET)
#define RGB_G_OFF()  HAL_GPIO_WritePin(RGB_LED_G_PORT, RGB_LED_G_PIN, GPIO_PIN_RESET)
#define RGB_B_ON()   HAL_GPIO_WritePin(RGB_LED_B_PORT, RGB_LED_B_PIN, GPIO_PIN_SET)
#define RGB_B_OFF()  HAL_GPIO_WritePin(RGB_LED_B_PORT, RGB_LED_B_PIN, GPIO_PIN_RESET)

/* Turn off all RGB channels */
#define RGB_OFF()    do { RGB_R_OFF(); RGB_G_OFF(); RGB_B_OFF(); } while(0)

/* Level colours:
 *   Easy   = GREEN
 *   Medium = BLUE
 *   Hard   = RED  */
#define RGB_SET_EASY()    do { RGB_R_OFF(); RGB_G_ON();  RGB_B_OFF(); } while(0)
#define RGB_SET_MEDIUM()  do { RGB_R_OFF(); RGB_G_OFF(); RGB_B_ON();  } while(0)
#define RGB_SET_HARD()    do { RGB_R_ON();  RGB_G_OFF(); RGB_B_OFF(); } while(0)

/* ============================================================
 *  JOYSTICK — ADC1 (PA0=VRx, PA1=VRy)
 *  NOTE: Same pins, different channel numbers per MCU family:
 *    F446RE: PA0=IN0, PA1=IN1
 *    L476RG: PA0=IN5, PA1=IN6
 * ============================================================ */
#define JOYSTICK_ADC            hadc1
#ifdef BOARD_F446RE
#define JOYSTICK_X_CHANNEL      ADC_CHANNEL_0
#define JOYSTICK_Y_CHANNEL      ADC_CHANNEL_1
#define JOYSTICK_SAMPLE_TIME    ADC_SAMPLETIME_84CYCLES
#endif
#ifdef BOARD_L476RG
#define JOYSTICK_X_CHANNEL      ADC_CHANNEL_5
#define JOYSTICK_Y_CHANNEL      ADC_CHANNEL_6
#define JOYSTICK_SAMPLE_TIME    ADC_SAMPLETIME_92CYCLES_5
#endif
#define JOYSTICK_CENTER_X       2048
#define JOYSTICK_CENTER_Y       2048
#define JOYSTICK_DEADZONE       200

/* ============================================================
 *  POTENTIOMETER — ADC2 (PC0) — buzzer volume
 *  NOTE: Same pin, different channel numbers per MCU family:
 *    F446RE: PC0=IN10
 *    L476RG: PC0=IN1
 * ============================================================ */
#define POT_ADC             hadc2
#ifdef BOARD_F446RE
#define POT_CHANNEL         ADC_CHANNEL_10
#define POT_SAMPLE_TIME     ADC_SAMPLETIME_84CYCLES
#endif
#ifdef BOARD_L476RG
#define POT_CHANNEL         ADC_CHANNEL_1
#define POT_SAMPLE_TIME     ADC_SAMPLETIME_92CYCLES_5
#endif
#define POT_MAX_VALUE       4095
#define POT_MIN_VOL         10
#define POT_MAX_VOL         100

/* ============================================================
 *  BUZZER — TIM3 CH3 (PB0)
 * ============================================================ */
#define BUZZER_TIM          htim3
#define BUZZER_CHANNEL      TIM_CHANNEL_3
#define BUZZER_TICK_FREQ    HW_TIM_TICK_FREQ
#define BUZZER_MIN_FREQ     20
#define BUZZER_MAX_FREQ     20000

/* ============================================================
 *  7-SEGMENT DISPLAY
 * ============================================================ */
#define SEG_A_PORT      GPIOA
#define SEG_A_PIN       GPIO_PIN_6
#define SEG_B_PORT      GPIOA
#define SEG_B_PIN       GPIO_PIN_7
#define SEG_C_PORT      GPIOC
#define SEG_C_PIN       GPIO_PIN_10
#define SEG_D_PORT      GPIOA
#define SEG_D_PIN       GPIO_PIN_8
#define SEG_E_PORT      GPIOA
#define SEG_E_PIN       GPIO_PIN_9
#define SEG_F_PORT      GPIOC
#define SEG_F_PIN       GPIO_PIN_6
#define SEG_G_PORT      GPIOC
#define SEG_G_PIN       GPIO_PIN_7
#define DIG_1_PORT      GPIOC
#define DIG_1_PIN       GPIO_PIN_8
#define DIG_2_PORT      GPIOC
#define DIG_2_PIN       GPIO_PIN_9
#define SEG_TIM         htim6

/* ============================================================
 *  BUTTONS
 * ============================================================ */
#define BTN_SELECT_PORT     GPIOC
#define BTN_SELECT_PIN      GPIO_PIN_1
#define BTN_RESET_PORT      GPIOC
#define BTN_RESET_PIN       GPIO_PIN_2
#define BTN_DEBOUNCE_MS     200

/* ============================================================
 *  COUNTDOWN TIMER — TIM7
 * ============================================================ */
#define COUNTDOWN_TIM       htim7

/* ============================================================
 *  GAME TIMING
 * ============================================================ */
#define GAME_TIME_EASY      60
#define GAME_TIME_MEDIUM    45
#define GAME_TIME_HARD      30

/* ============================================================
 *  BUZZER FREQUENCIES PER LEVEL
 * ============================================================ */
#define BUZZER_LEVEL1_FREQ  523     /* C5  — Easy   */
#define BUZZER_LEVEL2_FREQ  659     /* E5  — Medium */
#define BUZZER_LEVEL3_FREQ  784     /* G5  — Hard   */
#define BUZZER_WIN_FREQ     1047    /* C6  — Correct answer */
#define BUZZER_LOSE_FREQ    262     /* C4  — Wrong / timeout */
#define BUZZER_SELECT_FREQ  440     /* A4  — Menu select */

#endif /* HW_CONFIG_H */
