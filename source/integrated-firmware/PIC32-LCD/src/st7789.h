#ifndef ST7789_H
#define ST7789_H

#include <stdbool.h>
#include <stdint.h>

enum { LCD_WIDTH = 320, LCD_HEIGHT = 240 };

void st7789_prepare_pins(void);
bool st7789_start(uint16_t background);
bool st7789_fill(unsigned x, unsigned y, unsigned width, unsigned height, uint16_t color);
/* Repeat one row of pixels down a rectangle. */
bool st7789_repeat_row(unsigned x, unsigned y, unsigned width, unsigned height,
                       const uint16_t *pixels);
bool st7789_draw_glyph(unsigned x, unsigned y, const uint8_t columns[5],
                       unsigned scale, uint16_t foreground, uint16_t background);

#endif
