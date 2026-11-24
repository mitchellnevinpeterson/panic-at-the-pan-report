#ifndef PANIC_SCORE_TIMER_BENCH_CONFIG_H
#define PANIC_SCORE_TIMER_BENCH_CONFIG_H

#include "ScoreTimerDisplay.h"

#define BENCH_WELCOME_TEXT "Welcome to Panic at the Pan"
#define BENCH_WELCOME_STEP_MS 50u /* One pixel per step; lower is faster. */

/* Two 8x32 strips ONLY: PIC -> top labels (4 chips) -> bottom values (4 chips).
 * The defaults reproduce the Lab matrix driver's row/bit reversal.
 * Change each panel's rotation or chip order if its physical orientation differs.
 */
static const ScoreTimerDisplay_Config BenchDisplayConfig = {
    .top_first = true,
    .reverse_modules = { false, false }, /* Top, bottom. */
    .rotation = { 180, 180 },            /* 0, 90, 180, 270 per 8x8 tile. */
    .intensity = 1                      /* Low starting brightness, range0..15. */
};

#endif
