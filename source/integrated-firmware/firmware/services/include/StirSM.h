/* Credit measured changes in commanded angle; holding a stick earns no time. */
#ifndef PANIC_STIRSM_H
#define PANIC_STIRSM_H
#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"
#include "GameConfig.h"
#include "ES_Configure.h"
#include "ES_Types.h"

typedef enum { STIR_IDLE, STIR_NOT_STIRRING, STIR_STIRRING, STIR_FAULT } StirSMState_t;
typedef struct {
    uint32_t (*now_ms)(void);
    uint32_t (*generation)(void);
    bool (*accept_input)(uint32_t detected_ms, uint32_t generation);
    bool (*accepts_round)(uint32_t round);
    bool (*motor_target)(uint32_t angle_mdeg, bool home, bool enabled);
    bool (*motion)(bool stirring, uint32_t detected_ms, uint32_t round);
    bool (*score_stir)(uint8_t ingredient, uint32_t round, uint32_t elapsed_ms);
} StirSMPort_t;
/* configured=false tracks ingredients but keeps the motor disabled and emits
 * no stir score. Configured operation requires explicit calibration values. */
bool StirSM_BindPort(const StirSMPort_t *port, const GameTuning_t *tuning);
bool StirSM_IsConfigured(void);
bool StirSM_HasFault(void);
uint32_t StirSM_ElapsedMs(void);
bool StirSM_PostLifecycle(ES_EventType_t type, uint32_t generation, uint32_t at_ms);
bool StirSM_PostIngredient(uint8_t ingredient, uint32_t round, uint32_t at_ms);
/* Publish each fresh joystick reading, including stationary/home readings.
 * Angles are [0,360000); centered/home movement is never credited.
 */
bool StirSM_PostMotion(uint32_t angle_mdeg, bool home, uint32_t detected_ms, uint32_t generation);
bool InitStirSM(uint8_t priority);
bool PostStirSM(ES_Event_t event);
ES_Event_t RunStirSM(ES_Event_t event);
StirSMState_t QueryStirSM(void);
#endif
