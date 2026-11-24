#include "StirSM.h"
#include "ES_Framework.h"
#include "GameSM.h"
#include <string.h>

enum { MESSAGE_SLOTS = 16 };
typedef struct {
    bool used;
    uint16_t ticket;
    ES_EventType_t type;
    uint8_t ingredient;
    uint32_t generation, at, angle;
    bool home;
    GameSMEndReason_t reason;
} Message;
static StirSMPort_t io;
static GameTuning_t tuning;
static Message messages[MESSAGE_SLOTS];
static uint8_t priority, ingredient, added_mask;
static uint16_t next_ticket;
static uint32_t round_generation;
static bool registered, initialized, posting_failed, playing;
static StirSMState_t state = STIR_IDLE;
static uint32_t tick_due, last_at, last_angle, elapsed, ingredient_at;
static bool have_angle, have_observation;

bool StirSM_BindPort(const StirSMPort_t *port, const GameTuning_t *configuration)
{
    if (registered || !port || !configuration ||
        (configuration->configured && !GameTuning_IsValid(configuration)) ||
        !port->now_ms || !port->generation || !port->accept_input || !port->accepts_round ||
        !port->motor_target || !port->motion || !port->score_stir) return false;
    io = *port;
    tuning = *configuration;
    return true;
}
bool StirSM_IsConfigured(void) { return tuning.configured; }
bool StirSM_HasFault(void) { return posting_failed || state == STIR_FAULT; }
StirSMState_t QueryStirSM(void) { return state; }

bool PostStirSM(ES_Event_t event)
{
    if (!ES_PostToService(priority, event)) { posting_failed = true; return false; }
    return true;
}
static bool post_message(ES_EventType_t type, uint8_t id, uint32_t generation,
                         uint32_t at, uint32_t angle, bool home, GameSMEndReason_t reason)
{
    Message *slot = 0;
    unsigned attempt;
    if (!registered || StirSM_HasFault()) return false;
    for (attempt = 0; attempt < MESSAGE_SLOTS; ++attempt) {
        ++next_ticket;
        slot = &messages[next_ticket % MESSAGE_SLOTS];
        if (!slot->used) break;
    }
    if (attempt == MESSAGE_SLOTS) { posting_failed = true; return false; }
    *slot = (Message){true, next_ticket, type, id, generation, at, angle, home, reason};
    if (!PostStirSM((ES_Event_t){type, next_ticket})) { slot->used = false; return false; }
    return true;
}
bool StirSM_PostIngredient(uint8_t id, uint32_t round, uint32_t at_ms)
{
    if (id >= INGREDIENT_COUNT) return false;
    return post_message(INGREDIENT_ADDED, id, round, at_ms, 0, false, GAME_COMPLETED);
}

uint32_t StirSM_ElapsedMs(void) { return elapsed; }
bool StirSM_PostLifecycle(ES_EventType_t type, uint32_t generation, uint32_t at_ms)
{
    if (type != GAME_START && type != GAME_END && type != GAME_RESET) return false;
    return post_message(type, 0, generation, at_ms, 0, false, GAME_COMPLETED);
}
bool StirSM_PostMotion(uint32_t angle, bool home, uint32_t at, uint32_t generation)
{
    if (angle >= 360000u) return false;
    /* STIR_START is an owned angle observation, not a persistent movement latch. */
    return post_message(home ? STIR_STOP : STIR_START, 0, generation, at, angle, home, GAME_COMPLETED);
}
static bool motion(bool moving, uint32_t at)
{
    StirSMState_t target = moving ? STIR_STIRRING : STIR_NOT_STIRRING;
    if (state == target) return true;
    state = target;
    return io.motion(moving, at, round_generation);
}
static bool stop(uint32_t at)
{
    bool ok = ES_Timer_StopTimer(STIR_TICK_TIMER) == ES_Timer_OK;
    if (state == STIR_STIRRING && !io.motion(false, at, round_generation)) ok = false;
    /* Angle is ignored when disabled, including in unconfigured mode. */
    if (!io.motor_target(0, true, false)) ok = false;
    state = STIR_IDLE;
    have_angle = have_observation = false;
    return ok;
}
static ES_Event_t fail(void)
{
    if (io.motor_target) (void)stop(io.now_ms());
    state = STIR_FAULT;
    playing = false;
    return (ES_Event_t){ES_ERROR, 0};
}
bool InitStirSM(uint8_t service_priority)
{
    if (!io.now_ms) return false;
    priority = service_priority;
    initialized = posting_failed = playing = have_angle = have_observation = false;
    state = STIR_IDLE;
    next_ticket = added_mask = ingredient = 0;
    round_generation = elapsed = 0;
    memset(messages, 0, sizeof messages);
    registered = true;
    return PostStirSM((ES_Event_t){ES_INIT, 0});
}
static bool arm_tick(void)
{
    tick_due = io.now_ms() + STIR_TICK_MS;
    return ES_Timer_InitTimer(STIR_TICK_TIMER, STIR_TICK_MS) == ES_Timer_OK;
}
static bool result(void)
{
    /* The old earned round remains accepted during results, even though End
     * has already changed GameSM's input generation. No points on Reset. */
    return !tuning.configured || state == STIR_IDLE || !io.accepts_round(round_generation) ||
           io.score_stir(ingredient, round_generation, elapsed);
}
ES_Event_t RunStirSM(ES_Event_t event)
{
    const ES_Event_t done = {ES_NO_EVENT, 0};
    if (!registered || StirSM_HasFault() || event.EventType == ES_ERROR) return fail();
    if (event.EventType == ES_INIT) {
        if (initialized) return done;
        initialized = true;
        return stop(io.now_ms()) ? done : fail();
    }
    if (!initialized) return fail();
    if (event.EventType == ES_TIMEOUT) {
        if (event.EventParam != STIR_TICK_TIMER || state == STIR_IDLE ||
            !playing || (int32_t)(io.now_ms() - tick_due) < 0) return done;
        /* Do not erase the final result while End is queued: disable motor,
         * stop sampling, and retain ingredient/time until the owned End. */
        if (!io.accept_input(io.now_ms(), round_generation)) {
            if (!io.motor_target(0, true, false) ||
                ES_Timer_StopTimer(STIR_TICK_TIMER) != ES_Timer_OK) return fail();
            return done;
        }
        if (have_angle && io.now_ms() - last_at >= tuning.stir_debounce_ms) {
            have_angle = false;
            if (!motion(false, io.now_ms())) return fail();
        }
        return arm_tick() ? done : fail();
    }
    if (event.EventType != GAME_START && event.EventType != GAME_END &&
        event.EventType != GAME_RESET && event.EventType != INGREDIENT_ADDED &&
        event.EventType != STIR_START && event.EventType != STIR_STOP) return done;
    Message *slot = &messages[event.EventParam % MESSAGE_SLOTS];
    if (!slot->used || slot->ticket != event.EventParam || slot->type != event.EventType) return done;
    Message message = *slot;
    slot->used = false;
    if (message.generation != io.generation()) return done;
    if (event.EventType == GAME_END || event.EventType == GAME_RESET) {
        if (event.EventType == GAME_END && !result()) return fail();
        playing = false;
        if (!stop(message.at)) return fail();
        if (event.EventType == GAME_RESET) { elapsed = 0; added_mask = 0; }
        return done;
    }
    if (event.EventType == GAME_START) {
        if (playing && round_generation == message.generation) return done;
        if (!stop(message.at)) return fail();
        round_generation = message.generation;
        elapsed = added_mask = 0;
        playing = true;
        return done;
    }
    if (!playing || message.generation != round_generation ||
        !io.accept_input(message.at, message.generation)) return done;
    if (event.EventType == INGREDIENT_ADDED) {
        uint8_t bit = (uint8_t)(1u << message.ingredient);
        if (added_mask & bit) return done;
        if (!result()) return fail();
        added_mask |= bit;
        ingredient = message.ingredient;
        ingredient_at = message.at;
        elapsed = 0;
        have_angle = have_observation = false;
        if (!motion(false, message.at)) return fail();
        if (!tuning.configured) return done; /* Logical only; motor stays disabled. */
        return io.motor_target(tuning.stir_home_angle_mdeg, true, true) && arm_tick() ? done : fail();
    }
    if (state == STIR_IDLE || !tuning.configured ||
        (int32_t)(message.at - ingredient_at) < 0 ||
        (have_observation && (int32_t)(message.at - last_at) <= 0)) return done;
    if (!io.motor_target(message.home ? tuning.stir_home_angle_mdeg : message.angle,
                         message.home, true)) return fail();
    bool moving = false;
    if (!message.home && have_angle) {
        uint32_t delta = message.angle > last_angle ? message.angle - last_angle : last_angle - message.angle;
        if (delta > 180000u) delta = 360000u - delta;
        uint32_t dt = message.at - last_at;
        moving = dt <= tuning.stir_debounce_ms && delta >= tuning.stir_motion_threshold_mdeg;
        if (moving) {
            /* Credit only the observed interval. Ticks never extend motion.
             * This is commanded movement, not motor-position feedback. */
            uint32_t limit = GAME_LENGTH_S * 1000u;
            elapsed = dt > limit - elapsed ? limit : elapsed + dt;
        }
    }
    have_observation = true;
    have_angle = !message.home;
    last_angle = message.angle;
    last_at = message.at;
    return motion(moving, message.at) ? done : fail();
}
