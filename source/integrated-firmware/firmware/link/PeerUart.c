#include "PeerUart.h"
#include <xc.h>
#include <cp0defs.h>
#ifdef PEER_UART_ISR
#include <sys/attribs.h>
#endif
#define CAPACITY 256u
static uint8_t tx[CAPACITY];
static unsigned head,tail,count;
static PeerUartStats stats;
#ifdef PEER_UART_ISR
/* PIC2 only. The RX ISR (IPL4, above the IPL2 audio ISR) owns the ring head
 * and these counters; the foreground only reads them (32-bit reads are atomic)
 * and acts on changes, so no interrupt masking is needed. TX stays polled. */
static PeerRxRing rx_ring;
static volatile uint32_t isr_rx_bytes,isr_errors,isr_overruns;
static uint32_t seen_errors,seen_overruns;
PeerUartStats PeerUart_GetStats(void) {
    PeerUartStats result=stats;
    result.rx_bytes=isr_rx_bytes; result.rx_errors=isr_errors; result.overruns=isr_overruns;
    return result;
}
void __ISR(_UART_2_VECTOR, IPL4SOFT) PeerUart_RxInterrupt(void) {
    IFS1CLR=_IFS1_U2RXIF_MASK; /* Clear first: a byte arriving mid-drain re-raises it. */
    while(U2STAbits.URXDA) {
        bool bad=U2STAbits.FERR||U2STAbits.PERR; uint8_t b=(uint8_t)U2RXREG;
        ++isr_rx_bytes;
        if(bad) ++isr_errors; /* Byte dropped; Service faults the link. */
        else if(!PeerRxRing_Push(&rx_ring,b)) ++isr_overruns; /* Ring full. */
    }
    /* FIFO bytes were drained above; clearing OERR restarts the receiver. */
    if(U2STAbits.OERR) { U2STACLR=_U2STA_OERR_MASK; ++isr_overruns; }
}
#else
PeerUartStats PeerUart_GetStats(void) { return stats; }
#endif
bool PeerUart_TxIdle(void) { return count==0; }
#ifdef GAME_BUTTONS_BENCH
static PeerUartDiagnostics diagnostics;
/* Boot only. TX PPS is detached first, leaving physical TX21 idle HIGH.
 * Test bytes must never enter the peer protocol. This checks the UART engine,
 * not either external pad or the wire. See Microchip DS60001107H, 21.11.1. */
static bool startup_loopback(void) {
    static const uint8_t pattern[]={0x55,0xaa,0x00,0xff};
    if(RPB10R!=0) return false;
    U2MODESET=_U2MODE_LPBACK_MASK; /* Set only after enabling the UART. */
    for(unsigned i=0;i<sizeof pattern;++i) {
        uint32_t started=_CP0_GET_COUNT();
        U2TXREG=pattern[i]; /* Previous byte was fully received, FIFO is empty. */
        unsigned spins=0;
        while(!U2STAbits.URXDA) {
            if((uint32_t)(_CP0_GET_COUNT()-started)>=20000u || ++spins>=20000u)
                return false; /* At most 1 ms per byte at the configured clock. */
        }
        bool bad=U2STAbits.FERR || U2STAbits.PERR || U2STAbits.OERR;
        if((uint8_t)U2RXREG!=pattern[i] || bad) return false;
    }
    return true;
}
PeerUartDiagnostics PeerUart_GetDiagnostics(void) {
    PeerUartDiagnostics result=diagnostics;
    const unsigned pins=(1u<<8)|(1u<<10);
    const unsigned enables=_U2STA_UTXEN_MASK|_U2STA_URXEN_MASK;
    const unsigned modes=enables|_U2STA_ADDEN_MASK|_U2STA_UTXINV_MASK|
                         _U2STA_UTXBRK_MASK|_U2STA_ADM_EN_MASK;
    if(PMD5bits.U2MD || U2MODE!=(_U2MODE_ON_MASK|_U2MODE_BRGH_MASK) ||
       U2BRG!=42 || (U2STA&modes)!=enables) result.config_errors|=1u;
    if(U2RXR!=4 || RPB8R!=0) result.config_errors|=2u;
    if(RPB10R!=2) result.config_errors|=4u;
    if((ANSELB&pins) || (TRISB&pins)!=(1u<<8) || (ODCB&pins)) result.config_errors|=8u;
    if(DDPCONbits.JTAGEN || I2C1CONbits.ON) result.config_errors|=16u;
    if(OSCCONbits.PBDIV!=1) result.config_errors|=32u;
    return result;
}
#endif
void PeerUart_Init(void) {
    unsigned irq=__builtin_disable_interrupts();
#ifdef PEER_UART_ISR
    IEC1CLR=_IEC1_U2RXIE_MASK; /* Loopback self-test below polls the FIFO. */
#endif
    /* Match the known-working ingredient bench instead of relying on reset
     * defaults for the module clock and push-pull output. */
    PMD5bits.U2MD=0; U2MODE=0; U2STA=0;
    DDPCONbits.JTAGEN=0;
    ANSELBCLR=(1u<<8)|(1u<<10); LATBSET=1u<<10;
    ODCBCLR=(1u<<8)|(1u<<10);
    TRISBCLR=1u<<10; TRISBSET=1u<<8;
    SYSKEY=0; SYSKEY=0xaa996655; SYSKEY=0x556699aa; CFGCONbits.IOLOCK=0;
    RPB8R=0; RPB10R=0; U2RXR=4;
    U2MODEbits.BRGH=1; U2BRG=42; /* 116279 baud, +0.94%, PBCLK20MHz */
    U2STAbits.UTXEN=1; U2STAbits.URXEN=1; U2MODEbits.ON=1;
#ifdef GAME_BUTTONS_BENCH
    diagnostics=(PeerUartDiagnostics){0};
    diagnostics.loopback_passed=startup_loopback();
    /* On success OR timeout, discard test bytes and leave loopback mode. */
    U2MODE=0; U2STA=0; U2MODEbits.BRGH=1; U2BRG=42;
#endif
    RPB10R=2;
    CFGCONbits.IOLOCK=1; SYSKEY=0;
    U2STAbits.UTXEN=1; U2STAbits.URXEN=1; U2MODEbits.ON=1;
    head=tail=count=0;
    stats=(PeerUartStats){0};
#ifdef PEER_UART_ISR
    rx_ring.head=rx_ring.tail=0;
    isr_rx_bytes=isr_errors=isr_overruns=seen_errors=seen_overruns=0;
    U2STAbits.URXISEL=0; /* Interrupt while any byte is waiting. */
    IPC9bits.U2IP=4; IPC9bits.U2IS=0;
    IFS1CLR=_IFS1_U2RXIF_MASK; IEC1SET=_IEC1_U2RXIE_MASK;
    INTCONSET=_INTCON_MVEC_MASK;
#endif
    if(irq&1) __builtin_enable_interrupts();
}
bool PeerUart_Send(const uint8_t *bytes,size_t length,void *unused) {
    size_t i; (void)unused;
    if(length>CAPACITY-count) { ++stats.tx_full; return false; }
    for(i=0;i<length;i++) { tx[tail]=bytes[i]; tail=(tail+1)%CAPACITY; }
    count+=(unsigned)length; return true;
}
void PeerUart_Service(PeerLink *p) {
#ifdef GAME_BUTTONS_BENCH
    /* Opportunistic digital samples, not waveform capture or a continuity
     * test. No observed LOW does not prove that a short pulse never occurred. */
    unsigned pins=PORTB;
    if(pins&(1u<<8)) ++diagnostics.rx_high; else ++diagnostics.rx_low;
    if(pins&(1u<<10)) ++diagnostics.tx_high; else ++diagnostics.tx_low;
#endif
#ifdef PEER_UART_ISR
    /* Same fail-closed reactions as the polled path, from ISR-latched counts. */
    uint32_t overruns=isr_overruns,errors=isr_errors; uint8_t b;
    if(overruns!=seen_overruns) {
        seen_overruns=overruns; head=tail=count=0; PeerRxRing_Flush(&rx_ring);
        p->rx_count=0; p->discard=true; PeerLink_Fault(p);
    }
    if(errors!=seen_errors) { seen_errors=errors; p->discard=true; PeerLink_Fault(p); }
    unsigned budget=128;
    while(budget--&&PeerRxRing_Pop(&rx_ring,&b)) PeerLink_ReceiveByte(p,b);
#else
    unsigned budget=64;
    if(U2STAbits.OERR) {
        ++stats.overruns;
        U2STACLR=_U2STA_OERR_MASK; head=tail=count=0;
        while(U2STAbits.URXDA) { volatile unsigned discard=U2RXREG; (void)discard; }
        p->rx_count=0; p->discard=true; PeerLink_Fault(p);
    }
    while(budget--&&U2STAbits.URXDA) {
        bool bad=U2STAbits.FERR||U2STAbits.PERR; uint8_t b=(uint8_t)U2RXREG;
        ++stats.rx_bytes;
        if(bad) { ++stats.rx_errors; p->discard=true; PeerLink_Fault(p); } else PeerLink_ReceiveByte(p,b);
    }
#endif
    budget=64;
    while(budget--&&count&&!U2STAbits.UTXBF) { U2TXREG=tx[head]; ++stats.tx_bytes; head=(head+1)%CAPACITY; --count; }
}
