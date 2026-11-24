/* Sole score owner. Four-field port remains compatible with the +10 bench. */
#ifndef PANIC_SCORESM_H
#define PANIC_SCORESM_H

#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"
#include "GameConfig.h"
#include "ES_Configure.h"
#include "ES_Types.h"

typedef enum {
    SCORE_IDLE,
    SCORE_ACTIVE,
    SCORE_FAULT
} ScoreSMState_t;

typedef struct {
    uint32_t (*generation)(void); /* Current lifecycle generation for Reset. */
    bool (*accepts_round)(uint32_t round_generation); /* Playing or results. */
    bool (*display)(int32_t score); /* Foreground, bounded, preserves timer. */
    int32_t ingredient_points; /* Explicit bench tuning; not a recipe rule. */
    /* Optional final-game extensions; zero keeps the old bench behaviour.
     * recipe_mask bit i set: ingredient i is right (+ingredient_points);
     * clear: wrong (-ingredient_points). 0 = every ingredient is +points.
     * floor_zero: the score never goes below 0. */
    uint8_t recipe_mask;
    bool floor_zero;
} ScoreSMPort_t;

bool ScoreSM_BindPort(const ScoreSMPort_t *port);
/* Optional before Init: selects formal mode (configured=false keeps score zero).
 * No BindTuning call retains explicit ingredient_points-only bench behavior.
 * Full-game wiring MUST call this; invalid configured=true tuning is rejected.
 */
bool ScoreSM_BindTuning(const GameTuning_t *tuning);
bool ScoreSM_IsConfigured(void);
bool ScoreSM_HasFault(void);
int32_t ScoreSM_Value(void);
/* Owned foreground messages; raw ES EventParam is not the ingredient ID.
 * Only accepted IngredientSM output may call PostIngredient. Keep its earned
 * round generation, even if GameSM ends before ScoreSM processes this event.
 */
bool ScoreSM_PostLifecycle(ES_EventType_t type, uint32_t generation); /* Start/Reset */
bool ScoreSM_PostIngredient(uint8_t ingredient, uint32_t round_generation);

/* Final game: signed change in thousandths of a point (heat, burn, stir, flip)
 * for an earned round. The displayed/returned score is whole points
 * (total millipoints / 1000, rounded toward zero), floored per floor_zero. */
bool ScoreSM_PostDelta(uint32_t round_generation, int32_t millipoints);

/* Final game, several dishes: recipe for the NEXT round (same bit meaning as
 * ScoreSMPort_t.recipe_mask). Latched at that round's GAME_START, so a round
 * never changes recipe midway. False for bits beyond the five ingredients. */
bool ScoreSM_SetRecipeMask(uint8_t recipe_mask);

bool ScoreSM_PostHeat(uint8_t ingredient, uint32_t round, uint16_t closeness, uint32_t elapsed_ms);
bool ScoreSM_PostBurn(uint8_t ingredient, uint32_t round);
bool ScoreSM_PostStir(uint8_t ingredient, uint32_t round, uint32_t elapsed_ms);

bool InitScoreSM(uint8_t priority);
bool PostScoreSM(ES_Event_t event);
ES_Event_t RunScoreSM(ES_Event_t event);
ScoreSMState_t QueryScoreSM(void);

#endif
