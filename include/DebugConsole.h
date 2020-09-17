#ifndef __DEBUG_CONSOLE_H__
#define __DEBUG_CONSOLE_H__

#include "cyu3types.h"
#include "cyu3uart.h"

#define CHECK(x) do { CyU3PReturnStatus_t retval = (x); if(retval != CY_U3P_SUCCESS) { return retval; } } while (0)

void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status);
void CyFxAppErrorHandler(char* StringPtr,CyU3PReturnStatus_t Status);
CyU3PReturnStatus_t InitializeDebugConsole(uint8_t TraceLevel,CyU3PUartIntrCb_t UartCallback);

#endif
