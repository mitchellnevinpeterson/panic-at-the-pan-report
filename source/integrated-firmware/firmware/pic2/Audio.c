/* Adapted from teammates' PIC32-Current-Code/src/heat_audio.c.
   Reuses their unchanged procedural sizzle engine; PIC1 owns heat policy. */
#include "Audio.h"
#include "sizzle.h"
#include <xc.h>
#include <sys/attribs.h>
static volatile uint8_t zone;
void Pic2Audio_Set(bool enabled,uint8_t value) { zone=enabled&&value<=3?value:0; }
void Pic2Audio_Init(void) {
    unsigned irq=__builtin_disable_interrupts(); zone=0;
    ANSELBCLR=1u<<15; LATBCLR=1u<<15; TRISBCLR=1u<<15;
    SYSKEY=0; SYSKEY=0xaa996655; SYSKEY=0x556699aa; CFGCONbits.IOLOCK=0;
    RPB15R=5; CFGCONbits.IOLOCK=1; SYSKEY=0;
    T2CON=0; TMR2=0; PR2=255; OC1CON=0; OC1R=128; OC1RS=128;
    OC1CONbits.OCM=6; T2CONbits.ON=1; OC1CONbits.ON=1;
    T3CON=0; TMR3=0; PR3=1599; IPC3bits.T3IP=2; IPC3bits.T3IS=0;
    IFS0CLR=_IFS0_T3IF_MASK; IEC0SET=_IEC0_T3IE_MASK;
    INTCONSET=_INTCON_MVEC_MASK; T3CONbits.ON=1;
    if(irq&1) __builtin_enable_interrupts();
}
void __ISR(_TIMER_3_VECTOR,IPL2SOFT) Pic2Audio_Interrupt(void) {
    static SizzleState state=SIZZLE_INITIAL_STATE; uint8_t current=zone;
    /* Hard mute, including residual oil-drop tails; no startup cue. */
    OC1RS=current?(unsigned)(128+sizzle_sample(&state,(HeatZone)current)):128u;
    IFS0CLR=_IFS0_T3IF_MASK;
}
