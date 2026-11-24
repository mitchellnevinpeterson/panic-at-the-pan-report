#ifndef PANIC_JOYSTICK_SERVO_BENCH_2_CONFIG_H
#define PANIC_JOYSTICK_SERVO_BENCH_2_CONFIG_H

#include "Follower.h"
#include "ScoreTimerDisplay.h"

/* Restored original X-only settings for the checkpoint. Y never gates arming.
 * Pulse limits do not guarantee an angle; keep the previously tested travel.
 */
static const Follower_Config BenchFollowerConfig = {
    .adc_min = 0, .adc_center = 512, .adc_max = 1023, .deadband = 40,
    .pulse_min_us = 1250, .pulse_center_us = 1500, .pulse_max_us = 1750,
    .slew_us_per_step = 10,
    .reverse = false
};

static const ScoreTimerDisplay_Config BenchDisplayConfig = {
    .top_first = true,
    .reverse_modules = { false, false },
    .rotation = { 180, 180 },
    .intensity = 1
};

#endif
