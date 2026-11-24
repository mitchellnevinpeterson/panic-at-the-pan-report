/* Hardware contract only: calls return DRIVER_NOT_CONFIGURED until brought up.
 * Read output pointers remain unchanged on failure; callers must check status.
 * Calls must never wait for motion, an echo, or an SPI transfer to complete.
 */
#ifndef PANIC_ULTRASONIC_H
#define PANIC_ULTRASONIC_H
#include <stdbool.h>
#include <stdint.h>
#include "DriverStatus.h"

DriverStatus_t Ultrasonic_ReadMillimeters(uint16_t *distance_mm);

#endif
