/* Hardware contract only: calls return DRIVER_NOT_CONFIGURED until brought up.
 * Read output pointers remain unchanged on failure; callers must check status.
 * Calls must never wait for motion, an echo, or an SPI transfer to complete.
 */
#ifndef PANIC_SERVOS_H
#define PANIC_SERVOS_H
#include <stdbool.h>
#include <stdint.h>
#include "DriverStatus.h"

typedef enum { SPOON_STOPPED = 0, SPOON_CLOCKWISE, SPOON_COUNTERCLOCKWISE } SpoonDirection_t;

DriverStatus_t Servos_SetSpoonDirection(SpoonDirection_t direction);
DriverStatus_t Servos_SetFireRaised(bool raised);

#endif
