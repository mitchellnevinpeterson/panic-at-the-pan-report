#ifndef PANIC_SCORE_TIMER_DISPLAY_H
#define PANIC_SCORE_TIMER_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { DISPLAY_TOP = 0, DISPLAY_BOTTOM = 1 } ScoreTimerDisplay_Panel;
typedef enum {
    SCORE_TIMER_STARTING,
    SCORE_TIMER_BUSY,
    SCORE_TIMER_IDLE,
    SCORE_TIMER_FAULT
} ScoreTimerDisplay_State;

typedef struct {
    /* Top strip occupies devices0..3 when true; devices4..7 otherwise.
     * Device0 is nearest PIC DIN. Each strip contains four MAX7219s.
     */
    bool top_first;
    bool reverse_modules[2];
    uint16_t rotation[2];   /* Per 8x8 tile: 0, 90, 180 or 270 degrees. */
    uint8_t intensity;      /* 0..15; begin at 0 or 1. */
} ScoreTimerDisplay_Config;

/* Single foreground owner; not ISR-safe. No SPI wait loops in this module.
 * Setters change the pending image. Service() sends immutable snapshots.
 * Initialization is asynchronous and blanks all eight chips before enabling.
 */
bool ScoreTimerDisplay_Init(const ScoreTimerDisplay_Config *config);
ScoreTimerDisplay_State ScoreTimerDisplay_Service(void);
/* A 16x32 dashboard: SCORE / TIME headings above, values below.
 * Score uses the left 18 columns; remaining seconds use the right 13.
 * Each setter preserves the other value. Out-of-range values show dashes.
 */
bool ScoreTimerDisplay_ShowLabels(void); /* Restore headings after diagnostics. */
bool ScoreTimerDisplay_SetScore(int32_t score); /* -999..9999 */
bool ScoreTimerDisplay_SetTimerSeconds(uint32_t seconds); /* 0..999, seconds */
enum { DISPLAY_BANNER_CHAR_WIDTH = 8, DISPLAY_BANNER_MAX_CHARS = 64 };
/* Replace both strips with 2x-scaled text, vertically centered on the 16x32
 * display. left_x=32 starts off-screen right; decrease it to scroll left.
 * Maximum 64 characters. Clipping handles either edge without wrapping.
 */
bool ScoreTimerDisplay_SetBanner(const char *text, int16_t left_x);
/* Replace ONE strip with the 4x6 font doubled horizontally (8 px per char,
 * rows 1..6), leaving the other strip unchanged. Same left_x convention and
 * 64-character limit as SetBanner; centre short text with (32 - n*8) / 2.
 */
bool ScoreTimerDisplay_SetStripBanner(ScoreTimerDisplay_Panel panel,
                                      const char *text, int16_t left_x);
/* Diagnostics can replace a whole strip; at most eight 4x6 characters. */
bool ScoreTimerDisplay_SetText(ScoreTimerDisplay_Panel panel, const char *text);
/* Eight logical rows, bit31 = leftmost pixel; useful for wiring diagnostics. */
bool ScoreTimerDisplay_SetFrame(ScoreTimerDisplay_Panel panel,
                               const uint32_t rows[8]);

#endif
