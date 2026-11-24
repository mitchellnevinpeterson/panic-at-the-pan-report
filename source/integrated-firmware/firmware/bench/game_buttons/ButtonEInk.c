/* Foreground scheduler: coalesce ingredient additions, preserve an in-flight
 * frame, and always finish a requested white frame before drawing a new round.
 */
#include "ButtonEInk.h"
#include "BenchCanvas.h"
#include <stdio.h>
static EInkFrame frame;
static ButtonEInkStatus state;
static bool fault;
static uint32_t (*clock_ms)(void);
static uint32_t last_add;
bool ButtonEInk_Init(const EInkAsyncPort *port) {
    state=(ButtonEInkStatus){.clearing=true};
    /* No cooldown (user choice). The driver rejects 0 by design; 1 ms is none in practice. */
    /* Fast black/white waveform (user choice): ~2 s panel refresh, red drawn black. */
    fault=!EInkAsync_InitFastMono(port,1u);
    clock_ms=fault?0:port->now_ms;
    if(clock_ms) last_add=clock_ms();
    return !fault;
}
void ButtonEInk_Add(uint8_t ingredient) {
    uint8_t bit=(uint8_t)(1u<<ingredient);
    if(ingredient>=5 || (state.desired&bit)) return;
    state.desired|=bit;
    if(clock_ms) last_add=clock_ms();
}
void ButtonEInk_Clear(void) { state.desired=0; state.clearing=true; }
ButtonEInkStatus ButtonEInk_Status(void) {
    ButtonEInkStatus report=state;
    report.fault=fault;
    if(fault) report.building=report.refreshing=false;
    return report;
}
bool ButtonEInk_Service(void) {
    if(fault) return false;
    if(!EInkAsync_Service()) {
        fault=true;
        printf("EINK FAULT result=%u; check wiring before resetting\r\n",(unsigned)EInkAsync_Result());
        return false;
    }
    if(state.refreshing) {
        if(EInkAsync_IsBusy()) return true;
        state.refreshing=false; state.known=true; state.shown=state.selected;
        if(state.shown==0) state.clearing=false;
        printf("EINK DONE mask=0x%02X\r\n",state.shown);
    }
    /* Already white and no transfer active: do not refresh a blank screen
     * again just because Welcome/Intro reasserted Reset. Cancel old builds. */
    if(state.clearing && state.known && state.shown==0) {
        state.clearing=false; state.building=false;
    }
    uint8_t target=state.clearing?0:state.desired;
    if(state.building && state.selected!=target) state.building=false;
    if(!state.building) {
        if(state.known && target==state.shown) return true;
        if(!EInkAsync_CanStart()) return true;
        if(target && (uint32_t)(clock_ms()-last_add)<BUTTON_EINK_BATCH_MS) return true;
        state.selected=target; BenchCanvas_Start(&frame,target); state.building=true;
    }
    if(BenchCanvas_Service()) {
        if(!EInkAsync_Start(&frame)) { fault=true; return false; }
        state.building=false; state.refreshing=true;
        printf("EINK START mask=0x%02X\r\n",state.selected);
    }
    return true;
}
