#ifndef PANIC_GAME_IO_H
#define PANIC_GAME_IO_H
#include <stdbool.h>
#include <stdint.h>
bool GameIO_Init(void); /* Bind all six services before ES_Initialize. */
bool GameIO_Service(void);
void GameIO_Stop(void); /* Called on any framework/service fault. */
bool GameIO_Joystick(uint32_t angle_mdeg, bool home, bool moved, uint32_t at_ms);
#endif
