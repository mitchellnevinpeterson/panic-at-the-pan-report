#include "Treat.h"
#include "Servo50Hz.h"
#include <stdio.h>

#define NUDGE_US 25u

/* RAM-only calibration: Treat_Key nudges these and later moves use them. */
static uint16_t closed_us = TREAT_CLOSED_US, release_us = TREAT_RELEASE_US;
static bool rotated, at_release;
static bool timed_open;         /* game-opened: closes TREAT_OPEN_MS after opened_at */
static uint32_t opened_at;
#if TREAT_DRIVE_MS > 0
static bool driving;
static uint32_t moved_at;
#endif

static void move(bool release, uint32_t now_ms)
{
    uint16_t us = release ? release_us : closed_us;
    at_release = release;
    /* One line per move so a missed open/close is visible in the PIC1 log. */
    printf("TREAT -> %s %uus (%s)\r\n", release ? "release" : "closed", (unsigned)us,
           Servo50Hz_Write(SERVO_TREAT, us) ? "pulsing" : "WRITE FAILED");
#if TREAT_DRIVE_MS > 0
    /* Open: keep pulsing until the next close. On the bench the opening move at
     * the big score (E-ink refresh + big digits + music loading the supply)
     * sometimes did not finish within the drive window. Closed: stop pulses
     * after the window, so the gate cannot hunt and flicker the displays. */
    driving = !release;
    moved_at = now_ms;
#else
    (void)now_ms;               /* hold: keep pulsing */
#endif
}

void Treat_Init(uint32_t now_ms)
{
    rotated = timed_open = false;
    move(false, now_ms);
}

void Treat_Release(uint32_t now_ms)
{
    rotated = true;
    timed_open = TREAT_OPEN_MS > 0;
    opened_at = now_ms;
    move(true, now_ms);
}

void Treat_Close(uint32_t now_ms)
{
    rotated = timed_open = false;
    move(false, now_ms);
}

bool Treat_Rotated(void) { return rotated; }

void Treat_Service(uint32_t now_ms)
{
    if (timed_open && (uint32_t)(now_ms - opened_at) >= TREAT_OPEN_MS) {
        printf("TREAT auto-close %lu ms after opening\r\n", (unsigned long)TREAT_OPEN_MS);
        Treat_Close(now_ms);
    }
#if TREAT_DRIVE_MS > 0
    if (driving && (uint32_t)(now_ms - moved_at) >= TREAT_DRIVE_MS) {
        driving = false;
        Servo50Hz_Disable(SERVO_TREAT);
        printf("TREAT pulses off (holds by gearbox)\r\n");
    }
#else
    (void)now_ms;
#endif
}

static uint16_t nudge(uint16_t us, bool up)
{
    if (up) return us + NUDGE_US <= SERVO50HZ_MAX_US ? (uint16_t)(us + NUDGE_US) : us;
    return us >= SERVO50HZ_MIN_US + NUDGE_US ? (uint16_t)(us - NUDGE_US) : us;
}

bool Treat_Key(char key, uint32_t now_ms)
{
    switch (key) {
    case 'd': Servo50Hz_Report(); return true; /* register-level check */
    case 'o': rotated = true; timed_open = false; move(true, now_ms); break; /* stays open */
    case 'c': Treat_Close(now_ms); break;
    case '+': case '-':
        if (at_release) release_us = nudge(release_us, key == '+');
        else closed_us = nudge(closed_us, key == '+');
        move(at_release, now_ms);
        break;
    default: return false;
    }
    printf("TREAT closed=%u release=%u now=%u\r\n", (unsigned)closed_us,
           (unsigned)release_us, (unsigned)(at_release ? release_us : closed_us));
    return true;
}
