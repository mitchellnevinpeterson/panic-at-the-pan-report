/* CookSM: heat, burn, stir-circle and flip scoring (final_game_spec.md).
 * Owned foreground messages (ticket in EventParam, payload in a slot) as in
 * IngredientSM/ScoreSM. Every tick measures real elapsed time from now_ms, so
 * late or early ES timeouts never distort points. Score changes go to ScoreSM
 * as millipoints for the earned round; heat is batched to ~1 post per point.
 */
#include "CookSM.h"
#include "ScoreSM.h"
#include "ES_Configure.h"
#include "ES_Framework.h"
#include <string.h>

enum { MESSAGE_SLOTS = 16, INGREDIENTS = 5 };
#define DT_MAX_MS 500u           /* a stalled loop never awards more than this */
#define HEAT_DENOM (1000u * COOK_FALLOFF_MM) /* ms -> s and closeness fraction */

typedef struct {
    bool used;
    uint16_t ticket;
    ES_EventType_t type;
    uint8_t ingredient;
    uint32_t generation;
} Message;

static const uint16_t target_mm[INGREDIENTS] = COOK_TARGET_MM;
static const uint8_t ideal_circles[INGREDIENTS] = COOK_IDEAL_CIRCLES;
static const uint8_t ideal_flips[INGREDIENTS] = COOK_IDEAL_FLIPS;

static CookSMPort_t io;
static Message messages[MESSAGE_SLOTS];
static uint8_t priority;
static uint16_t next_ticket;
static bool bound, registered, initialized, posting_failed, faulted;
static bool seen_any;          /* a lifecycle generation has been accepted */
static uint32_t seen_generation;

static bool cooking, has_ingredient, burnt;
static uint8_t ingredient, flips, circles, burn_seq, zone_count, added_mask;
static uint32_t round_generation, last_ms, high_ms;
static uint32_t heat_rem;      /* sub-millipoint numerator, units of 1/HEAT_DENOM */
static int32_t heat_pending;   /* earned heat millipoints not yet posted */
static CookZone_t zone, zone_candidate;

static ES_Event_t fail(void)
{
    faulted = true;
    return (ES_Event_t){ES_ERROR, 0};
}

bool CookSM_BindPort(const CookSMPort_t *port)
{
    if (registered || !port || !port->now_ms || !port->distance_mm || !port->take_stir_circles)
        return false;
    io = *port;
    bound = true;
    return true;
}

bool CookSM_HasFault(void) { return posting_failed || faulted; }

bool PostCookSM(ES_Event_t event)
{
    if (!ES_PostToService(priority, event)) {
        posting_failed = true;
        return false;
    }
    return true;
}

/* Clears per-round state. burn_seq is a free-running edge counter for PIC2's
 * burnt cue, so it is never reset (a reset would replay the cue). */
static void clear_round(void)
{
    cooking = has_ingredient = burnt = false;
    ingredient = flips = circles = zone_count = added_mask = 0;
    high_ms = heat_rem = 0;
    heat_pending = 0;
    zone = zone_candidate = COOK_ZONE_NONE;
}

bool InitCookSM(uint8_t service_priority)
{
    if (!bound) return false;
    priority = service_priority;
    clear_round();
    round_generation = last_ms = seen_generation = 0;
    seen_any = false;
    burn_seq = 0;
    next_ticket = 0;
    initialized = posting_failed = faulted = false;
    memset(messages, 0, sizeof messages);
    registered = true;
    return PostCookSM((ES_Event_t){ES_INIT, 0});
}

static bool post_message(ES_EventType_t type, uint8_t value, uint32_t generation)
{
    unsigned int attempt;
    Message *slot = 0;
    if (!registered || CookSM_HasFault()) return false;
    for (attempt = 0; attempt < MESSAGE_SLOTS; ++attempt) {
        ++next_ticket;
        slot = &messages[next_ticket % MESSAGE_SLOTS];
        if (!slot->used) break;
    }
    if (attempt == MESSAGE_SLOTS) {
        posting_failed = true;
        return false;
    }
    *slot = (Message){true, next_ticket, type, value, generation};
    if (!PostCookSM((ES_Event_t){type, next_ticket})) {
        slot->used = false;
        return false;
    }
    return true;
}

bool CookSM_PostLifecycle(ES_EventType_t type, uint32_t generation)
{
    if (type != GAME_START && type != GAME_END && type != GAME_RESET) return false;
    return post_message(type, 0, generation);
}

bool CookSM_PostIngredient(uint8_t value, uint32_t round)
{
    if (value >= INGREDIENTS) return false;
    return post_message(INGREDIENT_ADDED, value, round);
}

bool CookSM_PostFlip(uint32_t round) { return post_message(FLIP_PRESSED, 0, round); }

CookOutput_t CookSM_Output(void)
{
    CookOutput_t out = {COOK_LCD_ATTRACT, COOK_ZONE_NONE, false, false, burn_seq, 0, 0, 0};
    if (!cooking) return out;
    out.lcd_mode = has_ingredient ? COOK_LCD_ZONE : COOK_LCD_WAITING;
    if (!has_ingredient) return out;
    out.zone = zone;
    out.sizzle = true;
    out.burnt = burnt;
    out.target_cm = (uint8_t)(target_mm[ingredient] / 10u);
    out.stirs = circles;
    out.flips = flips;
    return out;
}

static bool delta(int32_t millipoints)
{
    return millipoints == 0 || ScoreSM_PostDelta(round_generation, millipoints);
}

static bool flush_heat(void)
{
    int32_t amount = heat_pending;
    heat_pending = 0;
    return delta(amount);
}

/* Raw zone and |d - T| for the current target; no hand is LOW, closeness 0. */
static CookZone_t raw_zone(uint32_t *error_mm)
{
    uint16_t mm = 0;
    int32_t t = (int32_t)target_mm[ingredient];
    *error_mm = COOK_FALLOFF_MM;
    if (!io.distance_mm(&mm) || mm < COOK_HAND_MIN_MM || mm > COOK_HAND_MAX_MM)
        return COOK_ZONE_LOW;
    int32_t diff = (int32_t)mm - t;
    *error_mm = (uint32_t)(diff < 0 ? -diff : diff);
    if (diff < -(int32_t)COOK_BAND_MM) return COOK_ZONE_HIGH;
    if (*error_mm <= COOK_BAND_MM) return COOK_ZONE_RIGHT;
    return COOK_ZONE_LOW;
}

/* Live circle scoring: +1 for each circle up to the ingredient's ideal count,
 * -1 for every circle beyond it (oil and vegetables: ideal 0). Burnt
 * ingredients still score circles. */
static bool score_circles(uint8_t n)
{
    while (n--) {
        if (circles < UINT8_MAX) ++circles;
        if (!delta(circles <= ideal_circles[ingredient] ? COOK_CIRCLE_MILLI : -COOK_CIRCLE_MILLI))
            return false;
    }
    return true;
}

/* Accrue heat, burn and circles over the real time since the last call. */
static bool advance(CookZone_t *observed)
{
    uint32_t now = io.now_ms();
    uint32_t dt = now - last_ms;
    uint8_t new_circles = io.take_stir_circles(); /* always consume: no carry-over */
    last_ms = now;
    if (dt > DT_MAX_MS) dt = DT_MAX_MS;
    *observed = COOK_ZONE_NONE;
    if (!cooking || !has_ingredient) return true;

    uint32_t error_mm;
    CookZone_t z = raw_zone(&error_mm);
    *observed = z;
    if (!score_circles(new_circles)) return false;
    if (burnt) return true;
    if (error_mm < COOK_FALLOFF_MM) {
        /* MAX * dt/1000 * (FALLOFF-err)/FALLOFF, exact via remainder.
         * Worst case 1000*500*80 = 4e7 fits easily in 32 bits. */
        heat_rem += (uint32_t)COOK_HEAT_MAX_MILLI_PER_S * dt * (COOK_FALLOFF_MM - error_mm);
        heat_pending += (int32_t)(heat_rem / HEAT_DENOM);
        heat_rem %= HEAT_DENOM;
    }
    high_ms = z == COOK_ZONE_HIGH ? high_ms + dt : 0;
    if (high_ms >= COOK_BURN_MS) {
        burnt = true;
        burn_seq = (uint8_t)((burn_seq + 1u) & 3u);
        if (!flush_heat() || !delta(COOK_BURN_MILLI)) return false;
    }
    if (heat_pending >= 1000) return flush_heat();
    return true;
}

/* Close out the newest ingredient: post its pending heat (circles were live). */
static bool finish_ingredient(void)
{
    if (!has_ingredient) return true;
    return flush_heat();
}

static bool arm_tick(void)
{
    return ES_Timer_InitTimer(COOK_TICK_TIMER, COOK_TICK_MS) == ES_Timer_OK;
}

/* Lifecycle messages must not be older (wrap-safe) than the newest one
 * accepted, so a delayed Start/End/Reset can never undo a later one. */
static bool fresh(uint32_t generation)
{
    return !seen_any || (int32_t)(generation - seen_generation) >= 0;
}

static void see(uint32_t generation)
{
    seen_any = true;
    seen_generation = generation;
}

ES_Event_t RunCookSM(ES_Event_t event)
{
    const ES_Event_t done = {ES_NO_EVENT, 0};
    CookZone_t observed;
    if (CookSM_HasFault() || event.EventType == ES_ERROR || !registered) return fail();
    if (event.EventType == ES_INIT) {
        initialized = true; /* duplicate init must not clear an active round */
        return done;
    }
    if (!initialized) return fail();

    if (event.EventType == ES_TIMEOUT) {
        if (event.EventParam != COOK_TICK_TIMER || !cooking) return done; /* stale */
        if (!advance(&observed)) return fail();
        if (has_ingredient) {
            if (observed == zone_candidate) {
                if (zone_count < COOK_ZONE_CONFIRM_TICKS) ++zone_count;
            } else {
                zone_candidate = observed;
                zone_count = 1;
            }
            if (zone_count >= COOK_ZONE_CONFIRM_TICKS) zone = zone_candidate;
        }
        return arm_tick() ? done : fail();
    }
    if (event.EventType != GAME_START && event.EventType != GAME_END &&
        event.EventType != GAME_RESET && event.EventType != INGREDIENT_ADDED &&
        event.EventType != FLIP_PRESSED) return done;

    Message *slot = &messages[event.EventParam % MESSAGE_SLOTS];
    if (!slot->used || slot->ticket != event.EventParam || slot->type != event.EventType)
        return done; /* raw, duplicate or already-consumed queue entry */
    Message message = *slot;
    slot->used = false;

    switch (event.EventType) {
    case GAME_START:
        if (!fresh(message.generation) ||
            (cooking && message.generation == round_generation)) return done;
        see(message.generation);
        clear_round();
        round_generation = message.generation;
        cooking = true;
        last_ms = io.now_ms();
        return arm_tick() ? done : fail();
    case GAME_END:
        if (!cooking || !fresh(message.generation)) return done;
        see(message.generation);
        if (!advance(&observed) || !finish_ingredient()) return fail();
        clear_round();
        return ES_Timer_StopTimer(COOK_TICK_TIMER) == ES_Timer_OK ? done : fail();
    case GAME_RESET:
        if (!fresh(message.generation)) return done;
        see(message.generation);
        clear_round(); /* abandoned round: pending heat is discarded */
        return ES_Timer_StopTimer(COOK_TICK_TIMER) == ES_Timer_OK ? done : fail();
    case INGREDIENT_ADDED: {
        uint8_t bit = (uint8_t)(1u << message.ingredient);
        /* Defensive: IngredientSM already dedupes; a repeat must not rescore. */
        if (!cooking || message.generation != round_generation || (added_mask & bit))
            return done;
        added_mask |= bit;
        if (!advance(&observed) || !finish_ingredient()) return fail();
        uint32_t error_mm;
        has_ingredient = true;
        ingredient = message.ingredient;
        burnt = false;
        flips = circles = 0;
        high_ms = 0;
        /* New target: publish its zone at once rather than a stale one. */
        zone = zone_candidate = raw_zone(&error_mm);
        zone_count = 1;
        return done;
    }
    default: /* FLIP_PRESSED */
        if (!cooking || message.generation != round_generation || !has_ingredient)
            return done; /* empty pan: 0 */
        if (flips < UINT8_MAX) ++flips;
        return delta(flips <= ideal_flips[ingredient] ? COOK_FLIP_MILLI : -COOK_FLIP_MILLI)
            ? done : fail();
    }
}
