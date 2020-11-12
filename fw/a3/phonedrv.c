#include "phonedrv.h"
#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3error.h"
#include "cyu3usb.h"
#include "cyu3usbhost.h"
#include "cyu3usbotg.h"
#include "cyu3utils.h"
#include "otg.h"
#include "Zing.h"
#include "dma.h"

CyU3PReturnStatus_t
PhoneDriverInit ()
{
    uint16_t length, size, offset;
    CyU3PReturnStatus_t status;
    CyU3PUsbHostEpConfig_t epCfg;
    EpSize = glInEp = glOutEp = 0;

    /* Read first four bytes of configuration descriptor to determine
     * the total length. */
    status = CyFxSendSetupRqt (0x80, CY_U3P_USB_SC_GET_DESCRIPTOR,
            (CY_U3P_USB_CONFIG_DESCR << 8), 0, 4, glEp0Buffer);
    if (status != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (1, "[DriverInit] CY_U3P_USB_SC_GET_DESCRIPTOR config error\n");
        goto enum_error;
    }

    /* Identify the length of the data received. */
    length = CY_U3P_MAKEWORD(glEp0Buffer[3], glEp0Buffer[2]);
    if (length > 512)
    {
        CyU3PDebugPrint (1, "[DriverInit] length error\n");
        goto enum_error;
    }
    CyU3PDebugPrint (1, "[DriverInit] length=%d\n",length);

    /* Read the full configuration descriptor. */
    status = CyFxSendSetupRqt (0x80, CY_U3P_USB_SC_GET_DESCRIPTOR,
            (CY_U3P_USB_CONFIG_DESCR << 8), 0, length, glEp0Buffer);
    if (status != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (1, "[DriverInit] CY_U3P_USB_SC_GET_DESCRIPTOR config error\n");
        goto enum_error;
    }

    CyU3PDebugPrint (6, "Identify the EP characteristics\n");
    for(int i=0;i<length;i++) CyU3PDebugPrint (6,"%x ", glEp0Buffer[i]);
    CyU3PDebugPrint(6, "\n");

    /* Identify the EP characteristics. */
    offset = 0;
    while (offset < length)
    {
        if (glEp0Buffer[offset + 1] == CY_U3P_USB_ENDPNT_DESCR)
        {
            if (glEp0Buffer[offset + 3] != CY_U3P_USB_EP_BULK)
            {
                CyU3PDebugPrint (1, "[DriverInit] glEp0Buffer[%d](%d)!= CY_U3P_USB_EP_BULK(%d)\n",
                		offset+3,glEp0Buffer[offset+3],CY_U3P_USB_EP_BULK);
            }else{
                /* Retreive the information. */
                EpSize = CY_U3P_MAKEWORD(glEp0Buffer[offset + 5],glEp0Buffer[offset + 4]);
                CyU3PDebugPrint (1, "[DriverInit] EpSize=%d\n", EpSize);

                if (glEp0Buffer[offset + 2] & 0x80)
                {
                	glInEp = glEp0Buffer[offset + 2];
                	CyU3PDebugPrint (1, "[DriverInit] glInEp=%d(0x%x)\n", glInEp,glInEp);
                }
                else
                {
                	glOutEp = glEp0Buffer[offset + 2];
                	CyU3PDebugPrint (1, "[DriverInit] glOutEp=%d(0x%x)\n", glOutEp,glOutEp);
                }
            }
        }

        /* Advance to next descriptor. */
        offset += glEp0Buffer[offset];
    }

    CyU3PDebugPrint (1, "[DriverInit] glOutEp=%d(0x%x), glInEp=%d(0x%x), EpSize=%d\n", glOutEp,glOutEp,glInEp,glInEp,EpSize);

    /* Add the IN endpoint. */
    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof(epCfg));
    epCfg.type = CY_U3P_USB_EP_BULK;
    epCfg.mult = 1;
    epCfg.maxPktSize = EpSize;
    epCfg.pollingRate = 0;
    /* Since DMA buffer sizes can only be multiple of 16 bytes and
     * also since this is an interrupt endpoint where the max data
     * packet size is same as the maxPktSize field, the fullPktSize
     * has to be a multiple of 16 bytes. */
    //size = ((EpSize + 0x0F) & ~0x0F);
    size = EpSize;
    epCfg.fullPktSize = size;
    epCfg.isStreamMode = CyFalse;
    status = CyU3PUsbHostEpAdd (glInEp, &epCfg);
    if (status != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (1, "[DriverInit] CyU3PUsbHostEpAdd(glInEp) error\n");
        goto enum_error;
    }

    /* Add the OUT EP. */
    status = CyU3PUsbHostEpAdd (glOutEp, &epCfg);
    if (status != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (1, "[DriverInit] CyU3PUsbHostEpAdd(glOutEp) error\n");
        goto enum_error;
    }

    CyU3PDebugPrint (1, "PhoneDriverInit OK\n");
    return CY_U3P_SUCCESS;

enum_error:
	CyU3PDebugPrint (1, "PhoneDriverInit failed\n");
    return CY_U3P_ERROR_FAILURE;
}

void
PhoneDriverDeInit ()
{
    if (glInEp != 0)
    	CyU3PUsbHostEpAbort (glInEp);

    if (glOutEp != 0)
    	CyU3PUsbHostEpAbort (glOutEp);

    if (glInEp != 0)
    {
        CyU3PUsbHostEpRemove (glInEp);
        glInEp = 0;
    }

    if (glOutEp != 0)
    {
        CyU3PUsbHostEpRemove (glOutEp);
        glOutEp = 0;
    }
}
