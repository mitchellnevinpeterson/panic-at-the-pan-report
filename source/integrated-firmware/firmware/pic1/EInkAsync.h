#ifndef PANIC_EINK_ASYNC_H
#define PANIC_EINK_ASYNC_H
#include "EInkDisplay.h"

typedef enum { EINK_BUS_IDLE, EINK_BUS_BUSY, EINK_BUS_FAULT } EInkBusState;
typedef struct {
    bool (*start)(bool data, const uint8_t *bytes, size_t count);
    EInkBusState (*poll)(void);
    void (*reset)(bool high);
    bool (*busy_level)(void);
    uint32_t (*now_ms)(void);
} EInkAsyncPort;

/* IL0373/Z10 only: same normal waveform as the tested bench. Each Service
 * advances at most one transfer/wait; frame must remain immutable while busy.
 * Default cooldown is 180 s, including after boot. A caller may explicitly
 * select another nonzero interval with InitWithCooldown for its own bench.
 */
bool EInkAsync_Init(const EInkAsyncPort *port);
bool EInkAsync_InitWithCooldown(const EInkAsyncPort *port, uint32_t cooldown_ms);
/* Opt-in fast black/white full refresh (custom register waveform, ~2 s).
 * Black and white only; red pixels are drawn black. */
bool EInkAsync_InitFastMono(const EInkAsyncPort *port, uint32_t cooldown_ms);
bool EInkAsync_CanStart(void);
bool EInkAsync_Start(const EInkFrame *frame);
bool EInkAsync_Service(void); /* false = latched I/O/BUSY fault */
bool EInkAsync_IsBusy(void);
EInkResult EInkAsync_Result(void);
#endif
