/**
 * @file Game_3.c
 * @brief Word Search Game
 *
 * Screen: 240x240 pixels
 * Difficulty sets both grid size and word length:
 *   Easy   -> 5×5 grid, 3-letter words, horizontal only
 *   Medium -> 6×6 grid, 4-letter words, horizontal + vertical
 *   Hard   -> 7×7 grid, 5-letter words, all 8 directions
 *
 * Player flow:
 *   1. Navigate cursor to the START letter of the word, press SELECT.
 *   2. Navigate cursor to the END letter, press SELECT.
 *   3. If the cells between start and end spell the hidden word -> correct!
 *   Press RESET in phase 2 to cancel and reselect start.
 *   Wrong selection: -5 seconds.
 */

#include "Game_3.h"
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

#define WRONG_PENALTY 5
#define FRAME_MS      100
#define MAX_ROWS      7
#define MAX_COLS      7
#define MAX_WLEN      5

/* ── Word lists ── */
static const char WORDS_3[][4] = {
    "CAT","DOG","SUN","HAT","RUN",
    "BIG","TOP","FUN","MAP","CAR",
    "FLY","SKY","CUP","ACE","BOX"
};
#define N_WORDS_3 15

static const char WORDS_4[][5] = {
    "GAME","PLAY","STAR","FIRE","WIND",
    "ROCK","TREE","BIRD","FISH","JUMP",
    "MOON","ROAD","WAVE","RAIN","FROG"
};
#define N_WORDS_4 15

static const char WORDS_5[][6] = {
    "LIGHT","HEART","BRAVE","CLOUD","FLAME",
    "RIVER","STORM","TIGER","SPACE","MUSIC",
    "BLOOM","SWORD","GRAIN","FROST","STONE"
};
#define N_WORDS_5 15

/* ── Grid config per difficulty ── */
typedef struct {
    uint8_t rows, cols;
    uint8_t wlen;
    uint8_t allow_vert;
    uint8_t allow_diag;
} GridCfg;

static const GridCfg CFG[3] = {
    {5, 5, 3, 0, 0},  /* Easy:   H only */
    {6, 6, 4, 1, 0},  /* Medium: H + V */
    {7, 7, 5, 1, 1},  /* Hard:   all 8 dirs */
};

/* ── Game state ── */
static char    grid[MAX_ROWS][MAX_COLS];
static uint8_t grid_rows, grid_cols;
static uint8_t word_wlen;
static char    target_word[MAX_WLEN + 1];

static int8_t  word_r0, word_c0;
static int8_t  word_dr, word_dc;

static uint8_t cur_r, cur_c;
static uint8_t sel_r, sel_c;
static uint8_t phase;

static uint8_t score, time_left, game_over, max_time;

/* ── LCG RNG ── */
static uint32_t rng_seed;
static uint8_t rng8(uint8_t max) {
    rng_seed = rng_seed * 1664525UL + 1013904223UL;
    return (uint8_t)((rng_seed >> 16) % max);
}

/* ── Place word in grid ── */
static uint8_t try_place(uint8_t r0, uint8_t c0, int8_t dr, int8_t dc, uint8_t wlen)
{
    for (uint8_t i = 0; i < wlen; i++) {
        int16_t r = r0 + dr * i;
        int16_t c = c0 + dc * i;
        if (r < 0 || r >= grid_rows || c < 0 || c >= grid_cols) return 0;
    }
    return 1;
}

static void place_word(const GridCfg *cfg)
{
    int8_t dirs[8][2] = {
        {0,1},{0,-1},{1,0},{-1,0},
        {1,1},{1,-1},{-1,1},{-1,-1}
    };
    uint8_t ndir = 2;
    if (cfg->allow_vert) ndir = 4;
    if (cfg->allow_diag) ndir = 8;

    uint8_t placed = 0;
    for (uint8_t attempt = 0; attempt < 50 && !placed; attempt++) {
        uint8_t di = rng8(ndir);
        int8_t  dr = dirs[di][0];
        int8_t  dc = dirs[di][1];
        uint8_t r0 = rng8(cfg->rows);
        uint8_t c0 = rng8(cfg->cols);
        if (try_place(r0, c0, dr, dc, cfg->wlen)) {
            word_r0 = r0; word_c0 = c0;
            word_dr = dr; word_dc = dc;
            for (uint8_t i = 0; i < cfg->wlen; i++)
                grid[r0 + dr * i][c0 + dc * i] = target_word[i];
            placed = 1;
        }
    }
}

static void new_round(const GridCfg *cfg)
{
    grid_rows = cfg->rows;
    grid_cols = cfg->cols;
    word_wlen = cfg->wlen;

    if (cfg->wlen == 3) {
        uint8_t idx = rng8(N_WORDS_3);
        strncpy(target_word, WORDS_3[idx], sizeof(target_word) - 1);
    } else if (cfg->wlen == 4) {
        uint8_t idx = rng8(N_WORDS_4);
        strncpy(target_word, WORDS_4[idx], sizeof(target_word) - 1);
    } else {
        uint8_t idx = rng8(N_WORDS_5);
        strncpy(target_word, WORDS_5[idx], sizeof(target_word) - 1);
    }
    target_word[cfg->wlen] = '\0';

    for (uint8_t r = 0; r < grid_rows; r++)
        for (uint8_t c = 0; c < grid_cols; c++)
            grid[r][c] = 'A' + rng8(26);

    place_word(cfg);

    cur_r = 0; cur_c = 0;
    sel_r = 0; sel_c = 0;
    phase = 0;
}

/* ── Draw helpers ── */
static void draw_header(void)
{
    char buf[20];
    LCD_Draw_Rect(0, 0, 240, 28, C_NAVY, 1);
    LCD_printString("WORD SRCH", 48, 6, C_GREEN, 2);
    snprintf(buf, sizeof(buf), "Sc:%u", score);
    LCD_printString(buf, 4, 9, C_WHITE, 1);
    snprintf(buf, sizeof(buf), "T:%us", time_left);
    LCD_printString(buf, 190, 9, time_left <= 10 ? C_RED : C_WHITE, 1);
    uint8_t bar = (max_time > 0) ? (uint8_t)((time_left * 200U) / max_time) : 0;
    LCD_Draw_Rect(0, 28, 240, 4, C_GREY, 1);
    if (bar > 0) LCD_Draw_Rect(0, 28, bar, 4, time_left <= 10 ? C_RED : C_GREEN, 1);
}

static void draw_screen(void)
{
    LCD_Fill_Buffer(C_BLACK);
    draw_header();

    /* Word hint -- "Find: WORD" on one line using font 2 */
    char hint[20];
    snprintf(hint, sizeof(hint), "Find: %s", target_word);
    LCD_printString(hint, 4, 34, C_YELLOW, 2);   /* font 2 = 14px tall -> y+14=48 */

    /* Phase instruction */
    if (phase == 0)
        LCD_printString("SELECT=mark start", 4, 50, C_CYAN, 1);
    else
        LCD_printString("SELECT=end  RESET=undo", 4, 50, C_ORANGE, 1);

    /*
     * Grid layout -- fits within 240x240:
     *   grid_top = 62
     *   cw = 240 / cols  (exact integer)
     *   ch = (240 - 62) / rows  (leaves bottom margin)
     */
    uint8_t cw = (uint8_t)(240 / grid_cols);
    uint8_t ch = (uint8_t)((240 - 62) / grid_rows);
    /* Keep cells square-ish: use smaller dimension */
    if (ch > cw) ch = cw;
    uint16_t grid_top = 62;

    for (uint8_t r = 0; r < grid_rows; r++) {
        for (uint8_t c = 0; c < grid_cols; c++) {
            uint16_t x = (uint16_t)(c * cw);
            uint16_t y = grid_top + (uint16_t)(r * ch);

            uint8_t is_cur = (r == cur_r && c == cur_c);
            uint8_t is_sel = (phase == 1 && r == sel_r && c == sel_c);

            /* Cell background */
            if (is_cur)
                LCD_Draw_Rect(x + 1, y + 1, cw - 2, ch - 2, C_NAVY, 1);
            else if (is_sel)
                LCD_Draw_Rect(x + 1, y + 1, cw - 2, ch - 2, C_GREEN, 1);
            else
                LCD_Draw_Rect(x + 1, y + 1, cw - 2, ch - 2, C_BLACK, 1);

            /* Cell border */
            uint8_t border_col = is_cur ? C_YELLOW : (is_sel ? C_CYAN : C_GREY);
            LCD_Draw_Rect(x, y, cw, ch, border_col, 0);

            /* Letter -- centred in cell */
            char letter[2] = {grid[r][c], '\0'};
            uint8_t lcol = is_cur ? C_WHITE : (is_sel ? C_BLACK : C_CYAN);
            uint16_t lx = x + (cw > 12 ? (cw - 10) / 2 : 1);
            uint16_t ly = y + (ch > 16 ? (ch - 14) / 2 : 1);
            LCD_printString(letter, lx, ly, lcol, 2);
        }
    }

    LCD_Refresh(&lcd_cfg);
}

/* ── Validate player selection ── */
static uint8_t validate_selection(uint8_t er, uint8_t ec)
{
    int8_t dr = (int8_t)er - (int8_t)sel_r;
    int8_t dc = (int8_t)ec - (int8_t)sel_c;

    int8_t step_r = (dr == 0) ? 0 : (dr > 0 ? 1 : -1);
    int8_t step_c = (dc == 0) ? 0 : (dc > 0 ? 1 : -1);

    uint8_t len;
    if (dr == 0)
        len = (uint8_t)(dc < 0 ? -dc : dc) + 1;
    else if (dc == 0)
        len = (uint8_t)(dr < 0 ? -dr : dr) + 1;
    else {
        uint8_t adr = (uint8_t)(dr < 0 ? -dr : dr);
        uint8_t adc = (uint8_t)(dc < 0 ? -dc : dc);
        if (adr != adc) return 0;
        len = adr + 1;
    }
    if (len != word_wlen) return 0;

    if ((int8_t)sel_r != word_r0 || (int8_t)sel_c != word_c0) return 0;
    if (step_r != word_dr || step_c != word_dc) return 0;

    return 1;
}

/* ── Feedback screens ── */
static void draw_correct(void)
{
    LCD_Fill_Buffer(C_BLACK);
    draw_header();
    LCD_Draw_Rect(20, 90, 200, 80, C_GREEN, 1);
    LCD_printString("CORRECT!", 50, 116, C_BLACK, 2);
    LCD_Refresh(&lcd_cfg);
}

static void draw_wrong(void)
{
    LCD_Fill_Buffer(C_BLACK);
    draw_header();
    LCD_Draw_Rect(20, 90, 200, 80, C_RED, 1);
    LCD_printString("WRONG! -5s", 28, 116, C_WHITE, 2);
    LCD_Refresh(&lcd_cfg);
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

/* ── Main entry point ── */
MenuState Game3_Run(void)
{
    rng_seed  = HAL_GetTick() + 7;
    score     = 0;
    game_over = 0;
    time_left = Menu_GetTimeLimit();
    max_time  = time_left;

    const GridCfg *cfg;
    switch (g_difficulty) {
    case DIFFICULTY_EASY:   cfg = &CFG[0]; break;
    case DIFFICULTY_MEDIUM: cfg = &CFG[1]; break;
    default:                cfg = &CFG[2]; break;
    }

    g_tim7_ticks = 0;
    HAL_TIM_Base_Start_IT(&htim7);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    SevenSeg_SetNumber(time_left);

    buzzer_tone(&buzzer_cfg, 784, 40); HAL_Delay(200); buzzer_off(&buzzer_cfg);

    new_round(cfg);
    draw_screen();

    Direction last_dir = CENTRE;

    while (1) {
        uint32_t t0 = HAL_GetTick();

        /* Countdown */
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

        if (current_input.btn_reset_pressed) {
            if (phase == 1) {
                phase = 0;
                draw_screen();
            } else {
                break;
            }
        }

        if (!game_over && dir != last_dir && dir != CENTRE) {
            buzzer_tone(&buzzer_cfg, 800, 8); HAL_Delay(12); buzzer_off(&buzzer_cfg);

            if ((dir==N||dir==NW||dir==NE) && cur_r > 0)            cur_r--;
            if ((dir==S||dir==SW||dir==SE) && cur_r < grid_rows-1)  cur_r++;
            if ((dir==W||dir==NW||dir==SW) && cur_c > 0)            cur_c--;
            if ((dir==E||dir==NE||dir==SE) && cur_c < grid_cols-1)  cur_c++;
            draw_screen();
        }
        last_dir = dir;

        if (!game_over && current_input.btn_select_pressed) {
            if (phase == 0) {
                sel_r = cur_r;
                sel_c = cur_c;
                phase = 1;
                draw_screen();
            } else {
                uint8_t ok = validate_selection(cur_r, cur_c);
                if (ok) {
                    score++;
                    draw_correct();
                    snd_correct();
                    HAL_Delay(800);
                    new_round(cfg);
                    draw_screen();
                } else {
                    if (time_left > WRONG_PENALTY) time_left -= WRONG_PENALTY;
                    else time_left = 0;
                    SevenSeg_SetNumber(time_left);
                    draw_wrong();
                    snd_wrong();
                    HAL_Delay(600);
                    phase = 0;
                    draw_screen();
                }
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
