/* CookSM: heat, burn, stir and flip scoring for the final game
 * (GameButtons build). Rules: final_game_spec.md. Non-blocking ES service
 * driven by COOK_TICK_TIMER every COOK_TICK_MS. Posts signed score changes
 * to ScoreSM (sole score owner) via ScoreSM_PostDelta().
 */
#ifndef PANIC_COOKSM_H
#define PANIC_COOKSM_H

#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"
#include "ES_Configure.h"
#include "ES_Types.h"

/* --- Tuning (final_game_spec.md). Index = ingredient 0..4:
 *     0 Protein, 1 Carb, 2 Vegetables, 3 Spice, 4 Oil. --- */
#define COOK_TICK_MS            100u
#define COOK_TARGET_MM          { 140u, 240u, 320u, 280u, 100u }
#define COOK_IDEAL_CIRCLES      { 3u, 5u, 0u, 6u, 0u } /* full joystick circles */
#define COOK_IDEAL_FLIPS        { 2u, 4u, 1u, 3u, 0u } /* flips that earn points, per ingredient */
#define COOK_BAND_MM            40u    /* JUST RIGHT = target +/- band */
#define COOK_FALLOFF_MM         80u    /* heat points reach 0 this far from target */
#define COOK_HEAT_MAX_MILLI_PER_S 1000 /* +1 point/s exactly on target */
#define COOK_BURN_MS            3000u  /* continuous TOO MUCH before burning */
#define COOK_BURN_MILLI         (-10000)
#define COOK_CIRCLE_MILLI       1000   /* +1 per circle up to the ideal count, -1 beyond */
#define COOK_FLIP_MILLI         2000   /* +2 per flip up to the ideal count, -2 beyond */
#define COOK_HAND_MIN_MM        30u    /* readings outside 30..400 mm mean "no hand" */
#define COOK_HAND_MAX_MM        400u
#define COOK_ZONE_CONFIRM_TICKS 2u     /* published zone needs this many agreeing ticks */

typedef enum {
    COOK_LCD_ATTRACT = 0, /* Welcome, dish intro, end sequence: LCD sweeps by itself */
    COOK_LCD_WAITING = 1, /* cooking, pan empty: bar follows hand, label WAITING */
    COOK_LCD_ZONE = 2     /* cooking with an ingredient: zone label / BURNT, band marks */
} CookLcdMode_t;

/* Same numbering as the PIC2 audio HeatZone (SILENT, LOW, RIGHT, HIGH). */
typedef enum {
    COOK_ZONE_NONE = 0,  /* no ingredient (or not cooking) */
    COOK_ZONE_LOW = 1,   /* NOT ENOUGH: farther than the band, or no hand */
    COOK_ZONE_RIGHT = 2, /* JUST RIGHT */
    COOK_ZONE_HIGH = 3   /* TOO MUCH: closer than the band */
} CookZone_t;

typedef struct {
    CookLcdMode_t lcd_mode;
    CookZone_t zone;    /* confirmed zone (COOK_ZONE_CONFIRM_TICKS) */
    bool sizzle;        /* true while cooking with an ingredient in the pan */
    bool burnt;         /* current ingredient is burnt */
    uint8_t burn_seq;   /* +1 (mod 4) at each new burn; PIC2 plays the burnt cue on change */
    uint8_t target_cm;  /* current target in cm, 0 when none */
    uint8_t stirs;      /* this ingredient's full circles so far (0 with an empty pan) */
    uint8_t flips;      /* this ingredient's flips so far */
} CookOutput_t;

typedef struct {
    uint32_t (*now_ms)(void);
    /* Latest fresh hand distance (<= ~350 ms old, within 30..400 mm).
     * Returns false when there is no hand. */
    bool (*distance_mm)(uint16_t *mm);
    /* Full joystick circles completed since the last call (consumes them). */
    uint8_t (*take_stir_circles)(void);
} CookSMPort_t;

bool CookSM_BindPort(const CookSMPort_t *port);
/* Foreground ingress with owned round generation, like ScoreSM/IngredientSM.
 * GAME_START: begin cooking (WAITING). GAME_END: flush pending heat (and any
 * last circles), stop scoring, go ATTRACT. GAME_RESET: clear everything, ATTRACT. */
bool CookSM_PostLifecycle(ES_EventType_t type, uint32_t generation);
/* Accepted (first-time) ingredient only. Clears burn, sets the new target,
 * resets the flip and circle counters. Circles score live, as they happen. */
bool CookSM_PostIngredient(uint8_t ingredient, uint32_t round_generation);
/* Pan-flip limit switch press during cooking. Empty pan: no points. */
bool CookSM_PostFlip(uint32_t round_generation);
CookOutput_t CookSM_Output(void);
bool CookSM_HasFault(void);

bool InitCookSM(uint8_t priority);
bool PostCookSM(ES_Event_t event);
ES_Event_t RunCookSM(ES_Event_t event);

#endif
