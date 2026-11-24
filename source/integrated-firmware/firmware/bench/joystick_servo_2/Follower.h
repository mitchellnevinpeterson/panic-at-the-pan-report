#ifndef PANIC_BENCH_X_FOLLOWER_H
#define PANIC_BENCH_X_FOLLOWER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { FOLLOWER_WAIT_CENTER, FOLLOWER_ACTIVE, FOLLOWER_FAULT } Follower_State;
typedef struct {
    uint16_t adc_min, adc_center, adc_max, deadband;
    uint16_t pulse_min_us, pulse_center_us, pulse_max_us;
    uint16_t slew_us_per_step;
    bool reverse;
} Follower_Config;

enum { FOLLOWER_SAMPLE_TIMEOUT_MS = 100, FOLLOWER_CENTER_HOLD_MS = 500,
       FOLLOWER_STEP_MS = 20, FOLLOWER_ARM_BAND = 64 };

typedef struct {
    Follower_Config config;
    Follower_State state;
    uint16_t raw_x, raw_y, pulse_us;
    int32_t filtered_x_q8;
    uint32_t last_sample_ms, last_step_ms, center_since_ms;
    bool has_sample, centering;
} Follower;

/* Original checkpoint behavior: absolute X position controls the servo.
 * Y is diagnostic only. Feed paired ADC readings every 10 ms. Faults latch;
 * only ACTIVE authorizes PWM. No angular detection or learned centers.
 */
bool Follower_Init(Follower *f, const Follower_Config *config, uint32_t now_ms);
void Follower_Sample(Follower *f, uint16_t x, uint16_t y, uint32_t now_ms);
Follower_State Follower_Service(Follower *f, uint32_t now_ms);
void Follower_Fault(Follower *f);

#endif
