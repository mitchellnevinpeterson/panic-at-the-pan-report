#include "IngredientSM.h"
#include "GameConfig.h"
#include "ES_Framework.h"
#include <string.h>

#if INGREDIENT_COUNT > 8 || INGREDIENT_COUNT < 1
#error "Ingredient mask requires between one and eight ingredients."
#endif

enum { MESSAGE_SLOTS = 16 };
typedef struct {
    bool used;
    uint16_t ticket;
    ES_EventType_t type;
    uint8_t ingredient;
    uint32_t detected, generation;
} Message;

static IngredientSMPort_t io;
static Message messages[MESSAGE_SLOTS];
static uint8_t service_priority, added_mask;
static uint16_t next_ticket;
static uint32_t active_generation;
static uint32_t accepted_at;
static bool registered, initialized, posting_failed;
static IngredientSMState_t current_state = INGREDIENT_LOCKED;

static bool emit(IngredientSMAction_t action, uint8_t value)
{
    return io.output && io.output(action, value);
}

static ES_Event_t fail(void)
{
    current_state = INGREDIENT_FAULT;
    (void)emit(INGREDIENT_ACTION_STATE, INGREDIENT_FAULT);
    return (ES_Event_t){ES_ERROR, 0};
}

bool IngredientSM_BindPort(const IngredientSMPort_t *port)
{
    if (registered || !port || !port->generation || !port->accept_input || !port->output)
        return false;
    io = *port;
    return true;
}

bool IngredientSM_HasFault(void)
{
    return posting_failed || current_state == INGREDIENT_FAULT;
}

bool PostIngredientSM(ES_Event_t event)
{
    if (!ES_PostToService(service_priority, event)) {
        posting_failed = true;
        return false;
    }
    return true;
}

bool InitIngredientSM(uint8_t priority)
{
    if (!io.generation || !io.accept_input || !io.output) return false;
    service_priority = priority;
    current_state = INGREDIENT_LOCKED;
    added_mask = 0;
    active_generation = 0;
    next_ticket = 0;
    initialized = posting_failed = false;
    memset(messages, 0, sizeof messages);
    registered = true;
    return PostIngredientSM((ES_Event_t){ES_INIT, 0});
}

static bool post_message(ES_EventType_t type, uint8_t ingredient,
                         uint32_t detected, uint32_t generation)
{
    unsigned int attempt;
    Message *slot = 0;
    if (!registered || IngredientSM_HasFault()) return false;
    for (attempt = 0; attempt < MESSAGE_SLOTS; ++attempt) {
        ++next_ticket;
        slot = &messages[next_ticket % MESSAGE_SLOTS];
        if (!slot->used) break;
    }
    if (attempt == MESSAGE_SLOTS) {
        posting_failed = true;
        return false;
    }
    *slot = (Message){true, next_ticket, type, ingredient, detected, generation};
    if (!PostIngredientSM((ES_Event_t){type, next_ticket})) {
        slot->used = false;
        return false;
    }
    return true;
}

bool IngredientSM_PostLifecycle(ES_EventType_t type, uint32_t generation)
{
    if (type != GAME_START && type != GAME_END && type != GAME_RESET) return false;
    return post_message(type, 0, 0, generation);
}

bool IngredientSM_PostSwitch(uint8_t ingredient, uint32_t detected, uint32_t generation)
{
    if (!initialized || ingredient >= INGREDIENT_COUNT) return false;
    return post_message(SWITCH_FLIPPED, ingredient, detected, generation);
}

ES_Event_t RunIngredientSM(ES_Event_t event)
{
    const ES_Event_t done = {ES_NO_EVENT, 0};
    if (IngredientSM_HasFault() || event.EventType == ES_ERROR) return fail();
    if (!registered) return fail();
    if (event.EventType == ES_INIT) {
        initialized = true; /* Duplicate init must not clear an active round. */
        return done;
    }
    if (!initialized) return fail();
    if (event.EventType != SWITCH_FLIPPED && event.EventType != GAME_START &&
        event.EventType != GAME_END && event.EventType != GAME_RESET) return done;

    Message *slot = &messages[event.EventParam % MESSAGE_SLOTS];
    if (!slot->used || slot->ticket != event.EventParam || slot->type != event.EventType)
        return done; /* Raw, already-consumed or otherwise stale queue entry. */
    Message message = *slot;
    slot->used = false;
    if (message.generation != io.generation()) {
        if (event.EventType == SWITCH_FLIPPED &&
            !emit(INGREDIENT_ACTION_STALE, message.ingredient)) return fail();
        return done; /* An old lifecycle must not unlock/reset the current game. */
    }

    if (event.EventType == GAME_START) {
        if (current_state == INGREDIENT_READY && active_generation == message.generation)
            return done; /* Repeated Start never clears an accepted mask. */
        active_generation = message.generation;
        added_mask = 0;
        current_state = INGREDIENT_READY;
        return emit(INGREDIENT_ACTION_STATE, current_state) ? done : fail();
    }
    if (event.EventType == GAME_END || event.EventType == GAME_RESET) {
        current_state = INGREDIENT_LOCKED;
        if (event.EventType == GAME_RESET) added_mask = 0;
        return emit(INGREDIENT_ACTION_STATE, current_state) ? done : fail();
    }
    if (current_state != INGREDIENT_READY)
        return emit(INGREDIENT_ACTION_LOCKED, message.ingredient) ? done : fail();
    /* A switch may precede END in our queue even though GameSM has already
     * ended the round. Recheck the authoritative generation and live deadlines.
     */
    if (message.generation != active_generation ||
        !io.accept_input(message.detected, message.generation))
        return emit(INGREDIENT_ACTION_STALE, message.ingredient) ? done : fail();

    uint8_t bit = (uint8_t)(1u << message.ingredient);
    if (added_mask & bit)
        return emit(INGREDIENT_ACTION_DUPLICATE, message.ingredient) ? done : fail();
    added_mask |= bit;
    accepted_at = message.detected;
    /* Any partial downstream fan-out failure is fatal; never retry and score
     * an ingredient twice. The bench publishes only a terminal stub for now.
     */
    return emit(INGREDIENT_ACTION_ADDED, message.ingredient) ? done : fail();
}

IngredientSMState_t QueryIngredientSM(void) { return current_state; }
uint8_t IngredientSM_AddedMask(void) { return added_mask; }
uint32_t IngredientSM_LastAcceptedAt(void) { return accepted_at; }
