#ifndef ES_CONFIGURE_H
#define ES_CONFIGURE_H

/* Application-owned registration for the unmodified ME218 ES framework.
 * Higher service numbers have higher priority. Queue sizes are initial
 * engineering allocations, not validated worst-case capacities.
 */
#define MAX_NUM_SERVICES 8
#if defined(GAME_SM_BENCH) && defined(GAME_BUTTONS_BENCH)
/* Final game (GameButtonsPIC1): proven 3-service bench + CookSM. */
#define NUM_SERVICES 4
#define SERV_0_HEADER "ScoreSM.h"
#define SERV_0_INIT InitScoreSM
#define SERV_0_RUN RunScoreSM
#define SERV_0_QUEUE_SIZE 32
#define SERV_1_HEADER "CookSM.h"
#define SERV_1_INIT InitCookSM
#define SERV_1_RUN RunCookSM
#define SERV_1_QUEUE_SIZE 32
#define SERV_2_HEADER "IngredientSM.h"
#define SERV_2_INIT InitIngredientSM
#define SERV_2_RUN RunIngredientSM
#define SERV_2_QUEUE_SIZE 32
#define SERV_3_HEADER "GameSM.h"
#define SERV_3_INIT InitGameSM
#define SERV_3_RUN RunGameSM
#define SERV_3_QUEUE_SIZE 32
#define EVENT_CHECK_LIST CheckGameBenchIO
#elif defined(GAME_SM_BENCH)
#define NUM_SERVICES 3
#define SERV_0_HEADER "ScoreSM.h"
#define SERV_0_INIT InitScoreSM
#define SERV_0_RUN RunScoreSM
#define SERV_0_QUEUE_SIZE 32
#define SERV_1_HEADER "IngredientSM.h"
#define SERV_1_INIT InitIngredientSM
#define SERV_1_RUN RunIngredientSM
#define SERV_1_QUEUE_SIZE 32
#define SERV_2_HEADER "GameSM.h"
#define SERV_2_INIT InitGameSM
#define SERV_2_RUN RunGameSM
#define SERV_2_QUEUE_SIZE 32
#define EVENT_CHECK_LIST CheckGameBenchIO
#else
#define NUM_SERVICES 6
#define EVENT_CHECK_LIST CheckGameInputs

#define SERV_0_HEADER "PanDisplaySM.h"
#define SERV_0_INIT InitPanDisplaySM
#define SERV_0_RUN RunPanDisplaySM
#define SERV_0_QUEUE_SIZE 16
#define SERV_1_HEADER "ScoreSM.h"
#define SERV_1_INIT InitScoreSM
#define SERV_1_RUN RunScoreSM
#define SERV_1_QUEUE_SIZE 16
#define SERV_2_HEADER "StirSM.h"
#define SERV_2_INIT InitStirSM
#define SERV_2_RUN RunStirSM
#define SERV_2_QUEUE_SIZE 16
#define SERV_3_HEADER "HeatSM.h"
#define SERV_3_INIT InitHeatSM
#define SERV_3_RUN RunHeatSM
#define SERV_3_QUEUE_SIZE 16
#define SERV_4_HEADER "IngredientSM.h"
#define SERV_4_INIT InitIngredientSM
#define SERV_4_RUN RunIngredientSM
#define SERV_4_QUEUE_SIZE 16
#define SERV_5_HEADER "GameSM.h"
#define SERV_5_INIT InitGameSM
#define SERV_5_RUN RunGameSM
#define SERV_5_QUEUE_SIZE 16
#endif

typedef enum {
    ES_NO_EVENT = 0,
    ES_ERROR,
    ES_INIT,
    ES_TIMEOUT,
    ES_SHORT_TIMEOUT,
    SWITCH_FLIPPED,
    FINISH_PRESSED,
    STIR_START,
    STIR_STOP,
    USER_ACTIVITY,
    INGREDIENT_ADDED,
    HEAT_POINTS,
    BURNED,
    STIR_RESULT,
    GAME_END,
    GAME_RESET,
    START_PRESSED,
    GAME_START,
    GAME_ABORT,    /* fault recovery: GameSM returns to Welcome */
    SCORE_DELTA,   /* ScoreSM_PostDelta ticket */
    FLIP_PRESSED   /* CookSM_PostFlip ticket */
} ES_EventType_t;

#define NUM_DIST_LISTS 0

/* Timers post ES_TIMEOUT with EventParam = timer ID, not a separate enum. */
#define GAME_TICK_TIMER 0
#define IDLE_TIMER 1
#define RESULT_TIMER 2
#define TIMEOUT_DISPLAY_TIMER 3
#define HEAT_SAMPLE_TIMER 4
#define BURN_TIMER 5
#define STIR_TICK_TIMER 6
#define ICON_TIMER 7
#define ANIM_TIMER 8
#define WELCOME_SCROLL_TIMER 9 /* Shared by Welcome and the one-pass Dish intro. */
#define COOK_TICK_TIMER 4      /* Final game only (slot unused by the plain bench). */

#define TIMER_UNUSED ((pPostFunc)0)
#define TIMER0_RESP_FUNC PostGameSM
#define TIMER1_RESP_FUNC PostGameSM
#define TIMER2_RESP_FUNC PostGameSM
#define TIMER3_RESP_FUNC PostGameSM
#if defined(GAME_SM_BENCH) && defined(GAME_BUTTONS_BENCH)
#define TIMER4_RESP_FUNC PostCookSM
#define TIMER5_RESP_FUNC TIMER_UNUSED
#define TIMER6_RESP_FUNC TIMER_UNUSED
#define TIMER7_RESP_FUNC TIMER_UNUSED
#define TIMER8_RESP_FUNC TIMER_UNUSED
#elif defined(GAME_SM_BENCH)
#define TIMER4_RESP_FUNC TIMER_UNUSED
#define TIMER5_RESP_FUNC TIMER_UNUSED
#define TIMER6_RESP_FUNC TIMER_UNUSED
#define TIMER7_RESP_FUNC TIMER_UNUSED
#define TIMER8_RESP_FUNC TIMER_UNUSED
#else
#define TIMER4_RESP_FUNC PostHeatSM
#define TIMER5_RESP_FUNC PostHeatSM
#define TIMER6_RESP_FUNC PostStirSM
#define TIMER7_RESP_FUNC PostPanDisplaySM
#define TIMER8_RESP_FUNC PostPanDisplaySM
#endif
#define TIMER9_RESP_FUNC PostGameSM
#define TIMER10_RESP_FUNC TIMER_UNUSED
#define TIMER11_RESP_FUNC TIMER_UNUSED
#define TIMER12_RESP_FUNC TIMER_UNUSED
#define TIMER13_RESP_FUNC TIMER_UNUSED
#define TIMER14_RESP_FUNC TIMER_UNUSED
#define TIMER15_RESP_FUNC TIMER_UNUSED

#endif
