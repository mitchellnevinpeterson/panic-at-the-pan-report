#include "BenchModel.h"
void BenchInputs_Init(BenchInputs *p,uint32_t now) {
    p->raw=p->stable=31; p->started=now;
    for(unsigned i=0;i<5;++i) p->changed[i]=now;
}
uint8_t BenchInputs_Update(BenchInputs *p,uint8_t gpio,uint32_t now) {
    gpio &= 31;
    for(unsigned i=0;i<5;++i) {
        uint8_t bit=(uint8_t)(1u<<i);
        if((gpio^p->raw)&bit) { p->raw^=bit; p->changed[i]=now; }
        if((uint32_t)(now-p->changed[i])>=BENCH_DEBOUNCE_MS)
            p->stable=(p->stable&(uint8_t)~bit)|(gpio&bit);
    }
    return (uint8_t)(~p->stable)&31;
}
bool BenchInputs_Ready(const BenchInputs *p,uint32_t now) {
    return (uint32_t)(now-p->started)>=BENCH_DEBOUNCE_MS;
}
void BenchDisplay_Init(BenchDisplay *p,uint32_t now) {
    *p=(BenchDisplay){0}; p->finished=now;
}
void BenchDisplay_Receive(BenchDisplay *p,uint8_t mask,bool valid,uint32_t now) {
    p->valid=valid; p->received=now;
    if(!valid) return;
    p->live=mask&31;
    p->inhibited &= p->live; /* Only a witnessed OFF re-arms a held switch. */
    p->desired |= p->live & (uint8_t)~p->inhibited;
}
void BenchDisplay_Clear(BenchDisplay *p) {
    p->desired=0; p->inhibited=p->live; p->clear_requested=true;
}
bool BenchDisplay_Fresh(const BenchDisplay *p,uint32_t now) {
    return p->valid && (uint32_t)(now-p->received)<BENCH_LINK_TIMEOUT_MS;
}
uint32_t BenchDisplay_Wait(const BenchDisplay *p,uint32_t now) {
    uint32_t age=now-p->finished;
    return age<BENCH_COOLDOWN_MS ? BENCH_COOLDOWN_MS-age : 0;
}
bool BenchDisplay_Select(BenchDisplay *p,uint32_t now) {
    if(p->busy || !BenchDisplay_Fresh(p,now) || BenchDisplay_Wait(p,now) ||
       (p->known ? p->desired==p->displayed : (p->desired==0 && !p->clear_requested))) return false;
    p->selected=p->desired; p->busy=true; return true;
}
bool BenchDisplay_Current(const BenchDisplay *p,uint32_t now) {
    return BenchDisplay_Fresh(p,now) && p->selected==p->desired;
}
void BenchDisplay_Finish(BenchDisplay *p,uint32_t now) {
    p->displayed=p->selected; p->known=true; p->busy=false; p->finished=now;
    p->clear_requested=false;
}
