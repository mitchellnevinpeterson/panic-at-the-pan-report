#include <xc.h>
#include <cp0defs.h>
#include "BenchConfig.h"

/* Debugger watch variables. SPI success means peripheral transfer completion;
 * MAX7219 provides no acknowledgement, so this cannot detect a disconnected strip.
 */
volatile ScoreTimerDisplay_State DisplayStatus = SCORE_TIMER_STARTING;
volatile uint32_t DisplayBenchMilliseconds;
volatile unsigned int DisplayBenchPhase;

enum { WELCOME, DASHBOARD_READY, COUNTERS, HOLD_ZERO, FAULT };
enum { WELCOME_WIDTH = (sizeof BENCH_WELCOME_TEXT - 1u) * DISPLAY_BANNER_CHAR_WIDTH };

static uint32_t last_core_count;

static uint32_t milliseconds(void)
{
    uint32_t elapsed = (uint32_t)(_CP0_GET_COUNT() - last_core_count);
    uint32_t whole_ms = elapsed / 20000u; /* CP0 Count runs at SYSCLK/2. */
    last_core_count += whole_ms * 20000u;
    DisplayBenchMilliseconds += whole_ms;
    return DisplayBenchMilliseconds;
}

static void show_dashboard(void)
{
    (void)ScoreTimerDisplay_ShowLabels();
    (void)ScoreTimerDisplay_SetScore(0);
    (void)ScoreTimerDisplay_SetTimerSeconds(60);
    DisplayBenchPhase = DASHBOARD_READY;
}

int main(void)
{
    uint32_t now, last_change;
    unsigned int elapsed_seconds = 0;
    int16_t welcome_x = 32;
    last_core_count = _CP0_GET_COUNT();
    while (milliseconds() < 100u) {
        /* Initial power settling, before any SPI transaction. */
    }
    if (!ScoreTimerDisplay_Init(&BenchDisplayConfig)) {
        DisplayStatus = SCORE_TIMER_FAULT;
        DisplayBenchPhase = FAULT;
        for (;;) { }
    }
    (void)ScoreTimerDisplay_SetBanner(BENCH_WELCOME_TEXT, welcome_x);
    DisplayBenchPhase = WELCOME;
    last_change = milliseconds();

    for (;;) {
        now = milliseconds();
        DisplayStatus = ScoreTimerDisplay_Service();
        if (DisplayStatus == SCORE_TIMER_FAULT) {
            DisplayBenchPhase = FAULT;
            continue;
        }
        switch (DisplayBenchPhase) {
        case WELCOME:
            if (DisplayStatus != SCORE_TIMER_IDLE ||
                (uint32_t)(now - last_change) < BENCH_WELCOME_STEP_MS) {
                break;
            }
            last_change = now;
            if (welcome_x > -WELCOME_WIDTH) {
                --welcome_x;
                (void)ScoreTimerDisplay_SetBanner(BENCH_WELCOME_TEXT, welcome_x);
            } else {
                show_dashboard();
            }
            break;
        case DASHBOARD_READY:
            /* Start the first full second only after 0 / 60 reaches the LEDs. */
            if (DisplayStatus == SCORE_TIMER_IDLE) {
                last_change = now;
                DisplayBenchPhase = COUNTERS;
            }
            break;
        case COUNTERS:
            if ((uint32_t)(now - last_change) >= 1000u) {
                last_change += 1000u;
                ++elapsed_seconds;
                (void)ScoreTimerDisplay_SetScore((int32_t)(elapsed_seconds * 10u));
                (void)ScoreTimerDisplay_SetTimerSeconds(60u - elapsed_seconds);
                if (elapsed_seconds == 60) {
                    DisplayBenchPhase = HOLD_ZERO;
                }
            }
            break;
        case HOLD_ZERO:
            if ((uint32_t)(now - last_change) >= 3000u) {
                elapsed_seconds = 0;
                show_dashboard();
            }
            break;
        default:
            break;
        }
    }
}
