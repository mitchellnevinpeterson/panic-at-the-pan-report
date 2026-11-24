#include "ButtonSensor.h"
#include <string.h>
static uint32_t clock_ms(void *context) {
    ButtonSensor *s=context; return s->now(s->context);
}
static bool send_bytes(const uint8_t *data,size_t length,void *context) {
    ButtonSensor *s=context; return s->send(data,length,s->context);
}
static void event(const PeerEvent *e,void *context) {
    ButtonSensor *sensor=context;
    if(e->kind==PEER_STATUS) {
        Pic2Inputs_Reset(&sensor->inputs,sensor->raw,sensor->link.now(sensor->link.user));
        if(!e->synchronized) {
            sensor->lamp=sensor->start_lamp=false;
            memset(&sensor->control,0,sizeof sensor->control);
        }
    } else if(e->kind==PEER_CONTROL) {
        sensor->lamp=e->control.lamp; sensor->start_lamp=e->control.start_lamp;
        sensor->control=e->control;
    }
}
void ButtonSensor_Init(ButtonSensor *s,PeerNowFn now,PeerSendFn send,void *context) {
    s->raw=0xff; s->lamp=s->start_lamp=false; s->now=now; s->send=send; s->context=context;
    memset(&s->control,0,sizeof s->control);
    Pic2Inputs_Reset(&s->inputs,s->raw,now(context));
    PeerLink_Init(&s->link,PEER_SENSOR,1,clock_ms,send_bytes,event,s);
}
uint8_t ButtonSensor_Sample(ButtonSensor *s,uint8_t gpio) {
    s->raw=gpio;
    uint8_t old_stable=s->inputs.stable;
    uint8_t edges=Pic2Inputs_Update(&s->inputs,gpio,s->link.now(s->link.user),PeerLink_IsSynchronized(&s->link));
    for(unsigned i=0;i<8;i++) if(edges&(1u<<i)) {
        if(!PeerLink_SendInput(&s->link,(uint8_t)i,s->inputs.changed[i])) {
            PeerLink_Fault(&s->link); break;
        }
    }
    /* Debounced ingredient OFF is activity, never removal or a second add.
     * Ignore startup/resync samples; old-session transitions cannot leak. */
    uint8_t released=(uint8_t)(~old_stable & s->inputs.stable & 0x1fu);
    for(unsigned i=0;i<5;i++) if((released&(1u<<i)) && PeerLink_IsSynchronized(&s->link)) {
        if(!PeerLink_SendInput(&s->link,(uint8_t)(8+i),s->inputs.changed[i])) {
            PeerLink_Fault(&s->link); break;
        }
    }
    return edges;
}
bool ButtonSensor_Lamp(const ButtonSensor *s) {
    return s->lamp && PeerLink_IsSynchronized(&s->link);
}
bool ButtonSensor_StartLamp(const ButtonSensor *s) {
    return s->start_lamp && PeerLink_IsSynchronized(&s->link);
}
uint8_t ButtonSensor_Outputs(const ButtonSensor *s) {
    return (ButtonSensor_Lamp(s)?1u:0u)|(ButtonSensor_StartLamp(s)?2u:0u);
}
PeerControl ButtonSensor_Control(const ButtonSensor *s) {
    PeerControl none={0};
    return PeerLink_IsSynchronized(&s->link)?s->control:none;
}
bool ButtonSensor_SendRange(ButtonSensor *s,uint16_t mm,bool valid,uint32_t detection) {
    /* Best effort: a dropped sample is replaced by the next one in ~100 ms. */
    return PeerLink_SendSensor(&s->link,mm,valid,detection);
}
