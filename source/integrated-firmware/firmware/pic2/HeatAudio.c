/* Final-game PIC2 audio, adapted from PIC32-LCD/src/heat_audio.c + sizzle.h.
 * Differences from the teammate bench: no PPS write (RPB15R = 5 lives in
 * pic2_main's first PPS block), no volume encoder (volume fixed at max), the
 * zone comes from PIC1 instead of the local ranger, and a one-shot "burnt"
 * cue is mixed in here so sizzle.h stays untouched.
 */
#include "HeatAudio.h"
#include "sizzle.h" /* PIC32-LCD/src; brings HeatZone from heat_audio.h. */
#include <xc.h>
#include <sys/attribs.h>

#define AUDIO_PIN (1u << 15) /* RB15, DIP 26, OC1 */
#define PWM_COUNTS 256u
#define PWM_CENTER (PWM_COUNTS / 2u)
#define SAMPLE_COUNTS (20000000u / SIZZLE_SAMPLE_HZ)

/* Burnt cue: ~0.6 s square-wave buzz sliding 440 Hz -> 110 Hz. The pitch step
 * is Q8 phase-increment so the ISR needs no division. */
enum {
    BURN_SAMPLES = 7500,                       /* 0.6 s at 12.5 kHz */
    BURN_START_Q8 = (440 * 65536 / 12500) << 8, /* phase step at 440 Hz */
    BURN_END_Q8 = (110 * 65536 / 12500) << 8,
    BURN_SLIDE_Q8 = (BURN_START_Q8 - BURN_END_Q8) / BURN_SAMPLES,
    BURN_LEVEL = 56, BURN_ATTACK = 64, BURN_RELEASE = 1024
};

static volatile uint8_t heat_zone = HEAT_SILENT;
static volatile uint8_t burn_requests; /* Foreground increments; ISR compares. */
static uint8_t last_burn_seq;
static bool have_burn_seq;

void HeatAudio_Init(void)
{
    ANSELBCLR = AUDIO_PIN;
    LATBCLR = AUDIO_PIN;
    TRISBCLR = AUDIO_PIN;

    T2CON = 0;
    TMR2 = 0;
    PR2 = PWM_COUNTS - 1u; /* 78.125 kHz PWM at 20 MHz PBCLK. */
    OC1CON = 0;
    OC1R = PWM_CENTER;
    OC1RS = PWM_CENTER;
    OC1CONbits.OCM = 6u; /* PWM on Timer2, fault input disabled. */
    T2CONbits.ON = 1;
    OC1CONbits.ON = 1;

    T3CON = 0;
    TMR3 = 0;
    PR3 = SAMPLE_COUNTS - 1u; /* 12.5 kHz sample rate. */
    IPC3bits.T3IP = 2; /* Below the UART2 RX ISR (IPL4). */
    IPC3bits.T3IS = 0;
    IFS0CLR = _IFS0_T3IF_MASK;
    IEC0SET = _IEC0_T3IE_MASK;
    INTCONSET = _INTCON_MVEC_MASK;
    T3CONbits.ON = 1;
    __builtin_enable_interrupts();
}

void HeatAudio_Set(bool sizzle, uint8_t zone, uint8_t burn_seq)
{
    heat_zone = (sizzle && zone <= HEAT_HIGH) ? zone : HEAT_SILENT;
    burn_seq &= 3u;
    /* The first value only latches: a boot or reconnect is not a burn. */
    if (have_burn_seq && burn_seq != last_burn_seq) ++burn_requests;
    last_burn_seq = burn_seq;
    have_burn_seq = true;
}

bool HeatAudio_Healthy(void)
{
    return RPB15R == 5u && OC1CONbits.ON && T2CONbits.ON && T3CONbits.ON && IEC0bits.T3IE;
}

/* Signed sample of the burnt cue, 0 when idle. */
static int burn_sample(void)
{
    static uint8_t seen;
    static unsigned age = BURN_SAMPLES;
    static uint32_t step_q8;
    static uint16_t phase;
    if (seen != burn_requests) {
        seen = burn_requests;
        age = 0;
        step_q8 = BURN_START_Q8;
        phase = 0;
    }
    if (age >= BURN_SAMPLES) return 0;
    const unsigned remaining = BURN_SAMPLES - age;
    int level = BURN_LEVEL;
    if (age < BURN_ATTACK) level = level * (int)age / BURN_ATTACK;
    else if (remaining < BURN_RELEASE) level = level * (int)remaining / BURN_RELEASE;
    ++age;
    phase += (uint16_t)(step_q8 >> 8);
    step_q8 -= BURN_SLIDE_Q8;
    return (phase & 0x8000u) ? level : -level;
}

void __ISR(_TIMER_3_VECTOR, IPL2SOFT) HeatAudio_Sample(void)
{
    static SizzleState state = SIZZLE_INITIAL_STATE;
    /* No zone-change chime (user request): mark each new zone's cue as already
     * played so sizzle.h (shared with the benches) never starts it. The sizzle
     * texture still follows the zone and the music is not ducked. */
    if ((HeatZone)heat_zone != state.last_zone) {
        state.last_zone = (HeatZone)heat_zone;
        state.cue_age = CUE_TOTAL_SAMPLES;
    }
    int sample = sizzle_sample(&state, (HeatZone)heat_zone);
    const int burn = burn_sample();
    if (burn) sample = sample / 2 + burn; /* Duck music/sizzle under the buzz. */
    if (sample > 127) sample = 127;
    else if (sample < -128) sample = -128;
    /* Volume fixed at max: the bench's gain path reduces to unity here. */
    OC1RS = (unsigned)((int)PWM_CENTER + sample);
    IFS0CLR = _IFS0_T3IF_MASK;
}
