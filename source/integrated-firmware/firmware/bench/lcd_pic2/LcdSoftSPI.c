#include "LcdSoftSPI.h"

/* Core-timer ticks (20 MHz) per SCK half period. 40 = 2 us, the rate proven on
 * the LCD benches; the PIC2 game build passes 8 (0.4 us, ~1 MHz SCK incl.
 * overhead) so one HeatView step stays short. ST7789 allows far faster writes. */
#ifndef LCD_HALF_BIT_TICKS
#define LCD_HALF_BIT_TICKS 40u
#endif

/* Minimum half-bit hold measured from now; an interrupt only stretches it. */
static inline void half_bit_wait(void)
{
    const uint32_t start = ticks_now();
    while ((uint32_t)(ticks_now() - start) < (uint32_t)(LCD_HALF_BIT_TICKS)) {
    }
}

void display_spi_start(void)
{
    const uint32_t pins = LCD_MOSI | LCD_SCK | LCD_CS | LCD_DC | LCD_RST;
    unsigned irq = __builtin_disable_interrupts();
    DDPCONbits.JTAGEN = 0;
    ANSELBCLR = pins;
    ODCBCLR = pins;
    CNENBCLR = pins;
    CNPUBCLR = pins;
    CNPDBCLR = pins;
    LATBSET = LCD_CS | LCD_RST;
    LATBCLR = LCD_MOSI | LCD_SCK | LCD_DC;
    pps_unlock();
    RPB5R = 0;
    RPB7R = 0;
    RPB9R = 0;
    RPB13R = 0;
    pps_lock();
    TRISBCLR = pins;
    if (irq & 1u) __builtin_enable_interrupts();
}

bool display_spi_write(uint8_t value)
{
    /* Mode 0, MSB first; SCK period >= 2 * LCD_HALF_BIT_TICKS. Interrupts stay
     * enabled. Callers bound how many bytes they send at once (HeatView_Step).
     */
    for (uint8_t bit = 0x80u; bit; bit >>= 1) {
        LATBCLR = LCD_SCK;
        if (value & bit) LATBSET = LCD_MOSI;
        else LATBCLR = LCD_MOSI;
        half_bit_wait();
        LATBSET = LCD_SCK;
        half_bit_wait();
    }
    LATBCLR = LCD_SCK;
    return true; /* No LCD ACK/MISO: completion does not prove a visible image. */
}
