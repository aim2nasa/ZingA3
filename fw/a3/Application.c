#include "Application.h"
#include "gitcommit.h"
#include "DebugConsole.h"
#include "USB_Handler.h"
#include "gpif/PIB.h"
#include "i2c.h"
#include <gpio.h>
#include "ZingHw.h"
#include "cyu3error.h"
#include "Zing.h"
#include "dma.h"
#include "ControlCh.h"
#include "USB_EP0.h"

CyBool_t IsApplnActive = CyFalse;		//Whether the application is active or not

/* This function starts the application. This is called
 * when a SET_CONF event is received from the USB host. The endpoints
 * are configured and the DMA pipe is setup in this function. */
void AppStart(void)
{
	uint16_t size = 0;
	CyU3PEpConfig_t epCfg;
	CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
	CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();

    /* First identify the usb speed. Once that is identified,
     * create a DMA channel and start the transfer on this. */

    /* Based on the Bus Speed configure the endpoint packet size */
	switch(usbSpeed)
	{
	case CY_U3P_FULL_SPEED:
		size = 64;
		break;
	case CY_U3P_HIGH_SPEED:
		size = 512;
		break;
	case  CY_U3P_SUPER_SPEED:
		size = 1024;
		break;
	default:
		CyFxAppErrorHandler("Invalid USB speed",CY_U3P_ERROR_FAILURE);
		break;
	}

	CyU3PMemSet((uint8_t *)&epCfg, 0, sizeof (epCfg));
	epCfg.enable = CyTrue;
	epCfg.epType = CY_U3P_USB_EP_BULK;
	epCfg.burstLen = 1;
	epCfg.streams = 0;
	epCfg.pcktSize = size;

	apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CONTROL_OUT, &epCfg);
	if (apiRetStatus != CY_U3P_SUCCESS)
	{
		CyFxAppErrorHandler("CyU3PSetEpConfig(CY_FX_EP_CONTROL_OUT)",apiRetStatus);
	}

	apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CONTROL_IN, &epCfg);
	if (apiRetStatus != CY_U3P_SUCCESS)
	{
		CyFxAppErrorHandler("CyU3PSetEpConfig(CY_FX_EP_CONTROL_IN)",apiRetStatus);
    }

	CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
	epCfg.enable = CyTrue;
	epCfg.epType = CY_U3P_USB_EP_BULK;
	epCfg.burstLen = (usbSpeed == CY_U3P_SUPER_SPEED) ? (CY_FX_DATA_BURST_LENGTH) : 1;
	epCfg.streams = 0;
	epCfg.pcktSize = size;

	apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_DATA_OUT, &epCfg);
	if (apiRetStatus != CY_U3P_SUCCESS)
	{
		CyFxAppErrorHandler("CyU3PSetEpConfig(CY_FX_EP_DATA_OUT)",apiRetStatus);
	}

	apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_DATA_IN, &epCfg);
	if (apiRetStatus != CY_U3P_SUCCESS)
	{
		CyFxAppErrorHandler("CyU3PSetEpConfig(CY_FX_EP_DATA_IN)",apiRetStatus);
	}

	/* Update the status flag. */
	IsApplnActive = CyTrue;
}

/* This function stops the application. This shall be called whenever
 * a RESET or DISCONNECT event is received from the USB host. The endpoints are
 * disabled and the DMA pipe is destroyed by this function. */
void AppStop(void)
{
	CyU3PEpConfig_t epCfg;
	CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

	/* Update the flag. */
	IsApplnActive = CyFalse;

	/* Flush the endpoint memory */
	CyU3PUsbFlushEp(CY_FX_EP_CONTROL_OUT);
	CyU3PUsbFlushEp(CY_FX_EP_CONTROL_IN);
	CyU3PUsbFlushEp(CY_FX_EP_DATA_OUT);
	CyU3PUsbFlushEp(CY_FX_EP_DATA_IN);

	/* Disable endpoints. */
	CyU3PMemSet((uint8_t *)&epCfg, 0, sizeof (epCfg));
	epCfg.enable = CyFalse;

	/* Control OUT: Producer endpoint configuration. */
	apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CONTROL_OUT, &epCfg);
	if (apiRetStatus != CY_U3P_SUCCESS)
	{
		CyFxAppErrorHandler("CyU3PSetEpConfig(CY_FX_EP_CONTROL_OUT)",apiRetStatus);
	}

    /* Control IN: Consumer endpoint configuration. */
	apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CONTROL_IN, &epCfg);
	if (apiRetStatus != CY_U3P_SUCCESS)
	{
		CyFxAppErrorHandler("CyU3PSetEpConfig(CY_FX_EP_CONTROL_IN)",apiRetStatus);
    }

	/* Data OUT: Producer endpoint configuration */
	apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_DATA_OUT, &epCfg);
	if (apiRetStatus != CY_U3P_SUCCESS)
    {
		CyFxAppErrorHandler("CyU3PSetEpConfig(CY_FX_EP_DATA_OUT)",apiRetStatus);
    }

	/* Data IN: Consumer endpoint configuration */
	apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_DATA_IN, &epCfg);
	if (apiRetStatus != CY_U3P_SUCCESS)
	{
		CyFxAppErrorHandler("CyU3PSetEpConfig(CY_FX_EP_DATA_IN)",apiRetStatus);
	}
}

void ApplicationThread(uint32_t Value)
{
	CyU3PReturnStatus_t Status;

	Status = InitializeDebugConsole(6,NULL);
	CheckStatus("[App] InitializeDebugConsole", Status);

	CyU3PDebugPrint(4,"[App] Git:%s\n",GIT_INFO);

	Status = USBEP0RxThread_Create();
	CheckStatus("[App] USBEP0RxThread_Create", Status);

	Status = SetupGPIO();
	CheckStatus("[App] SetupGPIO", Status);

	Status = I2C_Init();
	CheckStatus("[App] I2C_Init", Status);

	Status = USB_Init();
	CheckStatus("[App] USB_Init", Status);

	Status = PIB_Init();
	CheckStatus("[App] PIB_Init", Status);

	initDma(CY_FX_EP_CONTROL_IN,CY_FX_EP_CONTROL_OUT,CY_FX_EP_DATA_IN,CY_FX_EP_DATA_OUT,CY_FX_DATA_BURST_LENGTH);
	CheckStatus("[App] DMA_Sync",DMA_Sync());

	Status = ControlChThread_Create();
	CheckStatus("[App] ControlChThread_Create", Status);

	Status = Zing_Init();
	CheckStatus("[App] Zing_Init", Status);

#if 0
	Zing_AutoHRCP();
	Zing_SetHRCP(PPC);
#endif
	Zing_SetHRCP(DEV);

	Status = USB_Connect();
	CheckStatus("[App] USB_Connect", Status);

	while(IsApplnActive == 0) {
		CyU3PThreadSleep(100);
	}

	CheckStatus("[App] DMA_Normal",DMA_Normal());

	uint32_t loop = 0;
	while (1)
	{
		CyU3PDebugPrint(4,"[App] Loop:%d ConIn:%d ConOut:%d DataIn:%d DataOut:%d\r",
				loop++,Dma.ControlInCount_,Dma.ControlOutCount_,Dma.DataInCount_,Dma.DataOutCount_);

		CyU3PThreadSleep(1000);
	}

	while (1);	// Hang here
}
