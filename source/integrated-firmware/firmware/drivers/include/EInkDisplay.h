#ifndef PANIC_EINK_DISPLAY_H
#define PANIC_EINK_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EINK_WIDTH 296u
#define EINK_HEIGHT 128u
#define EINK_PLANE_BYTES (EINK_WIDTH * EINK_HEIGHT / 8u)
#define EINK_REFRESH_INTERVAL_MS 180000u

typedef enum {
    EINK_PROFILE_NONE, EINK_IL0373, EINK_UC8151D, EINK_SSD1680
} EInkProfile;
typedef enum { EINK_WHITE, EINK_BLACK, EINK_RED } EInkColor;
typedef enum {
    EINK_OK, EINK_NOT_BOUND, EINK_BAD_ARGUMENT, EINK_NEEDS_PROFILE,
    EINK_COOLDOWN, EINK_IO_ERROR, EINK_BUSY_TIMEOUT,
    EINK_BUSY_NOT_ASSERTED, EINK_FAULT_LATCHED
} EInkResult;
typedef enum {
    EINK_IDLE, EINK_RESETTING, EINK_INITIALIZING, EINK_WRITING,
    EINK_REFRESHING, EINK_SLEEPING, EINK_FAULT
} EInkPhase;

/* Canonical masks: one means ink in that plane; white is zero in both.
 * Landscape (x,y) maps to panel-native (y,295-x), MSB-first.
 * Static allocation: two 4736-byte planes, no heap or external SRAM.
 */
typedef struct { uint8_t black[EINK_PLANE_BYTES], red[EINK_PLANE_BYTES]; } EInkFrame;
typedef struct {
    /* One complete SPI transaction. Return only after the last bit has shifted.
     * Implement a bounded transfer timeout and release SS on failure.
     */
    bool (*write)(bool data, const uint8_t *bytes, size_t count);
    void (*reset)(bool high);
    bool (*busy_level)(void);
    uint32_t (*now_ms)(void);
    void (*delay_ms)(uint32_t milliseconds);
} EInkPort;

/* Standalone, synchronous bench driver. Not an ES service: a refresh can block
 * for tens of seconds. Port callbacks must be bounded and time must advance.
 * Bind once at boot. No automatic controller detection or profile cycling.
 */
bool EInk_Bind(const EInkPort *port);
EInkResult EInk_Select(EInkProfile profile);
EInkProfile EInk_Profile(void);
EInkPhase EInk_Phase(void);
EInkResult EInk_Refresh(const EInkFrame *frame);
uint32_t EInk_CooldownRemaining(void);
const char *EInk_ProfileName(EInkProfile profile);
const char *EInk_ResultName(EInkResult result);
void EInk_Clear(EInkFrame *frame);
bool EInk_Pixel(EInkFrame *frame, int x, int y, EInkColor color);

#endif
