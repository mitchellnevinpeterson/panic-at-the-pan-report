#include "heat_lcd.h"
#include "heat_audio.h"
#include "st7789.h"
#include <string.h>

#define RGB565(r, g, b) ((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((b) >> 3))

enum {
    BACKGROUND = RGB565(13, 17, 26),
    PANEL = RGB565(24, 34, 49),
    BORDER = RGB565(66, 83, 106),
    TRACK = RGB565(10, 15, 24),
    WHITE = RGB565(247, 248, 239),
    MUTED = RGB565(159, 180, 202),
    ICE = RGB565(115, 217, 255),
    MINT = RGB565(121, 247, 179),
    AMBER = RGB565(255, 192, 87),
    CORAL = RGB565(255, 108, 76)
};

enum {
    HEAT_NEAR_MM = 50, HEAT_FAR_MM = 400,
    BAR_X = 30, BAR_Y = 143, BAR_WIDTH = 260, BAR_HEIGHT = 21,
    ICON_SIZE = 16, ICON_SCALE = 2,
    VOLUME_SEGMENT_WIDTH = 10, VOLUME_SEGMENT_PITCH = 13,
    VOLUME_WIDTH = (HEAT_AUDIO_VOLUME_MAX - 1) * VOLUME_SEGMENT_PITCH + VOLUME_SEGMENT_WIDTH
};

static const uint8_t glyphs[26][5] = {
    ['A'-'A'] = {0x7E, 0x11, 0x11, 0x11, 0x7E},
    ['C'-'A'] = {0x3E, 0x41, 0x41, 0x41, 0x22},
    ['E'-'A'] = {0x7F, 0x49, 0x49, 0x49, 0x41},
    ['G'-'A'] = {0x3E, 0x41, 0x49, 0x49, 0x7A},
    ['H'-'A'] = {0x7F, 0x08, 0x08, 0x08, 0x7F},
    ['I'-'A'] = {0x00, 0x41, 0x7F, 0x41, 0x00},
    ['J'-'A'] = {0x20, 0x40, 0x41, 0x3F, 0x01},
    ['L'-'A'] = {0x7F, 0x40, 0x40, 0x40, 0x40},
    ['M'-'A'] = {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    ['N'-'A'] = {0x7F, 0x04, 0x08, 0x10, 0x7F},
    ['O'-'A'] = {0x3E, 0x41, 0x41, 0x41, 0x3E},
    ['P'-'A'] = {0x7F, 0x09, 0x09, 0x09, 0x06},
    ['R'-'A'] = {0x7F, 0x09, 0x19, 0x29, 0x46},
    ['S'-'A'] = {0x46, 0x49, 0x49, 0x49, 0x31},
    ['T'-'A'] = {0x01, 0x01, 0x7F, 0x01, 0x01},
    ['U'-'A'] = {0x3F, 0x40, 0x40, 0x40, 0x3F},
    ['V'-'A'] = {0x1F, 0x20, 0x40, 0x20, 0x1F},
    ['W'-'A'] = {0x7F, 0x20, 0x18, 0x20, 0x7F}
};

static const char snowflake[ICON_SIZE][ICON_SIZE + 1] = {
    ".......2........",
    "....1..2..1.....",
    ".....1.2.1......",
    ".1....121....1..",
    "..1....2....1...",
    "...1...2...1....",
    "....2..3..2.....",
    "222222333222222.",
    "....2..3..2.....",
    "...1...2...1....",
    "..1....2....1...",
    ".1....121....1..",
    ".....1.2.1......",
    "....1..2..1.....",
    ".......2........",
    "................"
};

static const char flame[ICON_SIZE][ICON_SIZE + 1] = {
    "........1.......",
    ".......11.......",
    ".......112......",
    "......1122...1..",
    "......1222..11..",
    "..1..122222.11..",
    "..11.122222111..",
    ".1111222322211..",
    ".11222233322211.",
    ".11222333332211.",
    ".11223333332211.",
    "..122333333221..",
    "..112233332211..",
    "...1122222211...",
    ".....111111.....",
    "................"
};

static bool lcd_ready;
static int displayed_zone = -1;
#ifndef LCD_HEAT_ONLY
static unsigned displayed_volume = HEAT_AUDIO_VOLUME_MAX + 1u;
#endif
static unsigned displayed_fill = BAR_WIDTH + 1u;
static uint32_t filtered_distance_q8;

static bool draw_centered(unsigned y, const char *text, unsigned scale,
                          uint16_t color, uint16_t background)
{
    static const uint8_t blank[5] = {0};
    const unsigned width = (6u * (unsigned)strlen(text) - 1u) * scale;
    unsigned x = (LCD_WIDTH - width) / 2u;
    for (; *text != '\0'; ++text, x += 6u * scale) {
        const char letter = *text;
        const uint8_t *glyph = letter >= 'A' && letter <= 'Z' ? glyphs[letter - 'A'] : blank;
        if (!st7789_draw_glyph(x, y, glyph, scale, color, background)) {
            return false;
        }
    }
    return true;
}

static bool draw_icon(unsigned x, unsigned y,
                      const char sprite[ICON_SIZE][ICON_SIZE + 1], const uint16_t palette[4])
{
    uint16_t pixels[ICON_SIZE * ICON_SCALE];
    for (unsigned row = 0; row < ICON_SIZE; ++row) {
        for (unsigned col = 0; col < ICON_SIZE; ++col) {
            const char shade = sprite[row][col];
            const uint16_t color = palette[shade == '.' ? 0 : shade - '0'];
            pixels[2u * col] = color;
            pixels[2u * col + 1u] = color;
        }
        if (!st7789_repeat_row(x, y + row * ICON_SCALE, ICON_SIZE * ICON_SCALE,
                               ICON_SCALE, pixels)) {
            return false;
        }
    }
    return true;
}

static unsigned fill_at_distance(uint32_t distance_q8)
{
    const uint32_t near_q8 = HEAT_NEAR_MM * 256u;
    const uint32_t far_q8 = HEAT_FAR_MM * 256u;
    if (distance_q8 == 0u || distance_q8 >= far_q8) {
        return 0;
    }
    if (distance_q8 <= near_q8) {
        return BAR_WIDTH;
    }
    const uint32_t range_q8 = far_q8 - near_q8;
    return ((far_q8 - distance_q8) * BAR_WIDTH + range_q8 / 2u) / range_q8;
}

void heat_lcd_set_distance(uint32_t distance_mm)
{
    if (distance_mm == 0u) {
        filtered_distance_q8 = 0u;
        return;
    }
    if (distance_mm > HEAT_FAR_MM) {
        distance_mm = HEAT_FAR_MM;
    }
    if (distance_mm < HEAT_NEAR_MM) {
        distance_mm = HEAT_NEAR_MM;
    }

    const uint32_t sample_q8 = distance_mm * 256u;
    filtered_distance_q8 = filtered_distance_q8 == 0u ? sample_q8
                           : (2u * filtered_distance_q8 + sample_q8 + 1u) / 3u;
}

static uint16_t heat_color(unsigned column)
{
    static const uint8_t stops[][3] = {
        {50, 136, 246}, {81, 238, 179}, {255, 190, 67}, {255, 86, 55}
    };
    const unsigned position = column * 3u;
    const unsigned segment = position / BAR_WIDTH;
    const unsigned right = position % BAR_WIDTH;
    const unsigned left = BAR_WIDTH - right;
    const unsigned red = (stops[segment][0] * left + stops[segment + 1u][0] * right) / BAR_WIDTH;
    const unsigned green = (stops[segment][1] * left + stops[segment + 1u][1] * right) / BAR_WIDTH;
    const unsigned blue = (stops[segment][2] * left + stops[segment + 1u][2] * right) / BAR_WIDTH;
    return RGB565(red, green, blue);
}

static void update_heat_bar(void)
{
    const unsigned fill = fill_at_distance(filtered_distance_q8);
    if (fill == displayed_fill) {
        return;
    }

    uint16_t pixels[BAR_WIDTH];
    unsigned first = 0;
    unsigned end = BAR_WIDTH;
    if (displayed_fill <= BAR_WIDTH) {
        first = fill < displayed_fill ? fill : displayed_fill;
        end = fill > displayed_fill ? fill : displayed_fill;
    }
    for (unsigned col = first; col < end; ++col) {
        pixels[col - first] = col < fill ? heat_color(col) : TRACK;
    }
    if (st7789_repeat_row(BAR_X + first, BAR_Y, end - first, BAR_HEIGHT, pixels)) {
        displayed_fill = fill;
    } else {
        displayed_fill = BAR_WIDTH + 1u;
    }
}

bool heat_lcd_start(void)
{
    static const uint16_t ice_palette[] = {PANEL, RGB565(62, 137, 226), ICE, WHITE};
    static const uint16_t flame_palette[] = {PANEL, CORAL, AMBER, RGB565(255, 246, 178)};
    const unsigned cool_mark = BAR_X + fill_at_distance(HEAT_COOL_ABOVE_MM * 256u);
    const unsigned hot_mark = BAR_X + fill_at_distance(HEAT_HOT_BELOW_MM * 256u);

    lcd_ready = st7789_start(BACKGROUND)
             && draw_centered(24, "PAN HEAT", 6, WHITE, BACKGROUND)
             && st7789_fill(115, 73, 90, 3, AMBER)
             && st7789_fill(12, 88, 296, 102, PANEL)
             && draw_icon(31, 97, snowflake, ice_palette)
             && draw_icon(257, 97, flame, flame_palette)
             && st7789_fill(BAR_X - 2u, BAR_Y - 2u, BAR_WIDTH + 4u, BAR_HEIGHT + 4u, BORDER)
             && st7789_fill(cool_mark - 1u, 170, 3, 8, ICE)
             && st7789_fill(hot_mark - 1u, 170, 3, 8, CORAL)
             && st7789_fill(cool_mark + 2u, 173, hot_mark - cool_mark - 3u, 2, MINT);
#ifndef LCD_HEAT_ONLY
    lcd_ready = lcd_ready && draw_centered(201, "VOLUME", 2, MUTED, BACKGROUND);
#endif
    if (lcd_ready) {
        displayed_zone = -1;
        displayed_fill = BAR_WIDTH + 1u;
#ifndef LCD_HEAT_ONLY
        displayed_volume = HEAT_AUDIO_VOLUME_MAX + 1u;
#endif
        heat_lcd_update();
    }
    return lcd_ready;
}

void heat_lcd_update(void)
{
    static const struct {
        const char *label;
        uint16_t color;
    } zones[] = {
        [HEAT_SILENT] = {"WAITING", MUTED},
        [HEAT_LOW] = {"NOT ENOUGH", ICE},
        [HEAT_RIGHT] = {"JUST RIGHT", MINT},
        [HEAT_HIGH] = {"TOO MUCH", CORAL}
    };
    if (!lcd_ready) {
        return;
    }

    const HeatZone zone = heat_audio_zone();
    if ((int)zone != displayed_zone) {
        const bool drawn = st7789_fill(66, 102, 188, 24, PANEL)
                        && draw_centered(103, zones[zone].label, 3, zones[zone].color, PANEL);
        displayed_zone = drawn ? (int)zone : -1;
    }
    update_heat_bar();

#ifndef LCD_HEAT_ONLY
    const unsigned volume = heat_audio_volume();
    if (volume != displayed_volume) {
        for (unsigned i = 0; i < HEAT_AUDIO_VOLUME_MAX; ++i) {
            const bool filled = i < volume;
            if (displayed_volume <= HEAT_AUDIO_VOLUME_MAX && filled == (i < displayed_volume)) {
                continue;
            }
            const unsigned x = (LCD_WIDTH - VOLUME_WIDTH) / 2u + i * VOLUME_SEGMENT_PITCH;
            if (!st7789_fill(x, 222, VOLUME_SEGMENT_WIDTH, 6, filled ? MUTED : BORDER)) {
                displayed_volume = HEAT_AUDIO_VOLUME_MAX + 1u;
                return;
            }
        }
        displayed_volume = volume;
    }
#endif
}
