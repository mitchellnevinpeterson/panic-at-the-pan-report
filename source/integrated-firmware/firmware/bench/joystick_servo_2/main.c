#include <xc.h>
#include <cp0defs.h>
#include "BenchConfig.h"
#include "JoystickADC.h"
#include "SpoonServo.h"

volatile uint16_t JoystickRawX, JoystickRawY, ServoPulseUs;
volatile Follower_State ServoBenchState = FOLLOWER_WAIT_CENTER;
volatile ScoreTimerDisplay_State DisplayStatus = SCORE_TIMER_STARTING;
volatile uint32_t ServoBenchMilliseconds;

static Follower follower;
static uint32_t last_core_count;

static uint32_t milliseconds(void)
{
    uint32_t elapsed = (uint32_t)(_CP0_GET_COUNT() - last_core_count);
    uint32_t whole_ms = elapsed / 20000u;
    last_core_count += whole_ms * 20000u;
    ServoBenchMilliseconds += whole_ms;
    return ServoBenchMilliseconds;
}

static void show_value(ScoreTimerDisplay_Panel panel, char name, uint16_t value)
{
    char text[] = "X 0000";
    unsigned int i;
    text[0] = name;
    for (i = 6; i > 2; ) {
        text[--i] = '0' + value % 10u;
        value /= 10u;
    }
    (void)ScoreTimerDisplay_SetText(panel, text);
}

static void show_diagnostics(uint32_t now)
{
    show_value(DISPLAY_TOP, 'X', JoystickRawX);
    if (ServoBenchState == FOLLOWER_FAULT) {
        (void)ScoreTimerDisplay_SetText(DISPLAY_BOTTOM, "FAULT");
    } else if (ServoBenchState == FOLLOWER_WAIT_CENTER) {
        (void)ScoreTimerDisplay_SetText(DISPLAY_BOTTOM, "CENTER");
    } else if ((now / 2000u) & 1u) {
        show_value(DISPLAY_BOTTOM, 'Y', JoystickRawY);
    } else {
        show_value(DISPLAY_BOTTOM, 'P', ServoPulseUs);
    }
}

int main(void)
{
    uint32_t now, last_sample_request, last_display;
    uint16_t x, y;
    JoystickADC_State adc_state;
    SpoonServo_Init();
    last_core_count = _CP0_GET_COUNT();
    while (milliseconds() < 100u) { }
    now = milliseconds();
    (void)Follower_Init(&follower, &BenchFollowerConfig, now);
    JoystickADC_Init();
    if (!ScoreTimerDisplay_Init(&BenchDisplayConfig)) Follower_Fault(&follower);
    (void)ScoreTimerDisplay_SetText(DISPLAY_TOP, "JOYSTICK");
    (void)ScoreTimerDisplay_SetText(DISPLAY_BOTTOM, "CENTER");
    last_sample_request = now - 10u;
    last_display = now;

    for (;;) {
        now = milliseconds();
        DisplayStatus = ScoreTimerDisplay_Service();
        adc_state = JoystickADC_Poll(&x, &y);
        if (adc_state == JOYSTICK_ADC_READY) {
            JoystickRawX = x;
            JoystickRawY = y;
            Follower_Sample(&follower, x, y, now);
        } else if (adc_state == JOYSTICK_ADC_FAULT) {
            Follower_Fault(&follower);
        }
        if (DisplayStatus == SCORE_TIMER_FAULT) Follower_Fault(&follower);
        if ((uint32_t)(now - last_sample_request) >= 10u && JoystickADC_Start()) {
            last_sample_request = now;
        }
        ServoBenchState = Follower_Service(&follower, now);
        if (ServoBenchState == FOLLOWER_ACTIVE) {
            ServoPulseUs = follower.pulse_us;
            if (!SpoonServo_WritePulse(ServoPulseUs)) {
                Follower_Fault(&follower);
                ServoBenchState = FOLLOWER_FAULT;
                SpoonServo_Disable();
                ServoPulseUs = 0;
            }
        } else {
            SpoonServo_Disable();
            ServoPulseUs = 0;
        }
        if ((uint32_t)(now - last_display) >= 100u) {
            last_display = now;
            show_diagnostics(now);
        }
    }
}
