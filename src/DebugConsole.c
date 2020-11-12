#include "DebugConsole.h"
#include "cyu3error.h"
#include "macro.h"

static CyBool_t DebugTxEnabled = CyFalse;	// Set true once I can output messages to the Console

void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status)
{
	if (DebugTxEnabled)				// Need to wait until ConsoleOut is enabled
	{
		if (Status == CY_U3P_SUCCESS) CyU3PDebugPrint(4, "%s OK\r\n", StringPtr);
		else
		{
			// else hang here
			CyU3PDebugPrint(4, "\r\n%s failed, Status = %d\r\n", StringPtr, Status);
			while (1)
			{
				CyU3PDebugPrint(4, "?");
				CyU3PThreadSleep (1000);
			}
		}
	}
}

void CyFxAppErrorHandler(char* StringPtr,CyU3PReturnStatus_t Status)
{
	for (;;)
	{
		if(DebugTxEnabled) CyU3PDebugPrint(4,"Error in %s(0x%x)\n",StringPtr,Status);
		CyU3PThreadSleep(100);
	}
}

CyU3PReturnStatus_t InitializeDebugConsole(uint8_t TraceLevel,CyU3PUartIntrCb_t UartCallback)
{
	CyU3PUartConfig_t uartConfig;

	CHECK(CyU3PUartInit());											// Start the UART driver

	CyU3PMemSet ((uint8_t *)&uartConfig, 0, sizeof (uartConfig));
	uartConfig.baudRate = CY_U3P_UART_BAUDRATE_115200;
	uartConfig.stopBit  = CY_U3P_UART_ONE_STOP_BIT;
	uartConfig.parity   = CY_U3P_UART_NO_PARITY;
	uartConfig.txEnable = CyTrue;
	uartConfig.rxEnable = CyTrue;
	uartConfig.flowCtrl = CyFalse;
	uartConfig.isDma    = CyTrue;

	CHECK(CyU3PUartSetConfig(&uartConfig, UartCallback));			// Configure the UART hardware
	CHECK(CyU3PUartTxSetBlockXfer(0xFFFFFFFF));						// Send as much data as I need to
	CHECK(CyU3PDebugInit(CY_U3P_LPP_SOCKET_UART_CONS, TraceLevel));	// Attach the Debug driver above the UART driver

	DebugTxEnabled = CyTrue;

	CyU3PDebugPreamble(CyFalse);									// Skip preamble, debug info is targeted for a person
	return CY_U3P_SUCCESS;
}
