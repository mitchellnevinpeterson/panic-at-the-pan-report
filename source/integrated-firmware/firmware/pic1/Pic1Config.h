#ifndef PANIC_PIC1_CONFIG_H
#define PANIC_PIC1_CONFIG_H
#include "ScoreTimerDisplay.h"

/* Explicit integration choices; standalone bench configurations are untouched. */
#ifndef PIC1_JOYSTICK_MODE
#define PIC1_JOYSTICK_MODE 1 /* 1: tested X checkpoint; 2: experimental CW/CCW */
#endif
#ifndef PIC1_ENABLE_SPOON
#define PIC1_ENABLE_SPOON 0 /* TODO: approve mounted travel and combined-board run. */
#endif
#ifndef PIC1_ENABLE_EINK
#define PIC1_ENABLE_EINK 1 /* Tested IL0373 profile; async, 180 s cooldown. */
#endif
#define PIC1_HAND_PRESENCE_ACTIVITY 0 /* TODO: choose held-hand idle policy. */

static const ScoreTimerDisplay_Config Pic1DisplayConfig = {
    true, {false, false}, {180, 180}, 1
};
#endif
