#ifndef PANIC_LCD_SOFT_SPI_H
#define PANIC_LCD_SOFT_SPI_H
#include "board.h"
#include <stdbool.h>

/* PIC2 physical pins. No shared SPI1, UART2 or audio pins. */
#define LCD_MOSI (1u << 5)  /* RB5,  pin14 */
#define LCD_SCK  (1u << 7)  /* RB7,  pin16 */
#define LCD_CS   (1u << 9)  /* RB9,  pin18 */
#define LCD_DC   (1u << 12) /* RB12, pin23 */
#define LCD_RST  (1u << 13) /* RB13, pin24 */

void display_spi_start(void);
bool display_spi_write(uint8_t value);
#endif
