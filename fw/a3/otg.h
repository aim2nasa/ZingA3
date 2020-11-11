#ifndef __OTG_H__
#define __OTG_H__

#include "cyu3types.h"
#include "cyu3usbotg.h"

#define CY_FX_HOST_VBUS_ENABLE_VALUE    (CyTrue)        /* GPIO value for driving VBUS. */
#define CY_FX_HOST_VBUS_DISABLE_VALUE   (!CY_FX_HOST_VBUS_ENABLE_VALUE)
#define CY_FX_USB_CHANGE_EVENT          (1 << 0)        /* Event signaling that a peripheral change is detected. */
#define CY_FX_HOST_EP0_SETUP_SIZE       (32)            /* EP0 setup packet buffer size made multiple of 32 bytes. */
#define CY_FX_HOST_EP0_BUFFER_SIZE      (512)           /* EP0 data transfer buffer size. */
#define CY_FX_HOST_EP0_WAIT_TIMEOUT     (5000)          /* EP0 request timeout in clock ticks. */
#define CY_FX_HOST_PERIPHERAL_ADDRESS   (1)             /* USB host mode peripheral address to be used. */
#define CY_FX_HOST_DMA_BUF_COUNT        (4)             /* Number of buffers to be allocated for DMA channel. */
#define CY_FX_HOST_OWNER_NONE           (0)             /* Host controller is not associated with a class driver. */
#define CY_FX_HOST_OWNER_MOUSE_DRIVER   (1)             /* Host controller associated with mouse driver. */
#define CY_FX_HOST_OWNER_PHONE_DRIVER   (2)             /* Host controller associated with phone driver. */

void CyFxOtgEventCb (CyU3POtgEvent_t event,uint32_t input);
CyU3PReturnStatus_t CyFxApplnInit (void);
void CyFxApplnStart ();
void CyFxApplnStop ();
CyU3PReturnStatus_t CyFxSendSetupRqt (uint8_t type,uint8_t request,uint16_t value,uint16_t index,uint16_t length,uint8_t *buffer_p);

/* Buffer to send / receive data for EP0. */
uint8_t glEp0Buffer[CY_FX_HOST_EP0_BUFFER_SIZE] __attribute__ ((aligned (32)));

#endif
