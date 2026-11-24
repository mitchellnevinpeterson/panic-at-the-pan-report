#include "board.h"
#include "heat_audio.h"
#include "sizzle.h"
#include <stddef.h>

#define AUDIO_PIN (1u << 15) /* RB15, DIP 26 */
#define PWM_COUNTS 256u
#define PWM_CENTER (PWM_COUNTS / 2u)
#define SAMPLE_COUNTS (20000000u / SIZZLE_SAMPLE_HZ)
#define ZONE_CONFIRM_SAMPLES 3u
#define VOLUME_ENCODER_MASK (3u << 2) /* RA2 and RA3, DIP 9 and 10 */

enum { VOLUME_POLL_SAMPLES = 12 };

static volatile HeatZone heat_zone;
static HeatZone pending_zone;
static unsigned pending_samples;
static unsigned encoder_previous;
static int encoder_travel;
static volatile unsigned volume = HEAT_AUDIO_VOLUME_MAX;

void heat_audio_start(void)
{
    TRISASET = VOLUME_ENCODER_MASK;
    CNPUASET = VOLUME_ENCODER_MASK;
    encoder_previous = (PORTA & VOLUME_ENCODER_MASK) >> 2;

    ANSELBCLR = AUDIO_PIN;
    LATBCLR = AUDIO_PIN;
    TRISBCLR = AUDIO_PIN;
    pps_unlock();
    RPB15R = 5u; /* OC1 */
    pps_lock();

    T2CON = 0;
    TMR2 = 0;
    PR2 = PWM_COUNTS - 1u; /* 78.125 kHz at 20 MHz PBCLK. */
    OC1CON = 0;
    OC1R = PWM_CENTER;
    OC1RS = PWM_CENTER;
    OC1CONbits.OCM = 6u; /* PWM on Timer2 with the fault input disabled. */
    T2CONbits.ON = 1;
    OC1CONbits.ON = 1;

    T3CON = 0;
    TMR3 = 0;
    PR3 = SAMPLE_COUNTS - 1u; /* 12.5 kHz audio sample rate. */
    IPC3bits.T3IP = 2;
    IPC3bits.T3IS = 0;
    IFS0CLR = _IFS0_T3IF_MASK;
    IEC0SET = _IEC0_T3IE_MASK;
    INTCONSET = _INTCON_MVEC_MASK;
    T3CONbits.ON = 1;
    __builtin_enable_interrupts();
}

void heat_audio_play_startup_cue(void)
{
    heat_zone = HEAT_RIGHT;
    delay_us((1000000u / SIZZLE_SAMPLE_HZ) * CUE_TOTAL_SAMPLES);
    heat_zone = HEAT_SILENT;
}

const char *heat_audio_update(uint32_t distance_mm)
{
    static const char *const messages[] = {
        "HEAT: no valid distance\r\n", "HEAT: not enough\r\n",
        "HEAT: just right\r\n", "HEAT: too much\r\n"
    };
    HeatZone next = HEAT_SILENT;
    if (distance_mm != 0u) {
        if (distance_mm < HEAT_HOT_BELOW_MM) {
            next = HEAT_HIGH;
        } else if (distance_mm > HEAT_COOL_ABOVE_MM) {
            next = HEAT_LOW;
        } else {
            next = HEAT_RIGHT;
        }
    }

    /* Require several agreeing readings before switching the sound. */
    if (next == heat_zone) {
        pending_samples = 0;
        return NULL;
    }
    if (next != pending_zone) {
        pending_zone = next;
        pending_samples = 0;
    }
    if (++pending_samples < ZONE_CONFIRM_SAMPLES) {
        return NULL;
    }
    heat_zone = next;
    pending_samples = 0;
    return messages[next];
}

HeatZone heat_audio_zone(void)
{
    return heat_zone;
}

unsigned heat_audio_volume(void)
{
    return volume;
}

static void poll_volume_encoder(void)
{
    static const int8_t direction[16] = {
         0,  1, -1,  0,
        -1,  0,  0,  1,
         1,  0,  0, -1,
         0, -1,  1,  0
    };
    const unsigned current = (PORTA & VOLUME_ENCODER_MASK) >> 2;
    const unsigned previous = encoder_previous;
    encoder_previous = current;

    /* Bounce cancels out. A skipped quadrature state discards a partial turn. */
    if ((previous ^ current) == 3u) {
        encoder_travel = 0;
    }
    encoder_travel += direction[previous * 4u + current];
    if (encoder_travel == 4) {
        if (volume < HEAT_AUDIO_VOLUME_MAX) {
            ++volume;
        }
        encoder_travel = 0;
    } else if (encoder_travel == -4) {
        if (volume > 0u) {
            --volume;
        }
        encoder_travel = 0;
    }
}

void __ISR(_TIMER_3_VECTOR, IPL2SOFT) heat_audio_sample(void)
{
    static SizzleState state = SIZZLE_INITIAL_STATE;
    static unsigned encoder_poll;
    static int gain = HEAT_AUDIO_VOLUME_MAX * 256;
    if (++encoder_poll == VOLUME_POLL_SAMPLES) {
        encoder_poll = 0;
        poll_volume_encoder();
    }
    gain += ((int)volume * 256 - gain) / 16;
    const int sample = sizzle_sample(&state, heat_zone);
    OC1RS = (unsigned)((int)PWM_CENTER + sample * ((gain + 15) / 16) / 256);
    IFS0CLR = _IFS0_T3IF_MASK;
}
