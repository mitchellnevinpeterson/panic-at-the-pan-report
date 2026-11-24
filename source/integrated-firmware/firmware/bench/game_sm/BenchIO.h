#ifndef PANIC_GAME_BENCH_IO_H
#define PANIC_GAME_BENCH_IO_H
#include <stdbool.h>
#include <stdint.h>
bool GameBenchIO_Init(void);
bool CheckGameBenchIO(void);
#ifdef GAME_BUTTONS_BENCH
void BenchIO_SetNextDish(uint8_t index); /* force the next Start's dish once (tests) */
uint8_t BenchIO_Dish(void);              /* dish of the current or last game; DISH_COUNT before any */
/* Random dish, never `previous` (DISH_COUNT = none). Used at every Start. */
uint8_t BenchIO_RandomDish(uint32_t entropy, uint8_t previous);
#endif
#endif
