#ifndef PANIC_BOARD_CONFIG_H
#define PANIC_BOARD_CONFIG_H

/* Hardware allocation V2 is recorded in hardware/docs/pin_map.md.
 * GPIO/PPS/ADC/timer initialization is not implemented here yet.
 * This flag is descriptive, not a hardware enable switch.
 */
#define BOARD_IO_CONFIGURED 0
#define FIRMWARE_SCAFFOLD_ONLY 1

/* Inherited course platform (vendor/es_framework):
 * SYSCLK 40 MHz; PBCLK 20 MHz; core timer owns ES tick.
 * UART1 RX RB6 / TX RB7, 115200 baud (USE_RB2_3 is NOT defined).
 * ICSP: PGED1 RB0 / PGEC1 RB1, MCLR.
 * LED_DEBUG, if manually enabled in ES_Port.c, also owns RB15.
 * SPI1 display: SCK1 RB14, SDO1 RA1, manual SS RA0; no MISO.
 * SPI2 expander: SCK2 RB15, SDO2 RB11, SDI2 RB13, manual SS RB8.
 * Treat PWM: OC2 RB5; ultrasonic TRIG: RB12. Finish LED: MCP GPB0.
 * These are schematic reservations; the reused HAL is not configured for them.
 */

#endif
