#include "BenchWire.h"
static uint8_t crc8(const uint8_t *bytes,size_t n) {
    uint8_t crc=0;
    while(n--) {
        crc^=*bytes++;
        for(unsigned i=0;i<8;++i) crc=(uint8_t)((crc<<1)^((crc&128)?7:0));
    }
    return crc;
}
size_t BenchWire_Encode(const BenchPacket *p,uint8_t out[14]) {
    uint8_t b[6]={0x49,p->type,p->sequence,p->mask,p->valid?1:0,0};
    b[5]=crc8(b,5); size_t n=0; out[n++]=0x7e;
    for(unsigned i=0;i<6;++i) {
        if(b[i]==0x7e || b[i]==0x7d) { out[n++]=0x7d; out[n++]=b[i]^0x20; }
        else out[n++]=b[i];
    }
    out[n++]=0x7e; return n;
}
bool BenchWire_Receive(BenchParser *p,uint8_t byte,BenchPacket *out) {
    if(byte==0x7e) {
        bool good=!p->discard && !p->escape && p->count==6 &&
            p->bytes[0]==0x49 && (p->bytes[1]==BENCH_SNAPSHOT || p->bytes[1]==BENCH_ACK) &&
            p->bytes[3]<=31 && p->bytes[4]<=1 && crc8(p->bytes,5)==p->bytes[5];
        if(good) *out=(BenchPacket){p->bytes[1],p->bytes[2],p->bytes[3],p->bytes[4]!=0};
        p->count=0; p->escape=false; p->discard=false; return good;
    }
    if(p->discard) return false;
    if(p->escape) { byte^=0x20; p->escape=false; }
    else if(byte==0x7d) { p->escape=true; return false; }
    if(p->count==sizeof p->bytes) { p->discard=true; return false; }
    p->bytes[p->count++]=byte; return false;
}
