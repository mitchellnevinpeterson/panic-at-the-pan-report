/* Hardware contract only: calls return DRIVER_NOT_CONFIGURED until brought up.
 * Read output pointers remain unchanged on failure; callers must check status.
 * Calls must never wait for motion, an echo, or an SPI transfer to complete.
 */
#ifndef PANIC_LEDPANELS_H
#define PANIC_LEDPANELS_H
#include <stdbool.h>
#include <stdint.h>
#include "DriverStatus.h"

DriverStatus_t LedPanels_SetScore(int32_t score);
DriverStatus_t LedPanels_SetElapsedSeconds(uint16_t seconds);
DriverStatus_t LedPanels_SetFinishLight(bool on);
DriverStatus_t LedPanels_SetHeatPermille(uint16_t heat);
DriverStatus_t LedPanels_SetPanFrame(const uint32_t *rows, uint8_t row_count);

#endif
