#ifndef __DMA_H__
#define __DMA_H__

#include "cyu3types.h"
#include "cyu3dma.h"

typedef enum DMA_MODE_T
{
	DMA_UNINITIALIZED = -1,
	DMA_NORMAL = 0,
	DMA_SYNC,
	DMA_LP,
	DMA_SINKSOURCE,
} dma_mode_t;

typedef struct DMA_CHANNEL_T
{
	CyU3PDmaChannel Channel_;
	uint32_t Count_;
	uint8_t EP_;
	uint8_t *Buffer_;
} DMA_CHANNEL_T;

typedef struct DMA_T
{
	dma_mode_t Mode_;
	uint16_t DataBurstLength_;
	DMA_CHANNEL_T ControlOut_;
	DMA_CHANNEL_T ControlIn_;
	DMA_CHANNEL_T DataOut_;
	DMA_CHANNEL_T DataIn_;
} DMA_T;

DMA_T Dma;

void initDma(uint8_t controlInEP,uint8_t controlOutEP,uint8_t dataInEP,uint8_t dataOutEP,uint16_t dataBurstLength);
CyU3PReturnStatus_t createChannel(const char* name,
		CyU3PDmaChannelConfig_t *pDmaCfg,uint16_t size,uint16_t count,CyU3PDmaSocketId_t prodSckId,
		CyU3PDmaSocketId_t consSckId,uint32_t notification,CyU3PDmaCallback_t cb,
		CyU3PDmaChannel *handle,CyU3PDmaType_t type);
void dataChannelReset(uint8_t dataIn,uint8_t dataOut);
void channelReset(uint8_t controlIn,uint8_t controlOut,uint8_t dataIn,uint8_t dataOut);

CyU3PReturnStatus_t DMA_Sync();
CyU3PReturnStatus_t DMA_Normal();
CyU3PReturnStatus_t DMA_LoopBack();
CyU3PReturnStatus_t DMA_SinkSource();

void DMA_Normal_CtrlOut_Cb(CyU3PDmaChannel *handle,CyU3PDmaCbType_t evtype,CyU3PDmaCBInput_t *input);
void DMA_Normal_CtrlIn_Cb(CyU3PDmaChannel *handle,CyU3PDmaCbType_t evtype,CyU3PDmaCBInput_t *input);
void DMA_Normal_DataOut_Cb(CyU3PDmaChannel *handle,CyU3PDmaCbType_t evtype,CyU3PDmaCBInput_t *input);
void DMA_Normal_DataIn_Cb(CyU3PDmaChannel *handle,CyU3PDmaCbType_t evtype,CyU3PDmaCBInput_t *input);
void DMA_SinkSource_Cb(CyU3PDmaChannel *chHandle,CyU3PDmaCbType_t type,CyU3PDmaCBInput_t *input);
CyU3PReturnStatus_t DMASrcSinkFillInBuffers(void);
const char* dmaModeStr(dma_mode_t mode);

#endif
