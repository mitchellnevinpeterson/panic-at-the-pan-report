#include "BenchIO.h"
#ifdef GAME_BUTTONS_BENCH
#include "GameButtons.h"
#include "CookSM.h"
#include "Servo50Hz.h"
#include "Treat.h"
#include "Stirrer.h"
#endif
#include "GameSM.h"
#include "IngredientSM.h"
#include "ScoreSM.h"
#include "GameConfig.h"
#include "ES_Framework.h"
#include "terminal.h"
#include "GameBenchConfig.h"
#include <stdio.h>
#include <xc.h>

/* The course clock wraps every 65.536 s. Sample it in every checker and action,
 * not only when a user presses a key. All callbacks are foreground-only.
 */
static uint16_t previous_tick;
static uint32_t milliseconds;
static bool error_posted, supply_settled;
#ifdef GAME_BUTTONS_BENCH
static bool debug_uart_warned;
/* Which screen owns the LED strips, so late score updates (e.g. CookSM's
 * stir score at End) repaint the right view and never overwrite banners. */
static enum { SCREEN_BANNER, SCREEN_DASHBOARD, SCREEN_END_MESSAGE, SCREEN_BIG_SCORE } screen;
static const char *const dish_texts[DISH_COUNT] = DISH_TEXTS;
static const uint8_t dish_recipes[DISH_COUNT] = DISH_RECIPES;
static uint8_t dish = DISH_COUNT;      /* current dish; DISH_COUNT before the first game */
static uint8_t forced_dish = DISH_COUNT; /* one-shot override (tests); else random */
static uint32_t dish_rng = 0x9E3779B9u;

void BenchIO_SetNextDish(uint8_t index) { if (index < DISH_COUNT) forced_dish = index; }
uint8_t BenchIO_Dish(void) { return dish; }

/* Random dish, never the same as `previous` (DISH_COUNT = no previous game).
 * Entropy is the Start press time in ms: human timing makes it unpredictable;
 * xorshift32 spreads it. */
uint8_t BenchIO_RandomDish(uint32_t entropy, uint8_t previous)
{
    dish_rng ^= entropy;
    dish_rng ^= dish_rng << 13;
    dish_rng ^= dish_rng >> 17;
    dish_rng ^= dish_rng << 5;
    if (previous >= DISH_COUNT) return (uint8_t)(dish_rng % DISH_COUNT);
    return (uint8_t)((previous + 1u + dish_rng % (DISH_COUNT - 1u)) % DISH_COUNT);
}

static uint32_t now_ms(void);

/* Every Start serves a dish: its intro text and its recipe. */
static bool serve_next_dish(void)
{
    dish = forced_dish < DISH_COUNT ? forced_dish : BenchIO_RandomDish(now_ms(), dish);
    forced_dish = DISH_COUNT;
    printf("Dish %u/%u: %s (recipe 0x%02X)\r\n", (unsigned)dish + 1u, (unsigned)DISH_COUNT,
           dish_texts[dish], (unsigned)dish_recipes[dish]);
    return GameSM_SetDishText(dish_texts[dish]) && ScoreSM_SetRecipeMask(dish_recipes[dish]);
}
#endif
volatile GameSMState_t GameBenchState;

static void record_fault(const char *reason)
{
#ifdef GAME_BUTTONS_BENCH
    GameButtons_Fault(reason);
#else
    (void)reason;
#endif
}

static uint32_t now_ms(void)
{
    uint16_t tick = ES_Timer_GetTime();
    milliseconds += (uint16_t)(tick - previous_tick);
    previous_tick = tick;
    return milliseconds;
}

static bool service_display(void)
{
    /* Allow supply settling at boot; setters only update the pending frame. */
    if (!supply_settled) {
        if (now_ms() < 100u) return true;
        supply_settled = true;
    }
    if (ScoreTimerDisplay_Service() == SCORE_TIMER_FAULT) {
        record_fault("STATUS_DISPLAY_SPI");
        return false;
    }
    return true;
}

#ifdef GAME_BUTTONS_BENCH
static int16_t score_text(char text[6], int32_t score)
{
    if (score < -999) score = -999;
    if (score > 9999) score = 9999;
    int n = snprintf(text, 6, "%ld", (long)score);
    return (int16_t)((32 - n * DISPLAY_BANNER_CHAR_WIDTH) / 2); /* Centred left pixel. */
}

static bool show_score(int32_t score)
{
    char text[6];
    int16_t x = score_text(text, score);
    switch (screen) {
    case SCREEN_DASHBOARD: return ScoreTimerDisplay_SetScore(score);
    case SCREEN_END_MESSAGE: return ScoreTimerDisplay_SetStripBanner(DISPLAY_BOTTOM, text, x);
    case SCREEN_BIG_SCORE: return ScoreTimerDisplay_SetBanner(text, x);
    default: return true; /* Welcome / dish banners own the strips. */
    }
}

static bool cook_lifecycle(ES_EventType_t type)
{
    /* A rejected CookSM post never stops the game; overflow latches
     * CookSM_HasFault(), which CheckGameBenchIO reports. */
    if (!CookSM_PostLifecycle(type, GameSM_InputGeneration())) printf("CookSM lifecycle rejected\r\n");
    return true;
}
#endif

static bool output(GameSMAction_t action, int32_t value)
{
    static const char *const names[] = {
        "WELCOME", "DISH_INTRO", "COOKING_EARLY", "COOKING_FINISHABLE",
        "GAME_OVER", "TIMED_OUT", "FAULT"
    };
    bool ok = true;
    switch (action) {
    case GAME_ACTION_STATE:
        if (value < GAME_WELCOME || value > GAME_FAULT) return false;
        GameBenchState = (GameSMState_t)value;
#ifdef GAME_BUTTONS_BENCH
        GameButtons_State();
        /* The big score's open gate stays open through Welcome; every game
         * starts with the gate closed, whatever its state. */
        if (value == GAME_DISH_INTRO) {
            Treat_Close(now_ms());
            if (!serve_next_dish()) return false;
        }
#endif
        printf("\r\nState: %s\r\n", names[value]);
        break;
    case GAME_ACTION_WELCOME:
#ifdef GAME_BUTTONS_BENCH
        screen = SCREEN_BANNER;
#endif
        ok = ScoreTimerDisplay_SetBanner(WELCOME_TEXT, (int16_t)value);
        break;
    case GAME_ACTION_DISH:
#ifdef GAME_BUTTONS_BENCH
        screen = SCREEN_BANNER;
        ok = ScoreTimerDisplay_SetBanner(GameSM_DishText(), (int16_t)value); /* this round's dish */
#else
        ok = ScoreTimerDisplay_SetBanner(DISH_TEXT, (int16_t)value);
#endif
        break;
    case GAME_ACTION_DASHBOARD:
#ifdef GAME_BUTTONS_BENCH
        screen = SCREEN_DASHBOARD;
#endif
        /* Set all fields before Service can snapshot a new display frame. */
        ok = ScoreTimerDisplay_ShowLabels() && ScoreTimerDisplay_SetScore(0) &&
             ScoreTimerDisplay_SetTimerSeconds((uint32_t)value);
        break;
    case GAME_ACTION_COUNTDOWN:
        ok = ScoreTimerDisplay_SetTimerSeconds((uint32_t)value);
        break;
    case GAME_ACTION_FINISH_LED:
#ifdef GAME_BUTTONS_BENCH
        GameButtons_Lamp(value != 0);
        printf("Finish LED %s\r\n", value ? "ON" : "OFF");
#else
        printf("[STUB] Finish LED %s\r\n", value ? "ON" : "OFF");
#endif
        break;
    case GAME_ACTION_START:
#ifdef GAME_BUTTONS_BENCH
        GameButtons_Boundary();
#endif
        ok = IngredientSM_PostLifecycle(GAME_START, GameSM_InputGeneration()) &&
             ScoreSM_PostLifecycle(GAME_START, GameSM_InputGeneration());
#ifdef GAME_BUTTONS_BENCH
        ok = ok && cook_lifecycle(GAME_START);
        printf("GAME_START -> IngredientSM / ScoreSM / CookSM\r\n");
#else
        printf("GAME_START -> IngredientSM / ScoreSM\r\n");
#endif
        break;
    case GAME_ACTION_END:
#ifdef GAME_BUTTONS_BENCH
        /* Icons stay up during the end message; the big score clears them. */
        GameButtons_Boundary();
#endif
        ok = IngredientSM_PostLifecycle(GAME_END, GameSM_InputGeneration());
#ifdef GAME_BUTTONS_BENCH
        ok = ok && cook_lifecycle(GAME_END);
        printf("GAME_END (%s) -> IngredientSM / CookSM\r\n",
               value == GAME_COMPLETED ? "completed" : "idle");
#else
        printf("GAME_END -> IngredientSM; [STUB] pan: %s\r\n",
               value == GAME_COMPLETED ? "GAME OVER" : "TIMEOUT");
#endif
        break;
    case GAME_ACTION_RESET:
#ifdef GAME_BUTTONS_BENCH
        GameButtons_ClearPan();
        GameButtons_Boundary();
#endif
        ok = IngredientSM_PostLifecycle(GAME_RESET, GameSM_InputGeneration()) &&
             ScoreSM_PostLifecycle(GAME_RESET, GameSM_InputGeneration());
#ifdef GAME_BUTTONS_BENCH
        ok = ok && cook_lifecycle(GAME_RESET);
#endif
        printf("GAME_RESET -> IngredientSM / ScoreSM\r\n");
        break;
#ifdef GAME_BUTTONS_BENCH
    case GAME_ACTION_END_MESSAGE:
        if (!GameSM_EndText()) return false;
        if (screen != SCREEN_END_MESSAGE) {
            /* First frame: the bottom strip holds the final score as text. */
            screen = SCREEN_END_MESSAGE;
            ok = show_score(ScoreSM_Value());
        }
        ok = ok && ScoreTimerDisplay_SetStripBanner(DISPLAY_TOP, GameSM_EndText(), (int16_t)value);
        break;
    case GAME_ACTION_BIG_SCORE:
        screen = SCREEN_BIG_SCORE;
        ok = show_score(ScoreSM_Value());
        GameButtons_ClearPan();
        printf("Final score %ld; E-ink clearing\r\n", (long)ScoreSM_Value());
        break;
    case GAME_ACTION_TREAT:
        if (ScoreSM_Value() >= TREAT_MIN_SCORE) {
            Treat_Release(now_ms());
            printf("Treat released (score >= %d)\r\n", TREAT_MIN_SCORE);
        } else {
            printf("No treat (score < %d)\r\n", TREAT_MIN_SCORE);
        }
        break;
#else
    case GAME_ACTION_TREAT:
        printf("[STUB] Treat request x1 (no servo output)\r\n");
        break;
#endif
    default:
        return false;
    }
    Terminal_MoveBuffer2UART();
    if (!ok) record_fault("GAME_OUTPUT");
    return ok && service_display();
}

static bool ingredient_output(IngredientSMAction_t action, uint8_t value)
{
    if (action == INGREDIENT_ACTION_STATE) {
        static const char *const names[] = {"READY", "LOCKED", "FAULT"};
        if (value > INGREDIENT_FAULT) return false;
        printf("Ingredients: %s\r\n", names[value]);
    } else {
        if (value >= INGREDIENT_COUNT) return false;
        switch (action) {
        case INGREDIENT_ACTION_ADDED:
            if (!ScoreSM_PostIngredient(value, GameSM_InputGeneration())) return false;
#ifdef GAME_BUTTONS_BENCH
            if (!CookSM_PostIngredient(value, GameSM_InputGeneration()))
                printf("CookSM ingredient rejected\r\n");
            GameButtons_AddIngredient(value);
#endif
            printf("[Ingredient %u] ADDED\r\n", (unsigned)value + 1u);
#ifdef GAME_BUTTONS_BENCH
            printf("INGREDIENT_ADDED -> ScoreSM / CookSM / E-ink\r\n");
#else
            printf("INGREDIENT_ADDED -> ScoreSM; [STUB] Heat / Stir / Pan\r\n");
#endif
            break;
        case INGREDIENT_ACTION_DUPLICATE:
            printf("[Ingredient %u] DUPLICATE - ignored\r\n", (unsigned)value + 1u);
            break;
        case INGREDIENT_ACTION_LOCKED:
            printf("[Ingredient %u] LOCKED - ignored\r\n", (unsigned)value + 1u);
            break;
        case INGREDIENT_ACTION_STALE:
            printf("[Ingredient %u] STALE/EXPIRED - ignored\r\n", (unsigned)value + 1u);
            break;
        default:
            return false;
        }
    }
    Terminal_MoveBuffer2UART();
    return service_display();
}

static bool score_display(int32_t value)
{
#ifdef GAME_BUTTONS_BENCH
    if (!show_score(value)) return false;
#else
    if (!ScoreTimerDisplay_SetScore(value)) return false;
#endif
    printf("Score: %ld\r\n", (long)value);
    Terminal_MoveBuffer2UART();
    return service_display();
}

bool GameBenchIO_Init(void)
{
    const GameSMPort_t port = {now_ms, output};
    const IngredientSMPort_t ingredient_port = {
        GameSM_InputGeneration, GameSM_AcceptsGameplayInput, ingredient_output
    };
#ifdef GAME_BUTTONS_BENCH
    const ScoreSMPort_t score_port = {
        GameSM_InputGeneration, GameSM_AcceptsScoreRound, score_display,
        10, dish_recipes[0], true
    };
    const CookSMPort_t cook_port = { now_ms, GameButtons_DistanceMm, Stirrer_TakeCircles };
#else
    const ScoreSMPort_t score_port = {
        GameSM_InputGeneration, GameSM_AcceptsScoreRound, score_display,
        BENCH_INGREDIENT_POINTS, 0, false
    };
#endif
    previous_tick = 0;
    milliseconds = 0;
    error_posted = false;
    supply_settled = false;
#ifdef GAME_BUTTONS_BENCH
    debug_uart_warned = false;
    screen = SCREEN_BANNER;
    if (!GameButtons_Init(now_ms)) return false;
    /* After ButtonEInkPort_Init parked RA4/RB5: Timer2 servos, treat closed. */
    Servo50Hz_Init();
    Treat_Init(now_ms());
    Stirrer_Init(now_ms());
    if (!CookSM_BindPort(&cook_port)) return false;
#endif
    return ScoreTimerDisplay_Init(&BenchDisplayConfig) && GameSM_BindPort(&port) &&
           IngredientSM_BindPort(&ingredient_port) && ScoreSM_BindPort(&score_port);
}

static bool post_fault(const char *reason)
{
    record_fault(reason);
    if (error_posted) return false;
    error_posted = PostGameSM((ES_Event_t){ES_ERROR, 0});
    return error_posted;
}

bool CheckGameBenchIO(void)
{
    (void)now_ms();
#ifdef GAME_BUTTONS_BENCH
    if (!GameButtons_Service()) return post_fault("BUTTONS_ADAPTER");
    {
        uint32_t now = now_ms();
        GameSMState_t s = QueryGameSM();
        bool cooking = s == GAME_COOKING_EARLY || s == GAME_COOKING_FINISHABLE;
        Treat_Service(now);
        Stirrer_Service(now, cooking);
        /* Joystick moves renew idle only if JOYSTICK_IS_ACTIVITY (GameConfig.h).
         * The flag is always taken so a stale move never carries over. */
        bool stick_moved = Stirrer_TakeActivity();
        if (JOYSTICK_IS_ACTIVITY && stick_moved && cooking &&
            !GameSM_PostInput(USER_ACTIVITY, now, GameSM_InputGeneration()))
            return post_fault("GAME_INPUT_QUEUE");
    }
#endif
    Terminal_MoveBuffer2UART();
    if (!service_display()) return post_fault("STATUS_DISPLAY_SPI");
    if (GameSM_HasFault()) return post_fault("GAME_STATE_MACHINE");
    if (IngredientSM_HasFault()) return post_fault("INGREDIENT_STATE_MACHINE");
    if (ScoreSM_HasFault()) return post_fault("SCORE_STATE_MACHINE");
#ifdef GAME_BUTTONS_BENCH
    if (CookSM_HasFault()) return post_fault("COOK_STATE_MACHINE");
#endif

#ifdef GAME_BUTTONS_BENCH
    /* UART1 is an optional console; physical controls arrive on UART2.
     * Discard a damaged console burst without ending the physical game.
     * Bound the drain even if an unplugged/noisy adapter keeps sending. */
    if (U1STAbits.OERR || (U1STAbits.URXDA && (U1STAbits.FERR || U1STAbits.PERR))) {
        if (U1STAbits.OERR) U1STACLR = _U1STA_OERR_MASK;
        unsigned budget = 8;
        while (budget-- && U1STAbits.URXDA) (void)Terminal_ReadByte();
        if (!debug_uart_warned) {
            printf("Debug UART1 RX error discarded; physical game continues.\r\n");
            debug_uart_warned = true;
        }
        return false;
    }
#else
    /* Keyboard-only GameSMBench relies on UART1 for its game controls. */
    if (U1STAbits.OERR) {
        U1STACLR = _U1STA_OERR_MASK;
        return post_fault("DEBUG_UART_OVERRUN");
    }
    if (!U1STAbits.URXDA) return false;
    if (U1STAbits.FERR || U1STAbits.PERR) {
        (void)U1RXREG;
        return post_fault("DEBUG_UART_FRAME");
    }
#endif
    /* Never call the course blocking read without a buffered byte. */
    if (!U1STAbits.URXDA) return false;
    uint8_t key = Terminal_ReadByte();
    uint32_t detected = now_ms();
    uint32_t generation = GameSM_InputGeneration();
    ES_EventType_t type;
    switch (key) {
#ifndef GAME_BUTTONS_BENCH
    case 'b': case 'B': type = START_PRESSED; break;
    case 'f': case 'F': type = FINISH_PRESSED; break;
#else
    case 's': case '?': GameButtons_Status(); return false;
    case 'j': case 'J': Stirrer_Report(); return false; /* joystick/spoon diagnostics */
#endif
    case 'a': case 'A': type = USER_ACTIVITY; break;
    case '1': case '2': case '3': case '4': case '5':
        type = USER_ACTIVITY;
        break;
#ifdef GAME_BUTTONS_BENCH
    default: (void)Treat_Key((char)key, detected); return false; /* Treat calibration. */
#else
    default: return false; /* Newlines and unrelated keys have no game effect. */
#endif
    }
    if (!GameSM_PostInput(type, detected, generation)) return post_fault("GAME_INPUT_QUEUE");
    if (key >= '1' && key <= '5' &&
        !IngredientSM_PostSwitch((uint8_t)(key - '1'), detected, generation)) return post_fault("INGREDIENT_INPUT_QUEUE");
    printf("[RX] %c\r\n", key); /* Receipt only; GameSM still applies state/time gates. */
    Terminal_MoveBuffer2UART();
    return true;
}
