#include "PanDisplaySM.h"
#include "ES_Framework.h"
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
static PanDisplaySMPort_t io;
static GameTuning_t tuning;
static Message messages[MESSAGE_SLOTS];
static uint8_t priority, ingredient, added_mask;
static uint16_t next_ticket;
static uint32_t round_generation;
static bool registered, initialized, posting_failed, playing;
static PanDisplaySMState_t state = PAN_ATTRACT;
static uint32_t icon_due, last_motion_at;
static bool stirring, motion_seen;

bool PanDisplaySM_BindPort(const PanDisplaySMPort_t *port, const GameTuning_t *configuration)
{
    if (registered || !port || !configuration ||
        (configuration->configured && !GameTuning_IsValid(configuration)) ||
        !port->now_ms || !port->generation || !port->accept_input || !port->frame) return false;
    io = *port;
    tuning = *configuration;
    return true;
}
bool PanDisplaySM_IsConfigured(void) { return tuning.configured; }
bool PanDisplaySM_HasFault(void) { return posting_failed || state == PAN_FAULT; }
PanDisplaySMState_t QueryPanDisplaySM(void) { return state; }

bool PostPanDisplaySM(ES_Event_t event)
{
    if (!ES_PostToService(priority, event)) { posting_failed = true; return false; }
    return true;
}
static bool post_message(ES_EventType_t type, uint8_t id, uint32_t generation,
                         uint32_t at, uint32_t angle, bool home, GameSMEndReason_t reason)
{
    Message *slot = 0;
    unsigned attempt;
    if (!registered || PanDisplaySM_HasFault()) return false;
    for (attempt = 0; attempt < MESSAGE_SLOTS; ++attempt) {
        ++next_ticket;
        slot = &messages[next_ticket % MESSAGE_SLOTS];
        if (!slot->used) break;
    }
    if (attempt == MESSAGE_SLOTS) { posting_failed = true; return false; }
    *slot = (Message){true, next_ticket, type, id, generation, at, angle, home, reason};
    if (!PostPanDisplaySM((ES_Event_t){type, next_ticket})) { slot->used = false; return false; }
    return true;
}
bool PanDisplaySM_PostIngredient(uint8_t id, uint32_t round, uint32_t at_ms)
{
    if (id >= INGREDIENT_COUNT) return false;
    return post_message(INGREDIENT_ADDED, id, round, at_ms, 0, false, GAME_COMPLETED);
}

bool PanDisplaySM_PostLifecycle(ES_EventType_t type, uint32_t generation, uint32_t at_ms,
                                GameSMEndReason_t reason)
{
    if (type != GAME_START && type != GAME_END && type != GAME_RESET) return false;
    if (reason != GAME_COMPLETED && reason != GAME_IDLE_TIMEOUT) return false;
    return post_message(type, 0, generation, at_ms, 0, false, reason);
}
bool PanDisplaySM_PostMotion(bool moving, uint32_t at, uint32_t generation)
{
    return post_message(moving ? STIR_START : STIR_STOP, 0, generation, at, 0, false, GAME_COMPLETED);
}
static bool frame(PanDisplaySMState_t target)
{
    state = target;
    return io.frame(state, added_mask, ingredient);
}
static bool stop_timers(void)
{
    bool ok = ES_Timer_StopTimer(ICON_TIMER) == ES_Timer_OK;
    if (ES_Timer_StopTimer(ANIM_TIMER) != ES_Timer_OK) ok = false;
    return ok;
}
static ES_Event_t fail(void)
{
    (void)stop_timers();
    playing = false;
    state = PAN_FAULT;
    return (ES_Event_t){ES_ERROR, 0};
}
bool InitPanDisplaySM(uint8_t service_priority)
{
    if (!io.now_ms) return false;
    priority = service_priority;
    initialized = posting_failed = playing = stirring = motion_seen = false;
    state = PAN_ATTRACT;
    next_ticket = added_mask = ingredient = 0;
    round_generation = 0;
    memset(messages, 0, sizeof messages);
    registered = true;
    return PostPanDisplaySM((ES_Event_t){ES_INIT, 0});
}
ES_Event_t RunPanDisplaySM(ES_Event_t event)
{
    const ES_Event_t done = {ES_NO_EVENT, 0};
    if (!registered || PanDisplaySM_HasFault() || event.EventType == ES_ERROR) return fail();
    if (event.EventType == ES_INIT) {
        if (initialized) return done;
        initialized = true;
        return stop_timers() && frame(PAN_ATTRACT) ? done : fail();
    }
    if (!initialized) return fail();
    if (event.EventType == ES_TIMEOUT) {
        if (event.EventParam != ICON_TIMER || !playing || state != PAN_NEW_INGREDIENT ||
            (int32_t)(io.now_ms() - icon_due) < 0 ||
            !io.accept_input(io.now_ms(), round_generation)) return done;
        return frame(stirring ? PAN_SWIRL : PAN_VIEW) ? done : fail();
    }
    if (event.EventType != GAME_START && event.EventType != GAME_END &&
        event.EventType != GAME_RESET && event.EventType != INGREDIENT_ADDED &&
        event.EventType != STIR_START && event.EventType != STIR_STOP) return done;
    Message *slot = &messages[event.EventParam % MESSAGE_SLOTS];
    if (!slot->used || slot->ticket != event.EventParam || slot->type != event.EventType) return done;
    Message message = *slot;
    slot->used = false;
    if (message.generation != io.generation()) return done;
    if (event.EventType == GAME_START) {
        if (playing && round_generation == message.generation) return done;
        round_generation = message.generation;
        playing = true;
        stirring = motion_seen = false;
        added_mask = 0;
        return stop_timers() && frame(PAN_VIEW) ? done : fail();
    }
    if (event.EventType == GAME_RESET || event.EventType == GAME_END) {
        playing = stirring = motion_seen = false;
        if (event.EventType == GAME_RESET) added_mask = 0;
        PanDisplaySMState_t target = event.EventType == GAME_RESET ? PAN_ATTRACT :
            message.reason == GAME_COMPLETED ? PAN_GAME_OVER_TEXT : PAN_TIMEOUT_ICON;
        return stop_timers() && frame(target) ? done : fail();
    }
    if (!playing || message.generation != round_generation ||
        !io.accept_input(message.at, message.generation)) return done;
    if (event.EventType == INGREDIENT_ADDED) {
        uint8_t bit = (uint8_t)(1u << message.ingredient);
        if (added_mask & bit) return done;
        added_mask |= bit;
        ingredient = message.ingredient;
        icon_due = io.now_ms() + ICON_DISPLAY_MS;
        return frame(PAN_NEW_INGREDIENT) &&
               ES_Timer_InitTimer(ICON_TIMER, ICON_DISPLAY_MS) == ES_Timer_OK ? done : fail();
    }
    if (motion_seen && (int32_t)(message.at - last_motion_at) < 0) return done;
    motion_seen = true;
    last_motion_at = message.at;
    stirring = event.EventType == STIR_START;
    if (state == PAN_NEW_INGREDIENT) return done; /* Ingredient icon wins. */
    PanDisplaySMState_t target = stirring ? PAN_SWIRL : PAN_VIEW;
    return state == target || frame(target) ? done : fail();
}
