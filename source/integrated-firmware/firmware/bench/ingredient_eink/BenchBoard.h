#ifndef INGREDIENT_BENCH_BOARD_H
#define INGREDIENT_BENCH_BOARD_H
#include "BenchWire.h"
#include "EInkAsync.h"
void BenchBoard_Init(void);
uint32_t BenchBoard_Now(void);
bool BenchBoard_Poll(BenchPacket *);
void BenchBoard_Send(const BenchPacket *);
int BenchBoard_Key(void);
void BenchBoard_Text(const char *);
void BenchBoard_Hex(uint8_t);
void BenchBoard_Number(uint32_t);
const EInkAsyncPort *BenchBoard_EInkPort(void);
uint32_t BenchBoard_UartErrors(void);
#endif
