/* Final-game PAN HEAT screen, drawn incrementally (see HeatView.h).
 * Layout, colours, icons, bar gradient and distance smoothing are adapted from
 * the hardware-confirmed PIC32-LCD/src/heat_lcd.c (teammate code, not linked).
 *
 * Model: HeatView_Set() records the desired screen. The module also keeps a
 * model of what is on the glass (bar fill, label cells, marks). Each
 * HeatView_Step() issues one st7789 primitive that moves the glass one bounded
 * piece toward the desired state. st7789.c sends CASET(5) + RASET(5) + RAMWR(1)
 * = 11 bytes per primitive, then 2 bytes per pixel.
 */
#include "HeatView.h"
#include "LcdSoftSPI.h"
#include "st7789.h"

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
    HEAT_NEAR_MM = 50, HEAT_FAR_MM = 400, BAND_MM = 40,
    BAR_X = 30, BAR_Y = 143, BAR_WIDTH = 260, BAR_HEIGHT = 21,
    BAR_RUN = 16,                       /* columns per bar step: 11 + 2*16*21 = 683 B */
    ICON_SIZE = 16, ICON_SCALE = 2, ICON_Y = 97, SNOW_X = 31, FLAME_X = 257,
    MARK_Y = 170, MARK_H = 8, LINK_Y = 173, LINK_H = 2,
    PRIM_OVERHEAD = 11,
    MAX_PIXELS = (HEATVIEW_STEP_MAX_BYTES - PRIM_OVERHEAD) / 2, /* 506 */
    SWEEP_STEP_MS = 100, SWEEP_STEPS = (HEAT_FAR_MM - HEAT_NEAR_MM) / 5, /* 70 */
    FILTER_MS = 100,
    CLEAR_SLOTS = 16, MAX_LINES = 2, MAX_CELLS = 11,
    /* Bottom line below the panel (y 190..240): two groups "STIRS 12" and
     * "FLIPS 3", 8 cells of 18 px each at scale 3. */
    COUNT_Y = 205, COUNT_SCALE = 3, COUNT_GROUPS = 2, COUNT_CELLS = 8, COUNT_NAME = 5
};

/* 5x7 columns, LSB = top row. 0..25 A-Z, 26 space, 27..36 digits 0-9. */
static const uint8_t glyphs[37][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, /* A B */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* C D */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01}, /* E F */
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* G H */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01}, /* I J */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40}, /* K L */
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* M N */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06}, /* O P */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46}, /* Q R */
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, /* S T */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* U V */
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, {0x63, 0x14, 0x08, 0x14, 0x63}, /* W X */
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}, /* Y Z */
    {0x00, 0x00, 0x00, 0x00, 0x00},                                 /* space */
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 0 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 2 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39}, /* 4 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03}, /* 6 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E}  /* 8 9 */
};

static const char snowflake[ICON_SIZE][ICON_SIZE + 1] = {
    ".......2........", "....1..2..1.....", ".....1.2.1......", ".1....121....1..",
    "..1....2....1...", "...1...2...1....", "....2..3..2.....", "222222333222222.",
    "....2..3..2.....", "...1...2...1....", "..1....2....1...", ".1....121....1..",
    ".....1.2.1......", "....1..2..1.....", ".......2........", "................"
};

static const char flame[ICON_SIZE][ICON_SIZE + 1] = {
    "........1.......", ".......11.......", ".......112......", "......1122...1..",
    "......1222..11..", "..1..122222.11..", "..11.122222111..", ".1111222322211..",
    ".11222233322211.", ".11222333332211.", ".11223333332211.", "..122333333221..",
    "..112233332211..", "...1122222211...", ".....111111.....", "................"
};

typedef struct { int16_t x, y, w, h; } Rect;

/* Label layouts: fixed grids of glyph cells. Every label is padded to its
 * grid, so a label change redraws cells in place; only switching grids needs a
 * (small) clear of old-grid area the new grid does not cover. */
enum { LAY_NONE, LAY_ATTRACT, LAY_EVEN, LAY_ODD };
static const struct {
    uint8_t lines, scale, cells;
    int16_t x, y[MAX_LINES];
} layouts[] = {
    [LAY_NONE] = {0, 1, 0, 0, {0, 0}},
    [LAY_ATTRACT] = {2, 2, 11, 95, {99, 117}}, /* two centred 11-cell lines */
    [LAY_EVEN] = {1, 3, 10, 71, {103, 0}},     /* centred 10 cells, heat_lcd's y */
    [LAY_ODD] = {1, 3, 9, 80, {103, 0}}        /* centred 9 cells; inside EVEN */
};
static const Rect LABEL_AREA = {71, 99, 180, 32}; /* union of all grids */

enum { LBL_ATTRACT, LBL_WAITING, LBL_LOW, LBL_RIGHT, LBL_HIGH, LBL_BURNT, LBL_COUNT };
static const struct {
    uint8_t layout;
    uint16_t color;
    const char *text[MAX_LINES];
} labels[LBL_COUNT] = {
    [LBL_ATTRACT] = {LAY_ATTRACT, MUTED, {"WAITING FOR", "HEAT INPUTS"}},
    [LBL_WAITING] = {LAY_ODD, MUTED, {" WAITING ", 0}},
    [LBL_LOW] = {LAY_EVEN, ICE, {"NOT ENOUGH", 0}},
    [LBL_RIGHT] = {LAY_EVEN, MINT, {"JUST RIGHT", 0}},
    [LBL_HIGH] = {LAY_EVEN, CORAL, {" TOO MUCH ", 0}},
    [LBL_BURNT] = {LAY_ODD, AMBER, {"  BURNT  ", 0}}
};

/* Desired state (HeatView_Set). */
static uint8_t want_label = LBL_ATTRACT;
static unsigned want_fill;
static bool want_marks;
static unsigned want_cool, want_hot; /* mark x positions */
static uint32_t filtered_q8, filter_ms;

/* Glass model. cell_char 0 = unknown content. */
static bool ready, bar_turn;
static unsigned shown_fill;
static uint8_t shown_layout;
static char cell_char[MAX_LINES][MAX_CELLS];
static uint16_t cell_color[MAX_LINES][MAX_CELLS];
static Rect clears[CLEAR_SLOTS];
static unsigned clear_count;
static bool marks_shown;
static unsigned marks_cool, marks_hot, marks_drawn; /* drawn: bit0 cool, bit1 hot, bit2 link */
static Rect mark_erase;

/* Bottom counters: desired values and the glass model (0 = unknown cell). */
static const int16_t count_x[COUNT_GROUPS] = {10, 166};
static const char *const count_names[COUNT_GROUPS] = {"STIRS", "FLIPS"};
static bool want_counts;
static uint8_t want_count[COUNT_GROUPS];
static char count_char[COUNT_GROUPS][COUNT_CELLS];

static const uint8_t *glyph_for(char c)
{
    if (c >= 'A' && c <= 'Z') return glyphs[c - 'A'];
    if (c >= '0' && c <= '9') return glyphs[27 + (c - '0')];
    return glyphs[26];
}

static bool draw_centered(unsigned y, const char *text, unsigned scale, uint16_t color, uint16_t bg)
{
    unsigned n = 0;
    while (text[n] != '\0') ++n;
    unsigned x = (LCD_WIDTH - (6u * n - 1u) * scale) / 2u;
    for (; *text != '\0'; ++text, x += 6u * scale) {
        if (!st7789_draw_glyph(x, y, glyph_for(*text), scale, color, bg)) return false;
    }
    return true;
}

static bool draw_icon(unsigned x, unsigned y, const char sprite[ICON_SIZE][ICON_SIZE + 1],
                      const uint16_t palette[4])
{
    uint16_t pixels[ICON_SIZE * ICON_SCALE];
    for (unsigned row = 0; row < ICON_SIZE; ++row) {
        for (unsigned col = 0; col < ICON_SIZE; ++col) {
            const char shade = sprite[row][col];
            const uint16_t color = palette[shade == '.' ? 0 : shade - '0'];
            pixels[2u * col] = color;
            pixels[2u * col + 1u] = color;
        }
        if (!st7789_repeat_row(x, y + row * ICON_SCALE, ICON_SIZE * ICON_SCALE, ICON_SCALE, pixels))
            return false;
    }
    return true;
}

static unsigned fill_at_q8(uint32_t distance_q8)
{
    const uint32_t near_q8 = HEAT_NEAR_MM * 256u, far_q8 = HEAT_FAR_MM * 256u;
    if (distance_q8 == 0u || distance_q8 >= far_q8) return 0;
    if (distance_q8 <= near_q8) return BAR_WIDTH;
    const uint32_t range_q8 = far_q8 - near_q8;
    return ((far_q8 - distance_q8) * BAR_WIDTH + range_q8 / 2u) / range_q8;
}

static uint32_t clamp_mm(int32_t mm)
{
    return mm < HEAT_NEAR_MM ? HEAT_NEAR_MM : mm > HEAT_FAR_MM ? HEAT_FAR_MM : (uint32_t)mm;
}

static uint16_t heat_color(unsigned column)
{
    static const uint8_t stops[][3] = {{50, 136, 246}, {81, 238, 179}, {255, 190, 67}, {255, 86, 55}};
    const unsigned position = column * 3u;
    const unsigned segment = position / BAR_WIDTH;
    const unsigned right = position % BAR_WIDTH;
    const unsigned left = BAR_WIDTH - right;
    const unsigned red = (stops[segment][0] * left + stops[segment + 1u][0] * right) / BAR_WIDTH;
    const unsigned green = (stops[segment][1] * left + stops[segment + 1u][1] * right) / BAR_WIDTH;
    const unsigned blue = (stops[segment][2] * left + stops[segment + 1u][2] * right) / BAR_WIDTH;
    return RGB565(red, green, blue);
}

/* ---- desired state ---------------------------------------------------- */

void HeatView_SetCounts(uint8_t stirs, uint8_t flips)
{
    want_count[0] = stirs;
    want_count[1] = flips;
}

void HeatView_Set(HeatViewMode mode, uint8_t zone, bool burnt, uint8_t target_cm,
                  uint16_t distance_mm, uint32_t now_ms)
{
    want_counts = mode != HEATVIEW_ATTRACT; /* counters only while cooking */
    if (mode == HEATVIEW_ATTRACT) {
        /* Same triangle as LCDPIC2Bench: 400 -> 50 -> 400 mm, 5 mm per 100 ms. */
        const unsigned k = (unsigned)((now_ms / SWEEP_STEP_MS) % (2u * SWEEP_STEPS));
        const uint32_t mm = k <= SWEEP_STEPS ? HEAT_FAR_MM - 5u * k
                                             : HEAT_NEAR_MM + 5u * (k - SWEEP_STEPS);
        want_label = LBL_ATTRACT;
        want_fill = fill_at_q8(mm * 256u);
        want_marks = false;
        filtered_q8 = 0u; /* the next live reading starts fresh */
        return;
    }
    /* heat_lcd_set_distance smoothing, applied at the ranger's ~10 Hz rate so
     * calling Set every loop does not defeat it. No hand empties at once. */
    if (distance_mm == 0u) {
        filtered_q8 = 0u;
    } else {
        const uint32_t sample_q8 = clamp_mm(distance_mm) * 256u;
        if (filtered_q8 == 0u) {
            filtered_q8 = sample_q8;
            filter_ms = now_ms;
        } else if ((uint32_t)(now_ms - filter_ms) >= FILTER_MS) {
            filtered_q8 = (2u * filtered_q8 + sample_q8 + 1u) / 3u;
            filter_ms = now_ms;
        }
    }
    want_fill = fill_at_q8(filtered_q8);
    if (mode != HEATVIEW_ZONE) {
        want_label = LBL_WAITING;
        want_marks = false;
        return;
    }
    want_label = burnt ? LBL_BURNT : zone == 1u ? LBL_LOW : zone == 2u ? LBL_RIGHT
               : zone == 3u ? LBL_HIGH : LBL_WAITING;
    want_marks = target_cm != 0u; /* no target yet: nothing to mark */
    if (want_marks) {
        want_cool = BAR_X + fill_at_q8(clamp_mm(target_cm * 10 + BAND_MM) * 256u);
        want_hot = BAR_X + fill_at_q8(clamp_mm(target_cm * 10 - BAND_MM) * 256u);
    }
}

/* ---- bounded primitives ---------------------------------------------- */

/* Fill the top band of *r (<= MAX_PIXELS) and shrink it. Widths used here are
 * all <= 180 px, so a band is always >= 2 rows. */
static void fill_band(Rect *r, uint16_t color)
{
    unsigned rows = MAX_PIXELS / (unsigned)r->w;
    if (rows > (unsigned)r->h) rows = (unsigned)r->h;
    if (st7789_fill((unsigned)r->x, (unsigned)r->y, (unsigned)r->w, rows, color)) {
        r->y += (int16_t)rows;
        r->h -= (int16_t)rows;
    }
}

static void bar_step(void)
{
    uint16_t pixels[BAR_RUN];
    unsigned first, end;
    if (want_fill > shown_fill) {
        first = shown_fill;
        end = want_fill - first > BAR_RUN ? first + BAR_RUN : want_fill;
    } else {
        end = shown_fill;
        first = end - want_fill > BAR_RUN ? end - BAR_RUN : want_fill;
    }
    for (unsigned col = first; col < end; ++col)
        pixels[col - first] = col < want_fill ? heat_color(col) : TRACK;
    if (st7789_repeat_row(BAR_X + first, BAR_Y, end - first, BAR_HEIGHT, pixels))
        shown_fill = want_fill > shown_fill ? end : first;
}

/* ---- label ------------------------------------------------------------ */

static Rect line_rect(uint8_t layout, unsigned line)
{
    const unsigned s = layouts[layout].scale;
    Rect r = {layouts[layout].x, layouts[layout].y[line],
              (int16_t)(6u * s * layouts[layout].cells), (int16_t)(7u * s)};
    return r;
}

static bool overlaps(Rect a, Rect b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static void push_clear(Rect r)
{
    if (r.w <= 0 || r.h <= 0) return;
    if (clear_count < CLEAR_SLOTS) {
        clears[clear_count++] = r;
    } else { /* cannot happen with these layouts; stay correct anyway */
        clears[0] = LABEL_AREA;
        clear_count = 1;
    }
}

/* Queue a minus (all rects of layout `keep`). */
static void clear_outside(Rect a, uint8_t keep, unsigned from_line)
{
    if (a.w <= 0 || a.h <= 0) return;
    for (unsigned line = from_line; line < layouts[keep].lines; ++line) {
        const Rect b = line_rect(keep, line);
        if (!overlaps(a, b)) continue;
        const int16_t top = b.y > a.y ? b.y : a.y;
        const int16_t bottom = b.y + b.h < a.y + a.h ? b.y + b.h : a.y + a.h;
        clear_outside((Rect){a.x, a.y, a.w, (int16_t)(top - a.y)}, keep, line + 1u);
        clear_outside((Rect){a.x, bottom, a.w, (int16_t)(a.y + a.h - bottom)}, keep, line + 1u);
        clear_outside((Rect){a.x, top, (int16_t)(b.x - a.x), (int16_t)(bottom - top)}, keep, line + 1u);
        clear_outside((Rect){(int16_t)(b.x + b.w), top, (int16_t)(a.x + a.w - b.x - b.w),
                             (int16_t)(bottom - top)}, keep, line + 1u);
        return;
    }
    push_clear(a);
}

/* Switch the label grid: queue clears of old-grid area outside the new grid,
 * then treat the new cells as unknown unless they lie outside the old grid
 * (those are known background, so spaces there need no drawing). */
static void change_layout(uint8_t layout)
{
    const uint8_t old = shown_layout;
    for (unsigned line = 0; line < layouts[old].lines; ++line)
        clear_outside(line_rect(old, line), layout, 0);
    const unsigned s = layouts[layout].scale;
    for (unsigned line = 0; line < layouts[layout].lines; ++line) {
        for (unsigned c = 0; c < layouts[layout].cells; ++c) {
            const Rect cell = {(int16_t)(layouts[layout].x + 6u * s * c), layouts[layout].y[line],
                               (int16_t)(6u * s), (int16_t)(7u * s)};
            bool dirty = false;
            for (unsigned l = 0; l < layouts[old].lines; ++l)
                dirty = dirty || overlaps(cell, line_rect(old, l));
            cell_char[line][c] = dirty ? 0 : ' ';
            cell_color[line][c] = PANEL;
        }
    }
    shown_layout = layout;
}

static bool cell_ok(unsigned line, unsigned c)
{
    const char want = labels[want_label].text[line][c];
    const char have = cell_char[line][c];
    return have == want && (want == ' ' || cell_color[line][c] == labels[want_label].color);
}

static bool label_pending(void)
{
    if (clear_count != 0u || shown_layout != labels[want_label].layout) return true;
    for (unsigned line = 0; line < layouts[shown_layout].lines; ++line)
        for (unsigned c = 0; c < layouts[shown_layout].cells; ++c)
            if (!cell_ok(line, c)) return true;
    return false;
}

static void label_step(void)
{
    if (clear_count == 0u && shown_layout != labels[want_label].layout)
        change_layout(labels[want_label].layout);
    if (clear_count != 0u) {
        fill_band(&clears[clear_count - 1u], PANEL);
        if (clears[clear_count - 1u].h <= 0) --clear_count;
        return;
    }
    const unsigned s = layouts[shown_layout].scale;
    for (unsigned line = 0; line < layouts[shown_layout].lines; ++line) {
        for (unsigned c = 0; c < layouts[shown_layout].cells; ++c) {
            if (cell_ok(line, c)) continue;
            const char ch = labels[want_label].text[line][c];
            const uint16_t color = labels[want_label].color;
            if (st7789_draw_glyph(layouts[shown_layout].x + 6u * s * c, layouts[shown_layout].y[line],
                                  glyph_for(ch), s, color, PANEL)) {
                cell_char[line][c] = ch;
                cell_color[line][c] = color;
            }
            return;
        }
    }
}

/* ---- band marks ------------------------------------------------------- */

static bool marks_match(void)
{
    return marks_shown == want_marks && (!want_marks || (marks_cool == want_cool && marks_hot == want_hot));
}

static bool marks_pending(void)
{
    return mark_erase.h > 0 || !marks_match() || (want_marks && marks_drawn != 7u);
}

static void marks_step(void)
{
    if (mark_erase.h <= 0 && marks_shown && !marks_match()) {
        /* Erase the whole old mark span (<= 63 x 8 px) before drawing anew. */
        mark_erase = (Rect){(int16_t)(marks_cool - 1u), MARK_Y,
                            (int16_t)(marks_hot - marks_cool + 3u), MARK_H};
        marks_shown = false;
        marks_drawn = 0;
    }
    if (mark_erase.h > 0) {
        fill_band(&mark_erase, PANEL);
        return;
    }
    if (!want_marks) return;
    marks_shown = true;
    marks_cool = want_cool;
    marks_hot = want_hot;
    bool ok;
    unsigned bit;
    if (!(marks_drawn & 1u)) {
        bit = 1u;
        ok = st7789_fill(marks_cool - 1u, MARK_Y, 3, MARK_H, ICE);
    } else if (!(marks_drawn & 2u)) {
        bit = 2u;
        ok = st7789_fill(marks_hot - 1u, MARK_Y, 3, MARK_H, CORAL);
    } else {
        bit = 4u; /* connector only when there is room between the marks */
        ok = marks_hot <= marks_cool + 3u
             || st7789_fill(marks_cool + 2u, LINK_Y, marks_hot - marks_cool - 3u, LINK_H, MINT);
    }
    if (ok) marks_drawn |= bit;
}

/* ---- stir / flip counters --------------------------------------------- */

/* Desired character of cell c in group g: "STIRS 7 " / "FLIPS 12", or blank. */
static char count_want(unsigned g, unsigned c)
{
    if (!want_counts) return ' ';
    if (c < COUNT_NAME) return count_names[g][c];
    const unsigned v = want_count[g] > 99u ? 99u : want_count[g];
    const char tens = (char)('0' + v / 10u), ones = (char)('0' + v % 10u);
    if (c == COUNT_NAME) return ' ';
    if (c == COUNT_NAME + 1u) return v >= 10u ? tens : ones;
    return v >= 10u ? ones : ' ';
}

static bool counts_pending(void)
{
    for (unsigned g = 0; g < COUNT_GROUPS; ++g)
        for (unsigned c = 0; c < COUNT_CELLS; ++c)
            if (count_char[g][c] != count_want(g, c)) return true;
    return false;
}

static void counts_step(void)
{
    for (unsigned g = 0; g < COUNT_GROUPS; ++g) {
        for (unsigned c = 0; c < COUNT_CELLS; ++c) {
            const char ch = count_want(g, c);
            if (count_char[g][c] == ch) continue;
            const uint16_t color = c < COUNT_NAME ? MUTED : WHITE;
            if (st7789_draw_glyph(count_x[g] + 6u * COUNT_SCALE * c, COUNT_Y, glyph_for(ch),
                                  COUNT_SCALE, color, BACKGROUND))
                count_char[g][c] = ch;
            return;
        }
    }
}

/* ---- public ----------------------------------------------------------- */

bool HeatView_Step(void)
{
    if (!ready) return false;
    const bool overlay = label_pending() || marks_pending() || counts_pending();
    if (want_fill != shown_fill && (bar_turn || !overlay)) {
        bar_step();
        bar_turn = false;
    } else if (overlay) {
        /* Label and marks first, but alternate with the bar so it never starves. */
        if (label_pending()) label_step();
        else if (marks_pending()) marks_step();
        else counts_step();
        bar_turn = true;
    }
    return label_pending() || marks_pending() || counts_pending() || want_fill != shown_fill;
}

bool HeatView_Start(void)
{
    static const uint16_t ice_palette[] = {PANEL, RGB565(62, 137, 226), ICE, WHITE};
    static const uint16_t flame_palette[] = {PANEL, CORAL, AMBER, RGB565(255, 246, 178)};
    display_spi_start();
    st7789_prepare_pins();
    ready = st7789_start(BACKGROUND)
         && draw_centered(24, "PAN HEAT", 6, WHITE, BACKGROUND)
         && st7789_fill(115, 73, 90, 3, AMBER)
         && st7789_fill(12, 88, 296, 102, PANEL)
         && draw_icon(SNOW_X, ICON_Y, snowflake, ice_palette)
         && draw_icon(FLAME_X, ICON_Y, flame, flame_palette)
         && st7789_fill(BAR_X - 2u, BAR_Y - 2u, BAR_WIDTH + 4u, BAR_HEIGHT + 4u, BORDER)
         && st7789_fill(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, TRACK);
    /* The panel fill left label and mark areas blank and the bar empty. */
    shown_fill = 0;
    shown_layout = LAY_NONE;
    clear_count = 0;
    marks_shown = false;
    marks_drawn = 0;
    mark_erase.h = 0;
    bar_turn = false;
    for (unsigned g = 0; g < COUNT_GROUPS; ++g)   /* the bottom strip is plain background */
        for (unsigned c = 0; c < COUNT_CELLS; ++c) count_char[g][c] = ' ';
    /* Draw the current desired state now (bounded: a full redraw is < 100 steps). */
    for (unsigned i = 0; ready && i < 400u && HeatView_Step(); ++i) {
    }
    return ready;
}
