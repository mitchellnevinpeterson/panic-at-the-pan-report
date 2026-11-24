#ifndef PANIC_SPOON_SERVO_H
#define PANIC_SPOON_SERVO_H

#include <stdbool.h>
#include <stdint.h>

enum { SPOON_SERVO_MIN_US = 1000, SPOON_SERVO_MAX_US = 2000 };

/* Bench driver: exclusively owns Timer2 + OC4 and RA4/DIP12. PBCLK=20 MHz.
 * Init leaves signal LOW. First valid WritePulse enables 50 Hz pulses.
 * Pulse limits are electrical bounds, not a guarantee of safe travel.
 */
void SpoonServo_Init(void);
bool SpoonServo_WritePulse(uint16_t pulse_us);
void SpoonServo_Disable(void); /* Stops pulses; does not guarantee holding torque. */

#endif
