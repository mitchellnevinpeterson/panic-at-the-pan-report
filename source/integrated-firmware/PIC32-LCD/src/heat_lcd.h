#ifndef HEAT_LCD_H
#define HEAT_LCD_H

#include <stdbool.h>
#include <stdint.h>

bool heat_lcd_start(void);
void heat_lcd_set_distance(uint32_t distance_mm);
void heat_lcd_update(void);

#endif
