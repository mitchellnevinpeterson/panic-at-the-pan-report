#ifndef INGREDIENT_BENCH_MODEL_H
#define INGREDIENT_BENCH_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#define BENCH_LINK_TIMEOUT_MS 600u
#define BENCH_DEBOUNCE_MS 20u
#define BENCH_COOLDOWN_MS 3000u
typedef struct {
    uint8_t raw, stable;
    uint32_t changed[5], started;
} BenchInputs;
void BenchInputs_Init(BenchInputs *, uint32_t now);
uint8_t BenchInputs_Update(BenchInputs *, uint8_t gpio, uint32_t now);
bool BenchInputs_Ready(const BenchInputs *, uint32_t now);
typedef struct {
    uint8_t desired, selected, displayed, live, inhibited;
    uint32_t received, finished;
    bool valid, known, busy, clear_requested;
} BenchDisplay;
void BenchDisplay_Init(BenchDisplay *, uint32_t now);
void BenchDisplay_Receive(BenchDisplay *, uint8_t mask, bool valid, uint32_t now);
void BenchDisplay_Clear(BenchDisplay *);
bool BenchDisplay_Fresh(const BenchDisplay *, uint32_t now);
uint32_t BenchDisplay_Wait(const BenchDisplay *, uint32_t now);
bool BenchDisplay_Select(BenchDisplay *, uint32_t now);
bool BenchDisplay_Current(const BenchDisplay *, uint32_t now);
void BenchDisplay_Finish(BenchDisplay *, uint32_t now);
#endif
