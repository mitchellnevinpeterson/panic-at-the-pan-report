/* Game lifecycle service. Bind an explicit foreground I/O adapter before
 * ES_Initialize(); the full-game hardware adapter is not implemented yet.
 */
#ifndef PANIC_GAMESM_H
#define PANIC_GAMESM_H

#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"
#include "ES_Configure.h"
#include "ES_Types.h"

typedef enum {
    GAME_WELCOME,
    GAME_DISH_INTRO,
    GAME_COOKING_EARLY,
    GAME_COOKING_FINISHABLE,
    GAME_OVER,
    GAME_TIMED_OUT,
    GAME_FAULT
} GameSMState_t;

typedef enum {
    GAME_ACTION_STATE,
    GAME_ACTION_WELCOME,       /* value: banner left pixel, may be negative */
    GAME_ACTION_DISH,          /* one pass; same pixel coordinate as Welcome */
    GAME_ACTION_DASHBOARD,     /* batch headings + score zero + remaining */
    GAME_ACTION_COUNTDOWN,
    GAME_ACTION_FINISH_LED,
    GAME_ACTION_START,         /* lifecycle notifications to other services */
    GAME_ACTION_END,           /* value: GameSMEndReason_t */
    GAME_ACTION_RESET,
    GAME_ACTION_TREAT,
    /* GAME_BUTTONS_BENCH end sequence for completed games only. */
    GAME_ACTION_END_MESSAGE,   /* value: top-strip left pixel; text GameSM_EndText() */
    GAME_ACTION_BIG_SCORE      /* message gone: big final score for END_SCORE_MS */
} GameSMAction_t;

typedef enum { GAME_COMPLETED = 0, GAME_IDLE_TIMEOUT = 1 } GameSMEndReason_t;

typedef struct {
    uint32_t (*now_ms)(void); /* monotonic, unsigned wrap permitted */
    bool (*output)(GameSMAction_t action, int32_t value); /* bounded; no waits */
} GameSMPort_t;

bool GameSM_BindPort(const GameSMPort_t *port);
uint32_t GameSM_InputGeneration(void);
/* Effective capture/deadline time of the latest lifecycle transition. */
uint32_t GameSM_LifecycleTime(void);
bool GameSM_HasFault(void);
/* End message of the current completed game (TIME'S UP / GAME OVER), else 0. */
const char *GameSM_EndText(void);
/* Dish intro text (default DISH_TEXT). Set it before or as the dish intro
 * starts; the intro lasts until this text has scrolled off. Static storage,
 * 1..64 characters. */
bool GameSM_SetDishText(const char *text);
const char *GameSM_DishText(void);
/* Fault recovery: return to Welcome from any non-Welcome state. Carries the
 * current generation, so a late abort can never cancel a later round.
 */
bool GameSM_PostAbort(void);
/* Read-only gate for other gameplay services. Rechecks phase, generation,
 * detection time and live deadlines even when an end event is still queued.
 */
bool GameSM_AcceptsGameplayInput(uint32_t detected_ms, uint32_t generation);
/* Earned score may drain during results, but never over Welcome/Dish banners.
 * Input generation changes at End; the accepted round identity does not.
 */
bool GameSM_AcceptsScoreRound(uint32_t round_generation);
/* Foreground-only input ingress. Never pass a pointer in ES EventParam.
 * Returns false on invalid arguments or overflow; caller must stop on failure.
 * Hardware checkers must debounce/rearm the press before calling this API.
 * Raw START/FINISH/ACTIVITY posts are not a substitute for this timestamped API.
 */
bool GameSM_PostInput(ES_EventType_t type, uint32_t detected_ms,
                      uint32_t generation);

bool InitGameSM(uint8_t priority);
bool PostGameSM(ES_Event_t event);
ES_Event_t RunGameSM(ES_Event_t event);
GameSMState_t QueryGameSM(void);

#endif
