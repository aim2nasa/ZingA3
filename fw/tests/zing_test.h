#ifndef __ZING_TEST_H__
#define __ZING_TEST_H__

#include "Application.h"
#include "zing.h"
#include "dma.h"
#include "ControlCh.h"

void test_zing_init()
{
	initDmaCount();
	DMA_Sync_mode(CY_FX_EP_CONTROL_IN,CY_FX_EP_CONTROL_OUT,CY_FX_EP_DATA_IN,CY_FX_EP_DATA_OUT,CY_FX_DATA_BURST_LENGTH);

	assert("Control Channel Thread",ControlChThread_Create()==CY_U3P_SUCCESS);
	assert("1st Zing Init",Zing_Init()==CY_U3P_SUCCESS);
	assert("2nd Zing Init",Zing_Init()==CY_U3P_SUCCESS);
}

#endif
