#include "BenchIO.h"
#include "ES_Framework.h"
#include "terminal.h"
#include <stdio.h>

volatile ES_Return_t FirmwareStatus = Success;

int main(void)
{
    /* Course port owns config words, Core Timer tick, and UART1. */
    _HW_PIC32Init();
    printf("\r\nGameSMBench: b=Start, f=Finish, a=activity, 1-5=ingredients\r\n"
           "Start -> dish scroll -> 60s game. Finish after 30s; idle timeout after 20s.\r\n"
           "Bench scoring: +10 per new ingredient. All ingredient keys count as activity.\r\n"
           "Finish LED / treat / Heat / Stir / Pan remain terminal stubs.\r\n");
    FirmwareStatus = GameBenchIO_Init() ? ES_Initialize(ES_Timer_RATE_1mS) : FailedInit;
    if (FirmwareStatus == Success) FirmwareStatus = ES_Run();
    printf("\r\nFAULT: stopped (%d). Reset after checking wiring/UART.\r\n", FirmwareStatus);
    for (;;) Terminal_MoveBuffer2UART();
}
