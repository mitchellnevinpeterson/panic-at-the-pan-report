#include "ScoreSM.h"
#include "GameConfig.h"
#include "ES_Framework.h"
#include <string.h>

#if INGREDIENT_COUNT < 1 || INGREDIENT_COUNT > 8
#error "Score ingredient mask requires between one and eight ingredients."
#endif

enum { MESSAGE_SLOTS = 16, DISPLAY_MAX_SCORE = 9999, DISPLAY_MIN_SCORE = -999 };
/* One PostDelta is at most +/-1000 points; real awards are <= 10 points. */
#define DELTA_MAX_MILLI 1000000
#define MILLI_MAX ((int32_t)DISPLAY_MAX_SCORE * 1000)
#define MILLI_MIN ((int32_t)DISPLAY_MIN_SCORE * 1000)
typedef struct {
    bool used;
    uint16_t ticket;
    ES_EventType_t type;
    uint8_t ingredient;
    uint32_t generation;
    uint16_t closeness;
    uint32_t elapsed;
    int32_t delta; /* SCORE_DELTA millipoints */
} Message;
static ScoreSMPort_t io;
static Message messages[MESSAGE_SLOTS];
static uint8_t priority, scored_mask, burned_mask, stirred_mask;
static GameTuning_t tuning;
static bool full_scoring, formal_mode;
/* One million internal units per point avoids losing fractional tick awards. */
static int64_t balance;
static uint16_t next_ticket;
static uint32_t round_generation;
static int32_t score;
/* Non-formal (bench / final game) running total in millipoints; score is the
 * whole-point view. Saturated to the display range so it can never overflow. */
static int32_t milli;
static uint8_t next_recipe, round_recipe; /* per-round recipe (SetRecipeMask) */
static bool registered, initialized, posting_failed;
static ScoreSMState_t state = SCORE_IDLE;

static ES_Event_t fail(void)
{
    state = SCORE_FAULT;
    return (ES_Event_t){ES_ERROR, 0};
}

bool ScoreSM_BindPort(const ScoreSMPort_t *port)
{
    if (registered || !port || !port->generation || !port->accepts_round || !port->display ||
        port->ingredient_points < 0 || port->ingredient_points > (int32_t)(DISPLAY_MAX_SCORE / INGREDIENT_COUNT))
        return false;
    io = *port;
    next_recipe = round_recipe = port->recipe_mask;
    return true;
}

bool ScoreSM_SetRecipeMask(uint8_t recipe_mask)
{
    if (recipe_mask & (uint8_t)~((1u << INGREDIENT_COUNT) - 1u)) return false;
    next_recipe = recipe_mask;
    return true;
}

bool ScoreSM_BindTuning(const GameTuning_t *configuration)
{
    if (registered || !configuration ||
        (configuration->configured && !GameTuning_IsValid(configuration))) return false;
    tuning = *configuration;
    formal_mode = true;
    full_scoring = tuning.configured;
    return true;
}

bool ScoreSM_IsConfigured(void) { return full_scoring; }
bool ScoreSM_HasFault(void) { return posting_failed || state == SCORE_FAULT; }
int32_t ScoreSM_Value(void) { return score; }
ScoreSMState_t QueryScoreSM(void) { return state; }

bool PostScoreSM(ES_Event_t event)
{
    if (!ES_PostToService(priority, event)) {
        posting_failed = true;
        return false;
    }
    return true;
}

bool InitScoreSM(uint8_t service_priority)
{
    if (!io.generation || !io.accepts_round || !io.display) return false;
    priority = service_priority;
    state = SCORE_IDLE;
    score = milli = 0;
    scored_mask = burned_mask = stirred_mask = 0;
    balance = 0;
    round_generation = 0;
    next_ticket = 0;
    initialized = posting_failed = false;
    memset(messages, 0, sizeof messages);
    registered = true;
    return PostScoreSM((ES_Event_t){ES_INIT, 0});
}

static bool post_message(ES_EventType_t type, uint8_t ingredient, uint32_t generation,
                         uint16_t closeness, uint32_t elapsed, int32_t delta)
{
    unsigned int attempt;
    Message *slot = 0;
    if (!registered || ScoreSM_HasFault()) return false;
    for (attempt = 0; attempt < MESSAGE_SLOTS; ++attempt) {
        ++next_ticket;
        slot = &messages[next_ticket % MESSAGE_SLOTS];
        if (!slot->used) break;
    }
    if (attempt == MESSAGE_SLOTS) {
        posting_failed = true;
        return false;
    }
    *slot = (Message){true, next_ticket, type, ingredient, generation, closeness, elapsed, delta};
    if (!PostScoreSM((ES_Event_t){type, next_ticket})) {
        slot->used = false;
        return false;
    }
    return true;
}

bool ScoreSM_PostLifecycle(ES_EventType_t type, uint32_t generation)
{
    if (type != GAME_START && type != GAME_RESET) return false;
    return post_message(type, 0, generation, 0, 0, 0);
}

bool ScoreSM_PostIngredient(uint8_t ingredient, uint32_t generation)
{
    /* May queue behind ES_INIT/Start; publication comes only from IngredientSM. */
    if (ingredient >= INGREDIENT_COUNT) return false;
    return post_message(INGREDIENT_ADDED, ingredient, generation, 0, 0, 0);
}

bool ScoreSM_PostDelta(uint32_t generation, int32_t millipoints)
{
    if (millipoints > DELTA_MAX_MILLI || millipoints < -DELTA_MAX_MILLI) return false;
    return post_message(SCORE_DELTA, 0, generation, 0, 0, millipoints);
}

bool ScoreSM_PostHeat(uint8_t ingredient, uint32_t round, uint16_t closeness, uint32_t elapsed_ms)
{
    if (!full_scoring || ingredient >= INGREDIENT_COUNT || closeness > 1000u ||
        elapsed_ms > GAME_LENGTH_S * 1000u) return false;
    return post_message(HEAT_POINTS, ingredient, round, closeness, elapsed_ms, 0);
}
bool ScoreSM_PostBurn(uint8_t ingredient, uint32_t round)
{
    if (!full_scoring || ingredient >= INGREDIENT_COUNT) return false;
    return post_message(BURNED, ingredient, round, 0, 0, 0);
}
bool ScoreSM_PostStir(uint8_t ingredient, uint32_t round, uint32_t elapsed_ms)
{
    if (!full_scoring || ingredient >= INGREDIENT_COUNT || elapsed_ms > GAME_LENGTH_S * 1000u)
        return false;
    return post_message(STIR_RESULT, ingredient, round, 0, elapsed_ms, 0);
}

/* Quantize each award to micro-points, then retain exact integer fractions.
 * This avoids 10 * 0.1 displaying as zero additional points due to binary
 * rounding. Overflow saturates explicitly; display saturation does not erase
 * an ordinary above-display balance before a later penalty arrives. */
static void add_points(double delta)
{
    double scaled = delta * 1000000.0;
    scaled += scaled < 0 ? -0.5 : 0.5;
    int64_t units = scaled >= (double)INT64_MAX ? INT64_MAX :
                    scaled <= (double)INT64_MIN ? INT64_MIN : (int64_t)scaled;
    if (units > 0 && balance > INT64_MAX - units) balance = INT64_MAX;
    else if (units < 0 && balance < INT64_MIN - units) balance = INT64_MIN;
    else balance += units;
    if (!tuning.allow_negative_score && balance < 0) balance = 0;
    int64_t points = balance / 1000000;
    score = points > DISPLAY_MAX_SCORE ? DISPLAY_MAX_SCORE :
            points < -999 ? -999 : (int32_t)points;
}

/* Integer millipoint path (no BindTuning). Both operands are bounded far
 * below INT32 limits (|milli| <= 9 999 000, |delta| <= 2 000 000). */
static void add_milli(int32_t delta)
{
    milli += delta;
    if (milli > MILLI_MAX) milli = MILLI_MAX;
    if (milli < MILLI_MIN) milli = MILLI_MIN;
    if (io.floor_zero && milli < 0) milli = 0;
    score = milli / 1000; /* C division rounds toward zero */
}

ES_Event_t RunScoreSM(ES_Event_t event)
{
    const ES_Event_t done = {ES_NO_EVENT, 0};
    if (ScoreSM_HasFault() || event.EventType == ES_ERROR) return fail();
    if (!registered) return fail();
    if (event.EventType == ES_INIT) {
        initialized = true;
        return done;
    }
    if (!initialized) return fail();
    if (event.EventType != GAME_START && event.EventType != GAME_RESET &&
        event.EventType != INGREDIENT_ADDED && event.EventType != HEAT_POINTS &&
        event.EventType != BURNED && event.EventType != STIR_RESULT &&
        event.EventType != SCORE_DELTA) return done;

    Message *slot = &messages[event.EventParam % MESSAGE_SLOTS];
    if (!slot->used || slot->ticket != event.EventParam || slot->type != event.EventType) return done;
    Message message = *slot;
    slot->used = false;
    if (event.EventType == GAME_RESET) {
        if (message.generation != io.generation()) return done;
        score = milli = 0;
        scored_mask = burned_mask = stirred_mask = 0;
        balance = 0;
        state = SCORE_IDLE;
        return done; /* Clearing internal score must not overwrite a banner. */
    }
    if (!io.accepts_round(message.generation)) return done;
    if (event.EventType == GAME_START) {
        if (state == SCORE_ACTIVE && round_generation == message.generation) return done;
        round_generation = message.generation;
        round_recipe = next_recipe;
        score = milli = 0;
        scored_mask = burned_mask = stirred_mask = 0;
        balance = 0;
        state = SCORE_ACTIVE;
        return io.display(score) ? done : fail();
    }
    if (state != SCORE_ACTIVE || message.generation != round_generation) return done;
    if (event.EventType == SCORE_DELTA) {
        int32_t before = score;
        if (!formal_mode) add_milli(message.delta);
        else if (full_scoring) add_points(message.delta / 1000.0);
        /* Heat arrives every ~1 s; repaint only when the whole points change. */
        if (score == before) return done;
        return io.display(score) ? done : fail();
    }
    uint8_t bit = (uint8_t)(1u << message.ingredient);
    if (event.EventType == INGREDIENT_ADDED) {
        if (scored_mask & bit) return done;
        scored_mask |= bit;
        if (!formal_mode)
            add_milli((!round_recipe || (round_recipe & bit) ? 1 : -1) *
                      io.ingredient_points * 1000);
        else if (!full_scoring) score = 0;
        else add_points((tuning.recipe_mask & bit) ? (double)tuning.points_right :
                        -(double)tuning.penalty_wrong);
    } else {
        if (!full_scoring || !(scored_mask & bit)) return done;
        if (event.EventType == HEAT_POINTS) {
            if (burned_mask & bit) return done;
            add_points((double)tuning.heat_weight * message.closeness * message.elapsed / 1000000.0);
        } else if (event.EventType == BURNED) {
            if (burned_mask & bit) return done;
            burned_mask |= bit;
            add_points(-(double)tuning.penalty_burn);
        } else {
            if (stirred_mask & bit) return done;
            stirred_mask |= bit;
            /* Explicit opt-in curve: weight * max(0,1-|actual-optimal|/optimal).
             * Team must select this rule; no formula is silently assumed. */
            double optimal = tuning.optimal_stir_ms[message.ingredient];
            double error = message.elapsed > optimal ? message.elapsed - optimal : optimal - message.elapsed;
            double closeness = error >= optimal ? 0.0 : 1.0 - error / optimal;
            add_points((double)tuning.stir_weight * closeness);
        }
    }
    return io.display(score) ? done : fail();
}
