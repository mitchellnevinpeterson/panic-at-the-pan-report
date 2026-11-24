/* Game clock, idle/result timers, timer panel, Finish LED, dispenser. Scaffold only; see docs/architecture.md. */
#ifndef PANIC_GAMESM_H
#define PANIC_GAMESM_H

#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"

typedef enum {
    GAME_WELCOME,
    GAME_COOKING_EARLY,
    GAME_COOKING_FINISHABLE,
    GAME_OVER,
    GAME_TIMED_OUT
} GameSMState_t;

bool InitGameSM(uint8_t priority);
bool PostGameSM(ES_Event_t event);
ES_Event_t RunGameSM(ES_Event_t event);
GameSMState_t QueryGameSM(void);

#endif
