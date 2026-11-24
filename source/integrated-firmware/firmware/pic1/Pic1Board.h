#ifndef PANIC_PIC1_BOARD_H
#define PANIC_PIC1_BOARD_H
#include <stdbool.h>
#include <stdint.h>
#include "EInkAsync.h"

bool Pic1Board_Init(void);
uint32_t Pic1Board_Now(void);
bool Pic1Board_Service(void);
void Pic1Board_Stop(void);
bool Pic1Board_Motor(uint32_t angle_mdeg, bool home, bool enabled);
const EInkAsyncPort *Pic1Board_EInkPort(void);
#endif
