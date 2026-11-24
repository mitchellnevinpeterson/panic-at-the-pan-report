#ifndef PANIC_STATUS_DISPLAY_SPI_H
#define PANIC_STATUS_DISPLAY_SPI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    STATUS_SPI_IDLE,
    STATUS_SPI_BUSY,
    STATUS_SPI_FAULT
} StatusDisplaySPI_State;

/* U1 SPI2: SDO2/RB11/p22, SCK2/RB15/p26, manual SS/RB13/p24.
 * Assumes SYSCLK=40 MHz, PBCLK=20 MHz and an advancing CP0 Count.
 * Single foreground owner; call Poll frequently (well under 5 ms).
 */
void StatusDisplaySPI_Init(void);
/* Copies up to eight words. Array order is wire order: farthest chip first. */
bool StatusDisplaySPI_Start(const uint16_t *words, uint8_t count);
StatusDisplaySPI_State StatusDisplaySPI_Poll(void);

#endif
