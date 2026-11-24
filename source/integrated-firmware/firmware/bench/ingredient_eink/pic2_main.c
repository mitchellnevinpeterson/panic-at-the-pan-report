#include "BenchBoard.h"
#include "BenchModel.h"
#include "McpInputs.h"
static void status(bool ok,bool ack,uint8_t raw,uint8_t mask) {
    BenchBoard_Text("PIC2 MCP="); BenchBoard_Text(ok?"OK":"FAULT");
    BenchBoard_Text(" raw=0x"); BenchBoard_Hex(raw);
    BenchBoard_Text(" mask=0x"); BenchBoard_Hex(mask);
    BenchBoard_Text(" ACK="); BenchBoard_Text(ack?"OK":"WAIT/STALE");
    BenchBoard_Text(" uart_errors="); BenchBoard_Number(BenchBoard_UartErrors()); BenchBoard_Text("\r\n");
}
int main(void) {
    BenchBoard_Init();
    BenchBoard_Text("IngredientEInkPIC2 GPA0..4 active-low; 20ms debounce; s=status\r\n");
    BenchInputs inputs; BenchInputs_Init(&inputs,BenchBoard_Now());
    bool ok=McpInputs_Init(),sampled=false,have_ack=false,last_ack=false,last_ok=!ok;
    uint8_t raw=255,mask=0,sequence=0,last_mask=255,sent_mask=255;
    bool sent_valid=false;
    uint32_t last_send=0,last_status=0,retry=0,ack_at=0;
    for(;;) {
        uint32_t now=BenchBoard_Now(); BenchPacket p;
        if(BenchBoard_Poll(&p) && p.type==BENCH_ACK) { have_ack=true; ack_at=now; }
        if(ok && !McpInputs_Service(now,false)) { ok=false; sampled=false; retry=now; }
        if(!ok && (uint32_t)(now-retry)>=1000u) {
            retry=now; ok=McpInputs_Init(); BenchInputs_Init(&inputs,now); sampled=false;
        }
        if(ok && McpInputs_Take(&raw)) {
            mask=BenchInputs_Update(&inputs,raw,now); sampled=true;
        }
        bool valid=ok && sampled && BenchInputs_Ready(&inputs,now);
        if(mask!=sent_mask || valid!=sent_valid || (uint32_t)(now-last_send)>=100u) {
            last_send=now; sent_mask=mask; sent_valid=valid;
            p=(BenchPacket){BENCH_SNAPSHOT,sequence++,mask,valid}; BenchBoard_Send(&p);
        }
        bool ack=have_ack && (uint32_t)(now-ack_at)<BENCH_LINK_TIMEOUT_MS;
        int key=BenchBoard_Key();
        if(mask!=last_mask || ack!=last_ack || ok!=last_ok || key=='s' || key=='?' ||
           (uint32_t)(now-last_status)>=10000u) {
            last_status=now; last_mask=mask; last_ack=ack; last_ok=ok; status(ok,ack,raw,mask);
        }
    }
}
