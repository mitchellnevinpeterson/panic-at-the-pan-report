#ifndef PIC2_RANGE_H
#define PIC2_RANGE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint32_t mark, rise, next, sample_ticks;
    uint16_t mm;
    uint8_t state;
    bool trigger, valid, sample_ready;
} Pic2Range;
void Pic2Range_Init(Pic2Range *,uint32_t core_ticks);
/* 20 MHz free-running counter; never resets CP0. Call at high foreground rate. */
void Pic2Range_Poll(Pic2Range *,uint32_t core_ticks,bool echo);
#endif
