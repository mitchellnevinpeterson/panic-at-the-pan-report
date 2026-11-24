#ifndef GAME_BUTTON_EINK_PORT_H
#define GAME_BUTTON_EINK_PORT_H
#include "EInkAsync.h"
/* Owns SPI1/E-ink and parks the two unused servo outputs LOW. No UART,
 * SPI2/status-display, joystick ADC, or framework timer changes. */
const EInkAsyncPort *ButtonEInkPort_Init(uint32_t (*clock_ms)(void));
#endif
