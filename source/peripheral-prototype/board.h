#ifndef BOARD_H
#define BOARD_H

#include <xc.h>
#include <stdint.h>
#include <sys/attribs.h>

/* DeviceConfig.c sets SYSCLK to 40 MHz and PBCLK to 20 MHz.
 * The core timer increments at SYSCLK / 2. */
#define CORE_TICKS_PER_US 20u

static inline uint32_t ticks_now(void)
{
    return _CP0_GET_COUNT();
}

static inline void delay_us(uint32_t us)
{
    const uint32_t start = ticks_now();
    const uint32_t ticks = us * CORE_TICKS_PER_US;
    while ((uint32_t)(ticks_now() - start) < ticks) {
    }
}

static inline void pps_unlock(void)
{
    SYSKEY = 0;
    SYSKEY = 0xAA996655u;
    SYSKEY = 0x556699AAu;
    CFGCONbits.IOLOCK = 0;
}

static inline void pps_lock(void)
{
    CFGCONbits.IOLOCK = 1;
    SYSKEY = 0;
}

#endif
