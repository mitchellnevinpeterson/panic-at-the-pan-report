#include "GameSM.h"
#include "GameConfig.h"
#include "ES_Framework.h"
#include <string.h>

/* ES queue entries have a 16-bit payload. Tickets reference bounded, owned
 * metadata; generation changes reject input queued before Start/end/reset.
 * All ingress and callbacks run in the cooperative foreground, never an ISR.
 */
enum { INPUT_SLOTS = 16, BANNER_WIDTH = (sizeof WELCOME_TEXT - 1u) * 8u, DISH_TEXT_MAX = 64 };
#ifdef GAME_BUTTONS_BENCH
/* Longest end sequence (TIME'S UP pass + big score) stays under the 30 s limit. */
typedef char end_sequence_within_limit[
    (32u + (sizeof END_TIMEUP_TEXT - 1u) * 8u) * WELCOME_SCROLL_MS + END_SCORE_MS <= 30000u ? 1 : -1];
enum { END_NONE, END_MESSAGE, END_BIG_SCORE };
#endif
typedef struct {
    bool used;
    uint16_t ticket;
    ES_EventType_t type;
    uint32_t detected, generation;
} Input;

static GameSMPort_t io;
static Input inputs[INPUT_SLOTS];
static uint8_t priority;
static uint16_t next_ticket;
static uint32_t generation, entered, started, last_activity, tick_due, scroll_due;
static uint32_t round_generation;
static uint32_t lifecycle_time;
static const char *end_text;
static const char *dish_text = DISH_TEXT; /* GameSM_SetDishText */
static uint16_t dish_width = (sizeof DISH_TEXT - 1u) * 8u;
#ifdef GAME_BUTTONS_BENCH
static uint32_t big_started;
static uint16_t end_width;
static uint8_t end_phase;
#endif
static uint16_t shown_seconds;
static int16_t banner_x;
static bool initialized, posting_failed;
static GameSMState_t state = GAME_WELCOME;
static const uint8_t timers[] = {
    GAME_TICK_TIMER, IDLE_TIMER, RESULT_TIMER, TIMEOUT_DISPLAY_TIMER,
    WELCOME_SCROLL_TIMER
};

static bool reached(uint32_t now, uint32_t deadline)
{
    return (uint32_t)(now - deadline) < UINT32_C(0x80000000);
}

static bool cooking(void)
{
    return state == GAME_COOKING_EARLY || state == GAME_COOKING_FINISHABLE;
}

static bool emit(GameSMAction_t action, int32_t value)
{
    return io.output && io.output(action, value);
}

static bool stop_timers(void)
{
    unsigned int i;
    bool ok = true;
    for (i = 0; i < sizeof timers / sizeof timers[0]; ++i) {
        if (ES_Timer_StopTimer(timers[i]) != ES_Timer_OK) ok = false;
    }
    return ok;
}

static ES_Event_t fail(void)
{
    (void)stop_timers();
    state = GAME_FAULT;
    (void)emit(GAME_ACTION_FINISH_LED, 0);
    (void)emit(GAME_ACTION_STATE, GAME_FAULT);
    return (ES_Event_t){ES_ERROR, 0};
}

static bool arm(uint8_t timer, uint32_t delay)
{
    return delay > 0 && delay <= UINT16_MAX &&
           ES_Timer_InitTimer(timer, (uint16_t)delay) == ES_Timer_OK;
}

static uint16_t remaining(uint32_t at)
{
    uint32_t elapsed = (uint32_t)(at - started) / 1000u;
    return elapsed >= GAME_LENGTH_S ? 0u : (uint16_t)(GAME_LENGTH_S - elapsed);
}

static bool show_remaining(uint32_t at)
{
    uint16_t value = remaining(at);
    if (value == shown_seconds) return true;
    shown_seconds = value;
    return emit(GAME_ACTION_COUNTDOWN, value);
}

static bool welcome(uint32_t now)
{
    if (!stop_timers()) return false;
    ++generation;
    entered = now;
    lifecycle_time = now;
    state = GAME_WELCOME;
    end_text = 0;
#ifdef GAME_BUTTONS_BENCH
    end_phase = END_NONE;
#endif
    banner_x = 32;
    scroll_due = now + WELCOME_SCROLL_MS;
    return emit(GAME_ACTION_RESET, 0) && emit(GAME_ACTION_FINISH_LED, 0) &&
           emit(GAME_ACTION_STATE, state) && emit(GAME_ACTION_WELCOME, banner_x) &&
           arm(WELCOME_SCROLL_TIMER, WELCOME_SCROLL_MS);
}

static bool arm_tick(uint32_t now)
{
    uint32_t delay = GAME_TICK_MS - (uint32_t)(now - started) % GAME_TICK_MS;
    tick_due = now + delay;
    return arm(GAME_TICK_TIMER, delay);
}

static bool arm_idle(uint32_t now)
{
    uint32_t age = (uint32_t)(now - last_activity);
    return age < IDLE_TIMEOUT_MS && arm(IDLE_TIMER, IDLE_TIMEOUT_MS - age);
}

static bool start(uint32_t detected, uint32_t now)
{
    if (!stop_timers()) return false;
    ++generation;
    round_generation = generation;
    started = entered = last_activity = detected;
    lifecycle_time = detected;
    state = GAME_COOKING_EARLY;
    shown_seconds = remaining(now);
    return emit(GAME_ACTION_STATE, state) && emit(GAME_ACTION_START, 0) &&
           emit(GAME_ACTION_DASHBOARD, shown_seconds) && arm_tick(now) && arm_idle(now);
}

static bool dish_intro(uint32_t now)
{
    if (!stop_timers()) return false;
    ++generation;
    entered = now;
    lifecycle_time = now;
    state = GAME_DISH_INTRO;
    banner_x = 32;
    scroll_due = now + WELCOME_SCROLL_MS;
    /* Reassert Reset with this generation so delayed lower-priority reset
     * events cannot leave Ingredient/Score armed during the intro.
     */
    return emit(GAME_ACTION_RESET, 0) && emit(GAME_ACTION_STATE, state) &&
           emit(GAME_ACTION_DISH, banner_x) && arm(WELCOME_SCROLL_TIMER, WELCOME_SCROLL_MS);
}

static bool end(GameSMEndReason_t reason, uint32_t at, uint32_t now)
{
    if (!stop_timers()) return false;
    ++generation;
    entered = now;
    lifecycle_time = at;
    state = reason == GAME_COMPLETED ? GAME_OVER : GAME_TIMED_OUT;
    end_text = reason != GAME_COMPLETED ? 0 :
               (uint32_t)(at - started) >= GAME_LENGTH_S * 1000u ? END_TIMEUP_TEXT : END_FINISH_TEXT;
    if (!show_remaining(at) || !emit(GAME_ACTION_FINISH_LED, 0) ||
        !emit(GAME_ACTION_END, reason) || !emit(GAME_ACTION_STATE, state)) return false;
#ifdef GAME_BUTTONS_BENCH
    if (reason == GAME_COMPLETED) {
        /* One pass of the end message on the top strip; then big score. */
        end_phase = END_MESSAGE;
        end_width = (uint16_t)(strlen(end_text) * 8u);
        banner_x = 32;
        scroll_due = now + WELCOME_SCROLL_MS;
        return emit(GAME_ACTION_END_MESSAGE, banner_x) && arm(WELCOME_SCROLL_TIMER, WELCOME_SCROLL_MS);
    }
#else
    if (reason == GAME_COMPLETED) {
        return arm(RESULT_TIMER, RESULT_DISPLAY_MS) && emit(GAME_ACTION_TREAT, 1);
    }
#endif
#ifdef GAME_BUTTONS_BENCH
    /* Physical game rehearsal: queue white, then return directly to Welcome.
     * The asynchronous pan scheduler completes the clear in the background. */
    return welcome(now);
#else
    return arm(TIMEOUT_DISPLAY_TIMER, TIMEOUT_DISPLAY_MS);
#endif
}

static bool refresh(uint32_t now)
{
    if (!cooking()) return true;
    uint32_t elapsed = (uint32_t)(now - started);
    /* Completion wins a simultaneous game / idle deadline, regardless of
     * which ES timeout entered the queue first. Never infer time from ticks.
     */
    if (elapsed >= GAME_LENGTH_S * 1000u)
        return end(GAME_COMPLETED, started + GAME_LENGTH_S * 1000u, now);
    if ((uint32_t)(now - last_activity) >= IDLE_TIMEOUT_MS)
        return end(GAME_IDLE_TIMEOUT, last_activity + IDLE_TIMEOUT_MS, now);
    if (state == GAME_COOKING_EARLY && elapsed >= FINISH_ENABLE_S * 1000u) {
        state = GAME_COOKING_FINISHABLE;
        if (!emit(GAME_ACTION_FINISH_LED, 1) || !emit(GAME_ACTION_STATE, state)) return false;
    }
    return show_remaining(now);
}

bool GameSM_BindPort(const GameSMPort_t *port)
{
    if (initialized || !port || !port->now_ms || !port->output) return false;
    io = *port;
    return true;
}

uint32_t GameSM_InputGeneration(void) { return generation; }
const char *GameSM_EndText(void) { return end_text; }
const char *GameSM_DishText(void) { return dish_text; }

bool GameSM_SetDishText(const char *text)
{
    size_t length = 0;
    if (!text) return false;
    while (length <= DISH_TEXT_MAX && text[length]) ++length;
    if (length == 0 || length > DISH_TEXT_MAX) return false;
    dish_text = text;
    dish_width = (uint16_t)(length * 8u);
    return true;
}
bool GameSM_PostAbort(void) { return PostGameSM((ES_Event_t){GAME_ABORT, (uint16_t)generation}); }
uint32_t GameSM_LifecycleTime(void) { return lifecycle_time; }
bool GameSM_HasFault(void) { return posting_failed || state == GAME_FAULT; }

bool GameSM_AcceptsScoreRound(uint32_t epoch)
{
    return initialized && !GameSM_HasFault() && epoch == round_generation &&
           (cooking() || state == GAME_OVER || state == GAME_TIMED_OUT);
}

bool GameSM_AcceptsGameplayInput(uint32_t detected, uint32_t epoch)
{
    if (!initialized || GameSM_HasFault() || !cooking() || epoch != generation)
        return false;
    uint32_t now = io.now_ms();
    return reached(detected, started) &&
           (uint32_t)(now - detected) <= GAME_INPUT_MAX_AGE_MS &&
           (uint32_t)(now - started) < GAME_LENGTH_S * 1000u &&
           (uint32_t)(detected - started) < GAME_LENGTH_S * 1000u &&
           (uint32_t)(now - last_activity) < IDLE_TIMEOUT_MS;
}

bool PostGameSM(ES_Event_t event)
{
    if (!ES_PostToService(priority, event)) {
        posting_failed = true;
        return false;
    }
    return true;
}

bool InitGameSM(uint8_t service_priority)
{
    if (!io.now_ms || !io.output) return false; /* No implicit hardware/simulation. */
    priority = service_priority;
    initialized = posting_failed = false;
    state = GAME_WELCOME;
    memset(inputs, 0, sizeof inputs);
    next_ticket = 0;
    return PostGameSM((ES_Event_t){ES_INIT, 0});
}

bool GameSM_PostInput(ES_EventType_t type, uint32_t detected, uint32_t epoch)
{
    unsigned int attempt;
    Input *slot = 0;
    if (!initialized || GameSM_HasFault() ||
        (type != START_PRESSED && type != FINISH_PRESSED && type != USER_ACTIVITY)) return false;
    for (attempt = 0; attempt < INPUT_SLOTS; ++attempt) {
        ++next_ticket;
        slot = &inputs[next_ticket % INPUT_SLOTS];
        if (!slot->used) break;
    }
    if (attempt == INPUT_SLOTS) {
        posting_failed = true;
        return false;
    }
    *slot = (Input){true, next_ticket, type, detected, epoch};
    if (!PostGameSM((ES_Event_t){type, next_ticket})) {
        slot->used = false;
        return false;
    }
    /* Publish already-observed activity at ingress. An older queued idle
     * timeout must not win over activity already captured before its deadline.
     * Activity at/after the idle deadline cannot resurrect an expired game.
     */
    uint32_t now = io.now_ms();
    /* Only real activity renews idle; Start and (early) Finish never do. */
    if (cooking() && epoch == generation && type == USER_ACTIVITY &&
        (uint32_t)(now - detected) <= GAME_INPUT_MAX_AGE_MS &&
        reached(detected, last_activity) &&
        (uint32_t)(detected - last_activity) < IDLE_TIMEOUT_MS &&
        (uint32_t)(detected - started) < GAME_LENGTH_S * 1000u) {
        last_activity = detected;
    }
    return true;
}

static bool take_input(ES_Event_t event, uint32_t now, Input *input)
{
    Input *slot = &inputs[event.EventParam % INPUT_SLOTS];
    if (!slot->used || slot->ticket != event.EventParam || slot->type != event.EventType) return false;
    *input = *slot;
    slot->used = false;
    return input->generation == generation && reached(input->detected, entered) &&
           (uint32_t)(now - input->detected) <= GAME_INPUT_MAX_AGE_MS;
}

ES_Event_t RunGameSM(ES_Event_t event)
{
    ES_Event_t done = {ES_NO_EVENT, 0};
    Input input;
    bool valid_input = false;
    if (posting_failed || state == GAME_FAULT || event.EventType == ES_ERROR) return fail();
    if (!io.now_ms || !io.output) return fail();
    uint32_t now = io.now_ms();
    if (event.EventType == ES_INIT) {
        if (initialized) return done;
        initialized = true;
        return welcome(now) ? done : fail();
    }
    if (!initialized) return fail();
    if (event.EventType == GAME_ABORT) {
        /* Recoverable fault (link lost): no treat; Reset clears pan and score. */
        if (event.EventParam != (uint16_t)generation || state == GAME_WELCOME) return done;
        return welcome(now) ? done : fail();
    }
    if (event.EventType == START_PRESSED || event.EventType == FINISH_PRESSED ||
        event.EventType == USER_ACTIVITY) valid_input = take_input(event, now, &input);
    if (!refresh(now)) return fail();

    if (valid_input && input.generation == generation) {
        if (state == GAME_WELCOME && event.EventType == START_PRESSED)
            return dish_intro(now) ? done : fail();
        if (cooking() && event.EventType == FINISH_PRESSED &&
            (uint32_t)(input.detected - started) >= FINISH_ENABLE_S * 1000u)
            return end(GAME_COMPLETED, input.detected, now) ? done : fail();
        if (cooking() && event.EventType == USER_ACTIVITY && !arm_idle(now)) return fail();
    }
    if (event.EventType != ES_TIMEOUT) return done;
    if (state == GAME_DISH_INTRO && event.EventParam == WELCOME_SCROLL_TIMER && reached(now, scroll_due)) {
        uint32_t steps = (uint32_t)(now - scroll_due) / WELCOME_SCROLL_MS + 1u;
        if (steps >= (uint32_t)(banner_x + dish_width))
            return start(now, now) ? done : fail(); /* Full 60 s begins here. */
        banner_x -= (int16_t)steps;
        scroll_due += steps * WELCOME_SCROLL_MS;
        if (!emit(GAME_ACTION_DISH, banner_x) || !arm(WELCOME_SCROLL_TIMER, scroll_due - now)) return fail();
    } else if (state == GAME_WELCOME && event.EventParam == WELCOME_SCROLL_TIMER && reached(now, scroll_due)) {
        uint32_t steps = (uint32_t)(now - scroll_due) / WELCOME_SCROLL_MS + 1u;
        uint32_t cycle = 33u + BANNER_WIDTH;
        uint32_t phase = ((uint32_t)(32 - banner_x) + steps) % cycle;
        banner_x = (int16_t)(32 - (int32_t)phase);
        scroll_due += steps * WELCOME_SCROLL_MS;
        if (!emit(GAME_ACTION_WELCOME, banner_x) || !arm(WELCOME_SCROLL_TIMER, scroll_due - now)) return fail();
    } else if (cooking() && event.EventParam == GAME_TICK_TIMER && reached(now, tick_due)) {
        if (!arm_tick(now)) return fail();
    } else if (cooking() && event.EventParam == IDLE_TIMER) {
        if (!arm_idle(now)) return fail();
#ifdef GAME_BUTTONS_BENCH
    } else if (state == GAME_OVER && end_phase == END_MESSAGE &&
               event.EventParam == WELCOME_SCROLL_TIMER && reached(now, scroll_due)) {
        uint32_t steps = (uint32_t)(now - scroll_due) / WELCOME_SCROLL_MS + 1u;
        if (steps >= (uint32_t)(banner_x + end_width)) {
            /* Message fully gone: big score, E-ink clear and treat decision. */
            end_phase = END_BIG_SCORE;
            big_started = now;
            if (!emit(GAME_ACTION_BIG_SCORE, 0) || !emit(GAME_ACTION_TREAT, 1) ||
                !arm(RESULT_TIMER, END_SCORE_MS)) return fail();
            return done;
        }
        banner_x -= (int16_t)steps;
        scroll_due += steps * WELCOME_SCROLL_MS;
        if (!emit(GAME_ACTION_END_MESSAGE, banner_x) || !arm(WELCOME_SCROLL_TIMER, scroll_due - now)) return fail();
    } else if ((state == GAME_OVER && end_phase == END_BIG_SCORE && event.EventParam == RESULT_TIMER &&
                (uint32_t)(now - big_started) >= END_SCORE_MS) ||
#else
    } else if ((state == GAME_OVER && event.EventParam == RESULT_TIMER &&
                (uint32_t)(now - entered) >= RESULT_DISPLAY_MS) ||
#endif
               (state == GAME_TIMED_OUT && event.EventParam == TIMEOUT_DISPLAY_TIMER &&
                (uint32_t)(now - entered) >= TIMEOUT_DISPLAY_MS)) {
        if (!welcome(now)) return fail();
    }
    return done;
}

GameSMState_t QueryGameSM(void) { return state; }
