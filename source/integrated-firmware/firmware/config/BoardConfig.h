#ifndef PANIC_BOARD_CONFIG_H
#define PANIC_BOARD_CONFIG_H
/* Allocation reference only. Hardware enable choices live in pic1/Pic1Config.h.
 * SYSCLK=40 MHz, PBCLK=20 MHz. Both devices are PIC32MX170F256B DIP28.
 *
 * PIC1: UART1 RX15/RB6 TX16/RB7, 115200 (course framework).
 * SPI1 E-ink: MOSI3/RA1 SCK25/RB14 SS2/RA0 DC11/RB4 RST18/RB9 BUSY23/RB12.
 * SPI2 status: MOSI22/RB11 SCK26/RB15 SS24/RB13, eight MAX7219s.
 * Joystick6/RB2/AN4 +7/RB3/AN5. Spoon12/RA4/OC4 Timer2, 50 Hz.
 * Treat14/RB5 reserved, held LOW; no dispenser PWM. No flame servo.
 * Core timer supplies course ES tick; never reset CP0 Count.
 *
 * PIC2: TRIG6/RB2 ECHO7/RB3, nonblocking Core-counter polling.
 * Audio26/RB15/OC1 Timer2 PWM78.125 kHz +Timer3 samples12.5 kHz.
 * SPI1 MCP23S17 SS2/RA0 MOSI3/RA1 MISO22/RB11 SCK25/RB14.
 * GPA0..4 ingredients, GPA5 Finish, GPA6 disabled flip, GPA7 Start.
 * GPB0 Finish lamp; initialize LOW before enabling output.
 * Debug UART1 TX11/RB4 optional RX15/RB6 remains unused by the new PIC2 image.
 *
 * Both: UART2 TX21/RB10 RX17/RB8, crossed, 1152008N1.
 * SNAP PGED1/RB0/p4 and PGEC1/RB1/p5 retained.
 * Keep LED_DEBUG disabled in the preserved framework: RB15 belongs to SPI2.
 */
#endif
