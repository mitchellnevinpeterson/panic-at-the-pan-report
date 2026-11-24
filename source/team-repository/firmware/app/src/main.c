#include "BoardConfig.h"
#include "ES_Framework.h"
#include "ES_Port.h"

/* Inspect this symbol in the debugger if initialization or service run fails. */
volatile ES_Return_t FirmwareStatus = Success;

int main(void)
{
    /* Reused course setup initializes UART1; it is not a project I/O init. */
    _HW_PIC32Init();
    FirmwareStatus = ES_Initialize(ES_Timer_RATE_1mS);
    if (FirmwareStatus == Success) {
        FirmwareStatus = ES_Run();
    }
    /* Terminal fault halt only. Service timing must use ES timers/events. */
    for (;;) {
        /* No project actuator has been enabled by this scaffold. */
    }
}
