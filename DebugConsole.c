#include "DebugConsole.h"
#include "cyu3error.h"

static CyBool_t DebugTxEnabled = CyFalse;	// Set true once I can output messages to the Console

void CheckStatus(char* StringPtr, CyU3PReturnStatus_t Status)
// In this initial debugging stage I stall on an un-successful system call, else I display progress
// Note that this assumes that there were no errors bringing up the Debug Console
{
	if (DebugTxEnabled)				// Need to wait until ConsoleOut is enabled
	{
		if (Status == CY_U3P_SUCCESS) CyU3PDebugPrint(4, "%s OK\n", StringPtr);
		else
		{
			// else hang here
			CyU3PDebugPrint(4, "\r\n%s failed, Status = %d\n", StringPtr, Status);
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

void UartCallback(CyU3PUartEvt_t Event, CyU3PUartError_t Error)
// Handle characters typed in by the developer
// Later we will respond to commands terminated with a <CR>
{
	if (Event == CY_U3P_UART_EVENT_RX_DONE)
    {
    }
}

CyU3PReturnStatus_t InitializeDebugConsole(uint8_t TraceLevel)
{
	CyU3PUartConfig_t uartConfig;
	CyU3PReturnStatus_t Status = CY_U3P_SUCCESS;

	Status = CyU3PUartInit();		// Start the UART driver
	CheckStatus("CyU3PUartInit", Status);

	CyU3PMemSet ((uint8_t *)&uartConfig, 0, sizeof (uartConfig));
	uartConfig.baudRate = CY_U3P_UART_BAUDRATE_115200;
	uartConfig.stopBit  = CY_U3P_UART_ONE_STOP_BIT;
	uartConfig.parity   = CY_U3P_UART_NO_PARITY;
	uartConfig.txEnable = CyTrue;
	uartConfig.rxEnable = CyTrue;
	uartConfig.flowCtrl = CyFalse;
	uartConfig.isDma    = CyTrue;

	Status = CyU3PUartSetConfig(&uartConfig, UartCallback);				// Configure the UART hardware
	CheckStatus("CyU3PUartSetConfig", Status);

	Status = CyU3PUartTxSetBlockXfer(0xFFFFFFFF);						// Send as much data as I need to
	CheckStatus("CyU3PUartTxSetBlockXfer", Status);

	Status = CyU3PDebugInit(CY_U3P_LPP_SOCKET_UART_CONS, TraceLevel);	// Attach the Debug driver above the UART driver
	if (Status == CY_U3P_SUCCESS) DebugTxEnabled = CyTrue;
    CheckStatus("ConsoleOutEnabled", Status);

	CyU3PDebugPreamble(CyFalse);										// Skip preamble, debug info is targeted for a person
	return Status;
}
