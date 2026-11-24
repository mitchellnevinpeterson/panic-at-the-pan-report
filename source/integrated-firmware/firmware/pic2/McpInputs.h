#ifndef PIC2_MCP_H
#define PIC2_MCP_H
#include <stdbool.h>
#include <stdint.h>
bool McpInputs_Init(void);
/* GPB output bit mask. Init() retains the original GPB0-only behavior. */
bool McpInputs_InitOutputs(uint8_t output_mask);
/* Asynchronous SPI, one byte progress per call. False means peripheral fault. */
bool McpInputs_Service(uint32_t now_ms,bool lamp);
bool McpInputs_ServiceOutputs(uint32_t now_ms,uint8_t outputs);
bool McpInputs_Take(uint8_t *gpio);
/* True while a 3-byte SPI transaction is in flight (SS LOW). */
bool McpInputs_Busy(void);
#endif
