#ifndef PIC2_HEAT_VIEW_H
#define PIC2_HEAT_VIEW_H
#include <stdbool.h>
#include <stdint.h>

/* Final-game LCD heat screen (ST7789 via LcdSoftSPI + PIC32-LCD st7789.c).
 * Layout follows the confirmed PAN HEAT screen. Drawing is incremental so
 * PIC2 keeps servicing UART2, MCP and the ranger: HeatView_Step() issues at
 * most one bounded st7789 primitive (<= HEATVIEW_STEP_MAX_BYTES on the wire).
 */
enum { HEATVIEW_STEP_MAX_BYTES = 1024 };

typedef enum {
    HEATVIEW_ATTRACT = 0, /* bar sweeps by itself; label "WAITING FOR HEAT INPUTS" */
    HEATVIEW_WAITING = 1, /* bar follows distance; label "WAITING"; no band marks */
    HEATVIEW_ZONE = 2     /* bar follows distance; zone label or "BURNT"; band marks */
} HeatViewMode;

/* Blocking full-screen layout draw; call once at boot before the link starts.
 * False if the LCD transport reports failure. */
bool HeatView_Start(void);
/* Desired state; cheap, never draws. zone 0..3 (none, NOT ENOUGH, JUST RIGHT,
 * TOO MUCH). distance_mm 0 = no hand. now_ms drives the attract sweep. */
void HeatView_Set(HeatViewMode mode, uint8_t zone, bool burnt, uint8_t target_cm,
                  uint16_t distance_mm, uint32_t now_ms);
/* Bottom line "STIRS n  FLIPS n": the current ingredient's counts (shown
 * 0..99). Visible in WAITING and ZONE modes, blank in ATTRACT. Cheap. */
void HeatView_SetCounts(uint8_t stirs, uint8_t flips);
/* One bounded drawing step toward the desired state. True if more remain. */
bool HeatView_Step(void);

#endif
