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
	CyU3PDebugPrint (4, "CyU3PUsbGetSpeed = %d\n",usbSpeed);

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
	CheckStatus("[App] InitializeDebugConsole", InitializeDebugConsole(6,NULL));

	CyU3PDebugPrint(4,"[App] Git:%s\n",GIT_INFO);

	CheckStatus("[App] USBEP0RxThread_Create", USBEP0RxThread_Create());
	CheckStatus("[App] SetupGPIO", SetupGPIO());
	CheckStatus("[App] I2C_Init", I2C_Init());
	CheckStatus("[App] PIB_Init", PIB_Init());

	initDma(CY_FX_EP_CONTROL_IN,CY_FX_EP_CONTROL_OUT,CY_FX_EP_DATA_IN,CY_FX_EP_DATA_OUT,CY_FX_DATA_BURST_LENGTH);
	CheckStatus("[App] DMA_Sync",DMA_Sync());

	CheckStatus("[App] ControlChThread_Create", ControlChThread_Create());
	CheckStatus("[App] Zing_Init", Zing_Init());

#ifdef HRCP_PPC
	CheckStatus("[App] Zing_SetHRCP(PPC)",Zing_SetHRCP(PPC));
#else
	CheckStatus("[App] Zing_SetHRCP(DEV)",Zing_SetHRCP(DEV));
#endif

#ifdef OTG
	CheckStatus("[App] CyFxApplnInit()", CyFxApplnInit());
#else
	CheckStatus("[App] USB_Init", USB_Init());
	CheckStatus("[App] USB_Connect", USB_Connect());
	while(IsApplnActive == 0) {
		CyU3PThreadSleep(100);
	}

	CheckStatus("[App] DMA_Normal",DMA_Normal());
	CyU3PDebugPrint(4,"[App] DMA Nomal mode uses ");
#ifdef DMA_NORMAL_MANUAL
	CyU3PDebugPrint(4,"Manual mode\n");
#else
	CyU3PDebugPrint(4,"Auto Signal mode\n");
#endif	//DMA_NORMAL_MANUAL
#endif	//OTG

#ifndef OTG
	uint32_t loop = 0;
	while (1)
	{
		CyU3PDebugPrint(4,"[App] (%s)(%s) Loop:%d ConIn:%d ConOut:%d DataIn:%d DataOut:%d\r",
						Zing_GetHRCP()?"PPC":"DEV",dmaModeStr(Dma.Mode_),
						loop++,Dma.ControlIn_.Count_,Dma.ControlOut_.Count_,Dma.DataIn_.Count_,Dma.DataOut_.Count_);

		CyU3PThreadSleep(100);
	}
#else
	CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
	uint32_t evStat;
	for (;;)
	{
		/* Wait until a peripheral change event has been detected, or until the poll interval has elapsed. */
		status = CyU3PEventGet (&applnEvent, CY_FX_USB_CHANGE_EVENT, CYU3P_EVENT_OR_CLEAR,
				&evStat, CY_FX_HOST_POLL_INTERVAL);

		/* If a peripheral change has been detected, go through device discovery. */
		if ((status == CY_U3P_SUCCESS) && ((evStat & CY_FX_USB_CHANGE_EVENT) != 0))
		{
			CyU3PDebugPrint (4, "CY_FX_USB_CHANGE_EVENT event\r\n");
			/* Add some delay for debouncing. */
			CyU3PThreadSleep (100);

			/* Clear the CHANGE event notification, so that we do not react to stale events. */
			CyU3PEventGet (&applnEvent, CY_FX_USB_CHANGE_EVENT, CYU3P_EVENT_OR_CLEAR, &evStat, CYU3P_NO_WAIT);

			/* Stop previously started application. */
			if (glIsApplnActive)
			{
				CyFxApplnStop ();
			}

			/* If a peripheral got connected, then enumerate and start the application. */
			if (glIsPeripheralPresent)
			{
				CyU3PDebugPrint (2, "Enable host port\r\n");
				status = CyU3PUsbHostPortEnable ();
				if (status == CY_U3P_SUCCESS)
				{
					CyFxApplnStart ();
					CyU3PDebugPrint (2, "App start completed\r\n");

					/* If following conditions are met, then the phone is successfully initialized.
					 * it's time to create DMA Channel between PIB-USB */
					if (glIsApplnActive && HostOwner == CY_FX_HOST_OWNER_PHONE_DRIVER) {

					}
				}
				else
				{
					CyU3PDebugPrint (2, "HostPortEnable failed with code %d\r\n", status);
				}
			}
		}
	}
#endif

	while (1);	// Hang here
}

CyU3PThread ApplicationThreadHandle;

void CyFxApplicationDefine(void)
{
    void *StackPtr = NULL;
    uint32_t Status;

    StackPtr = CyU3PMemAlloc(APPLICATION_THREAD_STACK);
    Status = CyU3PThreadCreate(&ApplicationThreadHandle,	// Handle to my Application Thread
            "11:Step1",										// Thread ID and name
            ApplicationThread,								// Thread entry function
            1,												// Parameter passed to Thread
            StackPtr,										// Pointer to the allocated thread stack
            APPLICATION_THREAD_STACK,						// Allocated thread stack size
            APPLICATION_THREAD_PRIORITY,					// Thread priority
            APPLICATION_THREAD_PRIORITY,					// Thread priority so no preemption
            CYU3P_NO_TIME_SLICE,							// Time slice no supported
            CYU3P_AUTO_START								// Start the thread immediately
            );
    if (Status != CY_U3P_SUCCESS) goto InitError;

#ifdef OTG
    Status = CyU3PEventCreate (&applnEvent);
    if (Status != 0) goto InitError;
#endif

    return;
InitError:
	while(1);
}
