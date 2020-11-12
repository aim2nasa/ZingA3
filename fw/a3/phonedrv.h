#ifndef __PHONEDRV_H__
#define __PHONEDRV_H__

#include "cyu3dma.h"

typedef struct PHONEDRV_T
{
	uint8_t inEp;
	uint8_t outEp;
	uint16_t epSize;
} PHONEDRV_T;

PHONEDRV_T Phone;

CyU3PReturnStatus_t PhoneDriverInit (void);
void PhoneDriverDeInit (void);

#endif
