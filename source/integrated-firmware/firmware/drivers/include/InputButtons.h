/* Hardware contract only: calls return DRIVER_NOT_CONFIGURED until brought up.
 * Read output pointers remain unchanged on failure; callers must check status.
 * Calls must never wait for motion, an echo, or an SPI transfer to complete.
 */
#ifndef PANIC_INPUTBUTTONS_H
#define PANIC_INPUTBUTTONS_H
#include <stdbool.h>
#include <stdint.h>
#include "DriverStatus.h"

/* Contact levels only; the future event checker must debounce edges and
 * attach detection timestamps/session information before posting events.
 */
DriverStatus_t InputButtons_Read(uint8_t *ingredient_bits, bool *finish_pressed,
                                bool *start_pressed);

#endif
