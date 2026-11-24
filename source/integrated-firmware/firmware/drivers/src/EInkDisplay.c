/* Controller sequences adapted from Adafruit_EPD, revision recorded in
 * vendor/adafruit_epd/README.md. Copyright (c) 2025 limor fried, MIT License;
 * permission and warranty notice: vendor/adafruit_epd/LICENSE.txt.
 * Only normal tri-color updates are ported. No custom waveform or OTP writes.
 */
#include "EInkDisplay.h"
#include <string.h>

static EInkPort io;
static EInkProfile profile;
static EInkPhase phase;
static bool bound, attempted;
static uint32_t last_finished;
static EInkResult error;

bool EInk_Bind(const EInkPort *port)
{
    if (!port || !port->write || !port->reset || !port->busy_level ||
        !port->now_ms || !port->delay_ms) return false;
    io = *port;
    profile = EINK_PROFILE_NONE;
    phase = EINK_IDLE;
    attempted = false;
    error = EINK_OK;
    bound = true;
    return true;
}

EInkProfile EInk_Profile(void) { return profile; }
EInkPhase EInk_Phase(void) { return phase; }

uint32_t EInk_CooldownRemaining(void)
{
    uint32_t elapsed;
    if (!bound || !attempted) return 0;
    elapsed = (uint32_t)(io.now_ms() - last_finished);
    return elapsed < EINK_REFRESH_INTERVAL_MS ? EINK_REFRESH_INTERVAL_MS - elapsed : 0;
}

EInkResult EInk_Select(EInkProfile selected)
{
    if (!bound) return EINK_NOT_BOUND;
    if (phase == EINK_FAULT) return EINK_FAULT_LATCHED;
    if (selected < EINK_IL0373 || selected > EINK_SSD1680) return EINK_BAD_ARGUMENT;
    profile = selected; /* Selection does not reset the refresh lockout. */
    return EINK_OK;
}

static bool send(bool data, const uint8_t *bytes, size_t count)
{
    if (io.write(data, bytes, count)) return true;
    error = EINK_IO_ERROR;
    return false;
}

static bool command(uint8_t cmd, const uint8_t *args, size_t count)
{
    return send(false, &cmd, 1) && (!count || send(true, args, count));
}

static bool command1(uint8_t cmd, uint8_t arg) { return command(cmd, &arg, 1); }
static bool active(void) { return io.busy_level() == (profile == EINK_SSD1680); }

static bool wait_busy(bool desired, uint32_t timeout, EInkResult failure)
{
    uint32_t start = io.now_ms();
    for (;;) {
        /* UC8151D's vendor driver polls GET_STATUS before checking BUSY. */
        if (profile == EINK_UC8151D && !command(0x71, NULL, 0)) return false;
        if (active() == desired) return true;
        if ((uint32_t)(io.now_ms() - start) >= timeout) {
            error = failure;
            return false;
        }
        io.delay_ms(10);
    }
}

static void hardware_reset(void)
{
    io.reset(true); io.delay_ms(10);
    io.reset(false); io.delay_ms(10);
    io.reset(true); io.delay_ms(10);
}

static bool initialize(void)
{
    static const uint8_t il_power[] = { 0x03, 0x00, 0x2b, 0x2b, 0x09 };
    static const uint8_t il_boost[] = { 0x17, 0x17, 0x17 };
    static const uint8_t resolution[] = { 0x80, 0x01, 0x28 }; /* 128 x 296 */
    static const uint8_t uc_panel[] = { 0x0f, 0x89 };
    static const uint8_t ssd_source[] = { 0x41, 0x00, 0x32 };
    static const uint8_t x_window[] = { 0x00, 0x0f };
    static const uint8_t y_window[] = { 0x00, 0x00, 0x27, 0x01 };
    static const uint8_t gates[] = { 0x27, 0x01, 0x00 };
    phase = EINK_RESETTING;
    hardware_reset();
    if (profile == EINK_UC8151D) {
        io.delay_ms(10); hardware_reset();
        io.delay_ms(10); hardware_reset();
        io.delay_ms(10);
    }
    phase = EINK_INITIALIZING;
    if (profile == EINK_IL0373) {
        if (!command(0x01, il_power, sizeof il_power) ||
            !command(0x06, il_boost, sizeof il_boost) || !command(0x04, NULL, 0)) return false;
        io.delay_ms(200);
        if (!wait_busy(false, 10000, EINK_BUSY_TIMEOUT)) return false;
        if (!command1(0x00, 0xcf) || !command1(0x50, 0x37) ||
            !command1(0x30, 0x29) || !command1(0x82, 0x0a)) return false;
        io.delay_ms(20);
        return command(0x61, resolution, sizeof resolution);
    }
    if (profile == EINK_UC8151D) {
        if (!command(0x04, NULL, 0)) return false;
        io.delay_ms(10);
        return wait_busy(false, 10000, EINK_BUSY_TIMEOUT) &&
            command(0x00, uc_panel, sizeof uc_panel) &&
            command(0x61, resolution, sizeof resolution) && command1(0x50, 0x77);
    }
    io.delay_ms(100);
    if (!wait_busy(false, 10000, EINK_BUSY_TIMEOUT) || !command(0x12, NULL, 0)) return false;
    io.delay_ms(20);
    return wait_busy(false, 10000, EINK_BUSY_TIMEOUT) &&
        command1(0x11, 0x03) && command1(0x3c, 0x05) &&
        command1(0x2c, 0x36) && command1(0x03, 0x17) &&
        command(0x04, ssd_source, sizeof ssd_source) &&
        command(0x44, x_window, sizeof x_window) &&
        command(0x45, y_window, sizeof y_window) && command(0x01, gates, sizeof gates);
}

static bool ram_origin(void)
{
    static const uint8_t y_zero[] = { 0, 0 };
    return profile != EINK_SSD1680 ||
        (command1(0x4e, 0) && command(0x4f, y_zero, sizeof y_zero));
}

static bool write_plane(const uint8_t *pixels, bool red)
{
    /* Z10/Z13 swap the upstream buffer member names AND layer_colors bits:
     * BLACK=0b10 clears color_buffer[0], RED=0b01 clears black_buffer[1].
     * The resulting wire order is still black -> 0x10, red -> 0x13.
     * Use the panel wrapper plus drawPixel(), not member names alone.
     */
    uint8_t converted[64];
    size_t i, n, offset;
    uint8_t cmd = profile == EINK_SSD1680 ? (red ? 0x26 : 0x24) : (red ? 0x13 : 0x10);
    bool invert = !red || profile != EINK_SSD1680;
    if (!ram_origin() || !command(cmd, NULL, 0)) return false;
    for (offset = 0; offset < EINK_PLANE_BYTES; offset += n) {
        n = EINK_PLANE_BYTES - offset;
        if (n > sizeof converted) n = sizeof converted;
        for (i = 0; i < n; ++i) converted[i] = invert ? (uint8_t)~pixels[offset+i] : pixels[offset+i];
        if (!send(true, converted, n)) return false;
    }
    return true;
}

static bool finish_refresh(void)
{
    phase = EINK_REFRESHING;
    if (profile == EINK_SSD1680) {
        /* Z94 uses the panel's factory tri-color waveform. */
        if (!command1(0x22, 0xf4) || !command(0x20, NULL, 0)) return false;
    } else if (!command(0x12, NULL, 0)) return false;
    /* Require the refresh BUSY cycle: a floating/wrong-polarity wire must not
     * silently count as a successful refresh. It still cannot verify pixels.
     */
    if (!wait_busy(true, 2000, EINK_BUSY_NOT_ASSERTED) ||
        !wait_busy(false, 45000, EINK_BUSY_TIMEOUT)) return false;
    phase = EINK_SLEEPING;
    if (profile == EINK_SSD1680) {
        if (!command1(0x10, 0x01)) return false;
        io.delay_ms(100);
        return true;
    }
    if (!command1(0x50, profile == EINK_UC8151D ? 0xf7 : 0x17) ||
        !command(0x02, NULL, 0)) return false;
    io.delay_ms(100);
    if (!wait_busy(false, 10000, EINK_BUSY_TIMEOUT)) return false;
    return profile != EINK_UC8151D || command1(0x07, 0xa5);
}

EInkResult EInk_Refresh(const EInkFrame *frame)
{
    if (!bound) return EINK_NOT_BOUND;
    if (!frame) return EINK_BAD_ARGUMENT;
    if (phase == EINK_FAULT) return EINK_FAULT_LATCHED;
    if (profile == EINK_PROFILE_NONE) return EINK_NEEDS_PROFILE;
    if (EInk_CooldownRemaining()) return EINK_COOLDOWN;
    attempted = true;
    error = EINK_OK;
    if (!initialize()) goto fault;
    phase = EINK_WRITING;
    if (!write_plane(frame->black, false)) goto fault;
    io.delay_ms(2);
    if (!write_plane(frame->red, true) || !finish_refresh()) goto fault;
    last_finished = io.now_ms();
    phase = EINK_IDLE;
    return EINK_OK;
fault:
    io.reset(false); /* Stop the failed session; no automatic retry. */
    last_finished = io.now_ms();
    phase = EINK_FAULT;
    return error;
}

void EInk_Clear(EInkFrame *frame) { if (frame) memset(frame, 0, sizeof *frame); }

bool EInk_Pixel(EInkFrame *frame, int x, int y, EInkColor color)
{
    size_t index;
    uint8_t mask;
    if (!frame || x < 0 || x >= (int)EINK_WIDTH || y < 0 ||
        y >= (int)EINK_HEIGHT || color < EINK_WHITE || color > EINK_RED) return false;
    index = ((EINK_WIDTH - 1u - (unsigned)x) * EINK_HEIGHT + (unsigned)y) / 8u;
    mask = (uint8_t)(0x80u >> ((unsigned)y & 7u));
    frame->black[index] &= (uint8_t)~mask;
    frame->red[index] &= (uint8_t)~mask;
    if (color == EINK_BLACK) frame->black[index] |= mask;
    if (color == EINK_RED) frame->red[index] |= mask;
    return true;
}

const char *EInk_ProfileName(EInkProfile selected)
{
    switch (selected) {
    case EINK_IL0373: return "IL0373 / Z10 (older breakout)";
    case EINK_UC8151D: return "UC8151D / Z13 (2022 revision)";
    case EINK_SSD1680: return "SSD1680 / Z94 (2025 revision)";
    default: return "UNSELECTED";
    }
}

const char *EInk_ResultName(EInkResult result)
{
    static const char *const names[] = {
        "OK: BUSY cycle completed; inspect image", "port not bound", "bad argument",
        "select controller first", "refresh cooldown", "SPI transfer failed",
        "BUSY did not become ready", "refresh BUSY never asserted",
        "fault latched; check wiring/profile/power, then reset"
    };
    return (unsigned)result < sizeof names / sizeof names[0] ? names[result] : "unknown error";
}
