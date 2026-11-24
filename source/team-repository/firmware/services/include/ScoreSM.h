/* Score value and score panel. Scaffold only; see docs/architecture.md. */
#ifndef PANIC_SCORESM_H
#define PANIC_SCORESM_H

#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"

typedef enum {
    SCORE_IDLE,
    SCORE_ACTIVE
} ScoreSMState_t;

bool InitScoreSM(uint8_t priority);
bool PostScoreSM(ES_Event_t event);
ES_Event_t RunScoreSM(ES_Event_t event);
ScoreSMState_t QueryScoreSM(void);

#endif
