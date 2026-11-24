#ifndef PIC2_INPUTS_H
#define PIC2_INPUTS_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint8_t raw,stable,armed; uint32_t changed[8]; } Pic2Inputs;
void Pic2Inputs_Reset(Pic2Inputs *,uint8_t active_low_gpio,uint32_t now);
/* Returns press edges only after stable release then stable press (20 ms each).
   changed[id] is the beginning of the accepted stable press, not delivery time. */
uint8_t Pic2Inputs_Update(Pic2Inputs *,uint8_t gpio,uint32_t now,bool synchronized);
#endif
