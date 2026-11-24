#ifndef GAME_BUTTON_EINK_H
#define GAME_BUTTON_EINK_H
#include "EInkAsync.h"
/* An icon refresh starts only after additions pause this long, so a burst of
 * switches shares one refresh. White clears never wait. */
#define BUTTON_EINK_BATCH_MS 500u
typedef struct {
    uint8_t desired, selected, shown;
    bool known, clearing, building, refreshing, fault;
} ButtonEInkStatus;
bool ButtonEInk_Init(const EInkAsyncPort *port);
void ButtonEInk_Add(uint8_t ingredient);
/* White is a required frame before any following round's icons. */
void ButtonEInk_Clear(void);
bool ButtonEInk_Service(void);
ButtonEInkStatus ButtonEInk_Status(void);
#endif
