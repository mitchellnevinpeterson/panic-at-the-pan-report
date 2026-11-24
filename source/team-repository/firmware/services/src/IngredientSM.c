#include "IngredientSM.h"
#include "ES_Framework.h"

static uint8_t service_priority;
static IngredientSMState_t current_state = INGREDIENT_READY;

bool InitIngredientSM(uint8_t priority)
{
    service_priority = priority;
    current_state = INGREDIENT_READY;
    return PostIngredientSM((ES_Event_t){ ES_INIT, 0 });
}

bool PostIngredientSM(ES_Event_t event)
{
    return ES_PostToService(service_priority, event);
}

ES_Event_t RunIngredientSM(ES_Event_t event)
{
    if (event.EventType == ES_INIT) {
        current_state = INGREDIENT_READY;
        return (ES_Event_t){ ES_NO_EVENT, 0 };
    }
    /* TODO(team): implement transitions/actions from panic_at_the_pan_fsm.md.
     * Report unsupported work instead of silently pretending the game runs.
     * The framework treats this non-empty return as FailedRun.
     */
    return (ES_Event_t){ ES_ERROR, (uint16_t)event.EventType };
}

IngredientSMState_t QueryIngredientSM(void)
{
    return current_state;
}
