#ifndef PANIC_PAN_CANVAS_H
#define PANIC_PAN_CANVAS_H
#include "EInkDisplay.h"
#include "PanDisplaySM.h"
/* Incremental renderer: at most one 48-pixel sprite row or glyph row per call.
 * The pending display model may change while this immutable snapshot builds. */
bool PanCanvas_Start(EInkFrame *frame, PanDisplaySMState_t state, uint8_t mask);
bool PanCanvas_Service(void); /* true once complete */
#endif
