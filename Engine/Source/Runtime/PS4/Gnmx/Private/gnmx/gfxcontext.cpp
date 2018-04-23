/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <gnmx/gfxcontext.h>

#include <gnm/buffer.h>
#include <gnm/gpumem.h>
#include <gnm/platform.h>
#include <gnm/rendertarget.h>
#include <gnm/tessellation.h>
#include <gnm/measuredispatchcommandbuffer.h>

#include <algorithm>

using namespace sce::Gnm;
using namespace sce::Gnmx;

GfxContext::GfxContext(void)
{
}

GfxContext::~GfxContext(void)
{
}

void GfxContext::init(void *cueHeapAddr, uint32_t numRingEntries,
					  void *dcbBuffer, uint32_t dcbSizeInBytes, void *ccbBuffer, uint32_t ccbSizeInBytes)
{
	BaseGfxContext::init(dcbBuffer,dcbSizeInBytes,ccbBuffer,ccbSizeInBytes);
	m_cue.init( cueHeapAddr, numRingEntries);
	m_cue.bindCommandBuffers(&m_dcb, &m_ccb, &m_acb);
}


void GfxContext::init(void *cueHeapAddr, uint32_t numRingEntries, ConstantUpdateEngine::RingSetup ringSetup,
					  void *dcbBuffer, uint32_t dcbSizeInBytes, void *ccbBuffer, uint32_t ccbSizeInBytes)
{
	BaseGfxContext::init(dcbBuffer,dcbSizeInBytes,ccbBuffer,ccbSizeInBytes);
	m_cue.init(cueHeapAddr, numRingEntries, ringSetup);
	m_cue.bindCommandBuffers(&m_dcb, &m_ccb, &m_acb);
}


void GfxContext::reset(void)
{
	m_cue.advanceFrame();
	// Unbind all shaders in the CUE
	BaseGfxContext::reset();
}

#if defined(__ORBIS__)
int32_t GfxContext::submit(void)
{
	return submit(0);
}
int32_t GfxContext::submit(uint64_t workloadId)
{
	SCE_GNM_ASSERT_MSG((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "endDispatchDraw was not called before submit");


	const bool submitCcbs = ccbIsActive();
	void *dcbGpuAddrs[kMaxNumStoredSubmissions+1], *acbGpuAddrs[kMaxNumStoredSubmissions+1], *ccbGpuAddrs[kMaxNumStoredSubmissions+1];
	uint32_t dcbSizes[kMaxNumStoredSubmissions+1], acbSizes[kMaxNumStoredSubmissions+1], ccbSizes[kMaxNumStoredSubmissions+1];
	uint32_t numAcbSubmits = 0;
	uint32_t pipeId = 0, queueId = 0;
	if (m_pQueue != NULL) {
		pipeId = m_pQueue->m_pipeId;
		queueId = m_pQueue->m_queueId;
	}
	// Submit each previously stored range
	for(uint32_t iSub=0; iSub<m_submissionCount; ++iSub)
	{
		dcbSizes[iSub]    = m_submissionRanges[iSub].m_dcbSizeInDwords*sizeof(uint32_t);
		dcbGpuAddrs[iSub] = m_dcb.m_beginptr + m_submissionRanges[iSub].m_dcbStartDwordOffset;
		if (submitCcbs) {
			ccbSizes[iSub]    = m_submissionRanges[iSub].m_ccbSizeInDwords*sizeof(uint32_t);
			ccbGpuAddrs[iSub] = (ccbSizes[iSub] > 0) ? m_ccb.m_beginptr + m_submissionRanges[iSub].m_ccbStartDwordOffset : 0;
		}
		
		if (m_submissionRanges[iSub].m_acbSizeInDwords) {
			uint32_t sizeofAcbInDwords = m_submissionRanges[iSub].m_acbSizeInDwords;
			uint32_t *acbGpuAddr = m_acb.m_beginptr + m_submissionRanges[iSub].m_acbStartDwordOffset;
			// skip submission ranges which contain only the pushDispatchDrawAcbSubmitMarker() inserted at the start of every acb range:
			if (!(sizeofAcbInDwords == Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker() && Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr))) {
				acbSizes[numAcbSubmits] = sizeofAcbInDwords*sizeof(uint32_t);
				acbGpuAddrs[numAcbSubmits] = (void*)acbGpuAddr;
				SCE_GNM_ASSERT_MSG(Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr), "dispatch draw ACB submission %u does not start with pushDispatchDrawAcbSubmitMarker()", iSub);
				//NOTE: if the first ACB buffer (m_acb.m_beginptr) was eliminated because it contain only the pushDispatchDrawAcbSubmitMarker(), 
				// patch m_acb.m_beginptr to point to the first ACB because the DCB only has a pointer to m_acb.m_beginptr and won't be able to find the chain otherwise:
				if (numAcbSubmits != 0 || acbGpuAddr != m_acb.m_beginptr)
					Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker(numAcbSubmits != 0 ? (uint32_t*)acbGpuAddrs[numAcbSubmits-1] : m_acb.m_beginptr, numAcbSubmits != 0 ? acbSizes[numAcbSubmits-1] : Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker()*4, pipeId, queueId, acbGpuAddr);
				++numAcbSubmits;
			}
		}
	}
	// Submit anything left over after the final stored range
	dcbSizes[m_submissionCount]    = static_cast<uint32_t>(m_dcb.m_cmdptr - m_currentDcbSubmissionStart)*4;
	dcbGpuAddrs[m_submissionCount] = (void*)m_currentDcbSubmissionStart;
	if (submitCcbs) {
		ccbSizes[m_submissionCount]    = static_cast<uint32_t>(m_ccb.m_cmdptr - m_currentCcbSubmissionStart)*4;
		ccbGpuAddrs[m_submissionCount] = (ccbSizes[m_submissionCount] > 0) ? (void*)m_currentCcbSubmissionStart : nullptr;
	}
	if (m_acb.m_cmdptr > m_currentAcbSubmissionStart) {
		uint32_t sizeofAcbInDwords = static_cast<uint32_t>(m_acb.m_cmdptr - m_currentAcbSubmissionStart);
		uint32_t *acbGpuAddr = const_cast<uint32_t*>(m_currentAcbSubmissionStart);
		// skip submission ranges which contain only the pushDispatchDrawAcbSubmitMarker() inserted at the start of every acb range:
		if (!(sizeofAcbInDwords == Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker() && Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr))) {
			acbSizes[numAcbSubmits] = sizeofAcbInDwords*sizeof(uint32_t);
			acbGpuAddrs[numAcbSubmits] = acbGpuAddr;
			SCE_GNM_ASSERT_MSG(Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr), "dispatch draw ACB submission %u does not start with pushDispatchDrawAcbSubmitMarker()", m_submissionCount);
			//NOTE: if the first ACB buffer (m_acb.m_beginptr) was eliminated because it contain only the pushDispatchDrawAcbSubmitMarker(), 
			// patch m_acb.m_beginptr to point to the first ACB because the DCB only has a pointer to m_acb.m_beginptr and won't be able to find the chain otherwise:
			if (numAcbSubmits != 0 || acbGpuAddr != m_acb.m_beginptr)
				Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker(numAcbSubmits != 0 ? (uint32_t*)acbGpuAddrs[numAcbSubmits-1] : m_acb.m_beginptr, numAcbSubmits != 0 ? acbSizes[numAcbSubmits-1] : Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker()*4, pipeId, queueId, acbGpuAddr);
			++numAcbSubmits;
		}
	}
	if (numAcbSubmits != 0) {
		Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker((uint32_t*)acbGpuAddrs[numAcbSubmits-1], acbSizes[numAcbSubmits-1], pipeId, queueId, NULL);
		SCE_GNM_ASSERT_MSG(m_pQueue != NULL && m_pQueue->isMapped(), "ComputeQueue was not %s before submitting for dispatch draw", m_pQueue == NULL ? "set" : "mapped");
	}
	
	void** submitCcbGpuAddrs = NULL;
	uint32_t* submitCcbSizes = NULL;
	if (submitCcbs) {
		submitCcbGpuAddrs = ccbGpuAddrs;
		submitCcbSizes = ccbSizes;
	}

	int errSubmit;
	if(workloadId)
		errSubmit = Gnm::submitCommandBuffers(workloadId,m_submissionCount+1, dcbGpuAddrs, dcbSizes, submitCcbGpuAddrs, submitCcbSizes);
	else
		errSubmit = Gnm::submitCommandBuffers(m_submissionCount+1, dcbGpuAddrs, dcbSizes, submitCcbGpuAddrs, submitCcbSizes);

	if (numAcbSubmits && errSubmit == sce::Gnm::kSubmissionSuccess) 
	{
		ComputeQueue::SubmissionStatus err = m_pQueue->submit(numAcbSubmits, acbGpuAddrs, acbSizes);
		if (err == ComputeQueue::kSubmitFailQueueIsFull) {
			//FIXME: should this be some new SCE_GNM_WARNING() macro?
			if (sce::Gnm::getErrorResponseLevel() != sce::Gnm::kErrorResponseLevelIgnore)
				sce::Gnm::printErrorMessage(__FILE__, __LINE__, __FUNCTION__, "ComputeQueue for dispatch draw is full; waiting for space...");
			do {
				err = m_pQueue->submit(numAcbSubmits, acbGpuAddrs, acbSizes);
			} while (err == ComputeQueue::kSubmitFailQueueIsFull);
		}
		if (err != ComputeQueue::kSubmitOK)
			return err;
	}
	return errSubmit;
}

int32_t GfxContext::validate(void)
{
	void *dcbGpuAddrs[kMaxNumStoredSubmissions+1], *acbGpuAddrs[kMaxNumStoredSubmissions+1], *ccbGpuAddrs[kMaxNumStoredSubmissions+1];
	uint32_t dcbSizes[kMaxNumStoredSubmissions+1], acbSizes[kMaxNumStoredSubmissions+1], ccbSizes[kMaxNumStoredSubmissions+1];
	uint32_t numAcbSubmits = 0;

	const bool submitCcbs = ccbIsActive();

	// Submit each previously stored range
	for(uint32_t iSub=0; iSub<m_submissionCount; ++iSub)
	{
		dcbSizes[iSub]    = m_submissionRanges[iSub].m_dcbSizeInDwords*sizeof(uint32_t);
		dcbGpuAddrs[iSub] = m_dcb.m_beginptr + m_submissionRanges[iSub].m_dcbStartDwordOffset;
		if (submitCcbs) {
			ccbSizes[iSub]    = m_submissionRanges[iSub].m_ccbSizeInDwords*sizeof(uint32_t);
			ccbGpuAddrs[iSub] = (ccbSizes[iSub] > 0) ? m_ccb.m_beginptr + m_submissionRanges[iSub].m_ccbStartDwordOffset : nullptr;
		}
		else {
			ccbSizes[iSub]    = 0;
			ccbGpuAddrs[iSub] = nullptr;
		}
		if (m_submissionRanges[iSub].m_acbSizeInDwords) {
			acbSizes[numAcbSubmits]    = m_submissionRanges[iSub].m_acbSizeInDwords*sizeof(uint32_t);
			acbGpuAddrs[numAcbSubmits] = m_acb.m_beginptr + m_submissionRanges[iSub].m_acbStartDwordOffset;
			++numAcbSubmits;
		}
	}
	// Submit anything left over after the final stored range
	dcbSizes[m_submissionCount]    = static_cast<uint32_t>(m_dcb.m_cmdptr - m_currentDcbSubmissionStart)*4;
	dcbGpuAddrs[m_submissionCount] = (void*)m_currentDcbSubmissionStart;
	if (submitCcbs) {
		ccbSizes[m_submissionCount]    = static_cast<uint32_t>(m_ccb.m_cmdptr - m_currentCcbSubmissionStart)*4;
		ccbGpuAddrs[m_submissionCount] = (ccbSizes[m_submissionCount] > 0) ? (void*)m_currentCcbSubmissionStart : nullptr;
	}
	else
	{
		ccbSizes[m_submissionCount]    = 0;
		ccbGpuAddrs[m_submissionCount] = nullptr;
	}

	if (m_acb.m_cmdptr > m_currentAcbSubmissionStart) {
		acbSizes[numAcbSubmits]    = static_cast<uint32_t>(m_acb.m_cmdptr - m_currentAcbSubmissionStart)*4;
		acbGpuAddrs[numAcbSubmits] = (void*)m_currentAcbSubmissionStart;
		++numAcbSubmits;
	}
	if (numAcbSubmits) {
		SCE_GNM_UNUSED(acbSizes);
		int32_t ret = Gnm::validateDispatchCommandBuffers(numAcbSubmits, acbGpuAddrs, acbSizes);
		SCE_GNM_ASSERT_MSG(ret == Gnm::kSubmissionSuccess, "Dispatch Draw acb failed validation pass");
	}
	return Gnm::validateDrawCommandBuffers(m_submissionCount+1, dcbGpuAddrs, dcbSizes, ccbGpuAddrs, ccbSizes);
}
static int internalSubmitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
								  GfxContext* gfx)
{
	SCE_GNM_ASSERT_MSG((gfx->m_dispatchDrawFlags & GfxContext::kDispatchDrawFlagInDispatchDraw) == 0, "endDispatchDraw was not called before submitAndFlip");

	const bool submitCcbs = gfx->ccbIsActive();
	
	void *dcbGpuAddrs[GfxContext::kMaxNumStoredSubmissions+1], *acbGpuAddrs[GfxContext::kMaxNumStoredSubmissions+1], *ccbGpuAddrs[GfxContext::kMaxNumStoredSubmissions+1];
	uint32_t dcbSizes[GfxContext::kMaxNumStoredSubmissions+1], acbSizes[GfxContext::kMaxNumStoredSubmissions+1], ccbSizes[GfxContext::kMaxNumStoredSubmissions+1];
	uint32_t numAcbSubmits = 0;
	uint32_t pipeId = 0, queueId = 0;

	sce::Gnmx::ComputeQueue *pQueue = gfx->m_pQueue;
	if (pQueue != NULL) {
		pipeId = pQueue->m_pipeId;
		queueId = pQueue->m_queueId;
	}
	const uint32_t submissionCount = gfx->m_submissionCount;
	const GfxContext::SubmissionRange *submissionRanges = gfx->m_submissionRanges;
	const uint32_t* currentDcbSubmissionStart = gfx->m_currentDcbSubmissionStart;
	const uint32_t* currentCcbSubmissionStart = gfx->m_currentCcbSubmissionStart;
	const uint32_t* currentAcbSubmissionStart = gfx->m_currentAcbSubmissionStart;
	const GnmxDrawCommandBuffer &dcb = gfx->m_dcb;
	const GnmxConstantCommandBuffer &ccb = gfx->m_ccb;
	const GnmxDispatchCommandBuffer &acb = gfx->m_acb;
	// Submit each previously stored range
	for(uint32_t iSub=0; iSub<submissionCount; ++iSub)
	{
		dcbSizes[iSub]    = submissionRanges[iSub].m_dcbSizeInDwords*sizeof(uint32_t);
		dcbGpuAddrs[iSub] = dcb.m_beginptr + submissionRanges[iSub].m_dcbStartDwordOffset;
		if (submitCcbs) {
			ccbSizes[iSub]    = submissionRanges[iSub].m_ccbSizeInDwords*sizeof(uint32_t);
			ccbGpuAddrs[iSub] = (ccbSizes[iSub] > 0) ? ccb.m_beginptr + submissionRanges[iSub].m_ccbStartDwordOffset : 0;
		}
		if (submissionRanges[iSub].m_acbSizeInDwords) {
			uint32_t sizeofAcbInDwords = submissionRanges[iSub].m_acbSizeInDwords;
			uint32_t *acbGpuAddr = acb.m_beginptr + submissionRanges[iSub].m_acbStartDwordOffset;
			// skip submission ranges which contain only the pushDispatchDrawAcbSubmitMarker() inserted at the start of every acb range:
			if (!(sizeofAcbInDwords == sce::Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker() && sce::Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr))) {
				acbSizes[numAcbSubmits] = sizeofAcbInDwords*sizeof(uint32_t);
				acbGpuAddrs[numAcbSubmits] = acbGpuAddr;
				SCE_GNM_ASSERT_MSG(sce::Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr), "dispatch draw ACB submission %u does not start with pushDispatchDrawAcbSubmitMarker()", iSub);
				//NOTE: if the first ACB buffer (m_acb.m_beginptr) was eliminated because it contain only the pushDispatchDrawAcbSubmitMarker(), 
				// patch m_acb.m_beginptr to point to the first ACB because the DCB only has a pointer to m_acb.m_beginptr and won't be able to find the chain otherwise:
				if (numAcbSubmits != 0 || acbGpuAddr != acb.m_beginptr)
					sce::Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker(numAcbSubmits != 0 ? (uint32_t*)acbGpuAddrs[numAcbSubmits-1] : acb.m_beginptr, numAcbSubmits != 0 ? acbSizes[numAcbSubmits-1] : sce::Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker()*4, pipeId, queueId, acbGpuAddr);
				++numAcbSubmits;
			}
		}
	}
	// Submit anything left over after the final stored range
	dcbSizes[submissionCount]    = static_cast<uint32_t>(dcb.m_cmdptr - currentDcbSubmissionStart)*4;
	dcbGpuAddrs[submissionCount] = (void*)currentDcbSubmissionStart;
	if (submitCcbs) {
		ccbSizes[submissionCount]    = static_cast<uint32_t>(ccb.m_cmdptr - currentCcbSubmissionStart)*4;
		ccbGpuAddrs[submissionCount] = (ccbSizes[submissionCount] > 0) ? (void*)currentCcbSubmissionStart : nullptr;
	}
	if (acb.m_cmdptr > currentAcbSubmissionStart) {
		uint32_t sizeofAcbInDwords = static_cast<uint32_t>(acb.m_cmdptr - currentAcbSubmissionStart);
		uint32_t *acbGpuAddr = const_cast<uint32_t*>(currentAcbSubmissionStart);
		// skip submission ranges which contain only the pushDispatchDrawAcbSubmitMarker() inserted at the start of every acb range:
		if (!(sizeofAcbInDwords == sce::Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker() && sce::Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr))) {
			acbSizes[numAcbSubmits] = sizeofAcbInDwords*sizeof(uint32_t);
			acbGpuAddrs[numAcbSubmits] = acbGpuAddr;
			SCE_GNM_ASSERT_MSG(sce::Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr), "dispatch draw ACB submission %u does not start with pushDispatchDrawAcbSubmitMarker()", submissionCount);
			//NOTE: if the first ACB buffer (m_acb.m_beginptr) was eliminated because it contain only the pushDispatchDrawAcbSubmitMarker(), 
			// patch m_acb.m_beginptr to point to the first ACB because the DCB only has a pointer to m_acb.m_beginptr and won't be able to find the chain otherwise:
			if (numAcbSubmits != 0 || acbGpuAddr != acb.m_beginptr)
				sce::Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker(numAcbSubmits != 0 ? (uint32_t*)acbGpuAddrs[numAcbSubmits-1] : acb.m_beginptr, numAcbSubmits != 0 ? acbSizes[numAcbSubmits-1] : sce::Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker()*4, pipeId, queueId, acbGpuAddr);
			++numAcbSubmits;
		}
	}
	if (numAcbSubmits != 0) {
		sce::Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker((uint32_t*)acbGpuAddrs[numAcbSubmits-1], acbSizes[numAcbSubmits-1], pipeId, queueId, NULL);
		SCE_GNM_ASSERT_MSG(pQueue != NULL && pQueue->isMapped(), "ComputeQueue was not %s before submitting for dispatch draw", pQueue == NULL ? "set" : "mapped");
	}

	void** submitCcbGpuAddrs = NULL;
	uint32_t* submitCcbSizes = NULL;
	if (submitCcbs) {
		submitCcbGpuAddrs = ccbGpuAddrs;
		submitCcbSizes = ccbSizes;
	}

	int errSubmit;
	if( workloadId ) {
		errSubmit = sce::Gnm::submitAndFlipCommandBuffers(workloadId, submissionCount+1, dcbGpuAddrs, dcbSizes, submitCcbGpuAddrs, submitCcbSizes,
			videoOutHandle, displayBufferIndex, flipMode, flipArg);
	}
	else {
		errSubmit = sce::Gnm::submitAndFlipCommandBuffers( submissionCount+1, dcbGpuAddrs, dcbSizes, submitCcbGpuAddrs, submitCcbSizes,
			videoOutHandle, displayBufferIndex, flipMode, flipArg);
	}

	if (numAcbSubmits && errSubmit == sce::Gnm::kSubmissionSuccess) {
		ComputeQueue::SubmissionStatus err = pQueue->submit(numAcbSubmits, acbGpuAddrs, acbSizes);
		if (err == ComputeQueue::kSubmitFailQueueIsFull) {
			//FIXME: should this be some new SCE_GNM_WARNING() macro?
			if (sce::Gnm::getErrorResponseLevel() != sce::Gnm::kErrorResponseLevelIgnore)
				sce::Gnm::printErrorMessage(__FILE__, __LINE__, __FUNCTION__, "ComputeQueue for dispatch draw is full; waiting for space...");
			do {
				err = pQueue->submit(numAcbSubmits, acbGpuAddrs, acbSizes);
			} while (err == ComputeQueue::kSubmitFailQueueIsFull);
		}
		if (err != ComputeQueue::kSubmitOK)
			return err;
	}
	return errSubmit;

}
int32_t GfxContext::submitAndFlip(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg)
{
	m_dcb.prepareFlip();
	return internalSubmitAndFlip(0,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);
}
int32_t GfxContext::submitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg)
{
	m_dcb.prepareFlip();
	return internalSubmitAndFlip(workloadId,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);
}


int32_t GfxContext::submitAndFlip(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
								  void *labelAddr, uint32_t value)
{
	m_dcb.prepareFlip(labelAddr, value);
	return internalSubmitAndFlip(0,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);

}
int32_t GfxContext::submitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
								  void *labelAddr, uint32_t value)
{
	m_dcb.prepareFlip(labelAddr, value);
	return internalSubmitAndFlip(workloadId,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);
}

int32_t GfxContext::submitAndFlipWithEopInterrupt(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  EndOfPipeEventType eventType, CacheAction cacheAction)
{
	m_dcb.prepareFlipWithEopInterrupt(eventType, cacheAction);
	return internalSubmitAndFlip(0,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);

}
int32_t GfxContext::submitAndFlipWithEopInterrupt(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  EndOfPipeEventType eventType, CacheAction cacheAction)
{
	m_dcb.prepareFlipWithEopInterrupt(eventType, cacheAction);
	return internalSubmitAndFlip(workloadId,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);
}

int32_t GfxContext::submitAndFlipWithEopInterrupt(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  EndOfPipeEventType eventType, void *labelAddr, uint32_t value, CacheAction cacheAction)
{
	m_dcb.prepareFlipWithEopInterrupt(eventType, labelAddr, value, cacheAction);
	return internalSubmitAndFlip(0,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);

}
int32_t GfxContext::submitAndFlipWithEopInterrupt(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  EndOfPipeEventType eventType, void *labelAddr, uint32_t value, CacheAction cacheAction)
{
	m_dcb.prepareFlipWithEopInterrupt(eventType, labelAddr, value, cacheAction);
	return internalSubmitAndFlip(workloadId,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);
}
#endif // defined(__ORBIS__)

void GfxContext::setTessellationDataConstantBuffer(void *tcbAddr, ShaderStage domainStage)
{
	SCE_GNM_ASSERT_MSG(domainStage == Gnm::kShaderStageEs || domainStage == Gnm::kShaderStageVs, "domainStage (%d) must be kShaderStageEs or kShaderStageVs.", domainStage);

	Gnm::Buffer tcbdef;
	tcbdef.initAsConstantBuffer(tcbAddr, sizeof(Gnm::TessellationDataConstantBuffer));

	// Slot 19 is currently reserved by the compiler for the tessellation data cb:
	this->setConstantBuffers(Gnm::kShaderStageHs, Gnm::kShaderSlotTessellationDataConstantBuffer,1, &tcbdef);
	this->setConstantBuffers(domainStage, Gnm::kShaderSlotTessellationDataConstantBuffer,1, &tcbdef);
}

void GfxContext::setTessellationFactorBuffer(void *tessFactorMemoryBaseAddr)
{
	Gnm::Buffer tfbdef;
	tfbdef.initAsTessellationFactorBuffer(tessFactorMemoryBaseAddr, Gnm::kTfRingSizeInBytes);
	m_cue.setGlobalDescriptor(Gnm::kShaderGlobalResourceTessFactorBuffer, &tfbdef);
}


void GfxContext::setLsHsShaders(LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const HsShader *hsb, uint32_t numPatches)
{
	m_cue.setLsHsShaders(lsb,shaderModifier,fetchShaderAddr,hsb,numPatches);
}

void GfxContext::setLsHsShaders(LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const HsShader *hsb, uint32_t numPatches, sce::Gnm::TessellationDistributionMode distributionMode)
{
	m_cue.setLsHsShaders(lsb, shaderModifier, fetchShaderAddr, hsb, numPatches, distributionMode);
}

void GfxContext::setGsVsShaders(const GsShader *gsb)
{
	if ( gsb )
	{
		SCE_GNM_ASSERT_MSG(!gsb->isOnChip(), "setGsVsShaders called with an on-chip GS shader; use setOnChipGsVsShaders instead");
		m_dcb.enableGsMode(gsb->getGsMaxOutputPrimitiveDwordSize()); // TODO: use the entire gdb->copyShader->gsMode value
	}
	else
	{
		m_cue.setGsModeOff();
	}

	m_cue.setGsVsShaders(gsb);
}

void GfxContext::setOnChipGsVsShaders(const GsShader *gsb, uint32_t gsPrimsPerSubGroup)
{
	if ( gsb )
	{
		uint16_t esVertsPerSubGroup = (uint16_t)((gsb->m_inputVertexCountMinus1+1)*gsPrimsPerSubGroup);
		SCE_GNM_ASSERT_MSG(gsb->isOnChip(), "setOnChipGsVsShaders called with an off-chip GS shader; use setGsVsShaders instead");
		SCE_GNM_ASSERT_MSG(gsPrimsPerSubGroup > 0, "gsPrimsPerSubGroup must be greater than 0");
		SCE_GNM_ASSERT_MSG(gsPrimsPerSubGroup*gsb->getSizePerPrimitiveInBytes() <= 64*1024, "gsPrimsPerSubGroup*gsb->getSizePerPrimitiveInBytes() will not fit in 64KB LDS");
		SCE_GNM_ASSERT_MSG(esVertsPerSubGroup <= 2047, "gsPrimsPerSubGroup*(gsb->m_inputVertexCountMinus1+1) can't be greater than 2047");
		m_cue.enableGsModeOnChip(gsb->getGsMaxOutputPrimitiveDwordSize(), 
								 esVertsPerSubGroup, gsPrimsPerSubGroup);
	}
	else
	{
		m_cue.setGsModeOff();
	}

	m_cue.setGsVsShaders(gsb);
}

#if SCE_GNM_CUE2_ENABLE_CACHE
void GfxContext::setLsHsShaders(LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *lsCache, const HsShader *hsb, uint32_t numPatches, const ConstantUpdateEngine::InputParameterCache *hsCache)
{
	m_cue.setLsHsShaders(lsb, shaderModifier, fetchShaderAddr, lsCache,
						hsb,numPatches,hsCache);
}
void GfxContext::setLsHsShaders(LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *lsCache, const HsShader *hsb, uint32_t numPatches, sce::Gnm::TessellationDistributionMode distributionMode, const ConstantUpdateEngine::InputParameterCache *hsCache)
{
	m_cue.setLsHsShaders(lsb, shaderModifier, fetchShaderAddr, lsCache,
		hsb, numPatches, distributionMode, hsCache);
}

void GfxContext::setGsVsShaders(const GsShader *gsb, const ConstantUpdateEngine::InputParameterCache *cache)
{
	if ( gsb )
	{
		SCE_GNM_ASSERT_MSG(!gsb->isOnChip(), "setGsVsShaders called with an on-chip GS shader; use setOnChipGsVsShaders instead");
		m_dcb.enableGsMode(gsb->getGsMaxOutputPrimitiveDwordSize()); // TODO: use the entire gdb->copyShader->gsMode value
	}
	else
	{
		m_cue.setGsModeOff();
	}

	m_cue.setGsVsShaders(gsb,cache);
}

void GfxContext::setOnChipGsVsShaders(const GsShader *gsb, uint32_t gsPrimsPerSubGroup, const ConstantUpdateEngine::InputParameterCache *cache)
{
	if ( gsb )
	{
		uint16_t esVertsPerSubGroup = (uint16_t)((gsb->m_inputVertexCountMinus1+1)*gsPrimsPerSubGroup);
		SCE_GNM_ASSERT_MSG(gsb->isOnChip(), "setOnChipGsVsShaders called with an off-chip GS shader; use setGsVsShaders instead");
		SCE_GNM_ASSERT_MSG(gsPrimsPerSubGroup > 0, "gsPrimsPerSubGroup must be greater than 0");
		SCE_GNM_ASSERT_MSG(gsPrimsPerSubGroup*gsb->getSizePerPrimitiveInBytes() <= 64*1024, "gsPrimsPerSubGroup*gsb->getSizePerPrimitiveInBytes() will not fit in 64KB LDS");
		SCE_GNM_ASSERT_MSG(esVertsPerSubGroup <= 2047, "gsPrimsPerSubGroup*(gsb->m_inputVertexCountMinus1+1) can't be greater than 2047");
		m_cue.enableGsModeOnChip(gsb->getGsMaxOutputPrimitiveDwordSize(),
								 esVertsPerSubGroup, gsPrimsPerSubGroup);
	}
	else
	{
		m_cue.setGsModeOff();
	}

	m_cue.setGsVsShaders(gsb,cache);
}
#endif //SCE_GNM_CUE2_ENABLE_CACHE

void GfxContext::setEsGsRingBuffer(void *baseAddr, uint32_t ringSize, uint32_t maxExportVertexSizeInDword)
{
	SCE_GNM_ASSERT_MSG(baseAddr != NULL || ringSize == 0, "if baseAddr is NULL, ringSize must be 0.");
	Gnm::Buffer ringReadDescriptor;
	Gnm::Buffer ringWriteDescriptor;

	ringReadDescriptor.initAsEsGsReadDescriptor(baseAddr, ringSize);
	ringWriteDescriptor.initAsEsGsWriteDescriptor(baseAddr, ringSize);

	m_cue.setGlobalDescriptor(Gnm::kShaderGlobalResourceEsGsReadDescriptor, &ringReadDescriptor);
	m_cue.setGlobalDescriptor(Gnm::kShaderGlobalResourceEsGsWriteDescriptor, &ringWriteDescriptor);

	m_dcb.setupEsGsRingRegisters(maxExportVertexSizeInDword);
}

void GfxContext::setOnChipEsGsLdsLayout(uint32_t maxExportVertexSizeInDword)
{
	m_cue.setOnChipEsExportVertexSizeInDword((uint16_t)maxExportVertexSizeInDword);
	m_dcb.setupEsGsRingRegisters(maxExportVertexSizeInDword);
}

void GfxContext::setGsVsRingBuffers(void *baseAddr, uint32_t ringSize,
									const uint32_t vtxSizePerStreamInDword[4], uint32_t maxOutputVtxCount)
{
	SCE_GNM_ASSERT_MSG(baseAddr != NULL || ringSize == 0, "if baseAddr is NULL, ringSize must be 0.");
	SCE_GNM_ASSERT_MSG(vtxSizePerStreamInDword != NULL, "vtxSizePerStreamInDword must not be NULL.");
	Gnm::Buffer ringReadDescriptor;
	Gnm::Buffer ringWriteDescriptor;

	ringReadDescriptor.initAsGsVsReadDescriptor(baseAddr, ringSize);
	m_cue.setGlobalDescriptor(Gnm::kShaderGlobalResourceGsVsReadDescriptor, &ringReadDescriptor);

	for (uint32_t iStream = 0; iStream < 4; ++iStream)
	{
		ringWriteDescriptor.initAsGsVsWriteDescriptor(baseAddr, iStream,
													  vtxSizePerStreamInDword, maxOutputVtxCount);
		m_cue.setGlobalDescriptor((Gnm::ShaderGlobalResourceType)(Gnm::kShaderGlobalResourceGsVsWriteDescriptor0 + iStream),
								  &ringWriteDescriptor);
	}

	m_dcb.setupGsVsRingRegisters(vtxSizePerStreamInDword, maxOutputVtxCount);
}

void GfxContext::dispatchDraw(DispatchDrawTriangleCullIndexData const *pDispatchDrawIndexData, uint32_t primGroupThreshold, uint32_t pollIntervalThreshold, DrawModifier modifier)
{
	//To prevent dispatch draw deadlocks, this must match the value set to setVgtControl primGroupSize
	static const uint32_t kMinPrimgroupSize = 129;
	const uint32_t numPrimsPerVgt = kMinPrimgroupSize + ((m_dispatchDrawFlags & kDispatchDrawFlagsMaskPrimGroupSizeMinus129) >> kDispatchDrawFlagsShiftPrimGroupSizeMinus129);

	// The primGroupIndexCount must be a multiple of 2*numPrimsPerVgt primitives worth of indices
	// (numPrimsPerVgt*3*2 for triangle primitives) to prevent deadlocks related to VGT switching 
	// and dispatch draw IRB deallocation.
	uint32_t primGroupIndexCount = numPrimsPerVgt*3*2;
	// To prevent deadlocks caused by insufficient IRB space to ensure that at least one IRB
	// deallocation will fit and be processed by the VGT, we must ensure that the IRB has at least 
	// enough space for 3 maximal sized compute output groups and has space for at least 2 maximum
	// sized sub-draw flushes plus room to allocate a maximal sized compute output group.
	static const uint32_t kMaxComputeWorkGroupOutput = 513*3;
	uint32_t maxSubDrawIndexCount = primGroupIndexCount*primGroupThreshold;
	SCE_GNM_ASSERT_MSG(2*(maxSubDrawIndexCount > kMaxComputeWorkGroupOutput ? maxSubDrawIndexCount : kMaxComputeWorkGroupOutput) + kMaxComputeWorkGroupOutput <= m_dispatchDrawSharedData.m_bufferIrb.getNumElements(), "maxSubDrawIndexCount (%u) = primGroupThreshold (%u) * numPrimsPerVgt (%u) *3*2 exceeds the deadlock condition for IRB size in indices %u",
		maxSubDrawIndexCount, primGroupThreshold, numPrimsPerVgt, m_dispatchDrawSharedData.m_bufferIrb.getNumElements());

	uint32_t indexOffset = 0;		//Do we need to be able to set the index offset for dispatch draw?

	SCE_GNM_ASSERT_MSG((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) != 0, "dispatchDraw can only be called between beginDispatchDraw and endDispatchDraw");
	SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_magic == Gnmx::kDispatchDrawTriangleCullIndexDataMagic, "dispatchDraw: index data magic is wrong");
	SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_versionMajor == Gnmx::kDispatchDrawTriangleCullIndexDataVersionMajor, "dispatchDraw: index data version major index does not match");
	SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_numIndexDataBlocks > 0 && pDispatchDrawIndexData->m_numIndexDataBlocks <= 0xFFFF, "dispatchDraw: m_numIndexDataBlocks must be in the range [1:65536]");
	SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_numIndexBits >= 1 && pDispatchDrawIndexData->m_numIndexBits <= 16, "dispatchDraw: m_numIndexBits must be in the range [1:16]");
	SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_numIndexSpaceBits < pDispatchDrawIndexData->m_numIndexBits, "dispatchDraw: m_numIndexSpaceBits must be less than m_numIndexBits");

	// Store bufferInputData and numBlocksTotal to kShaderInputUsagePtrDispatchDraw data
	sce::Gnmx::CsShader const* pDdCsShader = m_cue.m_currentAcbCSB;
	if (pDdCsShader->m_version >= 1)
	{
		if (m_pDispatchDrawSharedData == NULL) {
			m_pDispatchDrawSharedData = (DispatchDrawSharedData*)m_acb.allocateFromCommandBuffer(sizeof(DispatchDrawSharedData), Gnm::kEmbeddedDataAlignment4);
			memcpy(m_pDispatchDrawSharedData, &m_dispatchDrawSharedData, sizeof(DispatchDrawSharedData));
		}
		bool const bIsInstancing = true;
		const size_t sizeofDispatchDrawData = bIsInstancing ? Gnmx::kSizeofDispatchDrawTriangleCullV1DataInstancingEnabled : Gnmx::kSizeofDispatchDrawTriangleCullV1DataInstancingDisabled;
		DispatchDrawData *pDispatchDrawData = (DispatchDrawData*)m_acb.allocateFromCommandBuffer(sizeofDispatchDrawData, Gnm::kEmbeddedDataAlignment4);
		pDispatchDrawData->m_pShared = m_pDispatchDrawSharedData;
		pDispatchDrawData->m_bufferInputIndexData = pDispatchDrawIndexData->m_bufferInputIndexData;
		pDispatchDrawData->m_numIndexDataBlocks = (uint16_t)pDispatchDrawIndexData->m_numIndexDataBlocks;
		pDispatchDrawData->m_numIndexBits = pDispatchDrawIndexData->m_numIndexBits;
		pDispatchDrawData->m_numInstancesPerTgMinus1 = pDispatchDrawIndexData->m_numInstancesPerTgMinus1;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4127) // constant if
#endif
		if (bIsInstancing)
		{
			//NOTE: only required by instancing shader
			pDispatchDrawData->m_instanceStepRate0Minus1 = m_dispatchDrawInstanceStepRate0Minus1;
			pDispatchDrawData->m_instanceStepRate1Minus1 = m_dispatchDrawInstanceStepRate1Minus1;
		}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
		// Pass a pointer to the dispatch draw data for the CUE to pass to shaders as kShaderInputUsagePtrDispatchDraw.
		// Its contents must not be modified until after this dispatchDraw call's shaders have finished running.
		m_cue.setDispatchDrawData(pDispatchDrawData, sizeofDispatchDrawData);
	}
	else
	{
		uint32_t firstBlock = 0;		//Do we need to be able to render a partial set of blocks?
		// We do not use the CCB to prefetch the dispatch draw control data.
		// Instead we allocate copies from the ACB which the CUE prefetches explicitly.
		// As the current version of DispatchDrawTriangleCullData contains data which changes with every draw call,
		// we must currently allocate and copy 84 bytes for every dispatchDraw call.
		// In the future, we could put data which changes infrequently DispatchDrawTriangleCullCommonData in a 
		// separate structure with a pointer in the base data, which would generally reduce this copy and command 
		// buffer allocation to 52 bytes per dispatchDraw call.
		DispatchDrawV0Data* pDispatchDrawData = (DispatchDrawV0Data*)m_acb.allocateFromCommandBuffer(sizeof(DispatchDrawV0Data), Gnm::kEmbeddedDataAlignment4);
		pDispatchDrawData->m_bufferIrb = m_dispatchDrawSharedData.m_bufferIrb;
		pDispatchDrawData->m_bufferInputIndexData = pDispatchDrawIndexData->m_bufferInputIndexData;
		pDispatchDrawData->m_numIndexDataBlocks = (uint16_t)pDispatchDrawIndexData->m_numIndexDataBlocks;
		pDispatchDrawData->m_gdsOffsetOfIrbWptr = m_dispatchDrawSharedData.m_gdsOffsetOfIrbWptr;
		pDispatchDrawData->m_sizeofIrbInIndices = m_dispatchDrawSharedData.m_bufferIrb.m_regs[2];
		pDispatchDrawData->m_clipCullSettings = m_dispatchDrawSharedData.m_cullSettings;
		pDispatchDrawData->m_numIndexBits = pDispatchDrawIndexData->m_numIndexBits;
		pDispatchDrawData->m_numInstancesPerTgMinus1 = pDispatchDrawIndexData->m_numInstancesPerTgMinus1;
		pDispatchDrawData->m_firstIndexDataBlock = (uint16_t)firstBlock;
		pDispatchDrawData->m_reserved = 0;
		pDispatchDrawData->m_quantErrorScreenX = m_dispatchDrawSharedData.m_quantErrorScreenX;
		pDispatchDrawData->m_quantErrorScreenY = m_dispatchDrawSharedData.m_quantErrorScreenY;
		pDispatchDrawData->m_gbHorizClipAdjust = m_dispatchDrawSharedData.m_gbHorizClipAdjust;
		pDispatchDrawData->m_gbVertClipAdjust = m_dispatchDrawSharedData.m_gbVertClipAdjust;
		//NOTE: only required by instancing shader
		pDispatchDrawData->m_instanceStepRate0Minus1 = m_dispatchDrawInstanceStepRate0Minus1;
		pDispatchDrawData->m_instanceStepRate1Minus1 = m_dispatchDrawInstanceStepRate1Minus1;
		//NOTE: only required by VRB shader
		pDispatchDrawData->m_bufferVrb = m_dispatchDrawSharedData.m_bufferVrb;

		// Pass a pointer to the dispatch draw data for the CUE to pass to shaders as kShaderInputUsagePtrDispatchDraw.
		// Its contents must not be modified until after this dispatchDraw call's shaders have finished running.
		m_cue.setDispatchDrawData(pDispatchDrawData, sizeof(DispatchDrawV0Data));
	}

	// m_cue.preDispatchDraw looks up settings from the currently set CsVsShader
	// NOTE: For the standard triangle culling dispatch draw shaders, many of these settings are fixed:
	//		orderedAppendMode = kDispatchOrderedAppendModeIndexPerThreadgroup
	//		dispatchDrawMode = kDispatchDrawModeIndexRingBufferOnly
	//		dispatchDrawIndexDeallocMask = 0xFC00  (CsShader.m_dispatchDrawIndexDeallocNumBits = 10)
	//		user SGPR locations may vary depending on the vertex shader source which is compiled
	Gnm::DispatchOrderedAppendMode orderedAppendMode;	//from CsShader.m_orderedAppendMode
	Gnm::DispatchDrawMode dispatchDrawMode = Gnm::kDispatchDrawModeIndexRingBufferOnly;	//from VsShader inputUsageSlots (kDispatchDrawModeIndexAndVertexRingBuffers if sgprVrbLoc is found)
	uint32_t dispatchDrawIndexDeallocMask = 0;	//from CsShader.m_dispatchDrawIndexDeallocNumBits
	uint32_t sgprKrbLoc = 0;	//get the user SGPR index from the CsShader inputUsageSlots kShaderInputUsageImmGdsKickRingBufferOffset, which must always be present
	uint32_t sgprVrbLoc = 0;	//get the user SGPR index from the VsShader inputUsageSlots kShaderInputUsageImmVertexRingBufferOffset, if any, or (uint32_t)-1 if not found; dispatchDrawMode returns kDispatchDrawModeIndexAndVertexRingBuffer if found.
	uint32_t sgprInstancesCs = 0;	//get the user SGPR index from the CsShader inputUsageSlots kShaderInputUsageImmDispatchDrawInstances, if any, or (uint32_t)-1 if not found
	uint32_t sgprInstancesVs = 0;	//get the user SGPR index from the VsShader inputUsageSlots kShaderInputUsageImmDispatchDrawInstances, if any, or (uint32_t)-1 if not found

	// Tell the CUE to set up CCB constant data for the current set shaders:
	m_cue.preDispatchDraw(&m_dcb, &m_acb, &m_ccb, &orderedAppendMode, &dispatchDrawIndexDeallocMask, &sgprKrbLoc, &sgprInstancesCs, &dispatchDrawMode, &sgprVrbLoc, &sgprInstancesVs);
	// Notify the GPU of the dispatchDrawIndexDeallocMask required by the current CsShader:
	if (dispatchDrawIndexDeallocMask != m_dispatchDrawIndexDeallocMask) {
		m_dispatchDrawIndexDeallocMask = dispatchDrawIndexDeallocMask;
		m_dcb.setDispatchDrawIndexDeallocationMask(dispatchDrawIndexDeallocMask);
	}


	uint32_t maxInstancesPerCall = (dispatchDrawIndexDeallocMask >> pDispatchDrawIndexData->m_numIndexBits) + ((dispatchDrawIndexDeallocMask & (0xFFFF << pDispatchDrawIndexData->m_numIndexSpaceBits)) != dispatchDrawIndexDeallocMask ? 1 : 0);
	if (maxInstancesPerCall == 0) {
		// For the standard triangle culling dispatch draw shaders, dispatchDrawIndexDeallocMask = 0xFC00,
		// which limits dispatchDraw calls to use no more than 63K (0xFC00) indices:
		uint32_t mask = dispatchDrawIndexDeallocMask, dispatchDrawIndexDeallocNumBits = 0;
		if (!(mask & 0xFF))	mask >>= 8, dispatchDrawIndexDeallocNumBits |= 8;
		if (!(mask & 0xF))	mask >>= 4, dispatchDrawIndexDeallocNumBits |= 4;
		if (!(mask & 0x3))	mask >>= 2, dispatchDrawIndexDeallocNumBits |= 2;
		dispatchDrawIndexDeallocNumBits |= (0x1 &~ mask);
		SCE_GNM_ASSERT_MSG(maxInstancesPerCall > 0, "dispatchDraw requires numIndexBits (%u) < 16 or numIndexSpaceBits (%u) > m_dispatchDrawIndexDeallocNumBits (%u) for the asynchronous compute shader", pDispatchDrawIndexData->m_numIndexBits, pDispatchDrawIndexData->m_numIndexSpaceBits, dispatchDrawIndexDeallocNumBits);
		m_cue.postDispatchDraw(&m_dcb, &m_acb, &m_ccb);
		return;
	}

	uint32_t numInstancesMinus1 = (m_dispatchDrawNumInstancesMinus1 & 0xFFFF);
	uint32_t numCalls = 1, numTgY = 1, numInstancesPerCall = 1;
	uint32_t numTgYLastCall = 1, numInstancesLastCall = 1;
	if (numInstancesMinus1 != 0) {
		// To implement instancing in software in an efficient way, the CS shader can pack some of the bits
		// of the instanceId into the output index data, provided the output indices are using a small enough
		// range of the available 63K index space.
		// This allows each dispatchDraw command buffer command to render multiple instances, reducing the
		// command processing and dispatch/draw overhead by a corresponding factor.  
		// If the total number of instances is greater than the number which can be rendered within a single 
		// dispatch, only a change to the kShaderInputUsageImmDispatchDrawInstances user SGPRs is required
		// between each dispatch, which keeps the overhead of additional instances to a minimum.
		// In addition, if an object is smaller than 256 triangles, it becomes possible to render multiple
		// instances of that object in a single thread group (each of which processes up to 512 triangles),
		// which correspondingly reduces the number of thread groups launched.

		// Here, we calculate how many dispatches and how many thread groups we will have to launch to
		// render the required total number of instances:
		uint32_t numInstances = numInstancesMinus1 + 1;
		uint32_t maxInstancesPerTg = pDispatchDrawIndexData->m_numInstancesPerTgMinus1+1;
		numInstancesLastCall = numInstances;
		numInstancesPerCall = numInstances;
		if (numInstances > maxInstancesPerCall) {
			numCalls = (numInstances + maxInstancesPerCall-1)/maxInstancesPerCall;
			numInstancesPerCall = maxInstancesPerCall;
			numInstancesLastCall -= (numCalls - 1) * maxInstancesPerCall;
		}
		if (numInstancesPerCall > maxInstancesPerTg)
			numTgY = (numInstancesPerCall + maxInstancesPerTg-1)/maxInstancesPerTg;
		if (numInstancesLastCall > maxInstancesPerTg)
			numTgYLastCall = (numInstancesLastCall + maxInstancesPerTg-1)/maxInstancesPerTg;
#ifdef SCE_GNM_DEBUG
		SCE_GNM_ASSERT_MSG(numInstancesPerCall*(numCalls-1) + numInstancesLastCall == numInstances && numTgY*(numCalls-1) + numTgYLastCall == (numInstances + maxInstancesPerTg-1)/maxInstancesPerTg, "dispatchDraw instancing internal error");
#endif
	}

	SCE_GNM_ASSERT_MSG(sgprKrbLoc < 16, "dispatchDraw requires an asynchronous compute shader with a kShaderInputUsageImmKickRingBufferOffset userdata sgpr");
	SCE_GNM_ASSERT_MSG(dispatchDrawMode == Gnm::kDispatchDrawModeIndexRingBufferOnly || sgprVrbLoc < 16, "dispatchDraw with a vertex ring buffer requires a VS shader with a kShaderInputUsageImmVertexRingBufferOffset userdata sgpr");
	SCE_GNM_ASSERT_MSG(dispatchDrawMode == Gnm::kDispatchDrawModeIndexRingBufferOnly || (m_dispatchDrawFlags & kDispatchDrawFlagVrbValid), "dispatchDraw with a vertex ring buffer requires setupDispatchDrawRingBuffers to be called with a valid vertex ring buffer");
	SCE_GNM_ASSERT_MSG((numInstancesMinus1 == 0) || (sgprInstancesCs < 16 && sgprInstancesVs < 16), "dispatchDraw with instancing requires asynchronous compute and VS shaders with kShaderInputUsageImmDispatchDrawInstances userdata sgprs");

	// Here we iterate over however many dispatchDraw commands are required to render all requested instances,
	// adjusting the kShaderInputUsageImmDispatchDrawInstances user SGPRs for each call:
	uint32_t firstInstance = 0;
	for (uint32_t nCall = 0; nCall+1 < numCalls; ++nCall, firstInstance += numInstancesPerCall) {
		uint32_t dispatchDrawInstances = (firstInstance<<16)|(numInstancesPerCall-1);
		m_acb.setUserData(sgprInstancesCs, dispatchDrawInstances);
		m_dcb.setUserData(Gnm::kShaderStageVs, sgprInstancesVs, dispatchDrawInstances);
		m_acb.dispatchDraw(pDispatchDrawIndexData->m_numIndexDataBlocks, numTgY, 1, orderedAppendMode, sgprKrbLoc);
		m_dcb.dispatchDraw(Gnm::kPrimitiveTypeTriList, indexOffset, primGroupIndexCount, primGroupThreshold, pollIntervalThreshold, dispatchDrawMode, sgprVrbLoc, modifier);
	}
	{
		uint32_t dispatchDrawInstances = (firstInstance<<16)|(numInstancesLastCall-1);
		if (sgprInstancesCs < 16)
			m_acb.setUserData(sgprInstancesCs, dispatchDrawInstances);
		if (sgprInstancesVs < 16)
			m_dcb.setUserData(Gnm::kShaderStageVs, sgprInstancesVs, dispatchDrawInstances);
	}
	m_acb.dispatchDraw(pDispatchDrawIndexData->m_numIndexDataBlocks, numTgYLastCall, 1, orderedAppendMode, sgprKrbLoc);
	m_dcb.dispatchDraw(Gnm::kPrimitiveTypeTriList, indexOffset, primGroupIndexCount, primGroupThreshold, pollIntervalThreshold, dispatchDrawMode, sgprVrbLoc, modifier);

	// Notify the CUE that we are done issuing draw commands that will refer to the current CCB constant data:
	m_cue.postDispatchDraw(&m_dcb, &m_acb, &m_ccb);
}


#ifdef SCE_GNMX_ENABLE_GFXCONTEXT_CALLCOMMANDBUFFERS
void GfxContext::callCommandBuffers(void *dcbAddr, uint32_t dcbSizeInDwords, void *ccbAddr, uint32_t ccbSizeInDwords)
{
	BaseGfxContext::callCommandBuffers(dcbAddr,dcbSizeInDwords,ccbAddr,ccbSizeInDwords);
	m_cue.invalidateAllBindings();
}
#endif
