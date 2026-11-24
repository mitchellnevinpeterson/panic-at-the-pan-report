#ifndef DISPLAY_SPI_H
#define DISPLAY_SPI_H

#include "board.h"
#include <stdbool.h>

static inline void display_spi_start(void)
{
    const uint32_t mosi = 1u << 1; /* RA1, DIP 3 */
    const uint32_t sck = 1u << 14; /* RB14, DIP 25 */
    SPI1CON = 0;
    SPI1CON2 = 0;
    SPI1STATCLR = _SPI1STAT_SPIROV_MASK;

    LATACLR = mosi;
    LATBCLR = sck;
    ANSELACLR = mosi;
    ANSELBCLR = sck;
    TRISACLR = mosi;
    TRISBCLR = sck;

    pps_unlock();
    RPA1R = 3u; /* SDO1 */
    pps_lock();

    SPI1BRG = 9u; /* 1 MHz at 20 MHz PBCLK */
    SPI1CONbits.MSTEN = 1;
    SPI1CONbits.CKE = 1; /* SPI mode 0 */
    SPI1CONbits.ON = 1;
}

static inline bool display_spi_write(uint8_t value)
{
    SPI1BUF = value;
    const uint32_t start = ticks_now();
    while (!SPI1STATbits.SPIRBF) {
        if ((uint32_t)(ticks_now() - start) >= 1000u * CORE_TICKS_PER_US) {
            return false;
        }
    }
    (void)SPI1BUF; /* Drain the received byte after the last clock. */
    return true;
}

#endif
