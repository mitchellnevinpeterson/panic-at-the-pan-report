#include "Inputs.h"
void Pic2Inputs_Reset(Pic2Inputs *p,uint8_t gpio,uint32_t now) {
    unsigned i; p->raw=p->stable=gpio; p->armed=0;
    for(i=0;i<8;i++) p->changed[i]=now;
}
uint8_t Pic2Inputs_Update(Pic2Inputs *p,uint8_t gpio,uint32_t now,bool synced) {
    unsigned i; uint8_t edges=0;
    if(!synced) { Pic2Inputs_Reset(p,gpio,now); return 0; }
    for(i=0;i<8;i++) {
        uint8_t bit=(uint8_t)(1u<<i);
        if((gpio^p->raw)&bit) { p->raw^=bit; p->changed[i]=now; }
        if((uint32_t)(now-p->changed[i])<20) continue;
        if(gpio&bit) { p->stable|=bit; p->armed|=bit; }
        else if(p->stable&bit) {
            p->stable&=(uint8_t)~bit;
            if(p->armed&bit) edges|=bit;
            p->armed&=(uint8_t)~bit;
        }
    }
    return edges; /* GPA6 = pan-flip limit switch, same rules as buttons. */
}
