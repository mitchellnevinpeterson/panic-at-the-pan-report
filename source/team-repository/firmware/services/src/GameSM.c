#include "GameSM.h"
#include "ES_Framework.h"

static uint8_t service_priority;
static GameSMState_t current_state = GAME_WELCOME;

bool InitGameSM(uint8_t priority)
{
    service_priority = priority;
    current_state = GAME_WELCOME;
    return PostGameSM((ES_Event_t){ ES_INIT, 0 });
}

bool PostGameSM(ES_Event_t event)
{
    return ES_PostToService(service_priority, event);
}

ES_Event_t RunGameSM(ES_Event_t event)
{
    if (event.EventType == ES_INIT) {
        current_state = GAME_WELCOME;
        return (ES_Event_t){ ES_NO_EVENT, 0 };
    }
    /* TODO(team): implement transitions/actions from panic_at_the_pan_fsm.md.
     * Report unsupported work instead of silently pretending the game runs.
     * The framework treats this non-empty return as FailedRun.
     */
    return (ES_Event_t){ ES_ERROR, (uint16_t)event.EventType };
}

GameSMState_t QueryGameSM(void)
{
    return current_state;
}
