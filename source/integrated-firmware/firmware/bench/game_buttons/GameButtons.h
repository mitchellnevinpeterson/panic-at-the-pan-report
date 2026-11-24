#ifndef GAME_BUTTONS_H
#define GAME_BUTTONS_H
#include <stdbool.h>
#include <stdint.h>
bool GameButtons_Init(uint32_t (*clock)(void));
bool GameButtons_Service(void);
void GameButtons_Boundary(void);
void GameButtons_Lamp(bool on);
void GameButtons_State(void);
void GameButtons_ClearPan(void);
void GameButtons_AddIngredient(uint8_t ingredient);
void GameButtons_Stop(void);
/* Unrecoverable: stops the adapter (lamps OFF). Preserve the first failure;
 * reason must have static storage duration. Link loss is NOT a fault: it
 * aborts the round back to Welcome (GameSM_PostAbort) and play continues. */
void GameButtons_Fault(const char *reason);
const char *GameButtons_FaultReason(void);
/* Last recoverable condition that aborted a round, or "NONE". */
const char *GameButtons_RecoveredReason(void);
/* CookSMPort_t.distance_mm: latest hand reading received <= 350 ms ago
 * and within COOK_HAND_MIN_MM..COOK_HAND_MAX_MM; false means no hand. */
bool GameButtons_DistanceMm(uint16_t *mm);
uint32_t GameButtons_UptimeMs(void);
void GameButtons_Status(void);
#endif
