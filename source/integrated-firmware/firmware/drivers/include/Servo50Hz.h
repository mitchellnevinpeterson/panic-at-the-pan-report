#ifndef PANIC_SERVO50HZ_H
#define PANIC_SERVO50HZ_H
#include <stdbool.h>
#include <stdint.h>

/* PIC1 two-channel 50 Hz servo driver, PBCLK = 20 MHz. Exclusively owns
 * Timer2, OC4 -> RA4/pin12 (spoon S51) and OC2 -> RB5/pin14 (treat MG995).
 * Timer2 runs continuously once initialized, so one channel never stops the
 * other. Call Servo50Hz_Init() AFTER ButtonEInkPort_Init() (that port parks
 * RA4/RB5 LOW and clears OC2CON/OC4CON). Init leaves both outputs LOW.
 */
typedef enum { SERVO_SPOON = 0, SERVO_TREAT = 1 } Servo50Hz_Channel;
enum { SERVO50HZ_MIN_US = 500, SERVO50HZ_MAX_US = 2500 }; /* electrical bounds */

void Servo50Hz_Init(void);
/* Enables pulses on that channel at the given width. False if out of bounds. */
bool Servo50Hz_Write(Servo50Hz_Channel channel, uint16_t pulse_us);
/* Stops pulses on that channel only (signal LOW, no holding torque). */
void Servo50Hz_Disable(Servo50Hz_Channel channel);
/* Diagnostic: live readback of RPA4R/RPB5R. False if Init never ran or the
 * PPS writes did not take (see the PIC2 PPS quirk); print it at boot. */
bool Servo50Hz_Routed(void);
/* Diagnostic: printf the live Timer2 / OC2 / OC4 / PPS / pin registers. */
void Servo50Hz_Report(void);

#endif
