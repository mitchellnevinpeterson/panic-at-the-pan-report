/* Once-per-round ingredient acceptance; bind a foreground port before init. */
#ifndef PANIC_INGREDIENTSM_H
#define PANIC_INGREDIENTSM_H

#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"
#include "ES_Configure.h"
#include "ES_Types.h"

typedef enum {
    INGREDIENT_READY,
    INGREDIENT_LOCKED,
    INGREDIENT_FAULT
} IngredientSMState_t;

typedef enum {
    INGREDIENT_ACTION_STATE,     /* value: IngredientSMState_t */
    INGREDIENT_ACTION_ADDED,     /* publish INGREDIENT_ADDED(value), once */
    INGREDIENT_ACTION_DUPLICATE,
    INGREDIENT_ACTION_LOCKED,
    INGREDIENT_ACTION_STALE
} IngredientSMAction_t;

typedef struct {
    uint32_t (*generation)(void); /* authoritative GameSM generation */
    bool (*accept_input)(uint32_t detected_ms, uint32_t generation);
    bool (*output)(IngredientSMAction_t action, uint8_t value);
} IngredientSMPort_t;

bool IngredientSM_BindPort(const IngredientSMPort_t *port);
bool IngredientSM_HasFault(void);
uint8_t IngredientSM_AddedMask(void); /* bit n: ingredient n, zero-based */
uint32_t IngredientSM_LastAcceptedAt(void); /* capture time of latest ADDED */
/* Foreground-only. Input adapters also post USER_ACTIVITY to GameSM, even
 * for duplicate ingredients. Use these owned-ticket APIs rather than raw
 * SWITCH_FLIPPED / lifecycle posts; no pointer goes in the 16-bit EventParam.
 * Lifecycle accepts GAME_START, GAME_END or GAME_RESET after service init.
 */
bool IngredientSM_PostLifecycle(ES_EventType_t type, uint32_t generation);
bool IngredientSM_PostSwitch(uint8_t ingredient, uint32_t detected_ms,
                             uint32_t generation);

bool InitIngredientSM(uint8_t priority);
bool PostIngredientSM(ES_Event_t event);
ES_Event_t RunIngredientSM(ES_Event_t event);
IngredientSMState_t QueryIngredientSM(void);

#endif
