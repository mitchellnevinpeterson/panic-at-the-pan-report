/* Pan matrix icons and animations. Scaffold only; see docs/architecture.md. */
#ifndef PANIC_PANDISPLAYSM_H
#define PANIC_PANDISPLAYSM_H

#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"

typedef enum {
    PAN_ATTRACT,
    PAN_NEW_INGREDIENT,
    PAN_VIEW,
    PAN_SWIRL,
    PAN_GAME_OVER_TEXT,
    PAN_TIMEOUT_ICON
} PanDisplaySMState_t;

bool InitPanDisplaySM(uint8_t priority);
bool PostPanDisplaySM(ES_Event_t event);
ES_Event_t RunPanDisplaySM(ES_Event_t event);
PanDisplaySMState_t QueryPanDisplaySM(void);

#endif
