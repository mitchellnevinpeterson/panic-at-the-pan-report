#include "HeatSM.h"
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
static HeatSMPort_t io;
static GameTuning_t tuning;
static Message messages[MESSAGE_SLOTS];
static uint8_t priority, ingredient, added_mask;
static uint16_t next_ticket;
static uint32_t round_generation;
static bool registered, initialized, posting_failed, playing;
static HeatSMState_t state = HEAT_PAN_EMPTY;
static uint32_t sample_due, last_sample, hot_since, ingredient_at;
static uint16_t previous_closeness;
static bool have_sample, hot_clock;

bool HeatSM_BindPort(const HeatSMPort_t *port, const GameTuning_t *configuration)
{
    if (registered || !port || !configuration ||
        (configuration->configured && !GameTuning_IsValid(configuration)) ||
        !port->now_ms || !port->generation || !port->accept_input || !port->accepts_round ||
        !port->read_distance || !port->output || !port->score_heat || !port->score_burn) return false;
    io = *port;
    tuning = *configuration;
    return true;
}
bool HeatSM_IsConfigured(void) { return tuning.configured; }
bool HeatSM_HasFault(void) { return posting_failed || state == HEAT_FAULT; }
HeatSMState_t QueryHeatSM(void) { return state; }

bool PostHeatSM(ES_Event_t event)
{
    if (!ES_PostToService(priority, event)) { posting_failed = true; return false; }
    return true;
}
static bool post_message(ES_EventType_t type, uint8_t id, uint32_t generation,
                         uint32_t at, uint32_t angle, bool home, GameSMEndReason_t reason)
{
    Message *slot = 0;
    unsigned attempt;
    if (!registered || HeatSM_HasFault()) return false;
    for (attempt = 0; attempt < MESSAGE_SLOTS; ++attempt) {
        ++next_ticket;
        slot = &messages[next_ticket % MESSAGE_SLOTS];
        if (!slot->used) break;
    }
    if (attempt == MESSAGE_SLOTS) { posting_failed = true; return false; }
    *slot = (Message){true, next_ticket, type, id, generation, at, angle, home, reason};
    if (!PostHeatSM((ES_Event_t){type, next_ticket})) { slot->used = false; return false; }
    return true;
}
bool HeatSM_PostIngredient(uint8_t id, uint32_t round, uint32_t at_ms)
{
    if (id >= INGREDIENT_COUNT) return false;
    return post_message(INGREDIENT_ADDED, id, round, at_ms, 0, false, GAME_COMPLETED);
}

bool HeatSM_PostLifecycle(ES_EventType_t type, uint32_t generation, uint32_t at_ms)
{
    if (type != GAME_START && type != GAME_END && type != GAME_RESET) return false;
    return post_message(type, 0, generation, at_ms, 0, false, GAME_COMPLETED);
}
static bool stop(void)
{
    have_sample = hot_clock = false;
    state = HEAT_PAN_EMPTY;
    bool ok = ES_Timer_StopTimer(HEAT_SAMPLE_TIMER) == ES_Timer_OK;
    if (ES_Timer_StopTimer(BURN_TIMER) != ES_Timer_OK) ok = false;
    if (!io.output(0, false, false)) ok = false;
    return ok;
}
static ES_Event_t fail(void)
{
    if (io.output) (void)stop();
    playing = false;
    state = HEAT_FAULT;
    return (ES_Event_t){ES_ERROR, 0};
}
bool InitHeatSM(uint8_t service_priority)
{
    if (!io.now_ms) return false;
    priority = service_priority;
    initialized = posting_failed = playing = have_sample = hot_clock = false;
    state = HEAT_PAN_EMPTY;
    next_ticket = added_mask = ingredient = 0;
    round_generation = 0;
    memset(messages, 0, sizeof messages);
    registered = true;
    return PostHeatSM((ES_Event_t){ES_INIT, 0});
}
static bool arm_sample(void)
{
    sample_due = io.now_ms() + HEAT_SAMPLE_MS;
    return ES_Timer_InitTimer(HEAT_SAMPLE_TIMER, HEAT_SAMPLE_MS) == ES_Timer_OK;
}
static bool sample(void)
{
    uint16_t mm;
    uint32_t at;
    uint32_t now = io.now_ms();
    /* Every valid sample must be fresh, from this ingredient, and distinct.
     * Invalid/repeated/out-of-order readings break the burn and scoring clocks. */
    if (!io.read_distance(&mm, &at) || now - at > tuning.heat_sample_max_age_ms ||
        (int32_t)(at - ingredient_at) < 0 ||
        ((have_sample || hot_clock) && ((int32_t)(at - last_sample) <= 0 ||
                         at - last_sample > tuning.heat_sample_max_age_ms))) {
        have_sample = hot_clock = false;
        if (state != HEAT_BURNING) state = HEAT_COOKING;
        return io.output(0, state == HEAT_BURNING, false) && arm_sample();
    }
    uint16_t heat = mm <= tuning.distance_min_mm ? 1000u :
                    mm >= tuning.distance_max_mm ? 0u :
                    (uint16_t)(((uint32_t)tuning.distance_max_mm - mm) * 1000u /
                               (tuning.distance_max_mm - tuning.distance_min_mm));
    uint16_t target = tuning.heat_target[ingredient];
    uint16_t error = heat > target ? heat - target : target - heat;
    uint16_t closeness = error >= target ? 0u : (uint16_t)((target - error) * 1000u / target);
    bool too_hot = (uint32_t)heat * 2u > (uint32_t)target * 3u;
    if (state != HEAT_BURNING) {
        if (have_sample && !io.score_heat(ingredient, round_generation,
                (uint16_t)(((uint32_t)previous_closeness + closeness) / 2u), at - last_sample))
            return false;
        if (!too_hot) {
            hot_clock = false;
            state = HEAT_COOKING;
        } else {
            if (!hot_clock) { hot_since = at; hot_clock = true; }
            state = HEAT_OVERHEATING;
            if (at - hot_since >= BURN_TIME_MS) {
                state = HEAT_BURNING;
                hot_clock = false;
                if (!io.score_burn(ingredient, round_generation)) return false;
            }
        }
    }
    have_sample = true;
    last_sample = at;
    previous_closeness = closeness;
    return io.output(heat, state == HEAT_BURNING, true) && arm_sample();
}
ES_Event_t RunHeatSM(ES_Event_t event)
{
    const ES_Event_t done = {ES_NO_EVENT, 0};
    if (!registered || HeatSM_HasFault() || event.EventType == ES_ERROR) return fail();
    if (event.EventType == ES_INIT) {
        if (initialized) return done;
        initialized = true;
        return stop() ? done : fail();
    }
    if (!initialized) return fail();
    if (event.EventType == ES_TIMEOUT) {
        if (event.EventParam != HEAT_SAMPLE_TIMER || state == HEAT_PAN_EMPTY ||
            !playing || (int32_t)(io.now_ms() - sample_due) < 0) return done;
        /* A queued tick cannot actuate or earn after the authoritative deadline. */
        if (!io.accept_input(io.now_ms(), round_generation)) return stop() ? done : fail();
        return sample() ? done : fail();
    }
    if (event.EventType != GAME_START && event.EventType != GAME_END &&
        event.EventType != GAME_RESET && event.EventType != INGREDIENT_ADDED) return done;
    Message *slot = &messages[event.EventParam % MESSAGE_SLOTS];
    if (!slot->used || slot->ticket != event.EventParam || slot->type != event.EventType) return done;
    Message message = *slot;
    slot->used = false;
    if (message.generation != io.generation()) return done;
    if (event.EventType == GAME_RESET || event.EventType == GAME_END) {
        playing = false;
        if (event.EventType == GAME_RESET) added_mask = 0;
        return stop() ? done : fail();
    }
    if (event.EventType == GAME_START) {
        if (playing && round_generation == message.generation) return done;
        playing = true;
        round_generation = message.generation;
        added_mask = 0;
        return stop() ? done : fail();
    }
    if (!playing || !tuning.configured || message.generation != round_generation ||
        !io.accept_input(message.at, message.generation)) return done;
    uint8_t bit = (uint8_t)(1u << message.ingredient);
    if (added_mask & bit) return done;
    added_mask |= bit;
    ingredient = message.ingredient;
    ingredient_at = message.at;
    /* Team explicitly selects whether sustained overheating transfers when
     * adding an ingredient. Burnt status never transfers to the new ingredient. */
    bool preserve = tuning.burn_transfer_rule == GAME_BURN_TRANSFER_PRESERVE &&
                    state == HEAT_OVERHEATING && hot_clock;
    have_sample = false;
    hot_clock = preserve;
    state = preserve ? HEAT_OVERHEATING : HEAT_COOKING;
    return io.output(0, false, false) && arm_sample() ? done : fail();
}
