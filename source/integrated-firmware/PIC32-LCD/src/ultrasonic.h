#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include <stdbool.h>
#include <stdint.h>

void ultrasonic_start(bool shared_board);
bool ultrasonic_poll(void);
void ultrasonic_write_text(const char *message);
uint32_t ultrasonic_distance_mm(void); /* Zero when no valid reading is available. */

#endif
