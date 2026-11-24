#ifndef HEAT_AUDIO_H
#define HEAT_AUDIO_H

#include <stdint.h>

typedef enum {
    HEAT_SILENT, HEAT_LOW, HEAT_RIGHT, HEAT_HIGH
} HeatZone;

enum {
    HEAT_AUDIO_VOLUME_MAX = 16,
    HEAT_HOT_BELOW_MM = 150,
    HEAT_COOL_ABOVE_MM = 280
};

void heat_audio_start(void);
void heat_audio_play_startup_cue(void);
const char *heat_audio_update(uint32_t distance_mm);

HeatZone heat_audio_zone(void);
unsigned heat_audio_volume(void);

#endif
