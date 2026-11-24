/* Hardware contract only: calls return DRIVER_NOT_CONFIGURED until brought up.
 * Read output pointers remain unchanged on failure; callers must check status.
 * Calls must never wait for motion, an echo, or an SPI transfer to complete.
 */
#ifndef PANIC_JOYSTICK_H
#define PANIC_JOYSTICK_H
#include <stdbool.h>
#include <stdint.h>
#include "DriverStatus.h"

DriverStatus_t Joystick_ReadRaw(uint16_t *x_adc, uint16_t *y_adc);

#endif
