#include "display_spi.h"
#ifdef XPRJ_Display_Dual
#include "heat_lcd.h"
#include "st7789.h"
#endif
#include "ultrasonic.h"
#include "heat_audio.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define ECS  (1u << 0) /* RA0, DIP 2 */
#define DC   (1u << 7) /* RB7, DIP 16 */
#define RST  (1u << 8) /* RB8, DIP 17 */
#define BUSY (1u << 9) /* RB9, DIP 18 */

#define NATIVE_WIDTH 128u
#define NATIVE_HEIGHT 296u
#define PLANE_BYTES (NATIVE_WIDTH * NATIVE_HEIGHT / 8u)
#define REFRESH_INTERVAL_TICKS (180000000u * CORE_TICKS_PER_US)

static uint8_t red_plane[PLANE_BYTES];
static uint8_t black_plane[PLANE_BYTES];

/* Inspect with MPLAB Watch: 0 startup, 1 power-up, 2 RAM write,
 * 3 refresh, 4 power-down, 5 done. Timeout flags are set independently. */
volatile unsigned display_stage;
volatile bool spi_timeout;
volatile bool busy_timeout;

static void spi_start(void)
{
    LATASET = ECS;
    LATBSET = RST;
    LATBCLR = DC;
    ANSELACLR = ECS;
    TRISACLR = ECS;
    TRISBCLR = DC | RST;
    TRISBSET = BUSY;

#ifdef XPRJ_Display_Dual
    st7789_prepare_pins();
#endif
    display_spi_start();
}

static bool spi_write(uint8_t value)
{
    if (!display_spi_write(value)) {
        spi_timeout = true;
        return false;
    }
    return true;
}

static bool command(uint8_t opcode, const uint8_t *data, size_t count)
{
    LATBCLR = DC;
    LATACLR = ECS;
    bool ok = spi_write(opcode);
    if (ok && count != 0u) {
        LATBSET = DC;
        for (size_t i = 0; i < count && ok; ++i) {
            ok = spi_write(data[i]);
        }
    }
    LATASET = ECS;
    return ok;
}

static bool poll_heat(void)
{
#ifdef XPRJ_Display_Dual
    heat_lcd_update();
#endif
    if (!ultrasonic_poll()) {
        return false;
    }
    const uint32_t distance_mm = ultrasonic_distance_mm();
#ifdef XPRJ_Display_Dual
    heat_lcd_set_distance(distance_mm);
#endif
    const char *message = heat_audio_update(distance_mm);
    if (message != NULL) {
        ultrasonic_write_text(message);
    }
    return true;
}

static bool wait_ready(uint32_t limit_us)
{
    const uint32_t start = ticks_now();
    do {
        if (!command(0x71u, NULL, 0)) { /* Read status command before BUSY sample */
            return false;
        }
        delay_us(10000u);
        if (PORTB & BUSY) {
            return true; /* UC8151D BUSY is active low */
        }
        poll_heat();
    } while ((uint32_t)(ticks_now() - start) < limit_us * CORE_TICKS_PER_US);

    busy_timeout = true;
    return false;
}

/* Z13 initialization follows Adafruit's UC8151D tri-color command sequence.
 * https://github.com/adafruit/Adafruit_EPD/blob/master/src/drivers/Adafruit_UC8151D.h */
static bool power_up(void)
{
    static const uint8_t panel[] = {0x0F, 0x89};
    static const uint8_t resolution[] = {0x80, 0x01, 0x28};
    static const uint8_t border[] = {0x77};

    for (unsigned i = 0; i < 3u; ++i) {
        LATBSET = RST;
        delay_us(10000u);
        LATBCLR = RST;
        delay_us(10000u);
        LATBSET = RST;
        delay_us(20000u);
    }

    return command(0x04u, NULL, 0) && wait_ready(5000000u)
           && command(0x00u, panel, sizeof(panel))
           && command(0x61u, resolution, sizeof(resolution))
           && command(0x50u, border, sizeof(border));
}

static bool power_down(void)
{
    static const uint8_t border[] = {0xF7};
    static const uint8_t sleep_key[] = {0xA5};

    return command(0x50u, border, sizeof(border))
           && command(0x02u, NULL, 0)
           && wait_ready(5000000u)
           && command(0x07u, sleep_key, sizeof(sleep_key));
}

static void ink_pixel(uint8_t *plane, unsigned x, unsigned y)
{
    /* Rotate landscape coordinates onto the 128-pixel controller rows. */
    if (x >= NATIVE_HEIGHT || y >= NATIVE_WIDTH) {
        return;
    }
    const unsigned col = y;
    const unsigned row = NATIVE_HEIGHT - 1u - x;
    plane[row * (NATIVE_WIDTH / 8u) + col / 8u] &=
        (uint8_t)~(0x80u >> (col % 8u));
}

static void draw_food_icons(void)
{
    enum { ICON_SIZE = 16, ICON_SCALE = 4, ICON_COUNT = 4 };
    /* '#' is black, 'r' is red, and '.' leaves the paper white. */
    static const char food[ICON_COUNT][ICON_SIZE][ICON_SIZE + 1] = {
        { /* Burger */
            "................", "................", ".....######.....",
            "...##......##...", "..#..#...#...#..", ".#......#.....#.",
            ".##############.", "................", ".rrrrrrrrrrrrrr.",
            "..rrrrrrrrrrrr..", ".##############.", ".##############.",
            ".#............#.", "..############..", "................",
            "................"
        },
        { /* Pizza */
            "................", "................", "....########....",
            "..##........##..", ".#............#.", ".##############.",
            "..#..rr..rr..#..", "..#..rr..rr..#..", "...#........#...",
            "...#...rr...#...", "....#..rr..#....", ".....#....#.....",
            ".....#....#.....", "......#..#......", ".......##.......",
            "................"
        },
        { /* Apple */
            "................", "..........###...", ".........##.....",
            "........#.......", "....rrr.#.rrr...", "...rrrrrrrrrrr..",
            "..rrrrrrrrrrrrr.", "..rr.rrrrrrrrrr.", "..rr.rrrrrrrrrr.",
            "..rrrrrrrrrrrrr.", "...rrrrrrrrrrr..", "...rrrrrrrrrrr..",
            "....rrrrrrrrr...", ".....rrr.rrr....", "................",
            "................"
        },
        { /* Ice cream */
            "................", "......rrrr......", "....rrrrrrrr....",
            "...rrr.rrrrrr...", "..rrr.rrrrrrrr..", "..rrrrrrrrrrrr..",
            ".rrrrrrrrrrrrrr.", ".rrrrrrrrrrrrrr.", "..############..",
            "...#..#..#..#...", "...#.#..#...#...", "....#..#...#....",
            ".....##...#.....", ".....#.#..#.....", "......#..#......",
            ".......##......."
        }
    };

    memset(red_plane, 0xFF, sizeof(red_plane));
    memset(black_plane, 0xFF, sizeof(black_plane));
    for (unsigned icon = 0; icon < ICON_COUNT; ++icon) {
        const unsigned left = 8u + icon * 72u;
        for (unsigned row = 0; row < ICON_SIZE; ++row) {
            for (unsigned col = 0; col < ICON_SIZE; ++col) {
                const char pixel = food[icon][row][col];
                if (pixel == '.') {
                    continue;
                }
                uint8_t *plane = (pixel == 'r') ? red_plane : black_plane;
                for (unsigned dy = 0; dy < ICON_SCALE; ++dy) {
                    for (unsigned dx = 0; dx < ICON_SCALE; ++dx) {
                        ink_pixel(plane, left + col * ICON_SCALE + dx,
                                  4u + row * ICON_SCALE + dy);
                    }
                }
            }
        }
    }
}

static void draw_distance(uint32_t cm)
{
    enum { DASH = 10, SPACE, LETTER_C, LETTER_M, SCALE = 5 };
    static const uint8_t glyphs[][7] = {
        {14, 17, 19, 21, 25, 17, 14}, /* 0 */
        {4, 12, 4, 4, 4, 4, 14},
        {14, 17, 1, 2, 4, 8, 31},
        {30, 1, 1, 14, 1, 1, 30},
        {2, 6, 10, 18, 31, 2, 2},
        {31, 16, 16, 30, 1, 1, 30},
        {14, 16, 16, 30, 17, 17, 14},
        {31, 1, 2, 4, 8, 8, 8},
        {14, 17, 17, 14, 17, 17, 14},
        {14, 17, 17, 15, 1, 1, 14}, /* 9 */
        {0, 0, 0, 31, 0, 0, 0},   /* - */
        {0, 0, 0, 0, 0, 0, 0},    /* space */
        {0, 0, 14, 16, 16, 16, 14}, /* c */
        {0, 0, 26, 21, 21, 21, 21}  /* m */
    };
    uint8_t label[6];
    unsigned count = 0;
    if (cm == 0u) {
        label[count++] = DASH;
        label[count++] = DASH;
        label[count++] = DASH;
    } else {
        if (cm >= 100u) {
            label[count++] = (uint8_t)(cm / 100u);
        }
        if (cm >= 10u) {
            label[count++] = (uint8_t)((cm / 10u) % 10u);
        }
        label[count++] = (uint8_t)(cm % 10u);
    }
    label[count++] = SPACE;
    label[count++] = LETTER_C;
    label[count++] = LETTER_M;

    const unsigned left = (NATIVE_HEIGHT - (count * 6u - 1u) * SCALE) / 2u;
    for (unsigned i = 0; i < count; ++i) {
        for (unsigned row = 0; row < 7u; ++row) {
            for (unsigned col = 0; col < 5u; ++col) {
                if ((glyphs[label[i]][row] & (16u >> col)) == 0u) {
                    continue;
                }
                for (unsigned dy = 0; dy < SCALE; ++dy) {
                    for (unsigned dx = 0; dx < SCALE; ++dx) {
                        ink_pixel(black_plane, left + (i * 6u + col) * SCALE + dx,
                                  82u + row * SCALE + dy);
                    }
                }
            }
        }
    }
}

static bool refresh_display(void)
{
    display_stage = 1;
    if (!power_up()) {
        return false;
    }
    display_stage = 2;
    /* On this panel, the READY test rendered the 0x13 plane in red. */
    if (!command(0x10u, black_plane, sizeof(black_plane))
        || !command(0x13u, red_plane, sizeof(red_plane))) {
        return false;
    }
    display_stage = 3;
    if (!command(0x12u, NULL, 0)) {
        return false;
    }
    delay_us(100000u);
    if (!wait_ready(45000000u)) {
        return false;
    }
    display_stage = 4;
    if (!power_down()) {
        return false;
    }
    display_stage = 5;
    return true;
}

int main(void)
{
    heat_audio_start();
    heat_audio_play_startup_cue();

    /* Keep the audio interrupt out of the sensor and display PPS setup. */
    __builtin_disable_interrupts();
    ultrasonic_start(true);
    spi_start();
    __builtin_enable_interrupts();
#ifdef XPRJ_Display_Dual
    if (!heat_lcd_start()) {
        ultrasonic_write_text("LCD SPI timeout\r\n");
    }
#endif
    uint32_t displayed_cm = UINT32_MAX;
    for (;;) {
        if (!poll_heat()) {
            delay_us(1000u);
            continue;
        }
        const uint32_t cm = (ultrasonic_distance_mm() + 5u) / 10u;
        if (cm == displayed_cm) {
            continue;
        }

        draw_food_icons();
        draw_distance(cm);
        if (refresh_display()) {
            displayed_cm = cm;
        }
        /* Keep sampling while the e-paper rests between full refreshes. */
        const uint32_t refresh_end = ticks_now();
        while ((uint32_t)(ticks_now() - refresh_end) < REFRESH_INTERVAL_TICKS) {
            poll_heat();
            delay_us(1000u);
        }
    }
}
