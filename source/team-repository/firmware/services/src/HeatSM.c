#include "HeatSM.h"
#include "ES_Framework.h"

static uint8_t service_priority;
static HeatSMState_t current_state = HEAT_PAN_EMPTY;

bool InitHeatSM(uint8_t priority)
{
    service_priority = priority;
    current_state = HEAT_PAN_EMPTY;
    return PostHeatSM((ES_Event_t){ ES_INIT, 0 });
}

bool PostHeatSM(ES_Event_t event)
{
    return ES_PostToService(service_priority, event);
}

ES_Event_t RunHeatSM(ES_Event_t event)
{
    if (event.EventType == ES_INIT) {
        current_state = HEAT_PAN_EMPTY;
        return (ES_Event_t){ ES_NO_EVENT, 0 };
    }
    /* TODO(team): implement transitions/actions from panic_at_the_pan_fsm.md.
     * Report unsupported work instead of silently pretending the game runs.
     * The framework treats this non-empty return as FailedRun.
     */
    return (ES_Event_t){ ES_ERROR, (uint16_t)event.EventType };
}

HeatSMState_t QueryHeatSM(void)
{
    return current_state;
}
