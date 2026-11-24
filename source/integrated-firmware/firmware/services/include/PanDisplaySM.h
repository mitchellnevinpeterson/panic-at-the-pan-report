/* Semantic pan frames only. The adapter coalesces/rate limits e-ink updates. */
#ifndef PANIC_PANDISPLAYSM_H
#define PANIC_PANDISPLAYSM_H
#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"
#include "GameConfig.h"
#include "GameSM.h"
#include "ES_Configure.h"
#include "ES_Types.h"

typedef enum { PAN_ATTRACT, PAN_NEW_INGREDIENT, PAN_VIEW, PAN_SWIRL,
               PAN_GAME_OVER_TEXT, PAN_TIMEOUT_ICON, PAN_FAULT } PanDisplaySMState_t;
typedef struct {
    uint32_t (*now_ms)(void);
    uint32_t (*generation)(void);
    bool (*accept_input)(uint32_t detected_ms, uint32_t generation);
    bool (*frame)(PanDisplaySMState_t state, uint8_t ingredient_mask, uint8_t latest_ingredient);
} PanDisplaySMPort_t;
/* Semantic states remain available when tuning is unconfigured. */
bool PanDisplaySM_BindPort(const PanDisplaySMPort_t *port, const GameTuning_t *tuning);
bool PanDisplaySM_IsConfigured(void);
bool PanDisplaySM_HasFault(void);
bool PanDisplaySM_PostLifecycle(ES_EventType_t type, uint32_t generation, uint32_t at_ms,
                                GameSMEndReason_t reason);
bool PanDisplaySM_PostIngredient(uint8_t ingredient, uint32_t round, uint32_t at_ms);
bool PanDisplaySM_PostMotion(bool stirring, uint32_t detected_ms, uint32_t generation);
bool InitPanDisplaySM(uint8_t priority);
bool PostPanDisplaySM(ES_Event_t event);
ES_Event_t RunPanDisplaySM(ES_Event_t event);
PanDisplaySMState_t QueryPanDisplaySM(void);
#endif
