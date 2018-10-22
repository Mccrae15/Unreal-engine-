// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FLCUE.cpp: LCUE subclass to support > 4MB dcbs.
=============================================================================*/

#include "GnmRHIPrivate.h" 
#include "FLCUE.h"

void SGnmCommandSubmission::AddSubmissionToQueue(void* DCB, uint32 SizeBytes)
{
	//0 sized submissions will cause gnm errors.  This can happen legitimately if a PrepareForSubmit
	//happens and then the next command causes a reserve failure.
	if (SizeBytes > 0)
	{
		SubmissionAddrs.Add(DCB);
		SubmissionSizesBytes.Add(SizeBytes);
		check(SizeBytes <= Gnm::kIndirectBufferMaximumSizeInBytes);	
	}
	checkf(SubmissionAddrs.Num() == SubmissionSizesBytes.Num(), TEXT("submission mismatch: %i, %i"), SubmissionAddrs.Num(), SubmissionSizesBytes.Num());
}

bool SGnmCommandSubmission::AddSubmissionToQueue(const SGnmCommandSubmission& Other)
{
	for (int32 i = 0; i < Other.SubmissionAddrs.Num(); ++i)
	{
		SubmissionAddrs.Add(Other.SubmissionAddrs[i]);
		SubmissionSizesBytes.Add(Other.SubmissionSizesBytes[i]);
	}
	checkf(SubmissionAddrs.Num() == SubmissionSizesBytes.Num(), TEXT("submission mismatch: %i, %i"), SubmissionAddrs.Num(), SubmissionSizesBytes.Num());
	return true;	
}

void SGnmCommandSubmission::Reset()
{
	SubmissionAddrs.Empty();
	SubmissionSizesBytes.Empty();
	BeginTimestamp = nullptr;
	EndTimestamp = nullptr;
}

static const uint32 CommandBufferSizeInDWords = ( (Gnm::kIndirectBufferMaximumSizeInBytes) / sizeof(uint32));
bool FLCUE::MasterHandleReserveFailed(sce::Gnm::CommandBuffer* DCBIn, uint32_t DwordCount, void* UserData)
{	
	FLCUE & Context = *(FLCUE*)UserData;

	//theoretically, the DCBIn could be a CCB, but LCUE doesn't use the CCB so it shouldn't be.
	check((void*)DCBIn == (void*)&Context.m_dcb);
	sce::Gnm::DrawCommandBuffer& DCB = *(sce::Gnm::DrawCommandBuffer*)DCBIn;

	// if our reserve ran out we need to allocate another chunk and adjust all the pointers.
	if (DCB.m_endptr == Context.ActualDCBEnd)
	{
		uint32 DCBSizeDWords = (Context.DCBAllocator->GetChunkSize() / sizeof(uint32));
		uint32* DCBBuffer = (uint32*)Context.DCBAllocator->AllocateChunk();

		//queue the current submission range
		Context.AddSubmissionToQueue();
				
		if (DCBBuffer)
		{			
			Context.ActualDCBStart = DCBBuffer;
			Context.ActualDCBEnd = DCBBuffer + DCBSizeDWords;
			Context.CurrentDCBStart = DCBBuffer;

			DCB.m_beginptr = (uint32*)Context.ActualDCBStart;
			DCB.m_cmdptr = (uint32*)Context.ActualDCBStart;
			DCB.m_endptr = (uint32*)Context.ActualDCBEnd;			
		}
		else
		{
			SCE_GNM_ERROR("Out of DCB command buffer space, and could not allocate more.");
			return false;
		}
	}
	// 
	else
	{
		//queues the current submission range and advances the DCB pointers.
		Context.AddSubmissionToQueue();
	}	
	return true;
}

uint32_t* FLCUE::ResourceBufferReserveFailed(sce::Gnmx::BaseConstantUpdateEngine* LWCUE, uint32_t SizeInDwords, uint32_t* ResultSizeInDwords, void* UserData)
{
	FLCUE & Context = *(FLCUE*)UserData;

	checkf(&Context.m_lwcue == LWCUE, TEXT("UserData: %p, LWCUE: %p"), UserData, LWCUE);
	check(ResultSizeInDwords);

	uint32 RBSizeDWords = (Context.ResourceBufferAllocator->GetChunkSize() / sizeof(uint32));
	check(SizeInDwords <= RBSizeDWords)

	uint32* ResourceBuffer = (uint32*)Context.ResourceBufferAllocator->AllocateChunk();
	check(ResourceBuffer);

	*ResultSizeInDwords = RBSizeDWords;
	return ResourceBuffer;
}

void FLCUE::AddSubmissionToQueue()
{
	// Register a new submit up to the current DCB command pointer
	sce::Gnmx::GnmxDrawCommandBuffer& DCB = m_dcb;
	StoredCommandListSubmission.AddSubmissionToQueue((void*)CurrentDCBStart, static_cast<uint32>(DCB.m_cmdptr - CurrentDCBStart) * sizeof(uint32));
	CurrentDCBStart = DCB.m_cmdptr;

	//Advance CB end pointer to the next (possibly artificial) boundary -- either current+(4MB-4), or the end of the actual buffer	
	DCB.m_endptr = FMath::Min(DCB.m_cmdptr + CommandBufferSizeInDWords, (uint32*)ActualDCBEnd);
}

void FLCUE::PrepareFlip()
{
	m_dcb.prepareFlip();
}

void FLCUE::init(TDCBAllocator& InDCBAllocator, TLCUEResourceAllocator& InResourceBufferAllocator, sce::Gnm::CommandCallbackFunc CallbackFunc, void* CallbackUserData)
{
	DCBAllocator = &InDCBAllocator;
	ResourceBufferAllocator = &InResourceBufferAllocator;

	uint32 ResourceBufferSizeInDwords = InResourceBufferAllocator.GetChunkSize() / sizeof(uint32);
	uint32* ResourceBufferInGarlic = (uint32*)ResourceBufferAllocator->AllocateChunk();
	
	uint32 DCBChunkSizeInDwords = (InDCBAllocator.GetChunkSize() / sizeof(uint32));
	
	//Compute a size <= to the hardware limit for a single DCB submission.  LCUE doesn't natively support larger sizes.
	uint32 VirtualDCBSizeInDWords = FMath::Min((uint32)(CommandBufferSizeInDWords), DCBChunkSizeInDwords);
	uint32* DCBBuffer = (uint32*)DCBAllocator->AllocateChunk();	

	//init the actual LCUE with the adjusted size, and a special handler.
	//engine expected to allocate and set globaltables as required.

	LightweightGfxContext::init((void*)DCBBuffer, VirtualDCBSizeInDWords*4, (void*)ResourceBufferInGarlic, ResourceBufferSizeInDwords*4,nullptr);
	InitCommon(DCBBuffer, VirtualDCBSizeInDWords, CallbackFunc, CallbackUserData);
}

void FLCUE::InitPreserveState(TDCBAllocator& InDCBAllocator, TLCUEResourceAllocator& InResourceBufferAllocator, sce::Gnm::CommandCallbackFunc CallbackFunc, void* CallbackUserData)
{
	DCBAllocator = &InDCBAllocator;
	ResourceBufferAllocator = &InResourceBufferAllocator;

	uint32 ResourceBufferSizeInDwords = InResourceBufferAllocator.GetChunkSize() / sizeof(uint32);
	uint32* ResourceBufferInGarlic = (uint32*)ResourceBufferAllocator->AllocateChunk();

	uint32 DCBChunkSizeInDwords = (InDCBAllocator.GetChunkSize() / sizeof(uint32));

	//Compute a size <= to the hardware limit for a single DCB submission.  LCUE doesn't natively support larger sizes.
	uint32 VirtualDCBSizeInDWords = FMath::Min((uint32)(CommandBufferSizeInDWords), DCBChunkSizeInDwords);
	uint32* DCBBuffer = (uint32*)DCBAllocator->AllocateChunk();

	//bypass LightweightGfxContext::init as it resets a ton of internal state.  The goal of the state preserving init is to reinit the context after a mid-frame submit,
	//but not to lose any state so we don't have to issue any new GPU commands to get back to a known state.
	sce::Gnmx::BaseConstantUpdateEngine* BaseCUE = (sce::Gnmx::BaseConstantUpdateEngine*)&m_lwcue;
	BaseCUE->init(&ResourceBufferInGarlic, 1, ResourceBufferSizeInDwords, nullptr);

	InitCommon(DCBBuffer, VirtualDCBSizeInDWords, CallbackFunc, CallbackUserData);
}

void FLCUE::Reserve(int32 NumBytes)
{
	m_dcb.reserveSpaceInDwords((NumBytes / sizeof(uint32)) + 1);
}

void FLCUE::InitCommon(uint32* DCBBuffer, uint32 VirtualDCBSizeInDwords, sce::Gnm::CommandCallbackFunc CallbackFunc, void* CallbackUserData)
{
	uint32 DCBChunkSizeInDwords = (DCBAllocator->GetChunkSize() / sizeof(uint32));
	StoredCommandListSubmission.Reset();

	//SDK 2.0 removes the reserve callbacks because it handles 'greater than 4MB dcbs internally'. However, we want small DCBs that grow on demand, so we still have to manage this stuff ourselves.
	m_dcb.init(DCBBuffer, DCBAllocator->GetChunkSize(), MasterHandleReserveFailed, this);

	setResourceBufferFullCallback(ResourceBufferReserveFailed, this);

	//keep track of the real start/end of the buffer the user gave us.
	ActualDCBStart = DCBBuffer;
	ActualDCBEnd = DCBBuffer + DCBChunkSizeInDwords;
	CurrentDCBStart = DCBBuffer;

	BufferFullCallback.m_func = CallbackFunc;
	BufferFullCallback.m_userData = CallbackUserData;
}

void FLCUE::PrepareForSubmit(ESubmitBehavior Behavior)
{
	StoredCommandListSubmission.AddSubmissionToQueue((void*)CurrentDCBStart, static_cast<uint32>(m_dcb.m_cmdptr - CurrentDCBStart) * sizeof(uint32));	
}

void FLCUE::FinalizeSubmit(ESubmitBehavior Behavior)
{
	switch (Behavior)
	{
	case ESubmitBehavior::EMidframe:
		CurrentDCBStart = m_dcb.m_cmdptr;
		StoredCommandListSubmission.Reset();
		break;
	case ESubmitBehavior::EMidframeReset:
		StoredCommandListSubmission.Reset();
		CurrentDCBStart = ActualDCBStart;
		m_dcb.m_beginptr = (uint32*)ActualDCBStart;
		m_dcb.m_cmdptr = (uint32*)ActualDCBStart;
		m_dcb.m_endptr = FMath::Min(m_dcb.m_cmdptr + CommandBufferSizeInDWords, (uint32*)ActualDCBEnd);
		m_lwcue.m_bufferCurrent = m_lwcue.m_bufferBegin[m_lwcue.m_bufferIndex];
		m_lwcue.m_bufferCurrentBegin = m_lwcue.m_bufferBegin[m_lwcue.m_bufferIndex];
		m_lwcue.m_bufferCurrentEnd = m_lwcue.m_bufferEnd[m_lwcue.m_bufferIndex];
		break;
	default:
		break;
	}
}

int32 FLCUE::submit(ESubmitBehavior Behavior)
{
	check(false);	
	
	PrepareForSubmit(Behavior);
	int32 RetVal = Gnm::submitCommandBuffers(StoredCommandListSubmission.SubmissionAddrs.Num(), StoredCommandListSubmission.SubmissionAddrs.GetData(), StoredCommandListSubmission.SubmissionSizesBytes.GetData(), 0, 0);
	FinalizeSubmit(Behavior);

	return RetVal;
}

int32 FLCUE::submitAndFlip(uint32 VideoOutHandler, uint32 BufferToFlip, uint32 FlipMode, int64 FlipArg)
{
	check(false);
	m_dcb.prepareFlip();
	PrepareForSubmit(ESubmitBehavior::ENormal);
	int32 RetVal = Gnm::submitAndFlipCommandBuffers(StoredCommandListSubmission.SubmissionAddrs.Num(), StoredCommandListSubmission.SubmissionAddrs.GetData(), StoredCommandListSubmission.SubmissionSizesBytes.GetData(), 0, 0, VideoOutHandler, BufferToFlip, FlipMode, FlipArg);
	FinalizeSubmit(ESubmitBehavior::ENormal);

	return RetVal;
}

const SGnmCommandSubmission& FLCUE::Finalize(uint64* BeginTimestamp, uint64* EndTimestamp)
{
	PrepareForSubmit(ESubmitBehavior::ENormal);		

	// wipe cmdptr to invalidate this context after a submit.
	m_dcb.m_cmdptr = nullptr;
	StoredCommandListSubmission.BeginTimestamp = BeginTimestamp;
	StoredCommandListSubmission.EndTimestamp = EndTimestamp;
	return StoredCommandListSubmission;
}

SGnmCommandSubmission FLCUE::PrepareCurrentCommands(uint64* BeginTimestamp, uint64* EndTimestamp)
{
	PrepareForSubmit(ESubmitBehavior::EMidframe);
	SGnmCommandSubmission OutCommands = StoredCommandListSubmission;
	OutCommands.BeginTimestamp = BeginTimestamp;
	OutCommands.EndTimestamp = EndTimestamp;
	FinalizeSubmit(ESubmitBehavior::EMidframe);
	return OutCommands;
}

void FLCUE::reset()
{
	StoredCommandListSubmission.Reset();
	LightweightGfxContext::reset();

	CurrentDCBStart = ActualDCBStart;
	
	m_dcb.m_beginptr = (uint32*)ActualDCBStart;
	m_dcb.m_cmdptr = (uint32*)ActualDCBStart;
	m_dcb.m_endptr = FMath::Min(m_dcb.m_cmdptr + CommandBufferSizeInDWords, (uint32*)ActualDCBEnd);
}
