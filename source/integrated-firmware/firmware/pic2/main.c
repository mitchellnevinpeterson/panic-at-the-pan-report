#include "PeerLink.h"
#include "PeerUart.h"
#include "Inputs.h"
#include "Range.h"
#include "Audio.h"
#include "McpInputs.h"
#include <xc.h>
static PeerLink link;
static Pic2Inputs inputs;
static Pic2Range range;
static bool lamp,mcp_ok;
static uint8_t last_gpio=0xff;
static uint32_t milliseconds,previous_ticks,remainder;
static uint32_t clock_ms(void *unused) {
    uint32_t ticks=_CP0_GET_COUNT(),delta=ticks-previous_ticks;
    (void)unused; previous_ticks=ticks;
    milliseconds+=delta/20000u; remainder+=delta%20000u;
    milliseconds+=remainder/20000u; remainder%=20000u; return milliseconds;
}
static void event(const PeerEvent *e,void *unused) {
    (void)unused;
    if(e->kind==PEER_STATUS) {
        Pic2Inputs_Reset(&inputs,last_gpio,clock_ms(0));
        if(!e->synchronized) { lamp=false; Pic2Audio_Set(false,0); }
    } else if(e->kind==PEER_CONTROL) {
        lamp=mcp_ok&&e->control.lamp;
        Pic2Audio_Set(mcp_ok&&e->control.audio_enabled,e->control.audio_zone);
    }
}
int main(void) {
    uint32_t retry=0;
    __builtin_disable_interrupts(); DDPCONbits.JTAGEN=0;
    ANSELBCLR=(1u<<2)|(1u<<3); LATBCLR=1u<<2; TRISBCLR=1u<<2; TRISBSET=1u<<3;
    previous_ticks=_CP0_GET_COUNT(); Pic2Inputs_Reset(&inputs,0xff,0);
    Pic2Range_Init(&range,previous_ticks); PeerUart_Init(); Pic2Audio_Init();
    PeerLink_Init(&link,PEER_SENSOR,1,clock_ms,PeerUart_Send,event,0);
    mcp_ok=McpInputs_Init(); __builtin_enable_interrupts();
    for(;;) {
        uint32_t now=clock_ms(0);
        Pic2Range_Poll(&range,_CP0_GET_COUNT(),(PORTB&(1u<<3))!=0);
        if(range.trigger) LATBSET=1u<<2; else LATBCLR=1u<<2;
        /* No health acknowledgement while the input expander is unavailable. */
        if(mcp_ok) PeerUart_Service(&link);
        PeerLink_Service(&link);
        if(!mcp_ok) {
            lamp=false; Pic2Audio_Set(false,0);
            if((uint32_t)(now-retry)>=1000) {
                retry=now; mcp_ok=McpInputs_Init(); PeerLink_Fault(&link);
                Pic2Inputs_Reset(&inputs,last_gpio,now);
            }
        } else if(!McpInputs_Service(now,lamp&&PeerLink_IsSynchronized(&link))) {
            mcp_ok=false; retry=now; PeerLink_Fault(&link);
        }
        if(mcp_ok&&McpInputs_Take(&last_gpio)) {
            uint8_t edges=Pic2Inputs_Update(&inputs,last_gpio,now,PeerLink_IsSynchronized(&link));
            unsigned i; for(i=0;i<8;i++) if(edges&(1u<<i)) PeerLink_SendInput(&link,(uint8_t)i,inputs.changed[i]);
        }
        if(range.sample_ready) {
            range.sample_ready=false;
            PeerLink_SendSensor(&link,range.mm,range.valid,now);
        }
    }
}
