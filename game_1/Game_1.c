/**
 * @file Game_1.c
 * @brief Shapes Game -- 4 rotating question types
 *
 * Screen: 240x240 pixels
 * Level 0: Speed race -- watch 3 shapes animate, pick the fastest
 * Level 1: Shape Math -- equation using shape values, pick numeric answer
 * Level 2: Shape Algebra -- solve system of 2 equations, find sq+tri
 * Level 3: Find Shape -- 3x3 grid, navigate and select correct size+colour
 */

#include "Game_1.h"
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

/* ── Colour palette indices ── */
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

/* ── Shape IDs ── */
#define SH_CIRCLE   0
#define SH_SQUARE   1
#define SH_TRIANGLE 2

static const uint8_t SH_COL[3]       = {C_BLUE, C_ORANGE, C_GREEN};
static const char*   SH_NAME[3]      = {"CIRCLE", "SQUARE", "TRIANG"};

/* ── Grid-find colour set ── */
static const uint8_t GF_COL[3]       = {C_RED, C_CYAN, C_YELLOW};
static const char*   GF_COL_NAME[3]  = {"RED", "CYAN", "YELLOW"};
static const char*   GF_SZ_NAME[2]   = {"SMALL", "BIG"};

/* ── Game state ── */
#define WRONG_PENALTY 5
#define FRAME_MS      100
#define GRID_N        3

/* Grid cell dimensions -- fit 3x3 in 240x240 minus 48px header */
#define GCW           78    /* cell width  (3*78=234, left margin 3) */
#define GCH           62    /* cell height (48 + 3*62 = 234)         */
#define GX0            3
#define GY0           48

static uint8_t score, time_left, game_over, max_time;
static uint8_t q_type;        /* 0-3 */
static uint8_t q_cursor;      /* answer choice 0-2 */
static uint8_t q_correct;     /* which choice is correct (255 = grid mode) */
static int8_t  ans_vals[3];   /* numeric answer choices */

/* Speed-level state */
static uint8_t spd_order[3];   /* shape index per lane */
static uint8_t spd_fastest;    /* lane index of fastest shape */

/* Math-level state */
static uint8_t math_val[3];    /* value assigned to each shape */
static uint8_t math_sh[4];     /* shapes in equation */

/* Algebra-level state */
static uint8_t alge_A, alge_B; /* eq1=A, eq2=B, answer=A+B */

/* Find-level state */
static uint8_t grid_sh[GRID_N][GRID_N];
static uint8_t grid_col[GRID_N][GRID_N];
static uint8_t grid_sz[GRID_N][GRID_N]; /* 0=small,1=big */
static uint8_t tgt_col, tgt_sz;
static uint8_t tgt_r, tgt_c;
static uint8_t cur_r, cur_c;

/* ── LCG RNG ── */
static uint32_t rng_seed;
static uint8_t rng8(uint8_t max) {
    rng_seed = rng_seed * 1664525UL + 1013904223UL;
    return (uint8_t)((rng_seed >> 16) % max);
}

/* ── Draw helpers ── */
static void draw_tri(uint16_t cx, uint16_t cy, uint8_t sz, uint8_t col)
{
    uint8_t half = sz / 2;
    for (int row = 0; row < sz; row++) {
        uint8_t hw = (uint8_t)((row * half) / (sz > 1 ? sz - 1 : 1));
        uint16_t rx = (cx > hw) ? (cx - hw) : 0;
        LCD_Draw_Rect(rx, cy - half + row, hw * 2 + 1, 1, col, 1);
    }
}

static void draw_shape(uint8_t sh, uint16_t cx, uint16_t cy, uint8_t sz, uint8_t col)
{
    uint8_t r = sz / 2;
    switch (sh) {
    case SH_CIRCLE:   LCD_Draw_Circle(cx, cy, r, col, 1); break;
    case SH_SQUARE:   LCD_Draw_Rect(cx - r, cy - r, sz, sz, col, 1); break;
    case SH_TRIANGLE: draw_tri(cx, cy, sz, col); break;
    }
}

static void draw_header(const char *title)
{
    char buf[20];
    LCD_Draw_Rect(0, 0, 240, 28, C_NAVY, 1);
    LCD_printString(title, 70, 6, C_YELLOW, 2);
    snprintf(buf, sizeof(buf), "Sc:%u", score);
    LCD_printString(buf, 4, 9, C_WHITE, 1);
    snprintf(buf, sizeof(buf), "T:%us", time_left);
    LCD_printString(buf, 190, 9, time_left <= 10 ? C_RED : C_WHITE, 1);
    /* Timer bar -- scaled to initial time limit */
    uint8_t bar = (max_time > 0) ? (uint8_t)((time_left * 200U) / max_time) : 0;
    LCD_Draw_Rect(0, 28, 240, 4, C_GREY, 1);
    if (bar > 0) LCD_Draw_Rect(0, 28, bar, 4, time_left <= 10 ? C_RED : C_GREEN, 1);
}

/* ── Level 0: Speed ── */
static void gen_speed(void)
{
    spd_order[0] = rng8(3);
    do { spd_order[1] = rng8(3); } while (spd_order[1] == spd_order[0]);
    do { spd_order[2] = rng8(3); } while (spd_order[2] == spd_order[0] || spd_order[2] == spd_order[1]);
    spd_fastest = rng8(3);
    q_correct   = spd_fastest;
    q_cursor    = 0;
}

static void run_speed_animation(void)
{
    uint8_t spd[3];
    for (uint8_t i = 0; i < 3; i++) {
        spd[i] = (i == spd_fastest) ? 5 : ((i == ((spd_fastest + 1) % 3)) ? 3 : 1);
    }
    /* 3 lanes spaced evenly in 240px height, below 32px header */
    static const uint16_t LANE_Y[3] = {90, 148, 206};
    uint16_t pos[3] = {10, 10, 10};
    uint8_t  done = 0;

    while (!done) {
        LCD_Fill_Buffer(C_BLACK);
        draw_header("SHAPES");
        LCD_printString("Watch closely!", 40, 36, C_WHITE, 1);
        for (int i = 0; i < 3; i++) {
            draw_shape(spd_order[i], pos[i], LANE_Y[i], 24, SH_COL[spd_order[i]]);
            pos[i] += spd[i];
            if (pos[i] > 224) done = 1;
        }
        LCD_Refresh(&lcd_cfg);
        HAL_Delay(50);
    }
    HAL_Delay(300);
}

static void draw_speed_q(void)
{
    LCD_Fill_Buffer(C_BLACK);
    draw_header("SHAPES");
    LCD_printString("Which was FASTEST?", 5, 36, C_CYAN, 1);

    static const uint16_t CX[3] = {40, 120, 200};
    for (int i = 0; i < 3; i++) {
        uint8_t hi = (i == q_cursor);
        /* Box: y=50, height=110 -> bottom y=160 */
        LCD_Draw_Rect(CX[i] - 32, 50, 64, 110, hi ? C_YELLOW : C_GREY, 0);
        if (hi) LCD_Draw_Rect(CX[i] - 31, 51, 62, 108, C_NAVY, 1);
        /* Shape centred inside box */
        draw_shape(spd_order[i], CX[i], 105, 36, SH_COL[spd_order[i]]);
        /* Name below box */
        LCD_printString(SH_NAME[spd_order[i]], CX[i] - 24, 163, hi ? C_YELLOW : C_GREY, 1);
    }
    LCD_printString("< >  SELECT=OK", 42, 210, C_GREY, 1);
    LCD_Refresh(&lcd_cfg);
}

/* ── Level 1: Shape Math ── */
static void gen_math(void)
{
    for (int i = 0; i < 3; i++) math_val[i] = 1 + rng8(4);
    for (int i = 0; i < 4; i++) math_sh[i]  = rng8(3);
    int8_t ans = (int8_t)(math_val[math_sh[0]] + math_val[math_sh[1]]
                          - math_val[math_sh[2]] + math_val[math_sh[3]]);
    if (ans < 1) ans = 1;
    uint8_t pos = rng8(3);
    q_correct = pos;
    ans_vals[pos]             = ans;
    ans_vals[(pos + 1) % 3]   = ans + 1;
    ans_vals[(pos + 2) % 3]   = (ans > 1) ? ans - 1 : ans + 2;
    q_cursor = 0;
}

static void draw_math(void)
{
    LCD_Fill_Buffer(C_BLACK);
    draw_header("SHAPES");
    LCD_printString("Shape Math:", 10, 36, C_CYAN, 1);

    char buf[8];
    /* Value legend: 3 shapes at y=62 */
    for (int i = 0; i < 3; i++) {
        uint16_t lx = 10 + i * 75;
        draw_shape(i, lx + 12, 62, 18, SH_COL[i]);
        snprintf(buf, sizeof(buf), "=%d", math_val[i]);
        LCD_printString(buf, lx + 25, 56, C_WHITE, 1);
    }
    LCD_Draw_Line(0, 78, 240, 78, C_GREY);

    /* Equation: sh[0] + sh[1] - sh[2] + sh[3] = ? at y=101 */
    static const char OPS[3] = {'+', '-', '+'};
    uint16_t ex = 4;
    for (int i = 0; i < 4; i++) {
        draw_shape(math_sh[i], ex + 13, 101, 22, SH_COL[math_sh[i]]);
        ex += 30;
        if (i < 3) {
            char op[2] = {OPS[i], '\0'};
            LCD_printString(op, ex, 94, C_WHITE, 2);
            ex += 16;
        }
    }
    LCD_printString("= ?", ex, 94, C_YELLOW, 2);

    /* Choices: boxes at y=130, height=52 */
    static const uint16_t CX[3] = {40, 120, 200};
    for (int i = 0; i < 3; i++) {
        uint8_t hi = (i == q_cursor);
        LCD_Draw_Rect(CX[i] - 26, 130, 52, 52, hi ? C_YELLOW : C_GREY, 0);
        snprintf(buf, sizeof(buf), "%d", ans_vals[i]);
        LCD_printString(buf, CX[i] - 6, 144, hi ? C_YELLOW : C_WHITE, 2);
    }
    LCD_printString("< >  SELECT=OK", 42, 210, C_GREY, 1);
    LCD_Refresh(&lcd_cfg);
}

/* ── Level 2: Shape Algebra ── */
static const uint8_t ALGE_PAIRS[6][2] = {
    {5, 2}, {6, 3}, {4, 5}, {7, 1}, {8, 2}, {3, 4}
};

static void gen_alge(void)
{
    uint8_t idx = rng8(6);
    alge_A = ALGE_PAIRS[idx][0];
    alge_B = ALGE_PAIRS[idx][1];
    uint8_t ans = alge_A + alge_B;
    uint8_t pos = rng8(3);
    q_correct = pos;
    ans_vals[pos]           = (int8_t)ans;
    ans_vals[(pos + 1) % 3] = (int8_t)(ans + 1);
    ans_vals[(pos + 2) % 3] = (ans > 1) ? (int8_t)(ans - 1) : (int8_t)(ans + 2);
    q_cursor = 0;
}

static void draw_alge(void)
{
    LCD_Fill_Buffer(C_BLACK);
    draw_header("SHAPES");
    LCD_printString("Shape Algebra:", 5, 36, C_CYAN, 1);

    char buf[12];
    /* Row 1: [SQ] + [CIRC] = A  (shapes centred at y=62) */
    draw_shape(SH_SQUARE,   28, 62, 22, C_ORANGE);
    LCD_printString("+", 44, 55, C_WHITE, 2);
    draw_shape(SH_CIRCLE,   72, 62, 22, C_BLUE);
    LCD_printString("=", 88, 55, C_WHITE, 2);
    snprintf(buf, sizeof(buf), "%u", alge_A);
    LCD_printString(buf, 106, 55, C_YELLOW, 2);

    /* Row 2: [TRI] - [CIRC] = B  (shapes centred at y=102) */
    draw_shape(SH_TRIANGLE, 28, 102, 22, C_GREEN);
    LCD_printString("-", 44, 95, C_WHITE, 2);
    draw_shape(SH_CIRCLE,   72, 102, 22, C_BLUE);
    LCD_printString("=", 88, 95, C_WHITE, 2);
    snprintf(buf, sizeof(buf), "%u", alge_B);
    LCD_printString(buf, 106, 95, C_YELLOW, 2);

    LCD_Draw_Line(10, 122, 200, 122, C_GREY);

    /* Row 3: [SQ] + [TRI] = ?  (shapes centred at y=142) */
    draw_shape(SH_SQUARE,   28, 142, 22, C_ORANGE);
    LCD_printString("+", 44, 135, C_WHITE, 2);
    draw_shape(SH_TRIANGLE, 72, 142, 22, C_GREEN);
    LCD_printString("= ?", 88, 135, C_YELLOW, 2);

    /* Choices: boxes at y=162, height=40 */
    static const uint16_t CX[3] = {40, 120, 200};
    for (int i = 0; i < 3; i++) {
        uint8_t hi = (i == q_cursor);
        LCD_Draw_Rect(CX[i] - 26, 162, 52, 40, hi ? C_YELLOW : C_GREY, 0);
        snprintf(buf, sizeof(buf), "%d", ans_vals[i]);
        LCD_printString(buf, CX[i] - 6, 173, hi ? C_YELLOW : C_WHITE, 2);
    }
    LCD_printString("< >  SELECT=OK", 42, 210, C_GREY, 1);
    LCD_Refresh(&lcd_cfg);
}

/* ── Level 3: Find Shape ── */
static void gen_find(void)
{
    tgt_col = rng8(3);
    tgt_sz  = rng8(2);
    uint8_t tgt_sh = rng8(3);

    for (int r = 0; r < GRID_N; r++)
        for (int c = 0; c < GRID_N; c++) {
            grid_sh[r][c]  = rng8(3);
            grid_col[r][c] = rng8(3);
            grid_sz[r][c]  = rng8(2);
            if (grid_col[r][c] == tgt_col && grid_sz[r][c] == tgt_sz)
                grid_sz[r][c] = 1 - tgt_sz;
        }
    tgt_r = rng8(GRID_N); tgt_c = rng8(GRID_N);
    grid_sh[tgt_r][tgt_c]  = tgt_sh;
    grid_col[tgt_r][tgt_c] = tgt_col;
    grid_sz[tgt_r][tgt_c]  = tgt_sz;

    cur_r = 0; cur_c = 0;
    q_correct = 255;
    q_cursor  = 0;
}

static void draw_find(void)
{
    LCD_Fill_Buffer(C_BLACK);
    draw_header("SHAPES");

    char buf[32];
    snprintf(buf, sizeof(buf), "Find %s %s shape:",
             GF_SZ_NAME[tgt_sz], GF_COL_NAME[tgt_col]);
    LCD_printString(buf, 2, 36, C_CYAN, 1);

    for (int r = 0; r < GRID_N; r++)
        for (int c = 0; c < GRID_N; c++) {
            uint16_t x = GX0 + c * GCW;
            uint16_t y = GY0 + r * GCH;
            uint8_t  hi = (r == cur_r && c == cur_c);
            LCD_Draw_Rect(x + 1, y + 1, GCW - 2, GCH - 2, hi ? C_YELLOW : C_GREY, 0);
            LCD_Draw_Rect(x + 2, y + 2, GCW - 4, GCH - 4, hi ? C_NAVY : C_BLACK, 1);
            /* Large shape = 26px, small = 14px */
            uint8_t sz = grid_sz[r][c] ? 26 : 14;
            draw_shape(grid_sh[r][c], x + GCW / 2, y + GCH / 2, sz, GF_COL[grid_col[r][c]]);
        }
    LCD_Refresh(&lcd_cfg);
}

/* ── Question dispatch ── */
static void new_question(void)
{
    q_type = rng8(4);
    switch (q_type) {
    case 0: gen_speed(); break;
    case 1: gen_math();  break;
    case 2: gen_alge();  break;
    case 3: gen_find();  break;
    }
}

static void draw_question(void)
{
    switch (q_type) {
    case 0: draw_speed_q(); break;
    case 1: draw_math();    break;
    case 2: draw_alge();    break;
    case 3: draw_find();    break;
    }
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

/* ── Game Over screen ── */
static void draw_gameover(void)
{
    LCD_Fill_Buffer(C_BLACK);
    draw_header("SHAPES");
    LCD_Draw_Rect(20, 80, 200, 110, C_RED, 1);
    LCD_printString("GAME OVER", 40, 100, C_WHITE, 2);
    char buf[20];
    snprintf(buf, sizeof(buf), "Score: %u", score);
    LCD_printString(buf, 55, 145, C_YELLOW, 2);
    LCD_Refresh(&lcd_cfg);
}

/* ── Main entry point ── */
MenuState Game1_Run(void)
{
    rng_seed  = HAL_GetTick() + 1;
    score     = 0;
    game_over = 0;
    time_left = Menu_GetTimeLimit();
    max_time  = time_left;

    g_tim7_ticks = 0;
    HAL_TIM_Base_Start_IT(&htim7);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    SevenSeg_SetNumber(time_left);

    buzzer_tone(&buzzer_cfg, 523, 40); HAL_Delay(200); buzzer_off(&buzzer_cfg);

    new_question();
    if (q_type == 0) run_speed_animation();
    draw_question();

    Direction last_dir = CENTRE;

    while (1) {
        uint32_t t0 = HAL_GetTick();

        /* Countdown tick */
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

            if (q_type == 3) {
                if ((dir==N||dir==NW||dir==NE) && cur_r > 0)         cur_r--;
                if ((dir==S||dir==SW||dir==SE) && cur_r < GRID_N-1)  cur_r++;
                if ((dir==W||dir==NW||dir==SW) && cur_c > 0)         cur_c--;
                if ((dir==E||dir==NE||dir==SE) && cur_c < GRID_N-1)  cur_c++;
            } else {
                if ((dir==W||dir==NW||dir==SW) && q_cursor > 0) q_cursor--;
                if ((dir==E||dir==NE||dir==SE) && q_cursor < 2) q_cursor++;
            }
            draw_question();
        }
        last_dir = dir;

        if (!game_over && current_input.btn_select_pressed) {
            uint8_t correct;
            if (q_type == 3) {
                correct = (cur_r == tgt_r && cur_c == tgt_c);
            } else {
                correct = (q_cursor == q_correct);
            }

            if (correct) {
                score++;
                snd_correct();
                HAL_Delay(300);
                new_question();
                if (q_type == 0) run_speed_animation();
                draw_question();
            } else {
                if (time_left > WRONG_PENALTY) time_left -= WRONG_PENALTY;
                else time_left = 0;
                SevenSeg_SetNumber(time_left);
                snd_wrong();
                HAL_Delay(300);
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
