#include "st7789.h"
#include "display_spi.h"
#include <stddef.h>

#define LCD_CS  (1u << 10) /* RB10, DIP 21 */
#define LCD_DC  (1u << 11) /* RB11, DIP 22 */
#define LCD_RST (1u << 13) /* RB13, DIP 24 */

enum {
    SWRESET = 0x01, SLPOUT = 0x11, NORON = 0x13, INVON = 0x21,
    DISPON = 0x29, CASET = 0x2A, RASET = 0x2B, RAMWR = 0x2C,
    MADCTL = 0x36, COLMOD = 0x3A
};

static bool command(uint8_t opcode, const uint8_t *data, size_t count)
{
    LATBCLR = LCD_CS | LCD_DC;
    bool ok = display_spi_write(opcode);
    LATBSET = LCD_DC;
    for (size_t i = 0; i < count && ok; ++i) {
        ok = display_spi_write(data[i]);
    }
    LATBSET = LCD_CS;
    return ok;
}

void st7789_prepare_pins(void)
{
    ANSELBCLR = LCD_RST;
    LATBSET = LCD_CS | LCD_RST;
    LATBCLR = LCD_DC;
    TRISBCLR = LCD_CS | LCD_DC | LCD_RST;
}

bool st7789_start(uint16_t background)
{
    const uint8_t rgb565 = 0x55;
    const uint8_t landscape = 0xA8; /* Swap row and column, reverse rows, BGR panel. */
    LATBCLR = LCD_RST;
    delay_us(20000u);
    LATBSET = LCD_RST;
    delay_us(120000u);
    if (!command(SWRESET, NULL, 0)) {
        return false;
    }
    delay_us(150000u);
    if (!command(SLPOUT, NULL, 0)) {
        return false;
    }
    delay_us(120000u);
    return command(COLMOD, &rgb565, 1)
           && command(MADCTL, &landscape, 1)
           && command(INVON, NULL, 0)
           && command(NORON, NULL, 0)
           && st7789_fill(0, 0, LCD_WIDTH, LCD_HEIGHT, background)
           && command(DISPON, NULL, 0);
}

/* On success the caller owns CS until all pixels have been written. */
static bool begin_pixels(unsigned x, unsigned y, unsigned width, unsigned height)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT || width == 0u || height == 0u
        || width > LCD_WIDTH - x || height > LCD_HEIGHT - y) {
        return false;
    }
    const unsigned right = x + width - 1u;
    const unsigned bottom = y + height - 1u;
    const uint8_t columns[] = {x >> 8, x, right >> 8, right};
    const uint8_t rows[] = {y >> 8, y, bottom >> 8, bottom};
    if (!command(CASET, columns, sizeof(columns))
        || !command(RASET, rows, sizeof(rows))) {
        return false;
    }
    LATBCLR = LCD_CS | LCD_DC;
    if (!display_spi_write(RAMWR)) {
        LATBSET = LCD_CS;
        return false;
    }
    LATBSET = LCD_DC;
    return true;
}

static inline bool write_pixel(uint16_t color)
{
    return display_spi_write(color >> 8) && display_spi_write((uint8_t)color);
}

bool st7789_fill(unsigned x, unsigned y, unsigned width, unsigned height, uint16_t color)
{
    if (!begin_pixels(x, y, width, height)) {
        return false;
    }
    bool ok = true;
    for (unsigned i = 0; i < width * height && ok; ++i) {
        ok = write_pixel(color);
    }
    LATBSET = LCD_CS;
    return ok;
}

bool st7789_repeat_row(unsigned x, unsigned y, unsigned width, unsigned height,
                       const uint16_t *pixels)
{
    if (!begin_pixels(x, y, width, height)) {
        return false;
    }
    bool ok = true;
    for (unsigned row = 0; row < height && ok; ++row) {
        for (unsigned col = 0; col < width && ok; ++col) {
            ok = write_pixel(pixels[col]);
        }
    }
    LATBSET = LCD_CS;
    return ok;
}

bool st7789_draw_glyph(unsigned x, unsigned y, const uint8_t columns[5],
                       unsigned scale, uint16_t foreground, uint16_t background)
{
    if (scale > LCD_HEIGHT / 7u || !begin_pixels(x, y, 6u * scale, 7u * scale)) {
        return false;
    }
    bool ok = true;
    for (unsigned row = 0; row < 7u * scale && ok; ++row) {
        const unsigned bit = 1u << (row / scale);
        for (unsigned col = 0; col < 6u && ok; ++col) {
            const uint16_t color = col < 5u && (columns[col] & bit) ? foreground : background;
            for (unsigned repeat = 0; repeat < scale && ok; ++repeat) {
                ok = write_pixel(color);
            }
        }
    }
    LATBSET = LCD_CS;
    return ok;
}
