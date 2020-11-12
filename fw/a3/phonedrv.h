#ifndef __PHONEDRV_H__
#define __PHONEDRV_H__

#include "cyu3dma.h"

uint8_t glInEp;
uint8_t glOutEp;
uint16_t EpSize;

CyU3PReturnStatus_t PhoneDriverInit (void);
void PhoneDriverDeInit (void);

#endif
