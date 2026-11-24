#ifndef BUTTON_SENSOR_H
#define BUTTON_SENSOR_H
#include "PeerLink.h"
#include "Inputs.h"
typedef struct {
    PeerLink link;
    Pic2Inputs inputs;
    uint8_t raw;
    bool lamp,start_lamp;
    PeerControl control; /* Last heartbeat control; zeroed when unsynchronized. */
    PeerNowFn now;
    PeerSendFn send;
    void *context;
} ButtonSensor;
void ButtonSensor_Init(ButtonSensor *,PeerNowFn,PeerSendFn,void *);
uint8_t ButtonSensor_Sample(ButtonSensor *,uint8_t gpio);
bool ButtonSensor_Lamp(const ButtonSensor *);
bool ButtonSensor_StartLamp(const ButtonSensor *);
uint8_t ButtonSensor_Outputs(const ButtonSensor *);
/* Full controller state (sizzle, zone, LCD mode, burnt, burn_seq, target);
 * all zero unless the link is synchronized. */
PeerControl ButtonSensor_Control(const ButtonSensor *);
/* Forward one ultrasonic sample; false (dropped) unless synchronized. */
bool ButtonSensor_SendRange(ButtonSensor *,uint16_t mm,bool valid,uint32_t local_detection_ms);
#endif
