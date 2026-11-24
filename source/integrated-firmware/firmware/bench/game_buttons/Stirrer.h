#ifndef GAME_BUTTONS_STIRRER_H
#define GAME_BUTTONS_STIRRER_H
#include <stdbool.h>
#include <stdint.h>

/* Joystick X (JoystickADC, PIC1 pins 6/7) -> spoon (Servo50Hz SERVO_SPOON),
 * side-to-side follower reused from bench/joystick_servo_2/Follower.c.
 * About +/-22.5 deg around home; endpoints are placeholders to calibrate.
 */
#ifndef STIR_PULSE_MIN_US
#define STIR_PULSE_MIN_US 1250u     /* TODO(calibrate) */
#endif
#ifndef STIR_PULSE_CENTER_US
#define STIR_PULSE_CENTER_US 1500u  /* home */
#endif
#ifndef STIR_PULSE_MAX_US
#define STIR_PULSE_MAX_US 1750u     /* TODO(calibrate) */
#endif
#define STIR_ACTIVITY_COUNTS 40u    /* X change that counts as idle activity */

void Stirrer_Init(uint32_t now_ms);  /* ADC + follower; spoon commanded home */
/* Call every main-loop pass. cooking=false holds the spoon at home. ADC or
 * follower faults re-initialize instead of latching. */
void Stirrer_Service(uint32_t now_ms, bool cooking);
/* Centre is learned (stick still 0.3 s at boot; re-learned after 3 s still
 * outside cooking). "Moving" = a push (>= 150 counts from rest for 100 ms
 * straight, re-armed back within 100) or a completed stir circle. */
bool Stirrer_OffCentre(void);     /* true while MOVING (diagnostic "moving=" in the report) */
bool Stirrer_TakeActivity(void);  /* true once per detected move */
/* Full joystick circles (8 sectors in one direction) since the last call. */
uint8_t Stirrer_TakeCircles(void);
uint16_t Stirrer_RawX(void);
uint16_t Stirrer_PulseUs(void);   /* last commanded spoon pulse */
void Stirrer_Report(void);        /* printf joystick/centre/motion/follower state ('j' key) */

#endif
