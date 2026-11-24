#include "McpInputs.h"
#include <xc.h>
static uint8_t bytes[3], index_byte, gpio;
static uint32_t deadline,last_poll,last_health;
static uint8_t operation;
static bool busy,fresh,check_latch;
static uint8_t applied_outputs,target_outputs,output_mask,direction;
static bool transfer(uint8_t value,uint8_t *result) {
    uint32_t start=_CP0_GET_COUNT(); SPI1BUF=value;
    while(!SPI1STATbits.SPIRBF) if((uint32_t)(_CP0_GET_COUNT()-start)>20000u) return false;
    *result=(uint8_t)SPI1BUF; return true;
}
static bool reg(uint8_t address,uint8_t value,bool read,uint8_t *out) {
    uint8_t dummy; bool good;
    LATACLR=1u; good=transfer(read?0x41:0x40,&dummy)&&transfer(address,&dummy)&&transfer(value,out);
    LATASET=1u; return good;
}
bool McpInputs_Init(void) {
    return McpInputs_InitOutputs(1u);
}
bool McpInputs_InitOutputs(uint8_t outputs) {
    if(outputs!=1u && outputs!=3u) return false;
    output_mask=outputs; direction=(uint8_t)~outputs;
    uint8_t dummy,check; unsigned irq=__builtin_disable_interrupts();
    ANSELACLR=3u; ANSELBCLR=(1u<<11)|(1u<<14);
    LATASET=1u; TRISACLR=3u; TRISBSET=1u<<11; TRISBCLR=1u<<14;
    SYSKEY=0; SYSKEY=0xaa996655; SYSKEY=0x556699aa; CFGCONbits.IOLOCK=0;
    RPA0R=0; RPA1R=3; SDI1R=3; CFGCONbits.IOLOCK=1; SYSKEY=0;
    SPI1CON=0; SPI1BRG=99; SPI1CONbits.MSTEN=1; SPI1CONbits.CKE=1;
    SPI1STATCLR=_SPI1STAT_SPIROV_MASK; SPI1CONbits.ON=1;
    if(irq&1) __builtin_enable_interrupts();
    busy=fresh=check_latch=false; applied_outputs=target_outputs=0; last_poll=last_health=0;
    /* 0x05 is IOCONA in BANK1, GPINTENB in BANK0. This first write
       normalizes BANK regardless of surviving MCP state after PIC reset. */
    if(!reg(0x05,0,false,&dummy)||!reg(0x0a,0,false,&dummy)||
       !reg(0x15,0,false,&dummy)|| /* OLATB LOW before output direction */
       !reg(0x00,0xff,false,&dummy)||!reg(0x01,direction,false,&dummy)||
       !reg(0x02,0,false,&dummy)||!reg(0x03,0,false,&dummy)||
       !reg(0x04,0,false,&dummy)||!reg(0x05,0,false,&dummy)||
       !reg(0x0c,0x40,false,&dummy)||!reg(0x0d,direction,false,&dummy)||
       !reg(0x0a,0,true,&check)||check!=0||
       !reg(0x01,0,true,&check)||check!=direction||
       !reg(0x15,0,true,&check)||check!=0) return false;
    return true;
}
bool McpInputs_Service(uint32_t now,bool lamp) {
    return McpInputs_ServiceOutputs(now,lamp?1u:0u);
}
bool McpInputs_ServiceOutputs(uint32_t now,uint8_t outputs) {
    outputs&=output_mask;
    if(SPI1STATbits.SPIROV) { SPI1STATCLR=_SPI1STAT_SPIROV_MASK; LATASET=1u; busy=false; return false; }
    if(busy) {
        /* Completion first: a foreground stall (LCD step, ISR, print) after the
         * byte finished is not an MCP fault. Only a byte still missing after
         * 1 ms is. */
        if(!SPI1STATbits.SPIRBF) {
            if((uint32_t)(_CP0_GET_COUNT()-deadline)>20000u) { LATASET=1u; busy=false; return false; }
            return true;
        }
        uint8_t received=(uint8_t)SPI1BUF;
        if(++index_byte==3) {
            LATASET=1u; busy=false;
            if(operation==1) { gpio=received; fresh=true; }
            else if(operation==2) { if(received!=direction) return false; }
            else if(operation==3) { if(received!=applied_outputs) return false; }
            else applied_outputs=target_outputs;
        } else { SPI1BUF=bytes[index_byte]; deadline=_CP0_GET_COUNT(); }
        return true;
    }
    if(outputs!=applied_outputs) {
        operation=0; target_outputs=outputs; bytes[0]=0x40; bytes[1]=0x15; bytes[2]=outputs;
    } else if((uint32_t)(now-last_health)>=50) {
        last_health=now; check_latch=!check_latch; operation=check_latch?3:2;
        bytes[0]=0x41; bytes[1]=check_latch?0x15:0x01; bytes[2]=0;
    } else if((uint32_t)(now-last_poll)>=2) {
        last_poll=now; operation=1; bytes[0]=0x41; bytes[1]=0x12; bytes[2]=0;
    } else return true;
    LATACLR=1u; index_byte=0; busy=true; deadline=_CP0_GET_COUNT(); SPI1BUF=bytes[0]; return true;
}
bool McpInputs_Busy(void) { return busy; }
bool McpInputs_Take(uint8_t *value) { if(!fresh) return false; fresh=false; *value=gpio; return true; }
