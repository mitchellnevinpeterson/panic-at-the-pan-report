#include "PeerLink.h"
#include <string.h>
enum { HEARTBEAT=1, READY, RESERVED, SENSOR, INPUT, ACK, HELLO };
static uint32_t now(PeerLink *p) { return p->now(p->user); }
static void put32(uint8_t *b,uint32_t v) { unsigned i; for(i=0;i<4;i++) b[i]=(uint8_t)(v>>(8*i)); }
static uint32_t get32(const uint8_t *b) { return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24); }
static uint16_t crc(const uint8_t *b,size_t n) {
    uint16_t c=0xffff; unsigned j;
    while(n--) { c^=(uint16_t)*b++<<8; for(j=0;j<8;j++) c=(uint16_t)((c<<1)^((c&0x8000)?0x1021:0)); }
    return c;
}
static bool emit(PeerLink *p,uint8_t type,uint16_t seq,uint32_t time,uint32_t a,uint32_t b,uint8_t flags) {
    uint8_t raw[23]={1}, wire[48]; size_t i,n=0; uint16_t c;
    raw[1]=type; put32(raw+2,p->epoch); raw[6]=(uint8_t)seq; raw[7]=(uint8_t)(seq>>8);
    put32(raw+8,time); put32(raw+12,a); put32(raw+16,b); raw[20]=flags;
    c=crc(raw,21); raw[21]=(uint8_t)c; raw[22]=(uint8_t)(c>>8);
    wire[n++]=0x7e;
    for(i=0;i<sizeof raw;i++) { if(raw[i]==0x7e||raw[i]==0x7d) { wire[n++]=0x7d; wire[n++]=raw[i]^0x20; } else wire[n++]=raw[i]; }
    wire[n++]=0x7e; return p->send(wire,n,p->user);
}
static void status(PeerLink *p,bool value,bool force) {
    PeerEvent e={0}; bool changed=p->synchronized!=value;
    p->synchronized=value;
    if(value&&changed) p->input_after=now(p);
    if(changed||force) { e.kind=PEER_STATUS; e.epoch=p->epoch; e.synchronized=value; p->event(&e,p->user); }
}
static void clear(PeerLink *p) {
    p->count=p->head=0; p->next_seq=1; p->received_seq=0; p->ready_seen=false;
    memset(&p->control,0,sizeof p->control);
}
void PeerLink_Init(PeerLink *p,PeerRole role,uint32_t epoch,PeerNowFn clock,PeerSendFn send,PeerEventFn event,void *user) {
    memset(p,0,sizeof *p); p->role=role; p->epoch=epoch?epoch:1;
    p->now=clock; p->send=send; p->event=event; p->user=user;
    p->have_epoch=role==PEER_CONTROLLER; clear(p); p->last_rx=now(p); p->last_tx=now(p)-100;
    /* Drain old wire/peer queues before a controller reboot can reuse an epoch.
       Sensor sees unsynchronized heartbeats throughout this startup guard. */
    p->ready_after=now(p)+PEER_TIMEOUT_MS;
}
bool PeerLink_IsSynchronized(const PeerLink *p) { return p->synchronized; }
void PeerLink_ResetSession(PeerLink *p) {
    if(p->role!=PEER_CONTROLLER) return;
    if(++p->epoch==0) ++p->epoch;
    clear(p); p->have_epoch=true; p->last_rx=now(p); p->last_tx=now(p)-100;
    status(p,false,true);
}
void PeerLink_Fault(PeerLink *p) {
    ++p->faults;
    if(p->role==PEER_CONTROLLER) PeerLink_ResetSession(p);
    else { clear(p); p->have_epoch=false; p->last_rx=now(p); p->last_tx=now(p)-100; status(p,false,true); }
}
static uint32_t mapped(PeerLink *p,uint32_t local) {
    /* A heartbeat's transmit time predates reception. The 20 ms allowance
       covers quantization and up to 2% relative clock error over 600 ms.
       Never use reception time as input time. */
    return p->map_remote+(local-p->map_local)-20u;
}
void PeerLink_SendControl(PeerLink *p,const PeerControl *control) {
    if(p->role!=PEER_CONTROLLER) return;
    p->control=*control;
    /* Out-of-range values fall back to the safe default, never alias. */
    if(p->control.audio_zone>3) p->control.audio_zone=0;
    if(p->control.lcd_mode>2) p->control.lcd_mode=0;
    p->control.burn_seq&=3u;
    if(p->control.flips>63u) p->control.flips=63u;
    p->last_tx=now(p)-100;
}
bool PeerLink_SendSensor(PeerLink *p,uint16_t mm,bool valid,uint32_t detection) {
    if(p->role!=PEER_SENSOR||!p->synchronized) return false;
    return emit(p,SENSOR,0,mapped(p,detection),mm,0,valid?1:0);
}
bool PeerLink_SendInput(PeerLink *p,uint8_t id,uint32_t detection) {
    PeerPending *q;
    if(p->role!=PEER_SENSOR||!p->synchronized||id>12) return false;
    if((int32_t)(detection-p->input_after)<0) return false;
    if(p->count==PEER_EVENT_CAPACITY) { PeerLink_Fault(p); return false; }
    q=&p->pending[(p->head+p->count)%PEER_EVENT_CAPACITY];
    q->id=id; q->seq=p->next_seq++; q->time=mapped(p,detection); q->born=now(p);
    if(p->count++==0) p->last_retry=now(p)-80;
    return true;
}
static void receive(PeerLink *p) {
    uint8_t *r=p->rx,type=r[1]; uint32_t epoch=get32(r+2),t=get32(r+8),a=get32(r+12);
    uint16_t seq=(uint16_t)(r[6]|((uint16_t)r[7]<<8)); PeerEvent e={0};
    if(r[0]!=1||crc(r,21)!=(uint16_t)(r[21]|((uint16_t)r[22]<<8))) { ++p->bad_frames; return; }
    ++p->valid_frames;
    if(type==HELLO&&p->role==PEER_CONTROLLER) {
        if(p->synchronized||p->ready_seen) PeerLink_ResetSession(p);
        p->last_tx=now(p)-100; return;
    }
    if(type==HEARTBEAT&&p->role==PEER_SENSOR) {
        /* A boot/fault must be observed by PIC1. Never adopt an already-live
           session from a buffered heartbeat and silently erase lost edges. */
        if(!p->have_epoch&&(r[20]&1)) { emit(p,HELLO,0,0,0,0,0); return; }
        if(p->have_epoch&&epoch!=p->epoch&&(int32_t)(epoch-p->epoch)<0) return;
        if(!p->have_epoch||epoch!=p->epoch) {
            clear(p); p->epoch=epoch; p->have_epoch=true; status(p,false,true);
        }
        if(p->synchronized&&!(r[20]&1)) { clear(p); status(p,false,true); }
        p->last_rx=now(p); p->map_local=now(p); p->map_remote=t; p->challenge=get32(r+16);
        status(p,(r[20]&1)!=0,false);
        e.kind=PEER_CONTROL; e.epoch=epoch; e.control.lamp=p->synchronized&&(a&1);
        e.control.audio_zone=(uint8_t)((a>>8)&3); e.control.audio_enabled=p->synchronized&&(a&2);
        e.control.start_lamp=p->synchronized&&(a&4);
        if(p->synchronized) { /* Unsynchronized = all-zero = attract screen. */
            e.control.burnt=(a&8)!=0; e.control.lcd_mode=(uint8_t)((a>>4)&3);
            if(e.control.lcd_mode>2) e.control.lcd_mode=0;
            e.control.burn_seq=(uint8_t)((a>>6)&3); e.control.target_cm=(uint8_t)(a>>16);
            e.control.flips=(uint8_t)((a>>10)&63u); e.control.stirs=(uint8_t)(a>>24);
        }
        p->event(&e,p->user);
        emit(p,READY,0,t,p->challenge,0,0); return;
    }
    if(epoch!=p->epoch||!p->have_epoch) return;
    if(type==READY&&p->role==PEER_CONTROLLER) {
        if(a!=p->challenge) return;
        p->last_rx=now(p); p->ready_seen=true;
        if((int32_t)(now(p)-p->ready_after)>=0) status(p,true,false);
        return;
    }
    if(!p->synchronized) return;
    e.epoch=epoch; e.time_ms=t;
    if(type==INPUT&&p->role==PEER_CONTROLLER&&a<=12) {
        if(seq==(uint16_t)(p->received_seq+1)) {
            p->received_seq=seq; e.kind=PEER_INPUT; e.input_id=(uint8_t)a; p->event(&e,p->user);
            /* Callback may reset the epoch; do not ACK into the new session. */
            if(p->epoch!=epoch) return;
        }
        if(seq==p->received_seq) emit(p,ACK,seq,0,0,0,0);
    } else if(type==ACK&&p->role==PEER_SENSOR&&p->count&&p->pending[p->head].seq==seq) {
        p->head=(p->head+1)%PEER_EVENT_CAPACITY; --p->count; p->last_retry=now(p)-80;
    } else if(type==SENSOR&&p->role==PEER_CONTROLLER&&a<=65535) {
        e.kind=PEER_SENSOR_READING; e.distance_mm=(uint16_t)a; e.valid=(r[20]&1)!=0; p->event(&e,p->user);
    }
}
void PeerLink_ReceiveByte(PeerLink *p,uint8_t byte) {
    if(byte==0x7e) {
        if(!p->discard&&!p->escaping&&p->rx_count==sizeof p->rx) receive(p);
        p->rx_count=0; p->escaping=false; p->discard=false; return;
    }
    if(p->discard) return;
    if(p->escaping) { byte^=0x20; p->escaping=false; }
    else if(byte==0x7d) { p->escaping=true; return; }
    if(p->rx_count==sizeof p->rx) { p->discard=true; return; }
    p->rx[p->rx_count++]=byte;
}
void PeerLink_Service(PeerLink *p) {
    uint32_t t=now(p);
    if((uint32_t)(t-p->last_rx)>=PEER_TIMEOUT_MS) PeerLink_Fault(p);
    if((uint32_t)(t-p->last_tx)>=100) {
        p->last_tx=t;
        if(p->role==PEER_CONTROLLER) {
            /* a: 0 lamp, 1 audio, 2 start lamp, 3 burnt, 4-5 lcd_mode,
               6-7 burn_seq, 8-9 audio_zone, 10-15 flips, 16-23 target_cm, 24-31 stirs. */
            uint32_t a=(p->control.lamp?1u:0u)|(p->control.audio_enabled?2u:0u)|
                (p->control.start_lamp?4u:0u)|(p->control.burnt?8u:0u)|
                ((uint32_t)p->control.lcd_mode<<4)|((uint32_t)p->control.burn_seq<<6)|
                ((uint32_t)p->control.audio_zone<<8)|((uint32_t)p->control.flips<<10)|
                ((uint32_t)p->control.target_cm<<16)|((uint32_t)p->control.stirs<<24);
            /* Stable challenge through an epoch; incremented epoch rejects old READY. */
            p->challenge=p->epoch^0xa97e314du;
            emit(p,HEARTBEAT,0,t,a,p->challenge,p->synchronized?1:0);
        } else if(!p->have_epoch) emit(p,HELLO,0,0,0,0,0);
        else emit(p,READY,0,p->map_remote,p->challenge,0,0);
    }
    if(p->count&&p->synchronized) {
        PeerPending *q=&p->pending[p->head];
        if((uint32_t)(t-q->born)>=1000) { PeerLink_Fault(p); return; }
        if((uint32_t)(t-p->last_retry)>=80) { p->last_retry=t; emit(p,INPUT,q->seq,q->time,q->id,0,0); }
    }
}
