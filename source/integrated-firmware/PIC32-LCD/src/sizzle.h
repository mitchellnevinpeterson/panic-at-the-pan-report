#ifndef SIZZLE_H
#define SIZZLE_H

#include "heat_audio.h"

#define SIZZLE_VOICES 8u
#define SIZZLE_INITIAL_STATE {.noise = 0x963DB54Du}
#define SIZZLE_SAMPLE_HZ 12500u
enum {
    CUE_NOTE_SAMPLES = 1550, CUE_GAP_SAMPLES = 220, CUE_RAMP_SAMPLES = 40,
    CUE_TOTAL_SAMPLES = 2 * CUE_NOTE_SAMPLES + CUE_GAP_SAMPLES
};

typedef struct {
    uint16_t phase;
    uint16_t rate;
    uint16_t envelope;
    uint8_t decay;
    uint8_t age;
} OilDrop;

typedef struct {
    uint32_t noise;
    int fast;
    int slow;
    int level;
    int flare;
    int steam;
    int steam_target;
    HeatZone last_zone;
    unsigned cue_age;
    uint16_t cue_phase;
    uint16_t music_tick;
    uint16_t lead_phase;
    uint16_t bass_phase;
    uint16_t lead_step;
    uint16_t bass_step;
    uint8_t music_step;
    uint8_t music_bar;
    int music_gain;
    OilDrop drops[SIZZLE_VOICES];
} SizzleState;

static inline int sine_sample(uint16_t phase)
{
    static const int8_t sine[] = {
          0,  25,  49,  71,  90,  106,  117,  125,
        127, 125, 117, 106,  90,   71,   49,   25,
          0, -25, -49, -71, -90, -106, -117, -125,
       -127,-125,-117,-106, -90,  -71,  -49,  -25
    };
    return sine[phase >> 11];
}

/* Original 32-bar loop at about 128 BPM. Each step is an eighth note. */
static inline int pan_music_sample(SizzleState *state, int target_gain)
{
    static const struct { uint8_t root; uint8_t phrase; } score[] = {
        {45, 0}, {41, 1}, {48, 0}, {43, 1}, {45, 2}, {41, 0}, {48, 2}, {43, 3},
        {45, 0}, {41, 2}, {48, 1}, {43, 2}, {41, 1}, {43, 0}, {45, 2}, {45, 3},
        {48, 2}, {43, 1}, {45, 0}, {41, 2}, {48, 0}, {43, 2}, {41, 1}, {43, 3},
        {45, 0}, {41, 1}, {48, 0}, {43, 1}, {41, 2}, {43, 2}, {45, 1}, {40, 3}
    };
    static const int8_t melody[][8] = {
        { 0, -1, 7, 12, -1, 7,  5, -1},
        {-1,  4, 7, -1, 12, 9, -1,  7},
        {12,  7, 9, -1,  5, 7, -1,  0},
        { 7, -1, 5,  2, -1, 0, -1, -1}
    };
    static const int8_t bass[] = {0, -1, 7, -1, 12, -1, 7, -1};
    /* Phase increments for C2 through B2 at 12.5 kHz. */
    static const uint16_t phase_step[] = {343, 363, 385, 408, 432, 457,
                                          485, 514, 544, 577, 611, 647};
    enum { STEP_SAMPLES = 2929 };

    if (state->music_tick == 0u) {
        const unsigned bar = state->music_bar;
        const unsigned step = state->music_step;
        const unsigned root = score[bar].root;
        int lead = melody[score[bar].phrase][step];
        if (root == 45u && lead == 4) {
            lead = 3;
        }
        const unsigned bass_note = root + (unsigned)(bass[step] < 0 ? 0 : bass[step]);
        const unsigned lead_note = root + 24u + (unsigned)(lead < 0 ? 0 : lead);
        state->lead_step = lead < 0 ? 0u :
            (uint16_t)(phase_step[lead_note % 12u] << (lead_note / 12u - 3u));
        state->bass_step = bass[step] < 0 ? 0u :
            (uint16_t)(phase_step[bass_note % 12u] << (bass_note / 12u - 3u));
        state->lead_phase = 0;
        state->bass_phase = 0;
    }

    /* Short plucks leave gaps for the oil pops. */
    const int attack = state->music_tick < 32u ? state->music_tick : 32;
    const int decay = (STEP_SAMPLES - state->music_tick) >> 5;
    const int envelope = decay * decay * attack / 8192;
    state->lead_phase += state->lead_step;
    state->bass_phase += state->bass_step;
    const int lead = sine_sample(state->lead_phase)
                   + sine_sample((uint16_t)(state->lead_phase * 2u)) / 3;
    const int bass_sample = sine_sample(state->bass_phase);
    const int mix = (2 * lead + bass_sample) * envelope / 512;

    if (++state->music_tick == STEP_SAMPLES) {
        state->music_tick = 0;
        if (++state->music_step == 8u) {
            state->music_step = 0;
            state->music_bar = (state->music_bar + 1u) % (sizeof(score) / sizeof(score[0]));
        }
    }
    state->music_gain += (target_gain - state->music_gain) / 32;
    return mix * state->music_gain / 4096;
}

/* Each drop starts with a snap and falls into a short, lower bubble sound. */
static inline int oil_drop_sample(OilDrop *drop, int snap, int bubble)
{
    if (drop->envelope < 256u) {
        return 0;
    }

    int amplitude = drop->envelope >> 8;
    int ring = sine_sample(drop->phase);
    /* The dry attack gives way to a damped, descending bubble. */
    int texture = (2 * bubble + ring) / 3;
    if (drop->age < 16u) {
        texture += 2 * snap * (16 - (int)drop->age) / 16;
        if (drop->age < 4u) {
            amplitude = amplitude * (int)drop->age / 4;
        }
        ++drop->age;
    }
    drop->phase += drop->rate;
    drop->rate -= drop->rate >> 6;
    drop->envelope -= (drop->envelope * (unsigned)drop->decay >> 8) + 1u;
    return texture * amplitude / 128;
}

static inline int heat_cue_sample(SizzleState *state, HeatZone zone)
{
    static const struct {
        unsigned hz[2];
        int volume;
    } cues[] = {
        {{   0,    0},  0},
        {{ 587,  392}, 32},
        {{ 523,  784}, 39},
        {{ 880,  880}, 45}
    };

    if (zone != state->last_zone) {
        state->last_zone = zone;
        state->cue_age = 0;
        state->cue_phase = 0;
    }
    if (zone == HEAT_SILENT || state->cue_age >= CUE_TOTAL_SAMPLES) {
        return 0;
    }

    const unsigned age = state->cue_age++;
    const unsigned note = age >= CUE_NOTE_SAMPLES + CUE_GAP_SAMPLES;
    const unsigned position = note ? age - CUE_NOTE_SAMPLES - CUE_GAP_SAMPLES : age;
    if (position >= CUE_NOTE_SAMPLES) {
        return 0;
    }
    if (position == 0u) {
        state->cue_phase = 0;
    }
    /* A quick strike excites the upper partials, which fade before the fundamental. */
    const int remaining = (int)(CUE_NOTE_SAMPLES - position) * 256 / CUE_NOTE_SAMPLES;
    const int decay = remaining * remaining / 256;
    const int attack = position < CUE_RAMP_SAMPLES ? (int)position * 256 / CUE_RAMP_SAMPLES : 256;
    state->cue_phase += (uint16_t)(cues[zone].hz[note] * 65536u / SIZZLE_SAMPLE_HZ);
    const int fundamental = sine_sample(state->cue_phase);
    const int partials = sine_sample((uint16_t)(2u * state->cue_phase)) / 4
                       + sine_sample((uint16_t)(3u * state->cue_phase)) / 8;
    const int bell = fundamental + partials * decay / 256;
    return bell * cues[zone].volume * (attack * decay / 256) / (128 * 256);
}

/* Returns a signed 8-bit sample for the PWM output or the Mac preview. */
static inline int sizzle_sample(SizzleState *state, HeatZone zone)
{
    static const struct {
        unsigned drops;
        unsigned clusters;
        unsigned steam_bursts;
        int strength;
        unsigned rate;
        unsigned decay;
        int bed;
        int hiss;
    } profiles[] = {
        {   0,  0, 0,  0,    0,  0,  0,  0 },
        {  44,  0, 0, 19, 2200,  6,  0,  0 },
        { 350, 15, 4, 31, 2900,  7, 17,  3 },
        {1030, 35, 9, 42, 4700,  9, 26,  9 }
    };

    uint32_t noise = state->noise;
    noise ^= noise << 13;
    noise ^= noise >> 17;
    noise ^= noise << 5;
    state->noise = noise;
    const int raw = (int)(noise >> 24) - 128;
    state->fast += (raw * 256 - state->fast) / 3;
    state->slow += (raw * 256 - state->slow) / 12;
    const int snap = raw - state->fast / 256;
    const int bubble = (state->fast - state->slow) / 128;

    /* Short clusters of bubbles sit under longer swells of steam. */
    if (((noise >> 8) & 0xffffu) < profiles[zone].clusters) {
        state->flare = 24576 + (int)(noise >> 24) * 96;
    }
    state->flare -= (state->flare + 2047) / 2048;
    if (((noise >> 16) ^ (noise & 0xffffu)) < profiles[zone].steam_bursts) {
        state->steam_target = 24576 + (int)(noise & 0xffu) * 128;
    }
    state->steam_target -= (state->steam_target + 4095) / 4096;
    state->steam += (state->steam_target - state->steam) / 320;

    const unsigned activity = profiles[zone].drops;
    const unsigned steam = (unsigned)state->steam >> 8;
    const unsigned cluster = (unsigned)state->flare >> 8;
    if ((noise & 0xffffu) < activity + activity * (cluster + steam) / 96u) {
        OilDrop *quietest = &state->drops[0];
        for (unsigned i = 1; i < SIZZLE_VOICES; ++i) {
            if (state->drops[i].envelope < quietest->envelope) {
                quietest = &state->drops[i];
            }
        }
        const unsigned variation = noise >> 24;
        if (quietest->envelope < 512u) {
            quietest->phase = 0;
            quietest->rate = profiles[zone].rate + variation * 28u;
            quietest->envelope = (profiles[zone].strength + (variation >> 3)) * 256;
            quietest->decay = profiles[zone].decay + (variation & 7u);
            quietest->age = 0;
        }
    }

    int pops = 0;
    for (unsigned i = 0; i < SIZZLE_VOICES; ++i) {
        pops += oil_drop_sample(&state->drops[i], snap, bubble);
    }

    /* Steam lifts the brighter frequencies above the low bubbling bed. */
    const int bed_gain = profiles[zone].bed;
    const int hiss_gain = profiles[zone].hiss;
    const int bed = (bubble * (bed_gain + bed_gain * (int)cluster / 192)
                    + snap * (hiss_gain + hiss_gain * (int)steam / 48)) / 128;
    /* Frying gain by zone, with 65536 representing full scale. */
    static const int frying_gain[] = {
        [HEAT_SILENT] = 0,
        [HEAT_LOW] = 65536,
        [HEAT_RIGHT] = 24576,
        [HEAT_HIGH] = 16384
    };
    const int target = frying_gain[zone];
    state->level += (target - state->level) / 128;
    const int cue = heat_cue_sample(state, zone);
    const int during_cue = state->cue_age > 0u && state->cue_age < CUE_TOTAL_SAMPLES;
    const int texture = (bed + pops) * (state->level / 256) / 256;
    /* Keep the music near its idle level while frying and duck it for chimes. */
    const int music_gain = during_cue ? 384 : zone == HEAT_SILENT ? 4096 : 3584;
    const int music = pan_music_sample(state, music_gain);
    int sample = cue + (during_cue ? texture / 2 : texture) + music;
    if (sample > 88) {
        sample = 88 + (sample - 88) / 6;
    } else if (sample < -88) {
        sample = -88 + (sample + 88) / 6;
    }
    if (sample < -128) {
        return -128;
    }
    if (sample > 127) {
        return 127;
    }
    return sample;
}

#endif
