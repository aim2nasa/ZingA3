#include "dma.h"
#include "cyu3error.h"
#include "cyu3usb.h"
#include "cyu3system.h"
#include "macro.h"
#include "Zing.h"
#include "phonedrv.h"

static void initDmaCount()
{
	Dma.Mode_= DMA_UNINITIALIZED;
	Dma.DataOut_.Count_ = Dma.DataIn_.Count_= Dma.ControlOut_.Count_ = Dma.ControlIn_.Count_ = 0;
}

void initDma(uint8_t controlInEP,uint8_t controlOutEP,uint8_t dataInEP,uint8_t dataOutEP,uint16_t dataBurstLength)
{
	initDmaCount();
	Dma.ControlIn_.EP_ = controlInEP;
	Dma.ControlOut_.EP_ = controlOutEP;
	Dma.DataIn_.EP_ = dataInEP;
	Dma.DataOut_.EP_ = dataOutEP;
	Dma.DataBurstLength_ = dataBurstLength;
}

static void setDmaChannelCfg(CyU3PDmaChannelConfig_t *pDmaCfg, uint16_t size, uint16_t count, CyU3PDmaSocketId_t prodSckId,
		CyU3PDmaSocketId_t consSckId, uint32_t notification, CyU3PDmaCallback_t cb)
{
	pDmaCfg->size  = size;
	pDmaCfg->count = count;
	pDmaCfg->prodSckId = prodSckId;
	pDmaCfg->consSckId = consSckId;
	pDmaCfg->dmaMode = CY_U3P_DMA_MODE_BYTE;
	pDmaCfg->notification = notification;
	pDmaCfg->cb = cb;
	pDmaCfg->prodHeader = 0;
	pDmaCfg->prodFooter = 0;
	pDmaCfg->consHeader = 0;
	pDmaCfg->prodAvailCount = 0;
}

CyU3PReturnStatus_t createChannel(const char* name,
		CyU3PDmaChannelConfig_t *pDmaCfg,uint16_t size,uint16_t count,CyU3PDmaSocketId_t prodSckId,
		CyU3PDmaSocketId_t consSckId,uint32_t notification,CyU3PDmaCallback_t cb,
		CyU3PDmaChannel *handle,CyU3PDmaType_t type)
{
	char msg[256]={0,};
	memcpy(msg,name,strlen(name));

	if(strcmp(name,"DmaNormal.DataIn")==0) {
#if PACKET_SUSPEND == 1
		notification |= CY_U3P_DMA_CB_PROD_SUSP | CY_U3P_DMA_CB_CONS_SUSP;
#endif
	}

	setDmaChannelCfg(pDmaCfg, size, count, prodSckId, consSckId, notification, cb);
	CHECK(CyU3PDmaChannelCreate(handle, type, pDmaCfg));

	if(strcmp(name,"DmaNormal.DataIn")==0) {
#if PACKET_SUSPEND == 1
		CyU3PDmaChannelSetSuspend(&handle,CY_U3P_DMA_SCK_SUSP_EOP,CY_U3P_DMA_SCK_SUSP_EOP);
#endif
	}

	CHECK(CyU3PDmaChannelSetXfer(handle, 0));	//Set DMA Channel transfer size to INFINITE
	return CY_U3P_SUCCESS;
}

void dataChannelReset(uint8_t dataIn,uint8_t dataOut)
{
	//Abort & destroy DMAs
	CyU3PDmaChannelAbort(&Dma.DataOut_.Channel_);
	CyU3PDmaChannelAbort(&Dma.DataIn_.Channel_);

	CyU3PDmaChannelDestroy(&Dma.DataOut_.Channel_);
	CyU3PDmaChannelDestroy(&Dma.DataIn_.Channel_);

	//Flush the Endpoint memory
	CyU3PUsbFlushEp(dataIn);
	CyU3PUsbFlushEp(dataOut);
}

void channelReset(uint8_t controlIn,uint8_t controlOut,uint8_t dataIn,uint8_t dataOut)
{
	//Abort & destroy DMAs
	CyU3PDmaChannelAbort(&Dma.ControlOut_.Channel_);
	CyU3PDmaChannelAbort(&Dma.ControlIn_.Channel_);
	CyU3PDmaChannelAbort(&Dma.DataOut_.Channel_);
	CyU3PDmaChannelAbort(&Dma.DataIn_.Channel_);

	CyU3PDmaChannelDestroy(&Dma.ControlOut_.Channel_);
	CyU3PDmaChannelDestroy(&Dma.ControlIn_.Channel_);
	CyU3PDmaChannelDestroy(&Dma.DataOut_.Channel_);
	CyU3PDmaChannelDestroy(&Dma.DataIn_.Channel_);

	//Flush the Endpoint memory
	CyU3PUsbFlushEp(controlIn);
	CyU3PUsbFlushEp(controlOut);
	CyU3PUsbFlushEp(dataIn);
	CyU3PUsbFlushEp(dataOut);
}

void DMA_Sync_DataOut_Cb(CyU3PDmaChannel *handle,CyU3PDmaCbType_t evtype,CyU3PDmaCBInput_t *input)
{
	switch (evtype)
	{
	case CY_U3P_DMA_CB_PROD_EVENT:
		CyU3PDebugPrint (6, "DMA_Sync_DataOut_Cb->CY_U3P_DMA_CB_PROD_EVENT, buffer(count:%d,size:%d,status:%d)\n",
				input->buffer_p.count,input->buffer_p.size,input->buffer_p.status);

		Dma.DataOut_.Count_++;
		break;
	default:
		break;
	}
}

void DMA_Sync_DataIn_Cb(CyU3PDmaChannel *handle,CyU3PDmaCbType_t evtype,CyU3PDmaCBInput_t *input)
{
	switch (evtype)
	{
	case CY_U3P_DMA_CB_PROD_EVENT:
		CyU3PDebugPrint (6, "DMA_Sync_DataIn_Cb->CY_U3P_DMA_CB_PROD_EVENT, buffer(count:%d,size:%d,status:%d)\n",
				input->buffer_p.count,input->buffer_p.size,input->buffer_p.status);

		Dma.DataIn_.Count_++;
		break;
	default:
		break;
	}
}

static CyU3PReturnStatus_t DMA_Sync_mode(uint8_t controlIn,uint8_t controlOut,uint8_t dataIn,uint8_t dataOut,uint16_t dataBurstLength)
{
	CyU3PDmaChannelConfig_t dmaCfg;
	uint16_t size = 1024; // super speed <- assumed condition , temporary code

	channelReset(controlIn,controlOut,dataIn,dataOut);

	CHECK(createChannel("DmaSync.ControlOut",
						&dmaCfg,
						size,
						8,
						CY_U3P_CPU_SOCKET_PROD,
						CY_U3P_PIB_SOCKET_0,
						CY_U3P_DMA_CB_PROD_EVENT,
						0,
						&Dma.ControlOut_.Channel_,
						CY_U3P_DMA_TYPE_MANUAL_OUT));

	CHECK(createChannel("DmaSync.ControlIn",
						&dmaCfg,
						size,
						8,
						CY_U3P_PIB_SOCKET_1,
						CY_U3P_CPU_SOCKET_CONS,
						CY_U3P_DMA_CB_PROD_EVENT,
						0,
						&Dma.ControlIn_.Channel_,
						CY_U3P_DMA_TYPE_MANUAL_IN));

	CHECK(createChannel("DmaSync.DataOut",
						&dmaCfg,
						size*dataBurstLength,
						4,
						CY_U3P_CPU_SOCKET_PROD,
						CY_U3P_PIB_SOCKET_2,
						CY_U3P_DMA_CB_PROD_EVENT,
						DMA_Sync_DataOut_Cb,
						&Dma.DataOut_.Channel_,
						CY_U3P_DMA_TYPE_MANUAL_OUT));

	CHECK(createChannel("DmaSync.DataIn",
						&dmaCfg,
						size*dataBurstLength,
						4,
						CY_U3P_PIB_SOCKET_3,
						CY_U3P_CPU_SOCKET_CONS,
						CY_U3P_DMA_CB_PROD_EVENT,
						DMA_Sync_DataIn_Cb,
						&Dma.DataIn_.Channel_,
						CY_U3P_DMA_TYPE_MANUAL_IN));

	Dma.Mode_ = DMA_SYNC;
#ifdef DEBUG
	CyU3PDebugPrint(4,"DMA_Sync_mode(%d) done\n", Dma.Mode_);
#endif
	return CY_U3P_SUCCESS;
}

void DMA_Normal_CtrlOut_Cb(CyU3PDmaChannel *handle,CyU3PDmaCbType_t evtype,CyU3PDmaCBInput_t *input)
{
	switch (evtype)
	{
	case CY_U3P_DMA_CB_PROD_EVENT:
		CyU3PDebugPrint (4, ">");
#ifdef DMA_NORMAL_MANUAL
	{
		CyU3PReturnStatus_t status = CyU3PDmaChannelCommitBuffer (handle, input->buffer_p.count, 0);
        if (status != CY_U3P_SUCCESS)
            CyU3PDebugPrint (4, "CyU3PDmaChannelCommitBuffer failed, Error code = %d\n", status);
	}
#endif
		Dma.ControlOut_.Count_++;
		break;
	case CY_U3P_DMA_CB_SEND_CPLT:	//override mode
		break;
	case CY_U3P_DMA_CB_CONS_EVENT:	//normal mode
		break;
	default:
		break;
	}
}

void DMA_Normal_CtrlIn_Cb(CyU3PDmaChannel *handle,CyU3PDmaCbType_t evtype,CyU3PDmaCBInput_t *input)
{
	switch (evtype)
	{
	case CY_U3P_DMA_CB_PROD_EVENT:
		CyU3PDebugPrint (4, "<");
#ifdef DMA_NORMAL_MANUAL
	{
		CyU3PReturnStatus_t status = CyU3PDmaChannelCommitBuffer (handle, input->buffer_p.count, 0);
        if (status != CY_U3P_SUCCESS)
            CyU3PDebugPrint (4, "CyU3PDmaChannelCommitBuffer failed, Error code = %d\n", status);
	}
#endif
		Dma.ControlIn_.Count_++;
		break;
	case CY_U3P_DMA_CB_SEND_CPLT:	//override mode
		break;
	case CY_U3P_DMA_CB_CONS_EVENT:	//normal mode
		break;
	default:
		break;
	}
}

void DMA_Normal_DataOut_Cb(CyU3PDmaChannel *handle,CyU3PDmaCbType_t evtype,CyU3PDmaCBInput_t *input)
{
	CyU3PDebugPrint(6, "DMA_Normal_DataOut_Cb(%d)\r\n",evtype);
	switch (evtype)
	{
	case CY_U3P_DMA_CB_PROD_EVENT:
		CyU3PDebugPrint (4, "o");
#ifdef DMA_NORMAL_MANUAL
	{
		CyU3PReturnStatus_t status = CyU3PDmaChannelCommitBuffer (handle, input->buffer_p.count, 0);
        if (status != CY_U3P_SUCCESS)
            CyU3PDebugPrint (4, "CyU3PDmaChannelCommitBuffer failed, Error code = %d\n", status);
	}
#endif
		Dma.DataOut_.Count_++;
		break;
	case CY_U3P_DMA_CB_SEND_CPLT:	//override mode
		break;
	case CY_U3P_DMA_CB_CONS_EVENT:	//normal mode
		break;
	default:
		break;
	}
}

void DMA_Normal_DataIn_Cb(CyU3PDmaChannel *handle,CyU3PDmaCbType_t evtype,CyU3PDmaCBInput_t *input)
{
	CyU3PDebugPrint(6, "DMA_Normal_DataIn_Cb(%d)\r\n",evtype);
	switch (evtype)
	{
	case CY_U3P_DMA_CB_PROD_EVENT:
		CyU3PDebugPrint (4, "i");
#ifdef DMA_NORMAL_MANUAL
	{
//    	for(int i=0;i<input->buffer_p.count;i++) CyU3PDebugPrint (6,"%x ", input->buffer_p.buffer[i]);
//    	CyU3PDebugPrint(6, "\n");
    	if (input->buffer_p.buffer[0]==0x50 &&
    		input->buffer_p.buffer[1]==0x49 &&
    		input->buffer_p.buffer[2]==0x4E &&
    		input->buffer_p.buffer[3]==0x47 &&
    		input->buffer_p.buffer[4]==0x20 &&
    		input->buffer_p.buffer[5]==0x4F &&
    		input->buffer_p.buffer[6]==0x4E )
    	{
    		/* PING ON message received */
            CyU3PDmaChannelDiscardBuffer (handle);	//Discard the current buffer to free it
    		CyU3PDebugPrint (4, ".");
    	}else{
    		/* Non PING ON message received */
    		CyU3PReturnStatus_t status = CyU3PDmaChannelCommitBuffer (handle, input->buffer_p.count, 0);
            if (status != CY_U3P_SUCCESS){
                CyU3PDebugPrint (4, "CyU3PDmaChannelCommitBuffer failed, Error code = %d\n", status);
            }else{
            	CyU3PDebugPrint (4, "=");
            }
    	}
	}
#endif
		Dma.DataIn_.Count_++;
		break;
	case CY_U3P_DMA_CB_SEND_CPLT:	//override mode
		break;
	case CY_U3P_DMA_CB_CONS_EVENT:	//normal mode
		break;
#if PACKET_SUSPEND == 1
	case CY_U3P_DMA_CB_PROD_SUSP:
		CyU3PDmaChannelResume(handle,CyTrue,CyTrue);
		break;
	case CY_U3P_DMA_CB_CONS_SUSP:
		CyU3PDmaChannelResume(handle,CyTrue,CyTrue);
		break;
#endif
	default:
		break;
	}
}

static CyU3PReturnStatus_t DMA_Normal_mode(uint8_t controlIn,uint8_t controlOut,uint8_t dataIn,uint8_t dataOut,uint16_t dataBurstLength)
{
	CyU3PDmaChannelConfig_t dmaCfg;
	uint16_t size = 1024; // super speed <- assumed condition , temporary code

	channelReset(controlIn,controlOut,dataIn,dataOut);

	CHECK(createChannel("DmaNormal.ControlOut",
						&dmaCfg,
						size,
						8,
						CY_U3P_UIB_SOCKET_PROD_1,
						CY_U3P_PIB_SOCKET_0,
						CY_U3P_DMA_CB_PROD_EVENT,
						DMA_Normal_CtrlOut_Cb,
						&Dma.ControlOut_.Channel_,
#ifdef DMA_NORMAL_MANUAL
						CY_U3P_DMA_TYPE_MANUAL));
#else
						CY_U3P_DMA_TYPE_AUTO_SIGNAL));
#endif

	CHECK(createChannel("DmaNormal.ControlIn",
						&dmaCfg,
						size,
						8,
						CY_U3P_PIB_SOCKET_1,
						CY_U3P_UIB_SOCKET_CONS_1,
						CY_U3P_DMA_CB_PROD_EVENT,
						DMA_Normal_CtrlIn_Cb,
						&Dma.ControlIn_.Channel_,
#ifdef DMA_NORMAL_MANUAL
						CY_U3P_DMA_TYPE_MANUAL));
#else
						CY_U3P_DMA_TYPE_AUTO_SIGNAL));
#endif

	CHECK(createChannel("DmaNormal.DataOut",
						&dmaCfg,
						size*dataBurstLength,
						4,
						CY_U3P_UIB_SOCKET_PROD_2,
						CY_U3P_PIB_SOCKET_2,
						CY_U3P_DMA_CB_PROD_EVENT,
						DMA_Normal_DataOut_Cb,
						&Dma.DataOut_.Channel_,
#ifdef DMA_NORMAL_MANUAL
						CY_U3P_DMA_TYPE_MANUAL));
#else
						CY_U3P_DMA_TYPE_AUTO_SIGNAL));
#endif

	CHECK(createChannel("DmaNormal.DataIn",
						&dmaCfg,
						size*dataBurstLength,
						4,
						CY_U3P_PIB_SOCKET_3,
						CY_U3P_UIB_SOCKET_CONS_2,
						CY_U3P_DMA_CB_PROD_EVENT,
						DMA_Normal_DataIn_Cb,
						&Dma.DataIn_.Channel_,
#ifdef DMA_NORMAL_MANUAL
						CY_U3P_DMA_TYPE_MANUAL));
#else
						CY_U3P_DMA_TYPE_AUTO_SIGNAL));
#endif

	Dma.Mode_ = DMA_NORMAL;
#ifdef DEBUG
	CyU3PDebugPrint(4,"DMA_Normal_mode(%d) done\n", Dma.Mode_);
#endif
	return CY_U3P_SUCCESS;
}

static CyU3PReturnStatus_t DMA_LoopBack_mode(uint8_t controlIn,uint8_t controlOut,uint8_t dataIn,uint8_t dataOut,uint16_t dataBurstLength)
{
	CyU3PDmaChannelConfig_t dmaCfg;
	uint16_t size = 1024; // super speed <- assumed condition , temporary code

	channelReset(controlIn,controlOut,dataIn,dataOut);

	CHECK(createChannel("DmaLoopback.ControlOut",
						&dmaCfg,
						size,
						8,
						CY_U3P_UIB_SOCKET_PROD_1,
						CY_U3P_UIB_SOCKET_CONS_1,
						0,
						0,
						&Dma.ControlOut_.Channel_,
						CY_U3P_DMA_TYPE_AUTO));

	CHECK(createChannel("DmaLoopback.DataOut",
						&dmaCfg,
						size*dataBurstLength,
						4,
						CY_U3P_UIB_SOCKET_PROD_2,
						CY_U3P_UIB_SOCKET_CONS_2,
						0,
						0,
						&Dma.DataOut_.Channel_,
						CY_U3P_DMA_TYPE_AUTO));

	Dma.Mode_ = DMA_LP;
#ifdef DEBUG
	CyU3PDebugPrint(4,"DMA_LoopBack_mode(%d) done\n", Dma.Mode_);
#endif
	return CY_U3P_SUCCESS;
}

void DMA_SinkSource_Cb(CyU3PDmaChannel *chHandle,CyU3PDmaCbType_t type,CyU3PDmaCBInput_t *input)
{
	CyU3PReturnStatus_t status = CY_U3P_SUCCESS;
	CyU3PDmaBuffer_t buf_p;

	Dma.DataOut_.Count_++;

	if (type == CY_U3P_DMA_CB_PROD_EVENT)
	{
		status = CyU3PDmaChannelDiscardBuffer(chHandle);
		if (status != CY_U3P_SUCCESS)
		{
			CyU3PDebugPrint(4, "DMA_SinkSource_Cb,CyU3PDmaChannelDiscardBuffer failed(0x%x)\n", status);
		}
	}
	if (type == CY_U3P_DMA_CB_CONS_EVENT)
	{
		status = CyU3PDmaChannelGetBuffer(chHandle, &buf_p, CYU3P_NO_WAIT);
		if (status == CY_U3P_SUCCESS)
		{
			status = CyU3PDmaChannelCommitBuffer(chHandle, buf_p.size, 0);	//Commit the full buffer with default status
			if (status != CY_U3P_SUCCESS)
			{
				CyU3PDebugPrint(4, "DMA_SinkSource_Cb,CyU3PDmaChannelCommitBuffer failed(0x%x)\n", status);
			}
		}
		else
		{
			CyU3PDebugPrint(4, "DMA_SinkSource_Cb,CyU3PDmaChannelGetBuffer failed(0x%x)\n", status);
		}
	}
}

CyU3PReturnStatus_t DMASrcSinkFillInBuffers(void)
{
	CyU3PDmaBuffer_t    buf_p;
	uint16_t            index = 0;

	/* Now preload all buffers in the MANUAL_OUT pipe with the required data. */
	for (index = 0; index < 8; index++)
	{
		CHECK(CyU3PDmaChannelGetBuffer(&Dma.ControlIn_.Channel_, &buf_p, CYU3P_NO_WAIT));

		CyU3PMemSet(buf_p.buffer, 0xA5, buf_p.size);
		CHECK(CyU3PDmaChannelCommitBuffer(&Dma.ControlIn_.Channel_, buf_p.size, 0));
	}

	/* Now preload all buffers in the MANUAL_OUT pipe with the required data. */
	for (index = 0; index < 4; index++)
	{
		CHECK(CyU3PDmaChannelGetBuffer(&Dma.DataIn_.Channel_, &buf_p, CYU3P_NO_WAIT));

		CyU3PMemSet(buf_p.buffer, 0xA5, buf_p.size);
		CHECK(CyU3PDmaChannelCommitBuffer(&Dma.DataIn_.Channel_, buf_p.size, 0));
	}
	return CY_U3P_SUCCESS;
}

static CyU3PReturnStatus_t DMA_SinkSource_mode(uint8_t controlIn,uint8_t controlOut,uint8_t dataIn,uint8_t dataOut,uint16_t dataBurstLength)
{
	CyU3PDmaChannelConfig_t dmaCfg;
	uint16_t size = 1024; // super speed <- assumed condition , temporary code

	channelReset(controlIn,controlOut,dataIn,dataOut);

	CHECK(createChannel("DmaSinkSource.ControlOut",
						&dmaCfg,
						size,
						8,
						CY_U3P_UIB_SOCKET_PROD_1,
						CY_U3P_CPU_SOCKET_CONS,
						CY_U3P_DMA_CB_PROD_EVENT,
						DMA_SinkSource_Cb,
						&Dma.ControlOut_.Channel_,
						CY_U3P_DMA_TYPE_MANUAL_IN));

	CHECK(createChannel("DmaSinkSource.ControlIn",
						&dmaCfg,
						size,
						8,
						CY_U3P_CPU_SOCKET_PROD,
						CY_U3P_UIB_SOCKET_CONS_1,
						CY_U3P_DMA_CB_CONS_EVENT,
						DMA_SinkSource_Cb,
						&Dma.ControlIn_.Channel_,
						CY_U3P_DMA_TYPE_MANUAL_OUT));

	CHECK(createChannel("DmaSinkSource.DataOut",
						&dmaCfg,
						size*dataBurstLength,
						4,
						CY_U3P_UIB_SOCKET_PROD_2,
						CY_U3P_CPU_SOCKET_CONS,
						CY_U3P_DMA_CB_PROD_EVENT,
						DMA_SinkSource_Cb,
						&Dma.DataOut_.Channel_,
						CY_U3P_DMA_TYPE_MANUAL_IN));

	CHECK(createChannel("DmaSinkSource.DataIn",
						&dmaCfg,
						size*dataBurstLength,
						4,
						CY_U3P_CPU_SOCKET_PROD,
						CY_U3P_UIB_SOCKET_CONS_2,
						CY_U3P_DMA_CB_CONS_EVENT,
						DMA_SinkSource_Cb,
						&Dma.DataIn_.Channel_,
						CY_U3P_DMA_TYPE_MANUAL_OUT));

	CHECK(DMASrcSinkFillInBuffers());

	Dma.Mode_ = DMA_SINKSOURCE;
#ifdef DEBUG
	CyU3PDebugPrint(4,"DMA_SinkSource_mode(%d) done\n", Dma.Mode_);
#endif
	return CY_U3P_SUCCESS;
}

CyU3PReturnStatus_t DMA_Sync()
{
	return DMA_Sync_mode(Dma.ControlIn_.EP_,Dma.ControlOut_.EP_,Dma.DataIn_.EP_,Dma.DataOut_.EP_,Dma.DataBurstLength_);
}

CyU3PReturnStatus_t DMA_Normal()
{
	return DMA_Normal_mode(Dma.ControlIn_.EP_,Dma.ControlOut_.EP_,Dma.DataIn_.EP_,Dma.DataOut_.EP_,Dma.DataBurstLength_);
}

CyU3PReturnStatus_t DMA_LoopBack()
{
	return DMA_LoopBack_mode(Dma.ControlIn_.EP_,Dma.ControlOut_.EP_,Dma.DataIn_.EP_,Dma.DataOut_.EP_,Dma.DataBurstLength_);
}

CyU3PReturnStatus_t DMA_SinkSource()
{
	return DMA_SinkSource_mode(Dma.ControlIn_.EP_,Dma.ControlOut_.EP_,Dma.DataIn_.EP_,Dma.DataOut_.EP_,Dma.DataBurstLength_);
}

const char* dmaModeStr(dma_mode_t mode)
{
	if(mode==DMA_UNINITIALIZED) return "DMA UNINITIALIZED";
	else if(mode==DMA_NORMAL) return "DMA NORMAL";
	else if(mode==DMA_SYNC) return "DMA SYNC";
	else if(mode==DMA_LP) return "DMA LOOPBACK";
	else if(mode==DMA_SINKSOURCE) return "DMA SINKSOURCE";

	return "NOT FOUND";
}
