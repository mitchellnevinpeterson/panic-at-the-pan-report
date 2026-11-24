/* Hardware contract only: calls return DRIVER_NOT_CONFIGURED until brought up.
 * Read output pointers remain unchanged on failure; callers must check status.
 * Calls must never wait for motion, an echo, or an SPI transfer to complete.
 */
#ifndef PANIC_SOUND_H
#define PANIC_SOUND_H
#include <stdbool.h>
#include <stdint.h>
#include "DriverStatus.h"

DriverStatus_t Sound_SetHeatPermille(uint16_t heat);
DriverStatus_t Sound_PlayBurn(void);

#endif
