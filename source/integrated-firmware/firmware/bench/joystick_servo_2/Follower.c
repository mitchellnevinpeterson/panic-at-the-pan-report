#include "Follower.h"
#include "SpoonServo.h"

bool Follower_Init(Follower *f, const Follower_Config *c, uint32_t now)
{
    if (f == 0) return false;
    *f = (Follower){ .state = FOLLOWER_FAULT };
    if (c == 0 || c->adc_max > 1023 || c->adc_min >= c->adc_center ||
        c->adc_center >= c->adc_max ||
        c->deadband >= c->adc_center - c->adc_min ||
        c->deadband >= c->adc_max - c->adc_center ||
        c->pulse_min_us < SPOON_SERVO_MIN_US ||
        c->pulse_max_us > SPOON_SERVO_MAX_US ||
        c->pulse_min_us >= c->pulse_center_us ||
        c->pulse_center_us >= c->pulse_max_us || c->slew_us_per_step == 0) {
        return false;
    }
    f->config = *c;
    f->state = FOLLOWER_WAIT_CENTER;
    f->pulse_us = c->pulse_center_us;
    f->filtered_x_q8 = (int32_t)c->adc_center * 256;
    f->last_sample_ms = now;
    f->last_step_ms = now;
    return true;
}

void Follower_Fault(Follower *f)
{
    f->state = FOLLOWER_FAULT;
}

void Follower_Sample(Follower *f, uint16_t x, uint16_t y, uint32_t now)
{
    int32_t delta;
    if (f->state == FOLLOWER_FAULT) return;
    if (x > 1023 || y > 1023 ||
        (uint32_t)(now - f->last_sample_ms) >= FOLLOWER_SAMPLE_TIMEOUT_MS) {
        Follower_Fault(f);
        return;
    }
    f->raw_x = x;
    f->raw_y = y;
    f->last_sample_ms = now;
    if (!f->has_sample) {
        f->filtered_x_q8 = (int32_t)x * 256;
        f->has_sample = true;
    } else {
        f->filtered_x_q8 += ((int32_t)x * 256 - f->filtered_x_q8) / 4;
    }
    if (f->state != FOLLOWER_WAIT_CENTER) return;
    delta = (int32_t)x - f->config.adc_center;
    if (delta < -FOLLOWER_ARM_BAND || delta > FOLLOWER_ARM_BAND) {
        f->centering = false;
    } else if (!f->centering) {
        f->centering = true;
        f->center_since_ms = now;
    } else if ((uint32_t)(now - f->center_since_ms) >= FOLLOWER_CENTER_HOLD_MS) {
        f->state = FOLLOWER_ACTIVE;
        f->filtered_x_q8 = (int32_t)f->config.adc_center * 256;
        f->last_step_ms = now;
    }
}

static uint16_t target_pulse(const Follower *f)
{
    const Follower_Config *c = &f->config;
    int32_t x = (f->filtered_x_q8 + 128) / 256;
    uint32_t amount, span, travel;
    bool positive;
    if (x < c->adc_min) x = c->adc_min;
    if (x > c->adc_max) x = c->adc_max;
    if (x < (int32_t)c->adc_center - c->deadband) {
        amount = c->adc_center - c->deadband - x;
        span = c->adc_center - c->deadband - c->adc_min;
        positive = c->reverse;
    } else if (x > (int32_t)c->adc_center + c->deadband) {
        amount = x - c->adc_center - c->deadband;
        span = c->adc_max - c->adc_center - c->deadband;
        positive = !c->reverse;
    } else {
        return c->pulse_center_us;
    }
    travel = positive ? c->pulse_max_us - c->pulse_center_us :
                        c->pulse_center_us - c->pulse_min_us;
    travel = (travel * amount + span / 2u) / span;
    return positive ? c->pulse_center_us + travel : c->pulse_center_us - travel;
}

Follower_State Follower_Service(Follower *f, uint32_t now)
{
    int32_t delta;
    if (f->state == FOLLOWER_FAULT) return f->state;
    if ((uint32_t)(now - f->last_sample_ms) >= FOLLOWER_SAMPLE_TIMEOUT_MS) {
        Follower_Fault(f);
    }
    if (f->state != FOLLOWER_ACTIVE ||
        (uint32_t)(now - f->last_step_ms) < FOLLOWER_STEP_MS) return f->state;
    f->last_step_ms = now;
    delta = (int32_t)target_pulse(f) - f->pulse_us;
    if (delta > f->config.slew_us_per_step) delta = f->config.slew_us_per_step;
    if (delta < -(int32_t)f->config.slew_us_per_step) delta = -(int32_t)f->config.slew_us_per_step;
    f->pulse_us = (uint16_t)((int32_t)f->pulse_us + delta);
    return f->state;
}
