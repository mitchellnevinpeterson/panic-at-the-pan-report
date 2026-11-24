#ifndef PANIC_GAME_CONFIG_H
#define PANIC_GAME_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/* Agreed timings from the build brief, in milliseconds unless noted. */
#define INGREDIENT_COUNT 5u
#define GAME_LENGTH_S 60u
#define FINISH_ENABLE_S 30u
#define GAME_TICK_MS 1000u
#define IDLE_TIMEOUT_MS 20000u
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
typedef struct {
    bool configured;
    uint16_t distance_min_mm;
    uint16_t distance_max_mm;
    uint16_t heat_target[INGREDIENT_COUNT];
    uint16_t optimal_stir_ms[INGREDIENT_COUNT];
    uint16_t stir_debounce_ms;
    uint16_t animation_period_ms;
    uint8_t recipe_mask;
    int32_t points_right;
    int32_t penalty_wrong;
    int32_t penalty_burn;
    float heat_weight;
    float stir_weight;
    bool allow_negative_score;
} GameTuning_t;

extern const GameTuning_t GameTuning;

#if FINISH_ENABLE_S < 30 || GAME_LENGTH_S < FINISH_ENABLE_S
#error "Game timing violates the project minimum duration."
#endif
#if RESULT_DISPLAY_MS > 30000
#error "Result display exceeds the course limit."
#endif

#endif
