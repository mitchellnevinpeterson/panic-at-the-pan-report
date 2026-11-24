/* Heat sampling, heat LEDs, sizzle, fire lift. Scaffold only; see docs/architecture.md. */
#ifndef PANIC_HEATSM_H
#define PANIC_HEATSM_H

#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"

typedef enum {
    HEAT_PAN_EMPTY,
    HEAT_COOKING,
    HEAT_OVERHEATING,
    HEAT_BURNING
} HeatSMState_t;

bool InitHeatSM(uint8_t priority);
bool PostHeatSM(ES_Event_t event);
ES_Event_t RunHeatSM(ES_Event_t event);
HeatSMState_t QueryHeatSM(void);

#endif
