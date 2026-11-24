#include "BenchIO.h"
#include "GameButtons.h"
#include "ES_Framework.h"
#include "terminal.h"
#include <stdio.h>
#include <xc.h>
#include <cp0defs.h>
#include "Servo50Hz.h"
volatile ES_Return_t FirmwareStatus=Success;
volatile unsigned int ButtonBenchBootStage;
volatile uint32_t ButtonBenchExceptionCause,ButtonBenchExceptionEPC;
/* Survives a software reset (not cleared by startup), so the next boot can
 * report why it reset and a persistent fault cannot loop silently. */
#define RESET_NOTE_MAGIC 0x50414E21u
#define QUICK_FAULT_MS 10000u   /* a fault this soon after boot counts as repeating */
#define MAX_QUICK_RESETS 3u     /* then halt and keep printing the reason */
typedef struct { uint32_t magic, quick; char reason[32]; } ResetNote;
static ResetNote reset_note __attribute__((persistent));

/* Startup/exception diagnostics bypass the course printf queue. Bound each
 * wait so a failed UART cannot itself block initialization forever. */
static void boot_text(const char *text) {
    if(!U1MODEbits.ON || !U1STAbits.UTXEN) return;
    while(*text) {
        unsigned int budget=100000u;
        while(U1STAbits.UTXBF) if(--budget==0) return;
        U1TXREG=(unsigned char)*text++;
    }
}
static void boot_hex(uint32_t value) {
    static const char digits[]="0123456789ABCDEF";
    char text[9];
    for(unsigned int i=0;i<8;++i) text[i]=digits[(value>>(28u-4u*i))&15u];
    text[8]=0; boot_text(text);
}
static void __attribute__((noreturn)) software_reset(void) {
    __builtin_disable_interrupts();
    SYSKEY=0; SYSKEY=0xAA996655u; SYSKEY=0x556699AAu;
    RSWRSTSET=_RSWRST_SWRST_MASK;
    (void)RSWRST; /* The read triggers the reset. */
    for(;;) { }
}
/* Unrecoverable: the reason is already printed; let the debug UART drain
 * (~300 ms), remember it, and reset. Repeated quick faults halt instead. */
static void __attribute__((noreturn)) fatal_reset(const char *reason,uint32_t uptime_ms,bool printf_queue) {
    unsigned int i;
    if(reset_note.magic!=RESET_NOTE_MAGIC) reset_note.quick=0;
    reset_note.quick=uptime_ms<QUICK_FAULT_MS?reset_note.quick+1u:0u;
    for(i=0;i<sizeof reset_note.reason-1u && reason[i];++i) reset_note.reason[i]=reason[i];
    reset_note.reason[i]=0; reset_note.magic=RESET_NOTE_MAGIC;
    bool halt=reset_note.quick>=MAX_QUICK_RESETS;
    for(;;) {
        uint32_t start=_CP0_GET_COUNT();
        while((uint32_t)(_CP0_GET_COUNT()-start)<6000000u) /* 300 ms at 20 MHz */
            if(printf_queue) Terminal_MoveBuffer2UART();
        if(!halt) software_reset();
        /* Repeating fault: stay stopped (PIC2 times out the link, lamps OFF). */
        boot_text("\r\n[HALT] repeated fault: "); boot_text(reset_note.reason);
        boot_text(". Fix it, then power-cycle.\r\n");
        for(i=0;i<16u;++i) { start=_CP0_GET_COUNT(); while((uint32_t)(_CP0_GET_COUNT()-start)<6000000u) { } }
    }
}
static uint32_t boot_count __attribute__((persistent)); /* cleared at power-on only */
static void report_previous_reset(void) {
    uint32_t rcon=RCON;
    bool software=(rcon&_RCON_SWR_MASK)!=0;
    /* Tells a firmware crash (SOFTWARE/WATCHDOG) from a supply dip (BROWN-OUT)
     * or the reset button (MCLR); the boot number exposes reset loops. */
    if(rcon&_RCON_POR_MASK) boot_count=0;
    ++boot_count;
    boot_text("[BOOT] #"); boot_hex(boot_count); boot_text(" since power-on, reset cause RCON=0x");
    boot_hex(rcon); boot_text(" -> ");
    if(rcon&_RCON_POR_MASK) boot_text("POWER-ON");
    else if(rcon&_RCON_BOR_MASK) boot_text("BROWN-OUT (supply dipped: servo/5V/3.3V power)");
    else if(software) boot_text("SOFTWARE (firmware fatal, see reason below)");
    else if(rcon&_RCON_WDTO_MASK) boot_text("WATCHDOG");
    else if(rcon&_RCON_EXTR_MASK) boot_text("MCLR (reset button / SNAP)");
    else boot_text("OTHER");
    boot_text("\r\n");
    RCONCLR=_RCON_SWR_MASK|_RCON_POR_MASK|_RCON_BOR_MASK|_RCON_EXTR_MASK|_RCON_WDTO_MASK;
    if(!software || reset_note.magic!=RESET_NOTE_MAGIC) {
        reset_note.magic=0; reset_note.quick=0; return;
    }
    reset_note.reason[sizeof reset_note.reason-1u]=0;
    boot_text("[BOOT] recovered by software reset after: ");
    boot_text(reset_note.reason); boot_text("\r\n");
}
/* XC32's documented general-exception override; retain evidence for SNAP
 * and print without printf, allocation, or framework services. */
void __attribute__((nomips16,noreturn)) _general_exception_handler(void) {
    ButtonBenchExceptionCause=_CP0_GET_CAUSE();
    ButtonBenchExceptionEPC=_CP0_GET_EPC();
    __builtin_disable_interrupts();
    boot_text("\r\n[EXCEPTION] stage=0x"); boot_hex(ButtonBenchBootStage);
    boot_text(" cause=0x"); boot_hex(ButtonBenchExceptionCause);
    boot_text(" epc=0x"); boot_hex(ButtonBenchExceptionEPC); boot_text("\r\n");
    /* No heartbeat meanwhile: PIC2's link timeout requests lamps OFF. */
    /* Real uptime once the run loop started, so a deterministic early exception
     * trips the 3-quick-faults halt instead of resetting forever. */
    fatal_reset("EXCEPTION",ButtonBenchBootStage==6?GameButtons_UptimeMs():0u,false);
}
int main(void) {
    /* Route the servo outputs before ANY PPS lock. On this hardware a later OCx
     * mapping did not take (status showed servo_pps=NOT_ROUTED, like PIC2's
     * audio pin), while mappings made before the first lock always work. PPS
     * starts unlocked after every reset, and the framework's UART1 routing
     * relies on that, so do NOT lock here. OC2/OC4 are still off: both pins
     * stay LOW until Servo50Hz_Init configures 50 Hz PWM. */
    LATACLR=1u<<4; TRISACLR=1u<<4;   /* RA4 pin12 spoon */
    LATBCLR=1u<<5; TRISBCLR=1u<<5;   /* RB5 pin14 treat */
    RPA4R=5; RPB5R=5;                /* OC4, OC2 (Table 11-2) */
    ButtonBenchBootStage=1;
    _HW_PIC32Init();
    /* Optional USB-UART TX can be unplugged. Keep RX15/RB6 at idle HIGH. */
    CNPDBCLR=1u<<6; CNPUBSET=1u<<6;
    ButtonBenchBootStage=2;
    boot_text("\r\n[BOOT] GameButtonsPIC1 UART1 ready: TX16 RX15 115200\r\n");
    report_previous_reset();
    printf("\r\nGameButtonsPIC1: physical Start/Finish; a=activity, 1-5=ingredients, s=status\r\n"
        "Wait for Link READY / Start lamp ON, release buttons, then press Start. b/f disabled.\r\n"
        "Dish scroll -> 60s game; Finish lamp after 30s of cooking. Idle timeout 20s -> Welcome.\r\n"
        "End: message, big score 3 s (treat if >= 30), Welcome. Treat keys: o c + -\r\n"
        "Link loss returns to Welcome; unrecoverable faults print the reason and reset.\r\n");
    ButtonBenchBootStage=3;
    boot_text("[BOOT] Initializing game IO\r\n");
    bool io_ready=GameBenchIO_Init();
    ButtonBenchBootStage=4;
    boot_text(io_ready?"[BOOT] Game IO ready; initializing ES\r\n":"[BOOT] Game IO FAILED\r\n");
    boot_text(Servo50Hz_Routed()?"[BOOT] Servo PPS routed (RA4 spoon, RB5 treat)\r\n":
                                 "[BOOT] Servo PPS NOT routed\r\n");
    if(!io_ready) GameButtons_Fault("GAME_IO_INIT");
    FirmwareStatus=io_ready?ES_Initialize(ES_Timer_RATE_1mS):FailedInit;
    ButtonBenchBootStage=5;
    if(FirmwareStatus==Success) {
        boot_text("[BOOT] ES ready; entering run loop\r\n");
        ButtonBenchBootStage=6;
        FirmwareStatus=ES_Run();
    } else {
        boot_text("[BOOT] ES initialization FAILED\r\n");
        GameButtons_Fault("ES_INIT");
    }
    ButtonBenchBootStage=7;
    GameButtons_Fault("ES_RUN_RETURNED"); /* Keeps the first recorded reason. */
    const char *reason=GameButtons_FaultReason();
    uint32_t uptime=GameButtons_UptimeMs();
    printf("\r\nFATAL: %s (status %d) at %lu ms. Software reset in 300 ms.\r\n",
           reason,(int)FirmwareStatus,(unsigned long)uptime);
    fatal_reset(reason,uptime,true);
}
