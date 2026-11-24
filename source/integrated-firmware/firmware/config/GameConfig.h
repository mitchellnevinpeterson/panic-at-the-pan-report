#ifndef PANIC_GAME_CONFIG_H
#define PANIC_GAME_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* Agreed timings from the build brief, in milliseconds unless noted. */
#define INGREDIENT_COUNT 5u
#define GAME_LENGTH_S 60u
#define FINISH_ENABLE_S 30u
#define GAME_TICK_MS 1000u
#define WELCOME_SCROLL_MS 25u /* 2x the original 50 ms per column. */
#define WELCOME_TEXT "Welcome to Panic at the Pan"
#define DISH_TEXT "YOUR DISH TODAY: chicken fried rice"
/* GameButtons: each game's dish is picked at random, never the same twice in
 * a row (BenchIO.c). Recipe bit i set = ingredient i is right: 0 Protein,
 * 1 Carb, 2 Vegetables, 3 Spice, 4 Oil. */
#define DISH_COUNT 3
#define DISH_TEXTS { DISH_TEXT, \
                    "YOUR DISH TODAY: chicken and broccoli stir-fry", \
                    "YOUR DISH TODAY: pasta aglio e olio" }
#define DISH_RECIPES { 0x1Bu,  /* fried rice: Protein, Carb, Spice, Oil; Vegetables wrong */ \
                      0x1Du,  /* stir-fry: Protein, Vegetables, Spice, Oil; Carb wrong */ \
                      0x1Au } /* aglio e olio: Carb, Spice, Oil; Protein, Vegetables wrong */
/* GameButtons end sequence: message on the top strip, then the big score. */
#define END_TIMEUP_TEXT "TIME'S UP. GAME OVER"
#define END_FINISH_TEXT "GAME OVER"
#define END_SCORE_MS 3000u
/* Every scrolled text uses the same 8-pixel character advance and step. */
#define DISH_INTRO_MS ((32u + (sizeof DISH_TEXT - 1u) * 8u) * WELCOME_SCROLL_MS)
#define GAME_INPUT_MAX_AGE_MS 1000u /* Local event backlog limit; UART2 pending. */
#define IDLE_TIMEOUT_MS 20000u
/* Joystick moves renew the idle timer. A "move" is a push (>= 150 counts from
 * rest for 100 ms straight) or a completed stir circle (Stirrer.c), so a resting
 * stick's noise and glitches never count. Set to 0 to take the joystick off the
 * activity list. */
#ifndef JOYSTICK_IS_ACTIVITY
#define JOYSTICK_IS_ACTIVITY 1
#endif
#define RESULT_DISPLAY_MS 10000u
#define TIMEOUT_DISPLAY_MS 5000u
#define HEAT_SAMPLE_MS 100u
#define BURN_TIME_MS 3000u
#define STIR_TICK_MS 50u
#define ICON_DISPLAY_MS 1500u
#define BURN_FACTOR 1.5f

/* No guessed tuning values. Zero-filled storage is invalid until configured.
 * Consumers must require configured == true before using these values.
 * In particular, heat targets and optimal stir durations must be positive.
 */
/* Select explicitly after the team approves the stir scoring curve. */
typedef enum {
    GAME_STIR_SCORE_UNCONFIGURED = 0,
    GAME_STIR_SCORE_LINEAR_RELATIVE_ERROR
} GameStirScoreRule_t;

typedef enum {
    GAME_BURN_TRANSFER_UNCONFIGURED = 0,
    GAME_BURN_TRANSFER_RESET,
    GAME_BURN_TRANSFER_PRESERVE
} GameBurnTransferRule_t;

typedef struct {
    bool configured;
    uint16_t distance_min_mm;
    uint16_t distance_max_mm;
    uint16_t heat_target[INGREDIENT_COUNT];
    uint16_t optimal_stir_ms[INGREDIENT_COUNT];
    /* Maximum gap between fresh distance readings; a gap breaks burn timing. */
    uint16_t heat_sample_max_age_ms;
    GameBurnTransferRule_t burn_transfer_rule;
    /* Motion timeout / maximum credited interval, not a minimum hold time. */
    uint16_t stir_debounce_ms;
    uint32_t stir_motion_threshold_mdeg;
    uint32_t stir_home_angle_mdeg;
    GameStirScoreRule_t stir_score_rule;
    uint16_t animation_period_ms;
    uint8_t recipe_mask;
    int32_t points_right;
    int32_t penalty_wrong;
    int32_t penalty_burn;
    float heat_weight; /* Points per second at perfect heat; elapsed-time integrated. */
    float stir_weight; /* Maximum points for one ingredient's completed stir result. */
    bool allow_negative_score;
} GameTuning_t;

extern const GameTuning_t GameTuning;

/* Header-only so the unchanged ingredient-only bench needs no extra link unit.
 * There are no fallback game values. Geometry/control calibration remains in
 * the hardware adapter; these fields describe normalized gameplay inputs.
 */
static inline bool GameTuning_IsValid(const GameTuning_t *t)
{
    if (!t || !t->configured || t->distance_min_mm >= t->distance_max_mm ||
        !t->heat_sample_max_age_ms ||
        (t->burn_transfer_rule != GAME_BURN_TRANSFER_RESET &&
         t->burn_transfer_rule != GAME_BURN_TRANSFER_PRESERVE) || !t->stir_debounce_ms ||
        !t->stir_motion_threshold_mdeg || t->stir_motion_threshold_mdeg > 180000u ||
        t->stir_home_angle_mdeg >= 360000u ||
        t->stir_score_rule != GAME_STIR_SCORE_LINEAR_RELATIVE_ERROR ||
        !t->recipe_mask || (t->recipe_mask & ~((1u << INGREDIENT_COUNT) - 1u)) ||
        t->points_right < 0 || t->penalty_wrong < 0 || t->penalty_burn < 0 ||
        !isfinite(t->heat_weight) || t->heat_weight < 0 ||
        !isfinite(t->stir_weight) || t->stir_weight < 0) return false;
    for (unsigned i = 0; i < INGREDIENT_COUNT; ++i)
        if (!t->heat_target[i] || t->heat_target[i] > 1000u || !t->optimal_stir_ms[i])
            return false;
    return true;
}

#if FINISH_ENABLE_S < 30 || GAME_LENGTH_S < FINISH_ENABLE_S
#error "Game timing violates the project minimum duration."
#endif
#if RESULT_DISPLAY_MS > 30000
#error "Result display exceeds the course limit."
#endif

#endif
