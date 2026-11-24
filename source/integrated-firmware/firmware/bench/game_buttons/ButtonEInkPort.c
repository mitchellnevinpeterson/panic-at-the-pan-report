/* SPI1 polling port from the hardware-tested IngredientEInkPIC1 bench. */
#include "ButtonEInkPort.h"
#include <xc.h>
#include <cp0defs.h>
#include <string.h>
static uint8_t bytes[32],count,index;
static uint32_t started;
static EInkBusState state;
static EInkAsyncPort port;
static void reset_pin(bool high) { if(high) LATBSET=1u<<9; else LATBCLR=1u<<9; }
static bool busy_level(void) { return (PORTB&(1u<<12))!=0; }
static bool start(bool data,const uint8_t *data_bytes,size_t n) {
    if(state!=EINK_BUS_IDLE || !data_bytes || !n || n>sizeof bytes) return false;
    memcpy(bytes,data_bytes,n); count=(uint8_t)n; index=0;
    if(data) LATBSET=1u<<4; else LATBCLR=1u<<4;
    LATACLR=1u; started=_CP0_GET_COUNT(); SPI1BUF=bytes[0];
    state=EINK_BUS_BUSY; return true;
}
static EInkBusState poll(void) {
    if(state!=EINK_BUS_BUSY) return state;
    uint32_t age=(uint32_t)(_CP0_GET_COUNT()-started);
    if(SPI1STATbits.SPIROV || age>=100000u) {
        SPI1CONCLR=_SPI1CON_ON_MASK; LATASET=1u; return state=EINK_BUS_FAULT;
    }
    if(age<20u || !SPI1STATbits.SPITBE || SPI1STATbits.SPIBUSY) return state;
    if(SPI1STATbits.SPIRBF) (void)SPI1BUF;
    if(++index==count) { LATASET=1u; return state=EINK_BUS_IDLE; }
    started=_CP0_GET_COUNT(); SPI1BUF=bytes[index]; return state;
}
const EInkAsyncPort *ButtonEInkPort_Init(uint32_t (*clock_ms)(void)) {
    unsigned irq=__builtin_disable_interrupts();
    DDPCONbits.JTAGEN=0;
    SYSKEY=0; SYSKEY=0xaa996655; SYSKEY=0x556699aa; CFGCONbits.IOLOCK=0;
    RPA0R=0; RPA1R=3; RPB4R=0; RPB9R=0; RPB12R=0;
    /* RA4/RB5 (servo pins) are routed to OC4/OC2 in pic1_main and owned by
     * Servo50Hz; un-routing them here left both servos without pulses. */
    CFGCONbits.IOLOCK=1; SYSKEY=0;
    PMD5bits.SPI1MD=0; SPI1CON=0; SPI1CON2=0;
    ANSELACLR=3u;
    ANSELBCLR=(1u<<4)|(1u<<5)|(1u<<9)|(1u<<12)|(1u<<14);
    ODCACLR=3u|(1u<<4); ODCBCLR=(1u<<4)|(1u<<5)|(1u<<9)|(1u<<14);
    LATASET=1u; LATACLR=2u|(1u<<4);
    LATBSET=1u<<9; LATBCLR=(1u<<4)|(1u<<5)|(1u<<14);
    TRISACLR=3u|(1u<<4); TRISBCLR=(1u<<4)|(1u<<5)|(1u<<9)|(1u<<14);
    TRISBSET=1u<<12; CNPUBCLR=1u<<12; CNPDBSET=1u<<12;
    SPI1BRG=99; (void)SPI1BUF; SPI1STATCLR=_SPI1STAT_SPIROV_MASK;
    SPI1CON=_SPI1CON_MSTEN_MASK|_SPI1CON_CKE_MASK|_SPI1CON_DISSDI_MASK|_SPI1CON_ON_MASK;
    state=EINK_BUS_IDLE;
    port=(EInkAsyncPort){start,poll,reset_pin,busy_level,clock_ms};
    if(irq&1) __builtin_enable_interrupts();
    return &port;
}
