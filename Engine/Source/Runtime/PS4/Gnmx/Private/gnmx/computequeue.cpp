/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/


#include <gnm.h>
#include <gnmx/computequeue.h>
#include <gnmx/computecontext.h>
#ifndef _MSC_VER
#include <pthread.h>
#endif


using namespace sce::Gnm;
using namespace sce::Gnmx;


void ComputeQueue::initialize(uint32_t pipeId, uint32_t queueId)
{
	m_reserved = 0;
	m_pipeId  = pipeId;
	m_queueId = queueId;

	m_vqueueId	  = 0;
}

#ifdef __ORBIS__
bool ComputeQueue::map(void *ringBaseAddr, uint32_t ringSizeInDW, void *readPtrAddr)
{
	SCE_GNM_ASSERT_MSG((uintptr_t(ringBaseAddr)&0xFF)==0, "Ring Base Address (0x%p) must be 256 bytes aligned.", ringBaseAddr);
	SCE_GNM_ASSERT_MSG((uintptr_t(readPtrAddr)&0x3)==0, "Read Ptr Address (0x%p) must be 4 bytes aligned.", readPtrAddr);
	SCE_GNM_ASSERT_MSG((ringSizeInDW & (ringSizeInDW-1)) == 0 && ringSizeInDW*4 >= 512, "Ring Size must be a power of 2 and at least 512");

	const int status = sce::Gnm::mapComputeQueue(&m_vqueueId, 
												 m_pipeId, m_queueId,
												 ringBaseAddr, ringSizeInDW, readPtrAddr);
	SCE_GNM_ASSERT_MSG(!status, "sce::Gnm::mapComputeQueue(&vqueueId, globalPipeId=%u, queueId=%u, ringBaseAddr=%p, ringSizeInDW=%u, readPtrAddr=%p) failed [err: 0x%08x].\n", m_pipeId, m_queueId, ringBaseAddr, ringSizeInDW, readPtrAddr, (uint32_t)status);

	m_dcbRoot.init((void*)ringBaseAddr, ringSizeInDW*4, 0, 0);
	m_readPtrAddr = (volatile uint32_t*)readPtrAddr;

	return m_vqueueId != 0;
}

void ComputeQueue::unmap()
{
	sce::Gnm::unmapComputeQueue(m_vqueueId);
	m_vqueueId = 0;
}


static bool prepareBuffersForSubmit(GnmxDispatchCommandBuffer *dcbRoot, volatile uint32_t* readPtrAddr, uint32_t numBuffers, void const*const *apDcb, uint32_t const *aDcbSizes)
{
	SCE_GNM_ASSERT_MSG(numBuffers == 0 || apDcb != NULL, "if numBuffers > 0, apDcb must not be NULL.");
	SCE_GNM_ASSERT_MSG(numBuffers == 0 || aDcbSizes != NULL, "if numBuffers > 0, aDcbSizes must not be NULL.");

	// TODO: Make sure the Ring doesn't overlap too fast. (check readPtr)
	const uint32_t kSizeOfIbPacketInDWords = 4;
	const uint32_t kNopSizeMask = 0x3;
	SCE_GNM_UNUSED(kNopSizeMask);

	// Check if queue is full:
	const uint32_t ringSizeInDWord = uint32_t(dcbRoot->m_endptr - dcbRoot->m_beginptr);
	const uint32_t readOffset      = *readPtrAddr;
	const uint32_t curOffset       = uint32_t(dcbRoot->m_cmdptr - dcbRoot->m_beginptr);
	const  int32_t relOffset       = readOffset - curOffset - 1;
	const uint32_t adjOffset       = relOffset + ((relOffset >> 31) & ringSizeInDWord);
	const uint32_t numOpenedSlots  = adjOffset / kSizeOfIbPacketInDWords;
	const uint32_t slotsLeftInRing = (ringSizeInDWord - curOffset) / kSizeOfIbPacketInDWords;

#if !SCE_GNM_SUPPORT_FOR_NOP_AT_END_OF_COMPUTEQUEUE
	SCE_GNM_ASSERT_MSG((curOffset&kNopSizeMask)==0, "The remaining space in the compute queue is not a multiple of 'callCommandBuffer' packets; please enable SCE_GNM_SUPPORT_FOR_NOP_AT_END_OF_COMPUTEQUEUE to ensure stability.");
#endif // !SCE_GNM_SUPPORT_FOR_NOP_AT_END_OF_COMPUTEQUEUE

	if ( numOpenedSlots < numBuffers )
		return false;

	if ( numBuffers < slotsLeftInRing )
	{
		for (uint32_t iSubmit = 0; iSubmit < numBuffers; ++iSubmit)
			dcbRoot->callCommandBuffer((void*)apDcb[iSubmit], aDcbSizes[iSubmit]/4);
	}
	else
	{
		uint32_t iSubmit = 0;
		while ( iSubmit < slotsLeftInRing )
		{
			dcbRoot->callCommandBuffer((void*)apDcb[iSubmit], aDcbSizes[iSubmit]/4);
			iSubmit++;
		}

#if SCE_GNM_SUPPORT_FOR_NOP_AT_END_OF_COMPUTEQUEUE
		const uint32_t nopSize = kSizeOfIbPacketInDWords - curOffset&kNopSizeMask;
		if ( nopSize )
			dcbRoot->insertNop(nopSize);
#endif // SCE_GNM_SUPPORT_FOR_NOP_AT_END_OF_COMPUTEQUEUE

		dcbRoot->resetBuffer();

		while ( iSubmit < numBuffers )
		{
			dcbRoot->callCommandBuffer((void*)apDcb[iSubmit], aDcbSizes[iSubmit]/4);
			iSubmit++;
		}
	}
	return true;

}
ComputeQueue::SubmissionStatus ComputeQueue::submit(uint32_t numBuffers, void const*const *apDcb, uint32_t const *aDcbSizes)
{
	if(prepareBuffersForSubmit(&m_dcbRoot,m_readPtrAddr,numBuffers,apDcb,aDcbSizes)) {
		sce::Gnm::dingDong(m_vqueueId,	uint32_t(m_dcbRoot.m_cmdptr - m_dcbRoot.m_beginptr));
		return kSubmitOK;
	}
	else
	{
		return kSubmitFailQueueIsFull;
	}
}
ComputeQueue::SubmissionStatus ComputeQueue::submit(uint64_t workloadId, uint32_t numBuffers, void const*const *apDcb, uint32_t const *aDcbSizes)
{
	if(prepareBuffersForSubmit(&m_dcbRoot,m_readPtrAddr,numBuffers,apDcb,aDcbSizes)) {
		sce::Gnm::dingDong(workloadId, m_vqueueId,	uint32_t(m_dcbRoot.m_cmdptr - m_dcbRoot.m_beginptr));
		return kSubmitOK;
	}
	else
	{
		return kSubmitFailQueueIsFull;
	}
}

ComputeQueue::SubmissionStatus ComputeQueue::submit(Gnmx::ComputeContext *cmpc)
{
	// TODO: Fix code to support multiple cmd buffer within a ComputeContext.

	SCE_GNM_ASSERT_MSG(cmpc != NULL, "cmpc must not be NULL.");
	return submit(cmpc->m_dcb.m_beginptr, (uint32_t)(cmpc->m_dcb.m_cmdptr - cmpc->m_dcb.m_beginptr)*4);
}
ComputeQueue::SubmissionStatus ComputeQueue::submit(uint64_t workloadId, Gnmx::ComputeContext *cmpc)
{
	// TODO: Fix code to support multiple cmd buffer within a ComputeContext.

	SCE_GNM_ASSERT_MSG(cmpc != NULL, "cmpc must not be NULL.");
	return submit(workloadId, cmpc->m_dcb.m_beginptr, (uint32_t)(cmpc->m_dcb.m_cmdptr - cmpc->m_dcb.m_beginptr)*4);
}
void ComputeQueue::reset()
{
	reset(0);
}
void ComputeQueue::reset(uint64_t workloadId)
{
	// This function resets the queue's pointer back to the beginning of the queue.

	// Because under the hood we have a ring buffer, we can't directly reset the ring's head offset.  Instead,
	// we must insert just enough NOP commands to bring the head back to zero.  This is what we're doing here.
	const uint32_t ringSpaceRemainingInDW = uint32_t(m_dcbRoot.m_endptr - m_dcbRoot.m_cmdptr);
	m_dcbRoot.insertNop(ringSpaceRemainingInDW);

	// Now, execute whatever command is left in the ring, including the newly added NOPs.
	m_dcbRoot.resetBuffer();
	if(workloadId)
		sce::Gnm::dingDong(workloadId, m_vqueueId, uint32_t(m_dcbRoot.m_cmdptr - m_dcbRoot.m_beginptr));
	else
		sce::Gnm::dingDong(m_vqueueId, uint32_t(m_dcbRoot.m_cmdptr - m_dcbRoot.m_beginptr));
		

	// Wait until it properly reset.
	while ( *m_readPtrAddr != 0 )
	{
		scePthreadYield();
	}
}


#endif // __ORBIS__
