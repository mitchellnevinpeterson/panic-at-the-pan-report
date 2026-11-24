#include "BenchBoard.h"
#include "BenchModel.h"
#include "BenchCanvas.h"
static BenchDisplay display;
static EInkFrame frame;
static bool building,refreshing,fault;
static void status(uint32_t now) {
    BenchBoard_Text("PIC1 link=");
    BenchBoard_Text(BenchDisplay_Fresh(&display,now)?"OK":"STALE/INVALID");
    BenchBoard_Text(" live=0x"); BenchBoard_Hex(display.live);
    BenchBoard_Text(" added=0x"); BenchBoard_Hex(display.desired);
    BenchBoard_Text(" selected=0x"); BenchBoard_Hex(display.selected);
    BenchBoard_Text(" shown=");
    if(display.known) BenchBoard_Hex(display.displayed); else BenchBoard_Text("unknown");
    BenchBoard_Text(" phase="); BenchBoard_Text(fault?"FAULT":refreshing?"REFRESH":building?"BUILD":"WAIT");
    BenchBoard_Text(" wait_s="); BenchBoard_Number((BenchDisplay_Wait(&display,now)+999)/1000);
    BenchBoard_Text(" uart_errors="); BenchBoard_Number(BenchBoard_UartErrors()); BenchBoard_Text("\r\n");
}
int main(void) {
    BenchBoard_Init(); BenchDisplay_Init(&display,BenchBoard_Now());
    fault=!EInkAsync_InitWithCooldown(BenchBoard_EInkPort(),BENCH_COOLDOWN_MS);
    BenchBoard_Text("IngredientEInkPIC1 IL0373 ON adds/OFF keeps; boot wait ");
    BenchBoard_Number(BENCH_COOLDOWN_MS/1000u);
    BenchBoard_Text("s; s=status c=clear\r\n");
    uint32_t last_status=0; uint8_t last_mask=255,last_live=255; bool last_fresh=false;
    for(;;) {
        uint32_t now=BenchBoard_Now(); BenchPacket p;
        if(BenchBoard_Poll(&p) && p.type==BENCH_SNAPSHOT) {
            BenchDisplay_Receive(&display,p.mask,p.valid,now);
            p.type=BENCH_ACK; BenchBoard_Send(&p);
        }
        int key=BenchBoard_Key();
        if(key=='c') {
            BenchDisplay_Clear(&display);
            BenchBoard_Text("CLEAR queued; held switches need OFF then ON; cooldown unchanged\r\n");
        }
        if(!fault && !EInkAsync_Service()) {
            fault=true;
            BenchBoard_Text("EINK FAULT: "); BenchBoard_Text(EInk_ResultName(EInkAsync_Result()));
            BenchBoard_Text("; correct fault before resetting\r\n");
        }
        if(!fault && refreshing && !EInkAsync_IsBusy()) {
            refreshing=false; BenchDisplay_Finish(&display,now);
            BenchBoard_Text("EINK DONE mask=0x"); BenchBoard_Hex(display.displayed);
            BenchBoard_Text("; cooldown "); BenchBoard_Number(BENCH_COOLDOWN_MS/1000u);
            BenchBoard_Text("s; visually verify panel\r\n");
        }
        if(!fault && building) {
            /* Cancel a stale/superseded build before sending any panel command. */
            if(!BenchDisplay_Current(&display,now)) { building=false; display.busy=false; }
            else if(BenchCanvas_Service()) {
                building=false;
                if(EInkAsync_Start(&frame)) {
                    refreshing=true;
                    BenchBoard_Text("EINK START mask=0x"); BenchBoard_Hex(display.selected); BenchBoard_Text("\r\n");
                } else { display.busy=false; }
            }
        }
        if(!fault && !building && !refreshing && EInkAsync_CanStart() && BenchDisplay_Select(&display,now)) {
            BenchCanvas_Start(&frame,display.selected); building=true;
        }
        bool fresh=BenchDisplay_Fresh(&display,now);
        if(display.desired!=last_mask || display.live!=last_live || fresh!=last_fresh || key=='s' || key=='?' || key=='c' ||
           (uint32_t)(now-last_status)>=10000u) {
            last_status=now; last_mask=display.desired; last_live=display.live; last_fresh=fresh; status(now);
        }
    }
}
