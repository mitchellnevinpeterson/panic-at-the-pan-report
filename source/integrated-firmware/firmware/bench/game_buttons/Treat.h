#ifndef GAME_BUTTONS_TREAT_H
#define GAME_BUTTONS_TREAT_H
#include <stdbool.h>
#include <stdint.h>

/* Treat dispenser (MG995 on Servo50Hz SERVO_TREAT). final_game_spec.md:
 * power-up -> closed; big score with score >= 50 -> rotate to release, then
 * close by itself TREAT_OPEN_MS later (or at the next game start if sooner).
 * Positions are placeholders until calibrated with Treat_Key on hardware.
 */
/* Bench-confirmed direction: 1000 us = CLOSED, 2000 us = OPEN (a swapped build
 * opened the gate at power-up and closed it at the big score). Refine the
 * exact positions with the o/c/+/- keys and copy the printed values here. */
#ifndef TREAT_CLOSED_US
#define TREAT_CLOSED_US 1000u
#endif
#ifndef TREAT_RELEASE_US
#define TREAT_RELEASE_US 2000u
#endif
/* 0 = keep pulsing to hold position; >0 = after a CLOSE, stop pulses this long
 * later (open always holds until the next close). Holding at "closed" made the
 * MG995 hunt and flicker the displays. Bench register dump ('d' key) confirmed
 * that restarting after a stop reloads OC2R and pulses correctly. */
#ifndef TREAT_DRIVE_MS
#define TREAT_DRIVE_MS 2500u
#endif
#define TREAT_MIN_SCORE 50
/* A game-opened gate closes by itself this long after opening; 0 = stay open
 * until the next game start. The 'o' calibration key never auto-closes. */
#ifndef TREAT_OPEN_MS
#define TREAT_OPEN_MS 20000u
#endif

void Treat_Init(uint32_t now_ms);           /* commands closed */
void Treat_Release(uint32_t now_ms);        /* rotate to release, set the flag, start the auto-close */
/* Unconditional close at every game start: closes the gate left open by the
 * last big score, or opened by hand. Clears the flag. */
void Treat_Close(uint32_t now_ms);
bool Treat_Rotated(void);
void Treat_Service(uint32_t now_ms);        /* call every main-loop pass */
/* PIC1 debug-UART calibration: 'o' release, 'c' closed, 'd' servo registers, '+'/'-' nudge the
 * last commanded position by 25 us and print it. True if the key was used. */
bool Treat_Key(char key, uint32_t now_ms);

#endif
