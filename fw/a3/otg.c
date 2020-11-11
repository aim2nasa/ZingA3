#ifdef OTG

#include "Application.h"
#include "cyu3error.h"
#include "cyu3system.h"
#include "cyu3usb.h"
#include "cyu3usbhost.h"
#include "cyu3gpio.h"
#include "cyu3utils.h"
#include "cyfxmousedrv.h"
#include "otg.h"
#include "Zing.h"
#include "phonedrv.h"

/* Setup packet buffer for host mode. */
uint8_t glSetupPkt[CY_FX_HOST_EP0_SETUP_SIZE] __attribute__ ((aligned (32)));

/* Buffer to hold the USB device descriptor. */
uint8_t glDeviceDesc[32] __attribute__ ((aligned (32)));

uint8_t glHostOwner = CY_FX_HOST_OWNER_NONE;    /* Current owner for the host controller. */

/* USB host stack event callback function. */
void CyFxHostEventCb (CyU3PUsbHostEventType_t evType, uint32_t evData)
{
    /* This is connect / disconnect event. Log it so that the
     * application thread can handle it. */
    if (evType == CY_U3P_USB_HOST_EVENT_CONNECT)
    {
        CyU3PDebugPrint (4, "Host connect event received\r\n");
        glIsPeripheralPresent = CyTrue;
    }
    else
    {
        CyU3PDebugPrint (4, "Host disconnect event received\r\n");
        glIsPeripheralPresent = CyFalse;
    }

    /* Signal the thread that a peripheral change has been detected. */
    CyU3PEventSet (&applnEvent, CY_FX_USB_CHANGE_EVENT, CYU3P_EVENT_OR);
}

/* Helper function to send EP0 setup packet. */
CyU3PReturnStatus_t CyFxSendSetupRqt (uint8_t type,uint8_t request,
		uint16_t value,uint16_t index,uint16_t length,uint8_t *buffer_p)
{
    CyU3PUsbHostEpStatus_t epStatus;
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    glSetupPkt[0] = type;
    glSetupPkt[1] = request;
    glSetupPkt[2] = CY_U3P_GET_LSB(value);
    glSetupPkt[3] = CY_U3P_GET_MSB(value);
    glSetupPkt[4] = CY_U3P_GET_LSB(index);
    glSetupPkt[5] = CY_U3P_GET_MSB(index);
    glSetupPkt[6] = CY_U3P_GET_LSB(length);
    glSetupPkt[7] = CY_U3P_GET_MSB(length);

    status = CyU3PUsbHostSendSetupRqt (glSetupPkt, buffer_p);
    if (status != CY_U3P_SUCCESS)
    {
        return status;
    }
    status = CyU3PUsbHostEpWaitForCompletion (0, &epStatus,
            CY_FX_HOST_EP0_WAIT_TIMEOUT);

    return status;
}

/* This function initializes the mouse driver application. */
void CyFxApplnStart ()
{
    uint16_t length;
    CyU3PReturnStatus_t status;
    CyU3PUsbHostEpConfig_t epCfg;

	CyU3PDebugPrint (2, "CyFxApplnStart()\r\n");

    /* Add EP0 to the scheduler. */
    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof(epCfg));
    epCfg.type = CY_U3P_USB_EP_CONTROL;
    epCfg.mult = 1;
    /* Start off with 8 byte EP0 packet size. */
    epCfg.maxPktSize = 8;
    epCfg.pollingRate = 0;
    epCfg.fullPktSize = 8;
    epCfg.isStreamMode = CyFalse;
    status = CyU3PUsbHostEpAdd (0, &epCfg);
    if (status != CY_U3P_SUCCESS)
    {
        goto enum_error;
    }

    CyU3PThreadSleep (100);

    /* Get the device descriptor. */
    status = CyFxSendSetupRqt (0x80, CY_U3P_USB_SC_GET_DESCRIPTOR,
            (CY_U3P_USB_DEVICE_DESCR << 8), 0, 8, glEp0Buffer);
    if (status != CY_U3P_SUCCESS)
    {
        goto enum_error;
    }

    CyU3PDebugPrint (6, "Device descriptor received\r\n");

    /* Identify the EP0 packet size and update the scheduler. */
    if (glEp0Buffer[7] != 8)
    {
        CyU3PUsbHostEpRemove (0);
        epCfg.maxPktSize = glEp0Buffer[7];
        epCfg.fullPktSize = glEp0Buffer[7];
        status = CyU3PUsbHostEpAdd (0, &epCfg);
        if (status != CY_U3P_SUCCESS)
        {
            goto enum_error;
        }
    }

    /* Read the full device descriptor. */
    status = CyFxSendSetupRqt (0x80, CY_U3P_USB_SC_GET_DESCRIPTOR,
            (CY_U3P_USB_DEVICE_DESCR << 8), 0, 18, glEp0Buffer);
    if (status != CY_U3P_SUCCESS)
    {
        goto enum_error;
    }

    /* Save the device descriptor. */
    CyU3PMemCopy (glDeviceDesc, glEp0Buffer, 18);

    /* Set the peripheral device address. */
    status = CyFxSendSetupRqt (0x00, CY_U3P_USB_SC_SET_ADDRESS,
            CY_FX_HOST_PERIPHERAL_ADDRESS, 0, 0, glEp0Buffer);
    if (status != CY_U3P_SUCCESS)
    {
        goto enum_error;
    }
    status = CyU3PUsbHostSetDeviceAddress (CY_FX_HOST_PERIPHERAL_ADDRESS);
    if (status != CY_U3P_SUCCESS)
    {
        goto enum_error;
    }

    CyU3PDebugPrint (6, "Device address set\r\n");

    /* Read first four bytes of configuration descriptor to determine
     * the total length. */
    status = CyFxSendSetupRqt (0x80, CY_U3P_USB_SC_GET_DESCRIPTOR,
            (CY_U3P_USB_CONFIG_DESCR << 8), 0, 4, glEp0Buffer);
    if (status != CY_U3P_SUCCESS)
    {
        goto enum_error;
    }

    /* Identify the length of the data received. */
    length = CY_U3P_MAKEWORD(glEp0Buffer[3], glEp0Buffer[2]);
    if (length > CY_FX_HOST_EP0_BUFFER_SIZE)
    {
        goto enum_error;
    }

    /* Read the full configuration descriptor. */
    status = CyFxSendSetupRqt (0x80, CY_U3P_USB_SC_GET_DESCRIPTOR,
            (CY_U3P_USB_CONFIG_DESCR << 8), 0, length, glEp0Buffer);
    if (status != CY_U3P_SUCCESS)
    {
        goto enum_error;
    }

    CyU3PDebugPrint (6, "Ep0Buffer========\r\n");
    for(int i=0;i<32;i++) CyU3PDebugPrint (6,"%x ", glEp0Buffer[i]);
    CyU3PDebugPrint(6, "\r\n");

	status = CyFxSendSetupRqt(0x00, CY_U3P_USB_SC_SET_CONFIGURATION,
		1, 0, 0, glEp0Buffer);
	if (status != CY_U3P_SUCCESS)
	{
	    CyU3PDebugPrint (6, "Set configured Error!!\r\n");
	    goto enum_error;
	}

    /* Identify if this is an HID mouse or MSC device that can be
     * supported. If the device cannot be supported, just disable
     * the port and wait for a new device to be attached. We support
     * only single interface with interface class = HID(0x03),
     * interface sub class = Boot (0x01)
     * and interface protocol = Mouse (0x02).
     * or single interface with interface class = MSC(0x08),
     * interface sub class 0x06 and interface protocol BOT (0x50). */
    if ((glEp0Buffer[5] == 1) && (glEp0Buffer[14] == 0x03) &&
            (glEp0Buffer[15] == 0x01) && (glEp0Buffer[16] == 0x02) &&
            (glEp0Buffer[28] == CY_U3P_USB_ENDPNT_DESCR))
    {
        CyU3PDebugPrint (6, "Mouse device detected\r\n");
        status = CyFxMouseDriverInit ();
        if (status == CY_U3P_SUCCESS)
        {
            glIsApplnActive = CyTrue;
            glHostOwner = CY_FX_HOST_OWNER_MOUSE_DRIVER;
            return;
        }
    }

    /* Any device is okay for the moment because I don't have enough information yet */
    if(1)
    {
    	Zing_DataWrite((uint8_t*)"PING ON", strlen("PING ON"));
        CyU3PDebugPrint (6, "Smart phone is detected\r\n");
        status = PhoneDriverInit ();
        if (status == CY_U3P_SUCCESS)
        {
            glIsApplnActive = CyTrue;
            glHostOwner = CY_FX_HOST_OWNER_PHONE_DRIVER;
            CyU3PDebugPrint (6, "Smart phone driver is initialized\r\n");
            return;
        }
    }

    /* We do not support this device. Fall-through to disable the USB port. */
    CyU3PDebugPrint (6, "Unknown device type\r\n");
    status = CY_U3P_ERROR_NOT_SUPPORTED;

enum_error:
    /* Remove EP0. and disable the port. */
    CyU3PUsbHostEpRemove (0);
    CyU3PUsbHostPortDisable ();
    CyU3PDebugPrint (4, "Application start failed with error: %d.\r\n", status);
}

/* This function disables the mouse driver application. */
void CyFxApplnStop ()
{
	CyU3PDebugPrint (2, "CyFxApplnStop()\r\n");
	switch (glHostOwner)
	{
		case CY_FX_HOST_OWNER_MOUSE_DRIVER:
			CyFxMouseDriverDeInit ();
			break;
		case CY_FX_HOST_OWNER_PHONE_DRIVER:
			PhoneDriverDeInit ();
			break;
		default:
			break;
	}

    /* Remove EP0. and disable the port. */
    CyU3PUsbHostEpRemove (0);
    CyU3PUsbHostPortDisable ();

    glHostOwner = CY_FX_HOST_OWNER_NONE;

    /* Clear state variables. */
    glIsApplnActive = CyFalse;
}

/* This function initializes the USB host stack. */
void CyFxUsbHostStart ()
{
    CyU3PUsbHostConfig_t hostCfg;
    CyU3PReturnStatus_t status;

    hostCfg.ep0LowLevelControl = CyFalse;
    hostCfg.eventCb = CyFxHostEventCb;
    hostCfg.xferCb = NULL;
    status = CyU3PUsbHostStart (&hostCfg);
    if (status != CY_U3P_SUCCESS)
    {
		CyU3PDebugPrint (4, "CyU3PUsbHostStart failed with status: %d\r\n",status);
		return;
    }
}

/* This function disables the USB host stack. */
void CyFxUsbHostStop ()
{
    /* Stop host mode application if running. */
    if (glIsApplnActive)
    {
        CyFxApplnStop ();
    }

    /* Stop the host module. */
    CyU3PUsbHostStop ();
}

/* This function enables / disables driving VBUS. */
void CyFxUsbVBusControl (CyBool_t isEnable)
{
    if (isEnable)
    {
        /* Drive VBUS only if no-one else is driving. */
        if (!CyU3POtgIsVBusValid ())
        {
            CyU3PGpioSimpleSetValue (21, CY_FX_HOST_VBUS_ENABLE_VALUE);
        }
    }
    else
    {
        CyU3PGpioSimpleSetValue (21, CY_FX_HOST_VBUS_DISABLE_VALUE);
    }

    CyU3PDebugPrint (2, "VBUS enable:%d\r\n", isEnable);
}

/* OTG event handler. */
void CyFxOtgEventCb (CyU3POtgEvent_t event,uint32_t input)
{
	CyU3PDebugPrint (4, "OTG Event: %d, Input: %d.\r\n", event, input);
	switch (event)
	{
		case CY_U3P_OTG_PERIPHERAL_CHANGE:
			if (input == CY_U3P_OTG_TYPE_A_CABLE)
			{
				CyFxUsbHostStart ();
				/* Enable the VBUS supply. */
				CyFxUsbVBusControl (CyTrue);
			}
			else
			{
				/* Make sure that the VBUS supply is disabled. */
				CyFxUsbVBusControl (CyFalse);

				/* Stop the previously started host stack. */
				if ((!CyU3POtgIsHostMode ()) && (CyU3PUsbHostIsStarted ()))
				{
					CyFxUsbHostStop ();
				}
			}
			break;

		case CY_U3P_OTG_VBUS_VALID_CHANGE:
			if (input)
			{
				/* Start the host mode stack. */
				if (!CyU3PUsbHostIsStarted ())
				{
					CyFxUsbHostStart ();
				}
			}
			else
			{
				/* If the OTG mode has changed, stop the previous stack. */
				if ((!CyU3POtgIsHostMode ()) && (CyU3PUsbHostIsStarted ()))
				{
					/* Stop the previously started host stack. */
					CyFxUsbHostStop ();
				}
			}
			break;

		default:
			/* do nothing */
			break;
	}
}

CyU3PReturnStatus_t CyFxApplnInit (void)
{
	CyU3POtgConfig_t otgCfg;
	CyU3PReturnStatus_t status;

	/* Wait until VBUS is stabilized. */
	CyU3PThreadSleep (100);

	glIsApplnActive = CyFalse;			/* Whether the application is active or not. */
	glIsPeripheralPresent = CyFalse;	/* Whether a remote peripheral is present or not. */

	/* Initialize the OTG module. */
	otgCfg.otgMode = CY_U3P_OTG_MODE_OTG;
	otgCfg.chargerMode = CY_U3P_OTG_CHARGER_DETECT_ACA_MODE;
	otgCfg.cb = CyFxOtgEventCb;
	status = CyU3POtgStart (&otgCfg);
	if (status != CY_U3P_SUCCESS) {
		CyU3PDebugPrint (4, "CyU3POtgStart failed with status: %d\r\n",status);
		return status;
	}

	/* Since VBATT or VBUS is required for OTG operation enable it. */
	return CyU3PUsbVBattEnable (CyTrue);
}

#endif
