#ifndef PIC2_AUDIO_H
#define PIC2_AUDIO_H
#include <stdbool.h>
#include <stdint.h>
void Pic2Audio_Init(void);
void Pic2Audio_Set(bool enabled,uint8_t zone);
#endif
