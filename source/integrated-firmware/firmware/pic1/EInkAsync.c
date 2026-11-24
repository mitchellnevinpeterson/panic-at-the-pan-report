/* IL0373 sequence adapted from the tested EInkDisplay.c port of Adafruit_EPD.
 * Copyright (c) 2025 limor fried, MIT; see vendor/adafruit_epd/LICENSE.txt.
 * No partial updates, custom LUTs or guessed profiles. Cooldown is configured
 * by the caller; the default remains EINK_REFRESH_INTERVAL_MS.
 */
#include "EInkAsync.h"

enum Op { RESET_HIGH, RESET_LOW, CMD, DATA, DELAY, READY, ASSERT_BUSY,
          BLACK_PLANE, RED_PLANE, TABLE, END };
typedef struct { enum Op op; uint16_t value; uint8_t bytes[5]; } Step;
static const Step script[] = {
    {RESET_HIGH,0,{0}}, {DELAY,10,{0}}, {RESET_LOW,0,{0}},
    {DELAY,10,{0}}, {RESET_HIGH,0,{0}}, {DELAY,10,{0}},
    {CMD,0x01,{0}}, {DATA,5,{0x03,0x00,0x2b,0x2b,0x09}},
    {CMD,0x06,{0}}, {DATA,3,{0x17,0x17,0x17}},
    {CMD,0x04,{0}}, {DELAY,200,{0}}, {READY,10000,{0}},
    {CMD,0x00,{0}}, {DATA,1,{0xcf}},
    {CMD,0x50,{0}}, {DATA,1,{0x37}},
    {CMD,0x30,{0}}, {DATA,1,{0x29}},
    {CMD,0x82,{0}}, {DATA,1,{0x0a}}, {DELAY,20,{0}},
    {CMD,0x61,{0}}, {DATA,3,{0x80,0x01,0x28}},
    {CMD,0x10,{0}}, {BLACK_PLANE,0,{0}}, {DELAY,2,{0}},
    {CMD,0x13,{0}}, {RED_PLANE,0,{0}},
    {CMD,0x12,{0}}, {ASSERT_BUSY,2000,{0}}, {READY,45000,{0}},
    {CMD,0x50,{0}}, {DATA,1,{0x17}}, {CMD,0x02,{0}},
    {DELAY,100,{0}}, {READY,10000,{0}}, {END,0,{0}}
};
/* Opt-in fast black/white mode. Init values and the 4-gray register LUT are
 * verbatim from Adafruit_EPD ThinkInk_290_Grayscale4_T5.h (IL0373, 296x128)
 * at the pinned revision. Both RAM planes carry black|red, so each pixel
 * uses only the pure-white or pure-black table; red pixels are black.
 * Not the tri-color panel's factory waveform: about 100 frames at 50 Hz. */
enum { MONO_LUT_BYTES = 42 };
static const uint8_t mono_luts[5][MONO_LUT_BYTES] = {
    { 0x00,0x0A,0x00,0x00,0x00,0x01, 0x60,0x14,0x14,0x00,0x00,0x01,   /* 0x20 VCOM */
      0x00,0x14,0x00,0x00,0x00,0x01, 0x00,0x13,0x0A,0x01,0x00,0x01 },
    { 0x40,0x0A,0x00,0x00,0x00,0x01, 0x90,0x14,0x14,0x00,0x00,0x01,   /* 0x21 WW */
      0x10,0x14,0x0A,0x00,0x00,0x01, 0xA0,0x13,0x01,0x00,0x00,0x01 },
    { 0x40,0x0A,0x00,0x00,0x00,0x01, 0x90,0x14,0x14,0x00,0x00,0x01,   /* 0x22 BW */
      0x00,0x14,0x0A,0x00,0x00,0x01, 0x99,0x0C,0x01,0x03,0x04,0x01 },
    { 0x40,0x0A,0x00,0x00,0x00,0x01, 0x90,0x14,0x14,0x00,0x00,0x01,   /* 0x23 WB */
      0x00,0x14,0x0A,0x00,0x00,0x01, 0x99,0x0B,0x04,0x04,0x01,0x01 },
    { 0x80,0x0A,0x00,0x00,0x00,0x01, 0x90,0x14,0x14,0x00,0x00,0x01,   /* 0x24 BB */
      0x20,0x14,0x0A,0x00,0x00,0x01, 0x50,0x13,0x01,0x00,0x00,0x01 }
};
static const Step mono_script[] = {
    {RESET_HIGH,0,{0}}, {DELAY,10,{0}}, {RESET_LOW,0,{0}},
    {DELAY,10,{0}}, {RESET_HIGH,0,{0}}, {DELAY,10,{0}},
    {CMD,0x01,{0}}, {DATA,5,{0x03,0x00,0x2b,0x2b,0x13}},
    {CMD,0x06,{0}}, {DATA,3,{0x17,0x17,0x17}},
    {CMD,0x04,{0}}, {DELAY,200,{0}}, {READY,10000,{0}},
    {CMD,0x00,{0}}, {DATA,1,{0x3f}}, /* Black/white mode, LUT from registers. */
    {CMD,0x30,{0}}, {DATA,1,{0x3c}},
    {CMD,0x82,{0}}, {DATA,1,{0x12}},
    {CMD,0x50,{0}}, {DATA,1,{0x97}},
    {CMD,0x20,{0}}, {TABLE,0,{0}}, {CMD,0x21,{0}}, {TABLE,1,{0}},
    {CMD,0x22,{0}}, {TABLE,2,{0}}, {CMD,0x23,{0}}, {TABLE,3,{0}},
    {CMD,0x24,{0}}, {TABLE,4,{0}},
    {CMD,0x61,{0}}, {DATA,3,{0x80,0x01,0x28}},
    {CMD,0x10,{0}}, {BLACK_PLANE,0,{0}}, {DELAY,2,{0}},
    {CMD,0x13,{0}}, {RED_PLANE,0,{0}}, /* Sends black|red in this mode. */
    {CMD,0x12,{0}}, {ASSERT_BUSY,2000,{0}}, {READY,45000,{0}},
    {CMD,0x50,{0}}, {DATA,1,{0x17}}, {CMD,0x02,{0}},
    {DELAY,100,{0}}, {READY,10000,{0}}, {END,0,{0}}
};
static const Step *program = script;
static bool mono;
static EInkAsyncPort io;
static const EInkFrame *pixels;
static uint32_t last_finished, step_started;
static uint32_t cooldown;
static size_t index, offset, transfer_count;
static bool bound, running, waiting, transferring;
static EInkResult result;

bool EInkAsync_Init(const EInkAsyncPort *port)
{
    return EInkAsync_InitWithCooldown(port, EINK_REFRESH_INTERVAL_MS);
}

bool EInkAsync_InitWithCooldown(const EInkAsyncPort *port, uint32_t cooldown_ms)
{
    if (!port || !port->start || !port->poll || !port->reset ||
        !port->busy_level || !port->now_ms || !cooldown_ms) return false;
    io = *port;
    cooldown = cooldown_ms;
    program = script;
    mono = false;
    bound = true;
    running = waiting = transferring = false;
    result = EINK_OK;
    last_finished = io.now_ms();
    io.reset(true);
    return true;
}

bool EInkAsync_InitFastMono(const EInkAsyncPort *port, uint32_t cooldown_ms)
{
    if (!EInkAsync_InitWithCooldown(port, cooldown_ms)) return false;
    program = mono_script;
    mono = true;
    return true;
}

static size_t chunked_length(const Step *s)
{
    return s->op == TABLE ? MONO_LUT_BYTES : EINK_PLANE_BYTES;
}

bool EInkAsync_CanStart(void)
{
    return bound && !running && result == EINK_OK &&
           (uint32_t)(io.now_ms() - last_finished) >= cooldown;
}
bool EInkAsync_IsBusy(void) { return running; }
EInkResult EInkAsync_Result(void) { return result; }

bool EInkAsync_Start(const EInkFrame *frame)
{
    if (!frame || !EInkAsync_CanStart()) return false;
    pixels = frame;
    index = offset = 0;
    running = true;
    waiting = transferring = false;
    return true;
}

static bool fail(EInkResult failure)
{
    result = failure;
    running = false;
    io.reset(false);
    last_finished = io.now_ms();
    return false;
}

bool EInkAsync_Service(void)
{
    uint8_t bytes[32];
    if (!bound || result != EINK_OK) return false;
    if (!running) return true;
    const Step *s = &program[index];
    if (transferring) {
        EInkBusState state = io.poll();
        if (state == EINK_BUS_FAULT) return fail(EINK_IO_ERROR);
        if (state == EINK_BUS_BUSY) return true;
        transferring = false;
        if (s->op == BLACK_PLANE || s->op == RED_PLANE || s->op == TABLE) {
            offset += transfer_count;
            if (offset < chunked_length(s)) return true;
            offset = 0;
        }
        ++index;
        return true;
    }
    switch (s->op) {
    case RESET_HIGH: case RESET_LOW:
        io.reset(s->op == RESET_HIGH); ++index; return true;
    case CMD:
        bytes[0] = (uint8_t)s->value;
        if (!io.start(false, bytes, 1)) return fail(EINK_IO_ERROR);
        transferring = true; return true;
    case DATA:
        if (!io.start(true, s->bytes, s->value)) return fail(EINK_IO_ERROR);
        transferring = true; return true;
    case BLACK_PLANE: case RED_PLANE: {
        const uint8_t *plane = s->op == BLACK_PLANE ? pixels->black : pixels->red;
        transfer_count = EINK_PLANE_BYTES - offset;
        if (transfer_count > sizeof bytes) transfer_count = sizeof bytes;
        for (size_t i = 0; i < transfer_count; ++i) {
            size_t at = offset + i;
            uint8_t ink = mono ? (uint8_t)(pixels->black[at] | pixels->red[at]) : plane[at];
            bytes[i] = (uint8_t)~ink;
        }
        if (!io.start(true, bytes, transfer_count)) return fail(EINK_IO_ERROR);
        transferring = true; return true;
    }
    case TABLE:
        transfer_count = MONO_LUT_BYTES - offset;
        if (transfer_count > sizeof bytes) transfer_count = sizeof bytes;
        if (!io.start(true, &mono_luts[s->value][offset], transfer_count)) return fail(EINK_IO_ERROR);
        transferring = true; return true;
    case DELAY: case READY: case ASSERT_BUSY: {
        /* One timestamp per step evaluation: a tick between two clock reads
         * must never turn a completed DELAY into a BUSY timeout. */
        uint32_t now = io.now_ms();
        if (!waiting) { waiting = true; step_started = now; }
        uint32_t elapsed = (uint32_t)(now - step_started);
        if (s->op == DELAY) {
            if (elapsed >= s->value) { waiting = false; ++index; }
            return true;
        }
        if ((s->op == READY && io.busy_level()) ||
            (s->op == ASSERT_BUSY && !io.busy_level())) {
            waiting = false; ++index; return true;
        }
        if (elapsed >= s->value)
            return fail(s->op == ASSERT_BUSY ? EINK_BUSY_NOT_ASSERTED : EINK_BUSY_TIMEOUT);
        return true;
    }
    case END:
        running = false; last_finished = io.now_ms(); return true;
    }
    return fail(EINK_BAD_ARGUMENT);
}
