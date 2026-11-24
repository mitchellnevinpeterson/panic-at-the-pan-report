#include "Range.h"
#include <string.h>
void Pic2Range_Init(Pic2Range *r,uint32_t ticks) { memset(r,0,sizeof *r); r->next=ticks; }
static void done(Pic2Range *r,uint32_t ticks,bool valid,uint32_t width) {
    r->state=0; r->trigger=false; r->valid=valid;
    r->mm=valid?(uint16_t)((width*343u)/40000u):0;
    r->sample_ticks=ticks; r->sample_ready=true; r->next=ticks+2000000u;
}
void Pic2Range_Poll(Pic2Range *r,uint32_t ticks,bool echo) {
    switch(r->state) {
    case 0:
        if((int32_t)(ticks-r->next)<0) break;
        if(echo) { done(r,ticks,false,0); break; }
        r->trigger=true; r->mark=ticks; r->state=1; break;
    case 1:
        if((uint32_t)(ticks-r->mark)>=240u) { r->trigger=false; r->mark=ticks; r->state=2; }
        break;
    case 2:
        if((uint32_t)(ticks-r->mark)>=600000u) done(r,ticks,false,0);
        else if(echo) { r->rise=ticks; r->state=3; }
        break;
    case 3: {
        uint32_t width=ticks-r->rise;
        if(width>=600000u) done(r,ticks,false,0);
        else if(!echo) done(r,ticks,width>=2000u&&width<=466472u,width);
        break;
    }
    default: done(r,ticks,false,0); break;
    }
}
