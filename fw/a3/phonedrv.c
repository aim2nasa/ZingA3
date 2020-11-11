#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyu3usb.h"
#include "cyu3usbhost.h"
#include "cyu3usbotg.h"
#include "cyu3utils.h"
#include "otg.h"

uint8_t glInEp = 0;
uint8_t glOutEp = 0;

CyU3PDmaChannel glDataOutCh;      	/* DMA channel for EP Data OUT */
CyU3PDmaChannel glDataInCh;       	/* DMA channel for EP Data IN */

void
PhoneDmaCb (CyU3PDmaChannel *ch,
        CyU3PDmaCbType_t type,
        CyU3PDmaCBInput_t *input)
{
    if (type == CY_U3P_DMA_CB_PROD_EVENT)
    {
        CyU3PDebugPrint (4, "PhoneDmaCb buffer(count:%d)\n",input->buffer_p.count);

        /* Discard the current buffer to free it. */
        CyU3PDmaChannelDiscardBuffer (ch);
    }
}

CyU3PReturnStatus_t
PhoneDriverInit ()
{
    uint16_t length, size, offset;
    CyU3PReturnStatus_t status;
    CyU3PUsbHostEpConfig_t epCfg;
    CyU3PDmaChannelConfig_t dmaCfg;
    uint16_t EpSize = 0;

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
                	CyU3PDebugPrint (1, "[DriverInit] glInEp=%d(%x)\n", glInEp,glInEp);
                }
                else
                {
                	glOutEp = glEp0Buffer[offset + 2];
                	CyU3PDebugPrint (1, "[DriverInit] glOutEp=%d(%x)\n", glOutEp,glOutEp);
                }
            }
        }

        /* Advance to next descriptor. */
        offset += glEp0Buffer[offset];
    }

    CyU3PDebugPrint (1, "[DriverInit] glOutEp=%d(%x), glInEp=%d(%x), EpSize=%d\n", glOutEp,glOutEp,glInEp,glInEp,EpSize);

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

    /* Create a DMA channel for IN EP. */
    CyU3PMemSet ((uint8_t *)&dmaCfg, 0, sizeof(dmaCfg));
    //dmaCfg.size = USB_EP_MAX_SIZE;
    dmaCfg.size = EpSize;
    dmaCfg.count = 1;
    dmaCfg.prodSckId = (CyU3PDmaSocketId_t)(CY_U3P_UIB_SOCKET_PROD_0 + (0x0F & glInEp));
    dmaCfg.consSckId = CY_U3P_CPU_SOCKET_CONS;
    dmaCfg.dmaMode = CY_U3P_DMA_MODE_BYTE;
    dmaCfg.notification = CY_U3P_DMA_CB_PROD_EVENT;
    dmaCfg.cb = PhoneDmaCb;
    dmaCfg.prodHeader = 0;
    dmaCfg.prodFooter = 0;
    dmaCfg.consHeader = 0;
    dmaCfg.prodAvailCount = 0;
    status = CyU3PDmaChannelCreate (&glDataInCh, CY_U3P_DMA_TYPE_MANUAL_IN, &dmaCfg);
    if (status != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (1, "[DriverInit] CyU3PDmaChannelCreate(glDataInCh) error(%d)\n", status);
        goto app_error;
    }

    /* Create a DMA channel for OUT EP. */
    dmaCfg.prodSckId = CY_U3P_CPU_SOCKET_PROD;
    dmaCfg.consSckId = (CyU3PDmaSocketId_t)(CY_U3P_UIB_SOCKET_CONS_0 + (0x0F & glOutEp));
    status = CyU3PDmaChannelCreate (&glDataOutCh, CY_U3P_DMA_TYPE_MANUAL_OUT, &dmaCfg);
    if (status != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (1, "[DriverInit] CyU3PDmaChannelCreate(glDataOutCh) error(%d)\n", status);
        goto app_error;
    }

    return status;

app_error:
    CyU3PDmaChannelDestroy (&glDataInCh);
    if (glInEp != 0)
    {
        CyU3PUsbHostEpRemove (glInEp);
        glInEp = 0;
    }
    CyU3PDmaChannelDestroy (&glDataOutCh);
    if (glOutEp != 0)
    {
        CyU3PUsbHostEpRemove (glOutEp);
        glOutEp = 0;
    }

enum_error:
    return CY_U3P_ERROR_FAILURE;
}

void
PhoneDriverDeInit ()
{
    CyU3PDmaChannelReset (&glDataInCh);
    if (glInEp != 0)
    	CyU3PUsbHostEpAbort (glInEp);

    CyU3PDmaChannelReset (&glDataOutCh);

    if (glOutEp != 0)
    	CyU3PUsbHostEpAbort (glOutEp);

    CyU3PDmaChannelDestroy (&glDataInCh);
    if (glInEp != 0)
    {
        CyU3PUsbHostEpRemove (glInEp);
        glInEp = 0;
    }
    CyU3PDmaChannelDestroy (&glDataOutCh);
    if (glOutEp != 0)
    {
        CyU3PUsbHostEpRemove (glOutEp);
        glOutEp = 0;
    }
}
