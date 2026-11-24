/* Accepted-ingredient mask. Scaffold only; see docs/architecture.md. */
#ifndef PANIC_INGREDIENTSM_H
#define PANIC_INGREDIENTSM_H

#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"

typedef enum {
    INGREDIENT_READY,
    INGREDIENT_LOCKED
} IngredientSMState_t;

bool InitIngredientSM(uint8_t priority);
bool PostIngredientSM(ES_Event_t event);
ES_Event_t RunIngredientSM(ES_Event_t event);
IngredientSMState_t QueryIngredientSM(void);

#endif
