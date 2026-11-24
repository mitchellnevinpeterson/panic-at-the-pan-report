#include "Stirrer.h"
#include "../joystick_servo_2/Follower.h"
#include "JoystickADC.h"
#include "Servo50Hz.h"
#include "SpoonServo.h"         /* Follower_Init's pulse bounds */
#include <stdio.h>

/* The joystick's resting position is LEARNED, not assumed to be 512: on the
 * bench this stick rests near X=330, which never armed the old fixed-centre
 * follower (spoon never moved) and counted as constant stirring.
 * Idle activity ("moving") is only something noise cannot fake:
 *  - a PUSH: the stick stays >= PUSH_RADIUS from its rest position for
 *    PUSH_HOLD_MS straight; the next push counts only after the stick has come
 *    back within RELEASE_RADIUS;
 *  - a completed stir circle (8 sectors in order).
 * Field bugs (Nov 18): "direction changes >= 8 deg / crosses the centre-zone
 * edge" and then "travels 80 counts within 1 s" both fired for a resting stick
 * (noise and multi-sample glitches), so games never timed out. */
#define SAMPLE_MS 10u
#define DEADBAND 40u
#define STABLE_COUNTS 8         /* "still" = within +/-8 counts on both axes */
#define LEARN_MS 300u           /* still this long at boot -> centre */
#define RELEARN_MS 3000u        /* still this long while not cooking -> re-centre */
#define LEARN_TIMEOUT_MS 3000u  /* no stillness at boot: use the running average */
#define CENTRE_RADIUS 60        /* counts; inside = centre zone */
#define PUSH_RADIUS 150         /* activity: pushed this far from rest... */
#define PUSH_HOLD_MS 100u       /* ...for this long straight (glitches are shorter) */
#define RELEASE_RADIUS 100      /* back inside this before the next push counts */
#define MOVE_HOLD_MS 300u       /* "moving" holds this long after a move */
#define CENTRE_MIN 200u         /* keeps Follower_Init's band checks valid */
#define CENTRE_MAX 823u
/* Each side's full-travel point is the farthest X actually reached, starting
 * at centre +/-120 and growing as the stick is pushed. On the bench the spoon
 * reached full travel one way only: this stick's two sides span very
 * different counts (rest ~330), so a fixed 0/1023 end left one side short. */
#define RANGE_START 120u
#define PULSE_HYSTERESIS_US 4u  /* ignore sub-4 us wander: a still stick never jitters */
/* A stir = one full joystick circle: the stick (outside the centre zone)
 * passes through all 8 45-degree sectors in one direction. Back-and-forth
 * cancels; returning to the centre discards a partial circle. */
#define SECTORS 8

typedef char stir_pulse_bounds_ok[(STIR_PULSE_MIN_US >= SPOON_SERVO_MIN_US &&
    STIR_PULSE_MIN_US < STIR_PULSE_CENTER_US && STIR_PULSE_CENTER_US < STIR_PULSE_MAX_US &&
    STIR_PULSE_MAX_US <= SPOON_SERVO_MAX_US) ? 1 : -1];

static Follower follower;
static Follower_Config config;
static uint32_t last_start, last_sample, last_service, boot_ms, last_move, still_since;
static uint16_t raw_x = 512u, raw_y = 512u, centre_x = 512u, centre_y = 512u;
static uint16_t pulse_us = STIR_PULSE_CENTER_US, still_x, still_y;
static uint32_t still_sum_x, still_sum_y, still_n;
static bool push_armed, far_run;       /* next push may count; currently beyond PUSH_RADIUS */
static uint32_t far_since;
static uint32_t moves_total;            /* activity events, for the report */
static bool fresh, learned, was_out, moved, activity, have_still;
static uint16_t hist_x[3], hist_y[3];   /* median-of-3 spike filter */
static uint8_t hist_n;
static int32_t smooth_x_q4;             /* extra low-pass on X (x16) */
static int8_t prev_sector = -1;
static int8_t sector_steps;             /* signed sector progress of this circle */
static uint8_t circles_pending;
static uint32_t circles_total;

static void restart_adc(uint32_t now_ms)
{
    JoystickADC_Init();
    last_start = now_ms - SAMPLE_MS;    /* start again on the next pass */
}

static void use_centre(uint16_t cx, uint16_t cy, uint32_t now_ms)
{
    if (cx < CENTRE_MIN) cx = CENTRE_MIN;
    if (cx > CENTRE_MAX) cx = CENTRE_MAX;
    centre_x = cx;
    centre_y = cy;
    config = (Follower_Config){
        .adc_min = (uint16_t)(cx - RANGE_START), .adc_center = cx,
        .adc_max = (uint16_t)(cx + RANGE_START), .deadband = DEADBAND,
        .pulse_min_us = STIR_PULSE_MIN_US, .pulse_center_us = STIR_PULSE_CENTER_US,
        .pulse_max_us = STIR_PULSE_MAX_US, .slew_us_per_step = 10, .reverse = false
    };
    (void)Follower_Init(&follower, &config, now_ms);
    learned = true;
    was_out = false;
    push_armed = true; far_run = false;
    printf("STIR centre learned x=%u y=%u\r\n", (unsigned)cx, (unsigned)cy);
}

void Stirrer_Init(uint32_t now_ms)
{
    restart_adc(now_ms);
    raw_x = raw_y = centre_x = centre_y = 512u;
    fresh = learned = was_out = moved = activity = have_still = false;
    hist_n = 0;
    prev_sector = -1; sector_steps = 0; circles_pending = 0; circles_total = 0;
    push_armed = true; far_run = false; moves_total = 0;
    boot_ms = last_sample = last_service = now_ms;
    still_n = 0;
    pulse_us = STIR_PULSE_CENTER_US;
    (void)Servo50Hz_Write(SERVO_SPOON, pulse_us);
}

static void move(uint32_t now_ms)
{
    ++moves_total;
    moved = true;
    last_move = now_ms;
    activity = true;
}

static void track_stillness(uint16_t x, uint16_t y, uint32_t now_ms, bool cooking)
{
    int dx = (int)x - (int)still_x, dy = (int)y - (int)still_y;
    if (!have_still || dx > STABLE_COUNTS || dx < -STABLE_COUNTS ||
        dy > STABLE_COUNTS || dy < -STABLE_COUNTS) {
        have_still = true;
        still_x = x; still_y = y; still_since = now_ms;
        still_sum_x = still_sum_y = still_n = 0;
    }
    still_sum_x += x; still_sum_y += y; ++still_n;
    uint32_t still_ms = now_ms - still_since;
    uint16_t ax = (uint16_t)(still_sum_x / still_n), ay = (uint16_t)(still_sum_y / still_n);
    if (!learned) {
        if (still_ms >= LEARN_MS || (uint32_t)(now_ms - boot_ms) >= LEARN_TIMEOUT_MS)
            use_centre(ax, ay, now_ms);
    } else if (!cooking && still_ms >= RELEARN_MS) {
        int ddx = (int)ax - (int)centre_x, ddy = (int)ay - (int)centre_y;
        if (ddx > STABLE_COUNTS || ddx < -STABLE_COUNTS || ddy > STABLE_COUNTS || ddy < -STABLE_COUNTS)
            use_centre(ax, ay, now_ms);
        still_since = now_ms;           /* at most one re-centre per still period */
    }
}

static int8_t sector(int32_t dx, int32_t dy)
{
    /* 0..7 counter-clockwise, 45 degrees each, integer comparisons only. */
    if (dy >= 0) {
        if (dx > 0) return dy < dx ? 0 : 1;
        return dy > -dx ? 2 : 3;
    }
    if (dx < 0) return -dy < -dx ? 4 : 5;
    return -dy > dx ? 6 : 7;
}

/* True when a full circle completes. */
static bool count_circle(bool out, int32_t dx, int32_t dy)
{
    bool done = false;
    if (!out) {                         /* back to centre: partial circle discarded */
        prev_sector = -1; sector_steps = 0;
        return false;
    }
    int8_t s = sector(dx, dy);
    if (prev_sector >= 0 && s != prev_sector) {
        int8_t step = (int8_t)((s - prev_sector + SECTORS) % SECTORS);
        if (step == 1) ++sector_steps;                /* one sector CCW */
        else if (step == SECTORS - 1) --sector_steps; /* one sector CW */
        /* a 2+ sector jump (glitch) is ignored */
        if (sector_steps >= SECTORS || sector_steps <= -SECTORS) {
            sector_steps = 0;
            if (circles_pending < UINT8_MAX) ++circles_pending;
            ++circles_total;
            done = true;
        }
    }
    prev_sector = s;
    return done;
}

static void detect_motion(uint16_t x, uint16_t y, uint32_t now_ms)
{
    int32_t dx = (int32_t)x - centre_x, dy = (int32_t)y - centre_y;
    int32_t r2 = dx * dx + dy * dy;
    bool out = r2 >= CENTRE_RADIUS * CENTRE_RADIUS;
    if (count_circle(out, dx, dy)) move(now_ms);    /* every full circle is activity */
    was_out = out;
    if (r2 <= RELEASE_RADIUS * RELEASE_RADIUS) {    /* back near rest: re-arm */
        push_armed = true;
        far_run = false;
        return;
    }
    if (r2 < PUSH_RADIUS * PUSH_RADIUS) {           /* in between: no push, run broken */
        far_run = false;
        return;
    }
    if (!far_run) { far_run = true; far_since = now_ms; }
    if (push_armed && (uint32_t)(now_ms - far_since) >= PUSH_HOLD_MS) {
        push_armed = false;                         /* one event per push */
        move(now_ms);
    }
}

static uint16_t median3(const uint16_t v[3])
{
    uint16_t a = v[0], b = v[1], c = v[2];
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) b = c;
    return a > b ? a : b;
}

static void take_sample(uint16_t x, uint16_t y, uint32_t now_ms, bool cooking)
{
    raw_x = x; raw_y = y;
    fresh = true;
    last_sample = now_ms;
    /* Median of the last 3 removes single-sample ADC spikes before anything
     * else (centre, range, motion, spoon) sees them. */
    if (hist_n == 0) {
        hist_x[0] = hist_x[1] = hist_x[2] = x;
        hist_y[0] = hist_y[1] = hist_y[2] = y;
        smooth_x_q4 = (int32_t)x << 4;
        hist_n = 3;
    } else {
        hist_x[0] = hist_x[1]; hist_x[1] = hist_x[2]; hist_x[2] = x;
        hist_y[0] = hist_y[1]; hist_y[1] = hist_y[2]; hist_y[2] = y;
    }
    uint16_t mx = median3(hist_x), my = median3(hist_y);
    smooth_x_q4 += (((int32_t)mx << 4) - smooth_x_q4) / 4;
    uint16_t sx = (uint16_t)((smooth_x_q4 + 8) >> 4);
    track_stillness(mx, my, now_ms, cooking);
    if (!learned) return;
    detect_motion(mx, my, now_ms);
    /* Grow each side's end to the farthest (spike-free) X reached; live config. */
    if (mx < config.adc_min) config.adc_min = follower.config.adc_min = mx;
    if (mx > config.adc_max) config.adc_max = follower.config.adc_max = mx;
    Follower_Sample(&follower, sx, my, now_ms);
}

void Stirrer_Service(uint32_t now_ms, bool cooking)
{
    uint16_t x, y;
    Follower_State state = FOLLOWER_WAIT_CENTER;
    last_service = now_ms;
    JoystickADC_State adc = JoystickADC_Poll(&x, &y);
    if (adc == JOYSTICK_ADC_READY) {
        take_sample(x, y, now_ms, cooking);
    } else if (adc == JOYSTICK_ADC_FAULT) {
        restart_adc(now_ms);            /* conversion timeout: recover, never latch */
    }
    if ((uint32_t)(now_ms - last_start) >= SAMPLE_MS && JoystickADC_Start()) {
        last_start = now_ms;
    }
    if ((uint32_t)(now_ms - last_sample) >= FOLLOWER_SAMPLE_TIMEOUT_MS) {
        fresh = false;                  /* stale samples must not count as stirring */
    }
    if (learned) {
        state = Follower_Service(&follower, now_ms);
        if (state == FOLLOWER_FAULT) {  /* sample timeout: re-arm around the centre */
            (void)Follower_Init(&follower, &config, now_ms);
            state = follower.state;
        }
    }
    uint16_t want = (cooking && state == FOLLOWER_ACTIVE) ? follower.pulse_us : STIR_PULSE_CENTER_US;
    /* Hysteresis: tiny wander around a still target never reaches the servo;
     * home and both ends are always exact. */
    unsigned diff = want > pulse_us ? (unsigned)(want - pulse_us) : (unsigned)(pulse_us - want);
    if (diff >= PULSE_HYSTERESIS_US || want == STIR_PULSE_CENTER_US ||
        want == STIR_PULSE_MIN_US || want == STIR_PULSE_MAX_US) pulse_us = want;
    (void)Servo50Hz_Write(SERVO_SPOON, pulse_us);
}

bool Stirrer_OffCentre(void)
{
    /* True while the stick is MOVING (report only; scoring uses circles). */
    return fresh && learned && moved && (uint32_t)(last_service - last_move) < MOVE_HOLD_MS;
}

bool Stirrer_TakeActivity(void)
{
    bool taken = activity;
    activity = false;
    return taken;
}

uint8_t Stirrer_TakeCircles(void)
{
    uint8_t n = circles_pending;
    circles_pending = 0;
    return n;
}

uint16_t Stirrer_RawX(void) { return raw_x; }
uint16_t Stirrer_PulseUs(void) { return pulse_us; }

void Stirrer_Report(void)
{
    static const char *const names[] = {"WAIT_CENTRE", "ACTIVE", "FAULT"};
    int dx = (int)raw_x - (int)centre_x, dy = (int)raw_y - (int)centre_y;
    unsigned r = 0;                     /* integer distance from rest */
    while ((r + 1u) * (r + 1u) <= (unsigned)(dx * dx + dy * dy)) ++r;
    printf("STIR x=%u y=%u centre=%u,%u range=%u..%u learned=%u dx=%d dy=%d r=%u zone=%s moving=%u "
           "moves=%lu armed=%u circles=%lu sector=%d steps=%d follower=%s spoon=%uus\r\n",
           (unsigned)raw_x, (unsigned)raw_y, (unsigned)centre_x, (unsigned)centre_y,
           (unsigned)config.adc_min, (unsigned)config.adc_max, (unsigned)learned,
           dx, dy, r, was_out ? "OUT" : "CENTRE", (unsigned)Stirrer_OffCentre(),
           (unsigned long)moves_total, (unsigned)push_armed, (unsigned long)circles_total, (int)prev_sector, (int)sector_steps,
           learned && follower.state <= FOLLOWER_FAULT ? names[follower.state] : "LEARNING",
           (unsigned)pulse_us);
}
