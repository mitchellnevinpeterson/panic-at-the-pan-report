#ifndef PIC2_HEAT_AUDIO_H
#define PIC2_HEAT_AUDIO_H
#include <stdbool.h>
#include <stdint.h>

/* Final-game PIC2 audio, adapted from PIC32-LCD/src/heat_audio.c + sizzle.h.
 * Timer2 PWM + OC1 (pin26) + Timer3 12.5 kHz ISR. Background music plays in
 * every state (also with the link down). RPB15R = 5 must already be set in
 * pic2_main's first PPS block (a later PPS write did not stick on hardware);
 * this module performs no PPS writes. Volume fixed at maximum.
 */
void HeatAudio_Init(void);
/* sizzle=false -> HEAT_SILENT (music only). zone 0..3 = HeatZone.
 * A change of burn_seq (0..3) plays the one-shot burnt cue. */
void HeatAudio_Set(bool sizzle, uint8_t zone, uint8_t burn_seq);
/* Diagnostics: true when pin26 is routed to OC1 and Timer2/Timer3/OC1 run. */
bool HeatAudio_Healthy(void);

#endif
