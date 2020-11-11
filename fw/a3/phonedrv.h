#ifndef __PHONEDRV_H__
#define __PHONEDRV_H__

#include "cyu3dma.h"

CyU3PDmaChannel glDataOutCh;      	/* DMA channel for EP Data OUT */
CyU3PDmaChannel glDataInCh;       	/* DMA channel for EP Data IN */

CyU3PReturnStatus_t PhoneDriverInit (void);
void PhoneDriverDeInit (void);

#endif
