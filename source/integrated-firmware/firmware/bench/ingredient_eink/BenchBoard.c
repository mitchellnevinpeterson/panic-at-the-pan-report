/* Standalone polling HAL. SPI transfer copied from integrated Pic1Board;
 * pins/clock/SPI mode match the preserved hardware-tested EInkBench. */
#include <xc.h>
#include <cp0defs.h>
#include <string.h>
#include "BenchBoard.h"
static uint32_t last_tick,milliseconds,uart_errors;
static uint8_t peer_tx[128],debug_tx[512];
static unsigned ph,pt,pc,dh,dt,dc;
static BenchParser parser;
uint32_t BenchBoard_Now(void) {
    uint32_t whole=(uint32_t)(_CP0_GET_COUNT()-last_tick)/20000u;
    last_tick+=whole*20000u; milliseconds+=whole; return milliseconds;
}
uint32_t BenchBoard_UartErrors(void) { return uart_errors; }
void BenchBoard_Text(const char *text) {
    size_t n=strlen(text);
    if(n>sizeof debug_tx-dc) return; /* Debug congestion never delays UART2. */
    while(*text) { debug_tx[dt]=(uint8_t)*text++; dt=(dt+1)%sizeof debug_tx; ++dc; }
}
void BenchBoard_Hex(uint8_t value) {
    static const char hex[]="0123456789ABCDEF";
    char b[3]={hex[value>>4],hex[value&15],0}; BenchBoard_Text(b);
}
void BenchBoard_Number(uint32_t value) {
    char b[11]; unsigned n=10; b[n]=0;
    do { b[--n]=(char)('0'+value%10); value/=10; } while(value);
    BenchBoard_Text(&b[n]);
}
void BenchBoard_Send(const BenchPacket *p) {
    uint8_t bytes[14]; size_t n=BenchWire_Encode(p,bytes);
    if(n>sizeof peer_tx-pc) { ++uart_errors; return; }
    for(size_t i=0;i<n;++i) { peer_tx[pt]=bytes[i]; pt=(pt+1)%sizeof peer_tx; ++pc; }
}
bool BenchBoard_Poll(BenchPacket *out) {
    if(U2STAbits.OERR) {
        U2STACLR=_U2STA_OERR_MASK; ++uart_errors;
        while(U2STAbits.URXDA) (void)U2RXREG;
        parser=(BenchParser){.discard=true};
    }
    unsigned budget=16;
    while(budget-- && pc && !U2STAbits.UTXBF) {
        U2TXREG=peer_tx[ph]; ph=(ph+1)%sizeof peer_tx; --pc;
    }
    budget=16;
    while(budget-- && dc && !U1STAbits.UTXBF) {
        U1TXREG=debug_tx[dh]; dh=(dh+1)%sizeof debug_tx; --dc;
    }
    budget=16;
    while(budget-- && U2STAbits.URXDA) {
        bool bad=U2STAbits.FERR||U2STAbits.PERR;
        uint8_t byte=(uint8_t)U2RXREG;
        if(bad) { ++uart_errors; parser=(BenchParser){.discard=true}; }
        else if(BenchWire_Receive(&parser,byte,out)) return true;
    }
    return false;
}
int BenchBoard_Key(void) {
    if(U1STAbits.OERR) U1STACLR=_U1STA_OERR_MASK;
    if(!U1STAbits.URXDA) return -1;
    bool bad=U1STAbits.FERR||U1STAbits.PERR;
    int c=(uint8_t)U1RXREG; return bad?-1:c;
}
#if BENCH_PIC == 1
static uint8_t spi_bytes[32],spi_count,spi_index;
static uint32_t spi_started;
static EInkBusState spi_state;
static void reset_pin(bool high) {
    if(high) LATBSET=1u<<9; else LATBCLR=1u<<9;
}
static bool busy_level(void) { return (PORTB&(1u<<12))!=0; }
static bool spi_start(bool data,const uint8_t *bytes,size_t count) {
    if(spi_state!=EINK_BUS_IDLE || !bytes || !count || count>sizeof spi_bytes) return false;
    memcpy(spi_bytes,bytes,count); spi_count=(uint8_t)count; spi_index=0;
    if(data) LATBSET=1u<<4; else LATBCLR=1u<<4;
    LATACLR=1u; spi_started=_CP0_GET_COUNT(); SPI1BUF=spi_bytes[0];
    spi_state=EINK_BUS_BUSY; return true;
}
static EInkBusState spi_poll(void) {
    if(spi_state!=EINK_BUS_BUSY) return spi_state;
    uint32_t age=(uint32_t)(_CP0_GET_COUNT()-spi_started);
    if(SPI1STATbits.SPIROV || age>=100000u) {
        SPI1CONCLR=_SPI1CON_ON_MASK; LATASET=1u; return spi_state=EINK_BUS_FAULT;
    }
    if(age<20u || !SPI1STATbits.SPITBE || SPI1STATbits.SPIBUSY) return spi_state;
    if(SPI1STATbits.SPIRBF) (void)SPI1BUF;
    if(++spi_index==spi_count) { LATASET=1u; return spi_state=EINK_BUS_IDLE; }
    spi_started=_CP0_GET_COUNT(); SPI1BUF=spi_bytes[spi_index]; return spi_state;
}
static const EInkAsyncPort eink_port={spi_start,spi_poll,reset_pin,busy_level,BenchBoard_Now};
const EInkAsyncPort *BenchBoard_EInkPort(void) { return &eink_port; }
#endif
void BenchBoard_Init(void) {
    __builtin_disable_interrupts(); DDPCONbits.JTAGEN=0;
    last_tick=_CP0_GET_COUNT(); milliseconds=uart_errors=0;
    ph=pt=pc=dh=dt=dc=0; parser=(BenchParser){0};
    SYSKEY=0; SYSKEY=0xaa996655; SYSKEY=0x556699aa; CFGCONbits.IOLOCK=0;
    /* UART2 is always PIC TX21/RB10, RX17/RB8, NOT debug UART1. */
    RPB10R=2; U2RXR=4; U1RXR=1;
#if BENCH_PIC == 1
    RPB7R=1; /* Debug TX16; RX15. */
    RPA0R=0; RPA1R=3; RPB4R=0; RPB9R=0; RPB12R=0;
    RPA4R=0; RPB5R=0; RPB11R=0; RPB13R=0; RPB15R=0;
#else
    RPB4R=1; /* PIC2's existing debug TX11; optional RX15. */
    RPB15R=0; /* Actual PIC2 audio: OC1 / RB15 / physical 26. */
#endif
    CFGCONbits.IOLOCK=1; SYSKEY=0;
    ANSELBCLR=(1u<<8)|(1u<<10);
    ODCBCLR=1u<<10; LATBSET=1u<<10; TRISBCLR=1u<<10; TRISBSET=1u<<8;
    TRISBSET=1u<<6;
#if BENCH_PIC == 1
    ODCBCLR=1u<<7; LATBSET=1u<<7; TRISBCLR=1u<<7;
    PMD5bits.SPI1MD=0; SPI1CON=0; SPI1CON2=0;
    ANSELACLR=3u;
    ANSELBCLR=(1u<<4)|(1u<<5)|(1u<<9)|(1u<<11)|(1u<<12)|(1u<<13)|(1u<<14)|(1u<<15);
    ODCACLR=3u|(1u<<4);
    ODCBCLR=(1u<<4)|(1u<<5)|(1u<<9)|(1u<<11)|(1u<<13)|(1u<<14)|(1u<<15);
    LATASET=1u; LATACLR=2u|(1u<<4);
    LATBSET=(1u<<9)|(1u<<13);
    LATBCLR=(1u<<4)|(1u<<5)|(1u<<11)|(1u<<14)|(1u<<15);
    TRISACLR=3u|(1u<<4);
    TRISBCLR=(1u<<4)|(1u<<5)|(1u<<9)|(1u<<11)|(1u<<13)|(1u<<14)|(1u<<15);
    TRISBSET=1u<<12; CNPUBCLR=1u<<12; CNPDBSET=1u<<12;
    OC4CON=0; OC2CON=0; /* Attached servo p12 and treat p14 stay LOW. */
    SPI1BRG=99; (void)SPI1BUF; SPI1STATCLR=_SPI1STAT_SPIROV_MASK;
    SPI1CON=_SPI1CON_MSTEN_MASK|_SPI1CON_CKE_MASK|_SPI1CON_DISSDI_MASK|_SPI1CON_ON_MASK;
    spi_state=EINK_BUS_IDLE;
#else
    ANSELBCLR=(1u<<2)|(1u<<4)|(1u<<15);
    ODCBCLR=(1u<<2)|(1u<<4)|(1u<<15);
    LATBSET=1u<<4; LATBCLR=(1u<<2)|(1u<<15);
    TRISBCLR=(1u<<2)|(1u<<4)|(1u<<15); OC1CON=0;
#endif
    PMD5bits.U1MD=0; PMD5bits.U2MD=0;
    U1MODE=0; U1STA=0; U1BRG=42; U1MODEbits.BRGH=1;
    U1STAbits.UTXEN=1; U1STAbits.URXEN=1; U1MODEbits.ON=1;
    U2MODE=0; U2STA=0; U2BRG=42; U2MODEbits.BRGH=1;
    U2STAbits.UTXEN=1; U2STAbits.URXEN=1; U2MODEbits.ON=1;
    __builtin_enable_interrupts();
}
