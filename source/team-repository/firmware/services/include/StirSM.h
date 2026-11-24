/* Per-ingredient stir time and circular spoon direction. Scaffold only; see docs/architecture.md. */
#ifndef PANIC_STIRSM_H
#define PANIC_STIRSM_H

#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"

typedef enum {
    STIR_IDLE,
    STIR_NOT_STIRRING,
    STIR_STIRRING
} StirSMState_t;

bool InitStirSM(uint8_t priority);
bool PostStirSM(ES_Event_t event);
ES_Event_t RunStirSM(ES_Event_t event);
StirSMState_t QueryStirSM(void);

#endif
