/* Foreground-only heat service. No sensor waits and no flame servo. */
#ifndef PANIC_HEATSM_H
#define PANIC_HEATSM_H
#include <stdbool.h>
#include <stdint.h>
#include "ES_Events.h"
#include "GameConfig.h"
#include "ES_Configure.h"
#include "ES_Types.h"

typedef enum { HEAT_PAN_EMPTY, HEAT_COOKING, HEAT_OVERHEATING, HEAT_BURNING, HEAT_FAULT } HeatSMState_t;
typedef struct {
    uint32_t (*now_ms)(void);
    uint32_t (*generation)(void);
    bool (*accept_input)(uint32_t detected_ms, uint32_t generation);
    bool (*accepts_round)(uint32_t round);
    /* Latest completed measurement only; false means invalid/no sample. */
    bool (*read_distance)(uint16_t *mm, uint32_t *sample_ms);
    /* valid=false silences sizzle and clears the heat bar; burning is latched. */
    bool (*output)(uint16_t heat, bool burning, bool valid);
    bool (*score_heat)(uint8_t ingredient, uint32_t round, uint16_t closeness, uint32_t elapsed_ms);
    bool (*score_burn)(uint8_t ingredient, uint32_t round);
} HeatSMPort_t;
/* configured=false is lifecycle-only with safe empty outputs. Invalid explicit
 * configured=true tuning is rejected. A binding copies configuration. */
bool HeatSM_BindPort(const HeatSMPort_t *port, const GameTuning_t *tuning);
bool HeatSM_IsConfigured(void);
bool HeatSM_HasFault(void);
bool HeatSM_PostLifecycle(ES_EventType_t type, uint32_t generation, uint32_t at_ms);
bool HeatSM_PostIngredient(uint8_t ingredient, uint32_t round, uint32_t at_ms);
bool InitHeatSM(uint8_t priority);
bool PostHeatSM(ES_Event_t event);
ES_Event_t RunHeatSM(ES_Event_t event);
HeatSMState_t QueryHeatSM(void);
#endif
