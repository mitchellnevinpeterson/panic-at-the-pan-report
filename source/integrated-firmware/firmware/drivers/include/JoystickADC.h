#ifndef PANIC_JOYSTICK_ADC_H
#define PANIC_JOYSTICK_ADC_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    JOYSTICK_ADC_IDLE, JOYSTICK_ADC_BUSY, JOYSTICK_ADC_READY, JOYSTICK_ADC_FAULT
} JoystickADC_State;

/* Owns ADC1, RB2/AN4 (DIP6) and RB3/AN5 (DIP7). PBCLK must be 20 MHz.
 * One foreground owner; no interrupts or blocking conversion waits.
 */
void JoystickADC_Init(void);
bool JoystickADC_Start(void); /* False while busy, faulted or initially settling. */
/* READY consumes a complete X/Y pair (0..1023); pointers unchanged otherwise.
 * FAULT latches until Init(). Call frequently, well within the 2 ms timeout.
 */
JoystickADC_State JoystickADC_Poll(uint16_t *x, uint16_t *y);

#endif
