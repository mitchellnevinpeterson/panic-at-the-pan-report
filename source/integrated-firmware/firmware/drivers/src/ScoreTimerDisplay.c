#include <string.h>
#include "ScoreTimerDisplay.h"
#include "StatusDisplaySPI.h"
#include "FontStuff.h"

static ScoreTimerDisplay_Config layout;
static uint32_t pending[2][8];
static uint8_t snapshot[8][8]; /* Physical device, digit register minus one. */
static uint8_t init_step, next_row, refresh_step;
static bool configured, dirty, sending;
static ScoreTimerDisplay_State state = SCORE_TIMER_FAULT;

static bool panel_valid(ScoreTimerDisplay_Panel panel)
{
    return panel == DISPLAY_TOP || panel == DISPLAY_BOTTOM;
}

bool ScoreTimerDisplay_Init(const ScoreTimerDisplay_Config *config)
{
    unsigned int i;
    if (!config || config->intensity > 15) {
        return false;
    }
    for (i = 0; i < 2; ++i) {
        if (config->rotation[i] > 270 || config->rotation[i] % 90 != 0) {
            return false;
        }
    }
    layout = *config;
    memset(pending, 0, sizeof pending);
    init_step = next_row = refresh_step = 0;
    dirty = true;
    sending = false;
    configured = true;
    state = SCORE_TIMER_STARTING;
    StatusDisplaySPI_Init();
    return true;
}

bool ScoreTimerDisplay_ShowLabels(void)
{
    /* Proportional 2/3-column glyphs keep both words readable in 32 pixels.
     * SCORE occupies x0..16; TIME occupies x20..31, with inter-letter gaps.
     * ###.##.###.##..##...###.#.#.#.##
     * #...#..#.#.#.#.#.....#..#.###.#.
     * ###.#..#.#.##..##....#..#.###.##
     * ..#.#..#.#.#.#.#.....#..#.#.#.#.
     * ###.##.###.#.#.##....#..#.#.#.##
     */
    static const uint32_t rows[8] = {
        0, UINT32_C(0xEDD98EAB), UINT32_C(0x895504BA),
        UINT32_C(0xE95984BB), UINT32_C(0x295504AA),
        UINT32_C(0xEDD584AB), 0, 0
    };
    return ScoreTimerDisplay_SetFrame(DISPLAY_TOP, rows);
}

bool ScoreTimerDisplay_SetFrame(ScoreTimerDisplay_Panel panel,
                               const uint32_t rows[8])
{
    if (!configured || !panel_valid(panel) || !rows) {
        return false;
    }
    memcpy(pending[panel], rows, sizeof pending[panel]);
    dirty = true;
    return true;
}

static void draw_text(uint32_t rows[8], const char *text,
                      unsigned int length, unsigned int start)
{
    unsigned int i, y, x;
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 || c > 126) {
            c = '?';       /* Keep inherited font lookup inside its table. */
        }
        for (y = 0; y < 6; ++y) {
            uint8_t bits = getFontLine(c, (int)y);
            for (x = 0; x < 4; ++x) {
                if (bits & (1u << (3 - x))) {
                    rows[y + 1] |= UINT32_C(1) << (31 - (start + i * 4 + x));
                }
            }
        }
    }
}

bool ScoreTimerDisplay_SetText(ScoreTimerDisplay_Panel panel, const char *text)
{
    uint32_t rows[8] = {0};
    unsigned int length = 0;
    if (!configured || !panel_valid(panel) || !text) {
        return false;
    }
    while (length < 8 && text[length]) {
        ++length;
    }
    if (text[length] != '\0') {
        return false;
    }
    draw_text(rows, text, length, (32 - length * 4) / 2);
    return ScoreTimerDisplay_SetFrame(panel, rows);
}

bool ScoreTimerDisplay_SetBanner(const char *text, int16_t left_x)
{
    uint32_t rows[16] = {0};
    unsigned int length = 0, i, x, y, dx, dy;
    if (!configured || !text) {
        return false;
    }
    while (length < DISPLAY_BANNER_MAX_CHARS && text[length]) {
        ++length;
    }
    if (text[length] != '\0') {
        return false;
    }
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        int32_t origin = (int32_t)left_x + (int32_t)i * DISPLAY_BANNER_CHAR_WIDTH;
        if (origin >= 32 || origin + DISPLAY_BANNER_CHAR_WIDTH <= 0) {
            continue;
        }
        if (c < 32 || c > 126) {
            c = '?';
        }
        for (y = 0; y < 6; ++y) {
            uint8_t bits = getFontLine(c, (int)y);
            for (x = 0; x < 4; ++x) {
                if (!(bits & (1u << (3 - x)))) {
                    continue;
                }
                for (dx = 0; dx < 2; ++dx) {
                    int32_t screen_x = origin + (int32_t)(x * 2 + dx);
                    if (screen_x < 0 || screen_x >= 32) {
                        continue;
                    }
                    for (dy = 0; dy < 2; ++dy) {
                        rows[2 + y * 2 + dy] |= UINT32_C(1) << (31 - screen_x);
                    }
                }
            }
        }
    }
    /* One foreground owner: both halves enter the same pending snapshot. */
    (void)ScoreTimerDisplay_SetFrame(DISPLAY_TOP, rows);
    return ScoreTimerDisplay_SetFrame(DISPLAY_BOTTOM, rows + 8);
}

bool ScoreTimerDisplay_SetStripBanner(ScoreTimerDisplay_Panel panel,
                                      const char *text, int16_t left_x)
{
    uint32_t rows[8] = {0};
    unsigned int length = 0, i, x, y, dx;
    if (!configured || !panel_valid(panel) || !text) {
        return false;
    }
    while (length < DISPLAY_BANNER_MAX_CHARS && text[length]) {
        ++length;
    }
    if (text[length] != '\0') {
        return false;
    }
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        int32_t origin = (int32_t)left_x + (int32_t)i * DISPLAY_BANNER_CHAR_WIDTH;
        if (origin >= 32 || origin + DISPLAY_BANNER_CHAR_WIDTH <= 0) {
            continue;
        }
        if (c < 32 || c > 126) {
            c = '?';
        }
        for (y = 0; y < 6; ++y) {
            uint8_t bits = getFontLine(c, (int)y);
            for (x = 0; x < 4; ++x) {
                if (!(bits & (1u << (3 - x)))) {
                    continue;
                }
                for (dx = 0; dx < 2; ++dx) {
                    int32_t screen_x = origin + (int32_t)(x * 2 + dx);
                    if (screen_x >= 0 && screen_x < 32) {
                        rows[y + 1] |= UINT32_C(1) << (31 - screen_x);
                    }
                }
            }
        }
    }
    return ScoreTimerDisplay_SetFrame(panel, rows);
}

static bool set_value(const char *text, unsigned int start, unsigned int width)
{
    unsigned int y, length = (unsigned int)strlen(text);
    uint32_t mask = ((UINT32_C(1) << width) - 1) << (32 - start - width);
    if (!configured) {
        return false;
    }
    for (y = 0; y < 8; ++y) {
        /* Column18 separates the two fields. */
        pending[DISPLAY_BOTTOM][y] &= ~(mask | (UINT32_C(1) << 13));
    }
    draw_text(pending[DISPLAY_BOTTOM], text, length, start + (width - length * 4) / 2);
    dirty = true;
    return true;
}

bool ScoreTimerDisplay_SetScore(int32_t score)
{
    char text[5], reversed[4];
    uint32_t magnitude;
    unsigned int count = 0, i = 0;
    if (score < -999 || score > 9999) {
        (void)set_value("----", 0, 18);
        return false;
    }
    magnitude = (uint32_t)(score < 0 ? -score : score);
    do {
        reversed[count++] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude);
    if (score < 0) {
        text[i++] = '-';
    }
    while (count) {
        text[i++] = reversed[--count];
    }
    text[i] = '\0';
    return set_value(text, 0, 18);
}

bool ScoreTimerDisplay_SetTimerSeconds(uint32_t seconds)
{
    char text[4], reversed[3];
    unsigned int count = 0, i = 0;
    if (seconds > 999) {
        (void)set_value("---", 19, 13);
        return false;
    }
    do {
        reversed[count++] = (char)('0' + seconds % 10);
        seconds /= 10;
    } while (seconds);
    while (count) {
        text[i++] = reversed[--count];
    }
    text[i] = '\0';
    return set_value(text, 19, 13);
}

static void capture_snapshot(void)
{
    unsigned int panel, x, y;
    memset(snapshot, 0, sizeof snapshot);
    for (panel = 0; panel < 2; ++panel) {
        unsigned int first = ((panel == DISPLAY_TOP) == layout.top_first) ? 0 : 4;
        for (y = 0; y < 8; ++y) {
            for (x = 0; x < 32; ++x) {
                unsigned int px = x % 8, py = y, tmp, tile = x / 8;
                unsigned int device = first + (layout.reverse_modules[panel] ? 3 - tile : tile);
                if (!(pending[panel][y] & (UINT32_C(1) << (31 - x)))) {
                    continue;
                }
                switch (layout.rotation[panel]) {
                case 90:  tmp = px; px = 7 - py; py = tmp; break;
                case 180: px = 7 - px; py = 7 - py; break;
                case 270: tmp = px; px = py; py = 7 - tmp; break;
                default: break;
                }
                snapshot[device][py] |= (uint8_t)(1u << (7 - px));
            }
        }
    }
    dirty = false;
}

ScoreTimerDisplay_State ScoreTimerDisplay_Service(void)
{
    uint16_t words[8], command;
    unsigned int i;
    StatusDisplaySPI_State bus;
    if (!configured || state == SCORE_TIMER_FAULT) {
        return SCORE_TIMER_FAULT;
    }
    bus = StatusDisplaySPI_Poll();
    if (bus == STATUS_SPI_FAULT) {
        state = SCORE_TIMER_FAULT;
        return state;
    }
    if (bus == STATUS_SPI_BUSY) {
        return state;
    }
    if (init_step < 14) {
        switch (init_step) {
        case 0: command = 0x0C00; break; /* Shutdown. */
        case 1: command = 0x0F00; break; /* Never use full-current display-test mode. */
        case 2: command = 0x0900; break; /* No Code-B decoding for matrices. */
        case 3: command = 0x0B07; break; /* All eight rows. */
        case 4: command = (uint16_t)(0x0A00 | layout.intensity); break;
        case 13: command = 0x0C01; break;
        default: command = (uint16_t)((init_step - 4u) << 8); break; /* Clear rows. */
        }
        for (i = 0; i < 8; ++i) {
            words[i] = command;
        }
        ++init_step;
    } else if (refresh_step) {
        /* Re-assert configuration after every frame. A 5 V dip resets a MAX7219
         * into shutdown (dark), and a glitch on the shared buffer can corrupt a
         * control register; the one-time boot sequence would never repair either.
         * No shutdown/clear here, so this never flickers. */
        switch (refresh_step) {
        case 1: command = 0x0F00; break;
        case 2: command = 0x0900; break;
        case 3: command = 0x0B07; break;
        case 4: command = (uint16_t)(0x0A00 | layout.intensity); break;
        default: command = 0x0C01; break;
        }
        for (i = 0; i < 8; ++i) {
            words[i] = command;
        }
        refresh_step = refresh_step == 5u ? 0u : (uint8_t)(refresh_step + 1u);
        state = SCORE_TIMER_BUSY;
    } else {
        if (!sending) {
            if (!dirty) {
                state = SCORE_TIMER_IDLE;
                return state;
            }
            capture_snapshot();
            next_row = 0;
            sending = true;
        }
        state = SCORE_TIMER_BUSY;
        for (i = 0; i < 8; ++i) {
            words[i] = (uint16_t)(((next_row + 1u) << 8) | snapshot[7 - i][next_row]);
        }
        if (++next_row == 8) {
            sending = false;
            refresh_step = 1;
        }
    }
    if (!StatusDisplaySPI_Start(words, 8)) {
        state = SCORE_TIMER_FAULT;
    }
    return state;
}
