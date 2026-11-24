#include "InputButtons.h"

DriverStatus_t InputButtons_Read(uint8_t *ingredient_bits, bool *finish_pressed,
                                bool *start_pressed)
{
    (void)ingredient_bits;
    (void)finish_pressed;
    (void)start_pressed;
    /* TODO(team): implement after parts, pins, and calibration are confirmed. */
    return DRIVER_NOT_CONFIGURED;
}

