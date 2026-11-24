#include "ButtonSensor.h"
#include "PeerUart.h"
#include "McpInputs.h"
#include "Range.h"
#include "HeatAudio.h"
#include "HeatView.h"
#include <xc.h>
#include <stdio.h>
#include <string.h>
#define US_TRIG (1u<<2)   /* RB2 pin6, through the U10 buffer */
#define US_ECHO (1u<<3)   /* RB3 pin7, through the divider */
#define AUDIO_PIN (1u<<15) /* RB15 pin26, OC1 */
/* HeatView_Step blocks the foreground: ~10 ms worst case at
 * LCD_HALF_BIT_TICKS=8, ~36 ms at the 40-tick fallback. Only start one while
 * the ranger is idle with this much time left before its next trigger, sized
 * for the slowest timing (36 ms + margin); the ranger idles ~100 ms per cycle. */
#define RANGE_GUARD_TICKS (48u*20000u)
static uint32_t last_tick,milliseconds;
static char debug_tx[768];
static unsigned head,tail,count;
static uint32_t now(void *unused) {
    (void)unused;
    uint32_t whole=(uint32_t)(_CP0_GET_COUNT()-last_tick)/20000u;
    last_tick+=whole*20000u; milliseconds+=whole; return milliseconds;
}
static void text(const char *s) {
    if(strlen(s)>sizeof debug_tx-count) return;
    while(*s) { debug_tx[tail]=*s++; tail=(tail+1)%sizeof debug_tx; ++count; }
}
static int debug_service(void) {
    unsigned budget=16;
    while(budget-- && count && !U1STAbits.UTXBF) {
        U1TXREG=debug_tx[head]; head=(head+1)%sizeof debug_tx; --count;
    }
    if(U1STAbits.OERR) U1STACLR=_U1STA_OERR_MASK;
    if(!U1STAbits.URXDA) return -1;
    bool bad=U1STAbits.FERR||U1STAbits.PERR;
    int c=(uint8_t)U1RXREG; return bad?-1:c;
}
/* Boot only: push queued text out before a blocking step (bounded 300 ms). */
static void debug_flush(void) {
    uint32_t start=_CP0_GET_COUNT();
    while(count && (uint32_t)(_CP0_GET_COUNT()-start)<300u*20000u) (void)debug_service();
}
/* Every PIC2 PPS assignment, in the FIRST unlock block (a later RPB15R write
 * did not take effect on this board). Later module writes are repeats. */
typedef struct { const char *name; volatile uint32_t *reg; uint32_t want; } PpsEntry;
static const PpsEntry pps[]={
    {"RPB4R(U1TX)",&RPB4R,1},  {"U1RXR(RB6)",&U1RXR,1},
    {"RPB10R(U2TX)",&RPB10R,2},{"U2RXR(RB8)",&U2RXR,4}, {"RPB8R",&RPB8R,0},
    {"RPA1R(SDO1)",&RPA1R,3},  {"SDI1R(RB11)",&SDI1R,3},{"RPA0R",&RPA0R,0},
    {"RPB15R(OC1)",&RPB15R,5},
    {"RPB5R",&RPB5R,0},{"RPB7R",&RPB7R,0},{"RPB9R",&RPB9R,0},{"RPB13R",&RPB13R,0},
    {"RPB2R",&RPB2R,0},{"RPB3R",&RPB3R,0}
};
static void pps_init(void) {
    SYSKEY=0; SYSKEY=0xaa996655; SYSKEY=0x556699aa; CFGCONbits.IOLOCK=0;
    RPB4R=1; U1RXR=1;              /* debug UART1: TX pin11 RB4, RX pin15 RB6 */
    RPB10R=2; U2RXR=4; RPB8R=0;    /* PeerLink UART2: TX pin21 RB10, RX pin17 RB8 */
    RPA1R=3; SDI1R=3; RPA0R=0;     /* MCP SPI1: SDO1 pin3 RA1, SDI1 pin22 RB11, SS GPIO RA0 */
    RPB15R=5;                      /* audio OC1 pin26 (OC1 stays off until HeatAudio_Init) */
    RPB5R=0; RPB7R=0; RPB9R=0; RPB13R=0; /* LCD soft SPI GPIO pins 14/16/18/24 */
    RPB2R=0; RPB3R=0;              /* ultrasonic TRIG pin6 / ECHO pin7 GPIO */
    CFGCONbits.IOLOCK=1; SYSKEY=0;
}
static bool pps_ok(void) {
    for(unsigned i=0;i<sizeof pps/sizeof pps[0];++i) if(*pps[i].reg!=pps[i].want) return false;
    return true;
}
static void pps_report(const char *when) {
    char item[40];
    text("PIC2 PPS "); text(when);
    for(unsigned i=0;i<sizeof pps/sizeof pps[0];++i) {
        uint32_t value=*pps[i].reg;
        snprintf(item,sizeof item," %s=%lu%s",pps[i].name,(unsigned long)value,value==pps[i].want?"":"!BAD");
        text(item);
    }
    text(pps_ok()?" -> OK\r\n":" -> MISMATCH\r\n");
}
static void quiet_pins(void) {
    DDPCONbits.JTAGEN=0;
    OC1CON=0; /* Audio output quiet; ultrasonic trigger LOW. */
    ANSELBCLR=US_TRIG|US_ECHO|AUDIO_PIN|(1u<<4)|(1u<<8)|(1u<<10);
    LATBCLR=US_TRIG|AUDIO_PIN; LATBSET=(1u<<4)|(1u<<10); /* UART TX idle HIGH */
    TRISBCLR=US_TRIG|AUDIO_PIN|(1u<<4)|(1u<<10); TRISBSET=US_ECHO|(1u<<6)|(1u<<8);
    ANSELACLR=1u; LATASET=1u; TRISACLR=1u; /* MCP SS HIGH */
    /* Keep the already wired LCD deselected. No LCD clocks or reset pulses. */
    ANSELBCLR=1u<<9; ODCBCLR=1u<<9; LATBSET=1u<<9; TRISBCLR=1u<<9;
}
static void debug_uart_init(void) {
    U1MODE=0; U1STA=0; U1BRG=42; U1MODEbits.BRGH=1;
    U1STAbits.UTXEN=1; U1STAbits.URXEN=1; U1MODEbits.ON=1;
}
static const char *lcd_name(HeatViewMode mode) {
    return mode==HEATVIEW_ZONE?"ZONE":mode==HEATVIEW_WAITING?"WAITING":"ATTRACT";
}
int main(void) {
    __builtin_disable_interrupts(); last_tick=_CP0_GET_COUNT();
    quiet_pins(); debug_uart_init(); pps_init();
    text("\r\nGameButtonsPIC2: Start GPA7, Finish GPA5, flip GPA6; Start LED GPB1, Finish LED GPB0;\r\n"
         "ranger TRIG6/ECHO7, LCD 14/16/18/23/24, speaker 26; s=status; 115200 baud\r\n");
    pps_report("boot");
    bool ok=McpInputs_InitOutputs(3u); /* Clear retained lamp latches first. */
    text(ok?"PIC2 MCP lamps cleared\r\nPIC2 drawing LCD layout...\r\n":"PIC2 MCP FAULT at boot\r\nPIC2 drawing LCD layout...\r\n");
    debug_flush();
    bool lcd=HeatView_Start(); /* Blocking, before audio and the link start. */
    text(lcd?"PIC2 LCD layout drawn\r\n":"PIC2 LCD FAULT (link/audio continue)\r\n");
    HeatAudio_Init(); /* Enables interrupts. */
    PeerUart_Init(); ButtonSensor sensor;
    ButtonSensor_Init(&sensor,now,PeerUart_Send,0);
    Pic2Range range; Pic2Range_Init(&range,_CP0_GET_COUNT());
    /* Same table after every module init: catches a later write that changed
     * a value (PeerUart's loopback detaches RPB10R and restores it). */
    pps_report("init");
    __builtin_enable_interrupts();
    bool old_ok=!ok,old_sync=false,old_lamp=false,old_start=false,seq_latched=false;
    HeatViewMode old_mode=HEATVIEW_ATTRACT;
    uint8_t last_seq=0,cue_seq=0;
    uint16_t local_mm=0; bool range_valid=false;
    uint32_t retry=0,last_status=0,range_samples=0,lcd_steps=0;
    for(;;) {
        uint32_t t=now(0); uint8_t raw,edges=0;
        Pic2Range_Poll(&range,_CP0_GET_COUNT(),(PORTB&US_ECHO)!=0);
        if(range.trigger) LATBSET=US_TRIG; else LATBCLR=US_TRIG;
        if(range.sample_ready) {
            range.sample_ready=false; ++range_samples;
            range_valid=range.valid; local_mm=range.valid?range.mm:0;
            (void)ButtonSensor_SendRange(&sensor,range.mm,range.valid,t);
        }
        if(ok) PeerUart_Service(&sensor.link);
        PeerLink_Service(&sensor.link);
        if(ok && !McpInputs_ServiceOutputs(t,ButtonSensor_Outputs(&sensor))) {
            ok=false; retry=t; PeerLink_Fault(&sensor.link);
            /* Best effort immediate LOW; disconnected SPI cannot guarantee a
             * latched MCP output. Health failure stays unavailable for 1 s. */
            (void)McpInputs_InitOutputs(3u);
        }
        if(!ok && (uint32_t)(t-retry)>=1000u) {
            retry=t; ok=McpInputs_InitOutputs(3u); PeerLink_Fault(&sensor.link);
        }
        if(ok && McpInputs_Take(&raw)) edges=ButtonSensor_Sample(&sensor,raw);
        bool sync=PeerLink_IsSynchronized(&sensor.link),lamp=ok&&ButtonSensor_Lamp(&sensor);
        bool start_lamp=ok&&ButtonSensor_StartLamp(&sensor);
        /* Controller state; all zero unless synchronized. A burn_seq change
         * while synchronized is a burn; the first value after (re)sync only
         * latches, so a PIC1 reboot never plays a stale burnt cue. */
        PeerControl c=ButtonSensor_Control(&sensor);
        if(!sync) seq_latched=false;
        else if(!seq_latched) { last_seq=c.burn_seq; seq_latched=true; }
        else if(c.burn_seq!=last_seq) { last_seq=c.burn_seq; ++cue_seq; }
        HeatAudio_Set(sync&&c.audio_enabled,c.audio_zone,cue_seq); /* Music always plays. */
        HeatViewMode mode=sync&&c.lcd_mode<=HEATVIEW_ZONE?(HeatViewMode)c.lcd_mode:HEATVIEW_ATTRACT;
        HeatView_Set(mode,c.audio_zone,c.burnt,c.target_cm,local_mm,t);
        HeatView_SetCounts(c.stirs,c.flips); /* bottom line: this ingredient's stirs/flips */
        /* One bounded LCD step per pass, only when nothing time-critical is
         * pending: ranger idle well before its next trigger, no MCP transfer
         * in flight, and no queued link TX (RX is interrupt-driven). */
        uint32_t ticks=_CP0_GET_COUNT();
        if(range.state==0 && (int32_t)(range.next-ticks)>=(int32_t)RANGE_GUARD_TICKS &&
           !McpInputs_Busy() && PeerUart_TxIdle()) {
            if(HeatView_Step()) ++lcd_steps;
        }
        int key=debug_service();
        if(ok!=old_ok || sync!=old_sync || lamp!=old_lamp || start_lamp!=old_start || edges || key=='s' || key=='?' ||
           mode!=old_mode || (uint32_t)(t-last_status)>=10000u) {
            char line[160];
            snprintf(line,sizeof line,"PIC2 MCP=%s link=%s raw=0x%02X edges=0x%02X start=%s finish=%s faults=%lu\r\n",
                ok?"OK":"FAULT",sync?"READY":"WAIT",sensor.raw,edges,start_lamp?"ON":"OFF",lamp?"ON":"OFF",(unsigned long)sensor.link.faults);
            text(line); old_ok=ok; old_sync=sync; old_lamp=lamp; old_start=start_lamp; old_mode=mode; last_status=t;
            PeerUartStats u=PeerUart_GetStats();
            char diagnostics[224];
            snprintf(diagnostics,sizeof diagnostics,
                "PIC2 UART2 tx=%lu rx=%lu overrun=%lu errors=%lu txfull=%lu frames=%lu bad=%lu\r\n",
                (unsigned long)u.tx_bytes,(unsigned long)u.rx_bytes,(unsigned long)u.overruns,
                (unsigned long)u.rx_errors,(unsigned long)u.tx_full,
                (unsigned long)sensor.link.valid_frames,(unsigned long)sensor.link.bad_frames);
            text(diagnostics);
            PeerUartDiagnostics d=PeerUart_GetDiagnostics();
            snprintf(diagnostics,sizeof diagnostics,
                "PIC2 U2CHECK loop=%s cfg=0x%02X rx17(L/H)=%lu/%lu tx21(L/H)=%lu/%lu\r\n",
                d.loopback_passed?"PASS":"FAIL",d.config_errors,
                (unsigned long)d.rx_low,(unsigned long)d.rx_high,
                (unsigned long)d.tx_low,(unsigned long)d.tx_high);
            text(diagnostics);
            snprintf(diagnostics,sizeof diagnostics,
                "PIC2 audio=%s RPB15R=%lu sizzle=%s zone=%u burns=%u range=%umm%s n=%lu lcd=%s/%s burnt=%s target=%ucm stirs=%u flips=%u steps=%lu pps=%s\r\n",
                HeatAudio_Healthy()?"OK":"FAULT",(unsigned long)RPB15R,sync&&c.audio_enabled?"ON":"OFF",
                c.audio_zone,cue_seq,local_mm,range_valid?"":"(none)",(unsigned long)range_samples,
                lcd?"OK":"FAULT",lcd_name(mode),c.burnt?"YES":"NO",c.target_cm,(unsigned)c.stirs,(unsigned)c.flips,(unsigned long)lcd_steps,
                pps_ok()?"OK":"MISMATCH");
            text(diagnostics);
        }
    }
}
