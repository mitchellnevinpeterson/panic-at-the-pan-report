/* PIC1 physical adapter for the final game: PeerLink inputs/sensor readings,
 * lamps, CookSM -> PIC2 control, E-ink, and recoverable link faults. */
#include "GameButtons.h"
#include "GameSM.h"
#include "IngredientSM.h"
#include "CookSM.h"
#include "GameConfig.h"
#include "PeerLink.h"
#include "PeerUart.h"
#include "ButtonEInk.h"
#include "ButtonEInkPort.h"
#include "Treat.h"
#include "Stirrer.h"
#include "Servo50Hz.h"
#include <stdio.h>
/* Hand activity: 3 in-range readings in a row, then at most every 500 ms. */
enum { HAND_STREAK = 3, HAND_ACTIVITY_MS = 500, HAND_FRESH_MS = 350 };
static PeerLink peer;
static PeerControl control;
static uint32_t (*clock_ms)(void);
static uint32_t generation, sync_started, last_report, abort_generation;
static uint32_t hand_rx_ms, hand_activity_ms;
static uint16_t hand_mm;
static uint8_t hand_streak;
static bool stopped, synchronized, expected_sync, abort_posted, eink_dead;
static bool hand_valid, hand_activity_sent;
static const char *fault_reason, *recovered_reason;
static uint32_t now(void *unused) { (void)unused; return clock_ms(); }
static bool cooking(void) {
    GameSMState_t s=QueryGameSM();
    return s==GAME_COOKING_EARLY || s==GAME_COOKING_FINISHABLE;
}
/* Link faults matter during the intro, cooking and the end sequence. */
static bool active(void) {
    GameSMState_t s=QueryGameSM();
    return s==GAME_DISH_INTRO || cooking() || s==GAME_OVER;
}
void GameButtons_Stop(void) {
    GameButtons_Fault("APPLICATION_STOP");
}
const char *GameButtons_FaultReason(void) { return fault_reason?fault_reason:"NONE"; }
void GameButtons_Fault(const char *reason) {
    if(stopped) return;
    fault_reason=reason?reason:"UNSPECIFIED";
    stopped=true; control=(PeerControl){0};
    ButtonEInk_Clear();
    PeerLink_SendControl(&peer,&control);
    printf("FIRST FAULT: %s at %lu ms\r\n",fault_reason,(unsigned long)clock_ms());
}
/* Recoverable: return the game to Welcome (no treat; Reset clears the pan).
 * One abort per generation, so a persisting condition cannot flood GameSM. */
static void recover(const char *reason) {
    uint32_t g=GameSM_InputGeneration();
    if(stopped || (abort_posted && abort_generation==g)) return;
    abort_posted=true; abort_generation=g; recovered_reason=reason;
    printf("RECOVER: %s at %lu ms -> Welcome\r\n",reason,(unsigned long)clock_ms());
    if(!GameSM_PostAbort()) GameButtons_Fault("GAME_ABORT_QUEUE");
}
static void hand_reading(const PeerEvent *e) {
    uint32_t t=clock_ms();
    bool in_range=e->valid && e->distance_mm>=COOK_HAND_MIN_MM && e->distance_mm<=COOK_HAND_MAX_MM;
    /* A gap in readings breaks "in a row" just like an out-of-range one. */
    if((uint32_t)(t-hand_rx_ms)>HAND_FRESH_MS) { hand_streak=0; hand_activity_sent=false; }
    /* A missed echo keeps the last valid distance (it still expires after
     * HAND_FRESH_MS), so one bad reading cannot reset CookSM's burn timer/zone. */
    if(e->valid) { hand_valid=true; hand_mm=e->distance_mm; hand_rx_ms=t; }
    if(!in_range) { hand_streak=0; hand_activity_sent=false; return; }
    if(hand_streak<HAND_STREAK) ++hand_streak;
    if(hand_streak<HAND_STREAK || stopped || !cooking() || generation!=GameSM_InputGeneration()) return;
    if(hand_activity_sent && (uint32_t)(t-hand_activity_ms)<HAND_ACTIVITY_MS) return;
    hand_activity_sent=true; hand_activity_ms=t;
    if(!GameSM_PostInput(USER_ACTIVITY,t,generation)) GameButtons_Fault("PEER_INPUT_QUEUE");
}
static void event(const PeerEvent *e,void *unused) {
    (void)unused;
    if(e->kind==PEER_STATUS) {
        bool lost=synchronized && !e->synchronized;
        synchronized=e->synchronized;
        if(synchronized) {
            expected_sync=false;
            /* ResetSession clears the transport's control copy. Reassert the
             * application lamps after a Welcome reconnect as well. */
            PeerLink_SendControl(&peer,&control);
        }
        /* A single UART2 byte error resets the session; PIC2 resyncs in ~100-200 ms.
         * Give it the 1.2 s PEER_SYNC_TIMEOUT grace (GameButtons_Service) instead of
         * aborting the round on a momentary glitch; a real loss still recovers. */
        if(lost && !expected_sync && active()) {
            expected_sync=true; sync_started=clock_ms();
            printf("Link glitch during round; resyncing (abort after %u ms)\r\n",
                   (unsigned)PEER_SYNC_TIMEOUT_MS);
        }
        printf("Link: %s\r\n",synchronized?"READY":"WAIT");
        return;
    }
    if(e->kind==PEER_SENSOR_READING) { if(synchronized) hand_reading(e); return; }
    if(stopped || !synchronized || generation!=GameSM_InputGeneration() || e->kind!=PEER_INPUT) return;
    ES_EventType_t type;
    if(e->input_id==7) type=START_PRESSED;
    else if(e->input_id==5) type=FINISH_PRESSED;
    else if(e->input_id<INGREDIENT_COUNT || e->input_id==6 ||
            (e->input_id>=8 && e->input_id<=12)) type=USER_ACTIVITY;
    else return;
    printf("[MCP] %s %u\r\n",e->input_id==7?"Start":e->input_id==5?"Finish":e->input_id==6?"Flip":
           e->input_id>=8?"Ingredient OFF":"Ingredient ON",e->input_id);
    /* Gate the flip before PostInput renews idle: an expired game earns nothing. */
    bool flip=e->input_id==6 && GameSM_AcceptsGameplayInput(e->time_ms,generation);
    if(!GameSM_PostInput(type,e->time_ms,generation) ||
       (e->input_id<INGREDIENT_COUNT && !IngredientSM_PostSwitch(e->input_id,e->time_ms,generation))) {
        GameButtons_Fault("PEER_INPUT_QUEUE"); return;
    }
    /* CookSM queue overflow latches CookSM_HasFault(); BenchIO checks it. */
    if(flip && !CookSM_PostFlip(generation)) printf("CookSM flip rejected\r\n");
}
bool GameButtons_Init(uint32_t (*clock)(void)) {
    clock_ms=clock; stopped=synchronized=expected_sync=abort_posted=eink_dead=false;
    hand_valid=hand_activity_sent=false; hand_streak=0; hand_mm=0;
    fault_reason=recovered_reason=0;
    generation=sync_started=last_report=abort_generation=hand_rx_ms=hand_activity_ms=0;
    control=(PeerControl){0};
    PeerUart_Init(); PeerLink_Init(&peer,PEER_CONTROLLER,1,now,PeerUart_Send,event,0);
    /* The pan display is optional: a failed init leaves the game playable. */
    if(!ButtonEInk_Init(ButtonEInkPort_Init(clock))) {
        eink_dead=true; printf("EINK init failed; game continues without the pan display\r\n");
    }
    return true;
}
void GameButtons_Boundary(void) {
    expected_sync=true; sync_started=clock_ms();
    generation=GameSM_InputGeneration(); control.lamp=false;
    control.start_lamp=QueryGameSM()==GAME_WELCOME && !stopped;
    PeerLink_ResetSession(&peer); PeerLink_SendControl(&peer,&control);
}
void GameButtons_Lamp(bool on) {
    control.lamp=on && !stopped; PeerLink_SendControl(&peer,&control);
}
void GameButtons_State(void) {
    if(QueryGameSM()==GAME_FAULT) { GameButtons_Fault("GAME_STATE_MACHINE"); return; }
    control.start_lamp=QueryGameSM()==GAME_WELCOME && !stopped;
    PeerLink_SendControl(&peer,&control);
}
void GameButtons_ClearPan(void) { ButtonEInk_Clear(); }
void GameButtons_AddIngredient(uint8_t ingredient) { if(!stopped) ButtonEInk_Add(ingredient); }
bool GameButtons_DistanceMm(uint16_t *mm) {
    if(!mm || !hand_valid || !synchronized || (uint32_t)(clock_ms()-hand_rx_ms)>HAND_FRESH_MS ||
       hand_mm<COOK_HAND_MIN_MM || hand_mm>COOK_HAND_MAX_MM) return false;
    *mm=hand_mm; return true;
}
uint32_t GameButtons_UptimeMs(void) { return clock_ms?clock_ms():0u; }
const char *GameButtons_RecoveredReason(void) { return recovered_reason?recovered_reason:"NONE"; }
/* PIC2 audio/LCD follow CookSM; send only on change (heartbeats repeat it). */
static void forward_cook(void) {
    if(stopped) return;
    CookOutput_t c=CookSM_Output();
    PeerControl next=control;
    next.audio_enabled=c.sizzle; next.audio_zone=(uint8_t)c.zone; next.lcd_mode=(uint8_t)c.lcd_mode;
    next.burnt=c.burnt; next.burn_seq=c.burn_seq; next.target_cm=c.target_cm;
    next.stirs=c.stirs; next.flips=c.flips>63u?63u:c.flips;
    if(next.audio_enabled==control.audio_enabled && next.audio_zone==control.audio_zone &&
       next.lcd_mode==control.lcd_mode && next.burnt==control.burnt &&
       next.burn_seq==control.burn_seq && next.target_cm==control.target_cm &&
       next.stirs==control.stirs && next.flips==control.flips) return;
    control=next; PeerLink_SendControl(&peer,&control);
}
bool GameButtons_Service(void) {
    PeerUart_Service(&peer); PeerLink_Service(&peer);
    if(!eink_dead && !ButtonEInk_Service()) {
        eink_dead=true; printf("EINK fault: pan display disabled; game continues\r\n");
    }
    if(expected_sync && active() && (uint32_t)(clock_ms()-sync_started)>=PEER_SYNC_TIMEOUT_MS)
        recover("PEER_SYNC_TIMEOUT");
    forward_cook();
    if((uint32_t)(clock_ms()-last_report)>=10000u) {
        last_report=clock_ms(); GameButtons_Status();
    }
    return !stopped;
}
void GameButtons_Status(void) {
    PeerUartStats u=PeerUart_GetStats();
    ButtonEInkStatus pan=ButtonEInk_Status();
    CookOutput_t c=CookSM_Output();
    printf("Buttons: link=%s start=%s finish=%s state=%u fault=%u reason=%s recovered=%s\r\n",
        synchronized?"READY":"WAIT",control.start_lamp?"ON":"OFF",control.lamp?"ON":"OFF",
        (unsigned)QueryGameSM(),stopped,GameButtons_FaultReason(),GameButtons_RecoveredReason());
    printf("EINK desired=0x%02X shown=%s0x%02X clear=%u phase=%s\r\n",
        pan.desired,pan.known?"":"unknown/",pan.shown,pan.clearing,
        eink_dead||pan.fault?"FAULT":pan.refreshing?"REFRESH":pan.building?"BUILD":"WAIT");
    printf("Treat rotated=%u servo_pps=%s spoon=%uus joyX=%u hand=%s%umm age=%lums\r\n",
        Treat_Rotated(),Servo50Hz_Routed()?"OK":"NOT_ROUTED",(unsigned)Stirrer_PulseUs(),(unsigned)Stirrer_RawX(),hand_valid?"":"invalid/",
        (unsigned)hand_mm,(unsigned long)(clock_ms()-hand_rx_ms));
    printf("Cook lcd=%u zone=%u sizzle=%u burnt=%u seq=%u target=%ucm stirs=%u flips=%u\r\n",
        (unsigned)c.lcd_mode,(unsigned)c.zone,c.sizzle,c.burnt,(unsigned)c.burn_seq,(unsigned)c.target_cm,
        (unsigned)c.stirs,(unsigned)c.flips);
    printf("PIC1 UART2 tx=%lu rx=%lu overrun=%lu errors=%lu txfull=%lu frames=%lu bad=%lu faults=%lu\r\n",
        (unsigned long)u.tx_bytes,(unsigned long)u.rx_bytes,(unsigned long)u.overruns,
        (unsigned long)u.rx_errors,(unsigned long)u.tx_full,(unsigned long)peer.valid_frames,
        (unsigned long)peer.bad_frames,(unsigned long)peer.faults);
    PeerUartDiagnostics d=PeerUart_GetDiagnostics();
    printf("PIC1 U2CHECK loop=%s cfg=0x%02X rx17(L/H)=%lu/%lu tx21(L/H)=%lu/%lu\r\n",
        d.loopback_passed?"PASS":"FAIL",d.config_errors,
        (unsigned long)d.rx_low,(unsigned long)d.rx_high,
        (unsigned long)d.tx_low,(unsigned long)d.tx_high);
}
