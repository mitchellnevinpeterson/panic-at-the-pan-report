#ifndef PEER_UART_H
#define PEER_UART_H
#include "PeerLink.h"
typedef struct {
    uint32_t tx_bytes, rx_bytes, overruns, rx_errors, tx_full;
} PeerUartStats;
PeerUartStats PeerUart_GetStats(void);
#ifdef GAME_BUTTONS_BENCH
typedef struct {
    uint32_t rx_low, rx_high, tx_low, tx_high;
    /* cfg: 1=UART/baud, 2=RX PPS, 4=TX PPS, 8=GPIO, 16=JTAG/I2C1,
     * 32=PBCLK divider. Zero means the checked configuration matches. */
    uint8_t config_errors;
    bool loopback_passed;
} PeerUartDiagnostics;
PeerUartDiagnostics PeerUart_GetDiagnostics(void);
#endif
void PeerUart_Init(void);
bool PeerUart_Send(const uint8_t *, size_t, void *);
void PeerUart_Service(PeerLink *);
/* True when the software TX queue is empty (safe to block the foreground
 * briefly without delaying an already queued frame). */
bool PeerUart_TxIdle(void);
/* Single-producer (UART2 RX ISR) / single-consumer (PeerUart_Service) byte
 * ring used with PEER_UART_ISR. 8-bit indices wrap at 256; one slot stays
 * empty, so 255 bytes fit. Header-only so the host tests can exercise it. */
typedef struct { volatile uint8_t head, tail; volatile uint8_t data[256]; } PeerRxRing;
static inline bool PeerRxRing_Push(PeerRxRing *r, uint8_t byte) {
    uint8_t head = r->head, next = (uint8_t)(head + 1u);
    if (next == r->tail) return false; /* Full: caller latches an overrun. */
    r->data[head] = byte; r->head = next; return true;
}
static inline bool PeerRxRing_Pop(PeerRxRing *r, uint8_t *byte) {
    uint8_t tail = r->tail;
    if (tail == r->head) return false;
    *byte = r->data[tail]; r->tail = (uint8_t)(tail + 1u); return true;
}
/* Consumer side only: discard everything received so far. */
static inline void PeerRxRing_Flush(PeerRxRing *r) { r->tail = r->head; }
#endif
