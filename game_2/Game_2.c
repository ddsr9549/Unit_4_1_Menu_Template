/**
 * @file Game_2.c
 * @brief Stroop Colour Game -- 3 question types
 *
 * Screen: 240x240 pixels
 * Each round:
 *   Phase 1 -- flash a colour word written in a DIFFERENT ink colour
 *   Phase 2 -- answer one of three question types:
 *     Type 0: "What was the INK colour?"   (ignore the word)
 *     Type 1: "What did the word SAY?"     (ignore the ink)
 *     Type 2: "Which has NOTHING in common with the word?"
 *
 * Flash time depends on difficulty: Easy=2000ms, Medium=1200ms, Hard=600ms.
 * Wrong answer: -5 seconds.
 */

#include "Game_2.h"
#include "InputHandler.h"
#include "Menu.h"
#include "LCD.h"
#include "Buzzer.h"
#include "Joystick.h"
#include "SevenSeg.h"
#include "tim.h"
#include <stdio.h>
#include <string.h>

extern ST7789V2_cfg_t  lcd_cfg;
extern Buzzer_cfg_t    buzzer_cfg;
extern Joystick_cfg_t  joystick_cfg;
extern Joystick_t      joystick_data;
extern volatile uint32_t g_tim7_ticks;

/* ── Colours ── */
#define C_BLACK  0
#define C_WHITE  1
#define C_RED    2
#define C_GREEN  3
#define C_BLUE   4
#define C_ORANGE 5
#define C_YELLOW 6
#define C_NAVY   9
#define C_GREY  13
#define C_CYAN  14

/* 4 word/ink colours used in the Stroop test */
#define NUM_COLS 4
static const char*   COL_WORD[NUM_COLS] = {"RED", "GREEN", "BLUE", "YELLOW"};
static const uint8_t COL_IDX[NUM_COLS]  = {C_RED, C_GREEN, C_BLUE, C_YELLOW};

#define WRONG_PENALTY 5
#define FRAME_MS      80

/* ── Game state ── */
static uint8_t score, time_left, game_over, max_time;
static uint8_t q_type;       /* 0=ink, 1=meaning, 2=neither */
static uint8_t q_cursor;     /* 0-2 selected choice */
static uint8_t q_correct;    /* position of correct answer */
static uint8_t choices[3];   /* colour index for each choice */
static uint8_t word_meaning; /* index into COL_WORD (the text) */
static uint8_t word_ink;     /* index into COL_IDX (the ink colour) */
static uint32_t flash_ms;    /* flash duration per difficulty */

/* ── LCG RNG ── */
static uint32_t rng_seed;
static uint8_t rng8(uint8_t max) {
    rng_seed = rng_seed * 1664525UL + 1013904223UL;
    return (uint8_t)((rng_seed >> 16) % max);
}

/* ── Build a question ── */
static void new_question(void)
{
    word_meaning = rng8(NUM_COLS);
    do { word_ink = rng8(NUM_COLS); } while (word_ink == word_meaning);

    q_type   = rng8(3);
    q_cursor = 0;

    if (q_type == 0) {
        uint8_t others[2]; uint8_t oi = 0;
        for (uint8_t i = 0; i < NUM_COLS && oi < 2; i++)
            if (i != word_ink && i != word_meaning) others[oi++] = i;
        uint8_t pos = rng8(3);
        q_correct = pos;
        choices[pos]             = word_ink;
        choices[(pos + 1) % 3]   = others[0];
        choices[(pos + 2) % 3]   = others[1];

    } else if (q_type == 1) {
        uint8_t others[2]; uint8_t oi = 0;
        for (uint8_t i = 0; i < NUM_COLS && oi < 2; i++)
            if (i != word_meaning && i != word_ink) others[oi++] = i;
        uint8_t pos = rng8(3);
        q_correct = pos;
        choices[pos]             = word_meaning;
        choices[(pos + 1) % 3]   = others[0];
        choices[(pos + 2) % 3]   = others[1];

    } else {
        uint8_t neither[2]; uint8_t ni = 0;
        for (uint8_t i = 0; i < NUM_COLS && ni < 2; i++)
            if (i != word_ink && i != word_meaning) neither[ni++] = i;
        uint8_t pos = rng8(3);
        q_correct = pos;
        choices[pos]             = neither[0];
        choices[(pos + 1) % 3]   = word_ink;
        choices[(pos + 2) % 3]   = word_meaning;
    }
}

/* ── Draw header ── */
static void draw_header(void)
{
    char buf[20];
    LCD_Draw_Rect(0, 0, 240, 28, C_NAVY, 1);
    LCD_printString("STROOP", 75, 6, C_CYAN, 2);
    snprintf(buf, sizeof(buf), "Sc:%u", score);
    LCD_printString(buf, 4, 9, C_WHITE, 1);
    snprintf(buf, sizeof(buf), "T:%us", time_left);
    LCD_printString(buf, 190, 9, time_left <= 10 ? C_RED : C_WHITE, 1);
    uint8_t bar = (max_time > 0) ? (uint8_t)((time_left * 200U) / max_time) : 0;
    LCD_Draw_Rect(0, 28, 240, 4, C_GREY, 1);
    if (bar > 0) LCD_Draw_Rect(0, 28, bar, 4, time_left <= 10 ? C_RED : C_GREEN, 1);
}

/* ── Flash the word ── */
static void flash_word(void)
{
    LCD_Fill_Buffer(C_BLACK);
    draw_header();
    LCD_printString("Remember this!", 32, 36, C_GREY, 1);
    LCD_Draw_Line(0, 48, 240, 48, C_GREY);

    /* Centre the colour word on screen, font size 3 */
    LCD_printString(COL_WORD[word_meaning], 50, 106, COL_IDX[word_ink], 3);
    LCD_Refresh(&lcd_cfg);

    uint32_t elapsed = 0;
    while (elapsed < flash_ms) {
        HAL_Delay(50);
        elapsed += 50;
        if (g_tim7_ticks > 0) {
            g_tim7_ticks = 0;
            if (time_left > 0) time_left--;
            SevenSeg_SetNumber(time_left);
        }
    }

    /* Blank the word */
    LCD_Fill_Buffer(C_BLACK);
    draw_header();
    LCD_Refresh(&lcd_cfg);
    HAL_Delay(200);
}

/* ── Draw question + choices ── */
static void draw_question(void)
{
    LCD_Fill_Buffer(C_BLACK);
    draw_header();

    /* Question text (2 lines) */
    if (q_type == 0) {
        LCD_printString("What was the", 22, 36, C_WHITE, 1);
        LCD_printString("INK colour?", 30, 46, C_CYAN, 1);
    } else if (q_type == 1) {
        LCD_printString("What did the", 22, 36, C_WHITE, 1);
        LCD_printString("word SAY?", 38, 46, C_CYAN, 1);
    } else {
        LCD_printString("Which has NOTHING", 6, 36, C_WHITE, 1);
        LCD_printString("in common with it?", 6, 46, C_CYAN, 1);
    }

    LCD_Draw_Line(0, 58, 240, 58, C_GREY);

    /* 3 choices -- centres at x=40, 120, 200 */
    static const uint16_t CX[3] = {40, 120, 200};

    if (q_type < 2) {
        /* Filled colour circles, centred at y=130 */
        for (int i = 0; i < 3; i++) {
            uint8_t hi = (i == q_cursor);
            LCD_Draw_Circle(CX[i], 130, hi ? 36 : 32, hi ? C_WHITE : C_GREY, 0);
            LCD_Draw_Circle(CX[i], 130, hi ? 32 : 29, COL_IDX[choices[i]], 1);
            LCD_printString(COL_WORD[choices[i]], CX[i] - 18, 170, hi ? C_YELLOW : C_GREY, 1);
        }
    } else {
        /* Colour-word text labels, centred at y=115 */
        for (int i = 0; i < 3; i++) {
            uint8_t hi = (i == q_cursor);
            if (hi) LCD_Draw_Rect(CX[i] - 36, 108, 72, 22, C_NAVY, 1);
            LCD_printString(COL_WORD[choices[i]], CX[i] - 20, 110, COL_IDX[choices[i]], 2);
        }
    }

    LCD_printString("< >  SELECT=OK", 42, 210, C_GREY, 1);
    LCD_Refresh(&lcd_cfg);
}

/* ── Feedback screen ── */
static void draw_feedback(uint8_t correct)
{
    LCD_Fill_Buffer(C_BLACK);
    draw_header();
    if (correct) {
        LCD_Draw_Rect(20, 80, 200, 80, C_GREEN, 1);
        LCD_printString("CORRECT!", 48, 106, C_BLACK, 2);
    } else {
        LCD_Draw_Rect(20, 80, 200, 80, C_RED, 1);
        LCD_printString("WRONG! -5s", 28, 106, C_WHITE, 2);
        LCD_printString("It was:", 78, 172, C_GREY, 1);
        LCD_printString(COL_WORD[choices[q_correct]], 78, 184,
                        COL_IDX[choices[q_correct]], 1);
    }
    LCD_Refresh(&lcd_cfg);
}

/* ── Audio / LED ── */
static void snd_correct(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    buzzer_tone(&buzzer_cfg, 1047, 50); HAL_Delay(100); buzzer_off(&buzzer_cfg);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_Delay(60);
    buzzer_tone(&buzzer_cfg, 1568, 50); HAL_Delay(120); buzzer_off(&buzzer_cfg);
}

static void snd_wrong(void) {
    buzzer_tone(&buzzer_cfg, 280, 60); HAL_Delay(350); buzzer_off(&buzzer_cfg);
}

static void snd_gameover(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    for (int i = 0; i < 3; i++) {
        buzzer_tone(&buzzer_cfg, 200, 70); HAL_Delay(200); buzzer_off(&buzzer_cfg);
        HAL_Delay(100);
    }
}

static void draw_gameover(void)
{
    LCD_Fill_Buffer(C_BLACK);
    draw_header();
    LCD_Draw_Rect(20, 80, 200, 110, C_RED, 1);
    LCD_printString("GAME OVER", 40, 100, C_WHITE, 2);
    char buf[20];
    snprintf(buf, sizeof(buf), "Score: %u", score);
    LCD_printString(buf, 55, 145, C_YELLOW, 2);
    LCD_Refresh(&lcd_cfg);
}

/* ── Main entry point ── */
MenuState Game2_Run(void)
{
    rng_seed  = HAL_GetTick() + 3;
    score     = 0;
    game_over = 0;
    time_left = Menu_GetTimeLimit();
    max_time  = time_left;

    switch (g_difficulty) {
    case DIFFICULTY_EASY:   flash_ms = 2000; break;
    case DIFFICULTY_MEDIUM: flash_ms = 1200; break;
    default:                flash_ms =  600; break;
    }

    g_tim7_ticks = 0;
    HAL_TIM_Base_Start_IT(&htim7);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    SevenSeg_SetNumber(time_left);

    buzzer_tone(&buzzer_cfg, 659, 40); HAL_Delay(200); buzzer_off(&buzzer_cfg);

    new_question();
    flash_word();
    draw_question();

    Direction last_dir = CENTRE;

    while (1) {
        uint32_t t0 = HAL_GetTick();

        if (g_tim7_ticks > 0 && !game_over) {
            g_tim7_ticks = 0;
            if (time_left > 0) time_left--;
            SevenSeg_SetNumber(time_left);
            if (time_left == 0) {
                game_over = 1;
                draw_gameover();
                snd_gameover();
                HAL_Delay(3000);
                break;
            }
        }

        Input_Read();
        Joystick_Read(&joystick_cfg, &joystick_data);
        Direction dir = joystick_data.direction;

        if (current_input.btn_reset_pressed) break;

        if (!game_over && dir != last_dir && dir != CENTRE) {
            buzzer_tone(&buzzer_cfg, 800, 8); HAL_Delay(12); buzzer_off(&buzzer_cfg);
            if ((dir==W||dir==NW||dir==SW) && q_cursor > 0) q_cursor--;
            if ((dir==E||dir==NE||dir==SE) && q_cursor < 2) q_cursor++;
            draw_question();
        }
        last_dir = dir;

        if (!game_over && current_input.btn_select_pressed) {
            uint8_t correct = (q_cursor == q_correct);
            draw_feedback(correct);

            if (correct) {
                score++;
                snd_correct();
            } else {
                if (time_left > WRONG_PENALTY) time_left -= WRONG_PENALTY;
                else time_left = 0;
                SevenSeg_SetNumber(time_left);
                snd_wrong();
            }
            HAL_Delay(800);

            if (time_left > 0) {
                new_question();
                flash_word();
                draw_question();
            }
        }

        uint32_t el = HAL_GetTick() - t0;
        if (el < FRAME_MS) HAL_Delay(FRAME_MS - el);
    }

    HAL_TIM_Base_Stop_IT(&htim7);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    SevenSeg_Clear();
    return MENU_STATE_HOME;
}
