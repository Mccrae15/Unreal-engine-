// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FLCUE.h: Class to support LCUE with > 4MB dcbs
=============================================================================*/

#pragma once
#include "GnmTempBlockAllocator.h"

enum class ESubmitBehavior
{
	ENormal, //Normal end of frame submit.  All DCBs are submitted. No internal state is changed.
	EMidframe, //All DCBs currently enqueued are submitted.  Commands can continue to be enqueued without waiting on the GPU.  In case we need to start the GPU working early, maybe on occlusion querries
	EMidframeReset, //All DCBs currently enqueued are submitted.  DCBs are reset.  A GPU wait must follow an EMidFrameReset as the dcb pointers are reset and the same memory will be used going forward.
};

/** Simple struct to hold commandlist submission data from a GraphicsContext */
struct SGnmCommandSubmission
{
	SGnmCommandSubmission()
	{
		Reset();
	}

	void AddSubmissionToQueue(void* DCB, uint32 SizeBytes);
	bool AddSubmissionToQueue(const SGnmCommandSubmission& Other);
	void Reset();

	// Maximum number of submissions that can be recorded.
	static const uint32 BaseNumStoredSubmissions = 32;

	// Stores the range of each previously-constructed submission (not including the one currently under construction).	
	TArray<uint32, TInlineAllocator<BaseNumStoredSubmissions> > SubmissionSizesBytes;
	TArray<void*, TInlineAllocator<BaseNumStoredSubmissions> > SubmissionAddrs;

	uint64* BeginTimestamp;
	uint64* EndTimestamp;
};

class FLCUE : public LCUE::LightweightGfxContext
{
public:	

	FLCUE() :
		DCBAllocator(nullptr),
		ResourceBufferAllocator(nullptr),
		CurrentDCBStart(nullptr),
		ActualDCBStart(nullptr),		
		ActualDCBEnd(nullptr)
	{
		StoredCommandListSubmission.Reset();
	}

	void PrepareFlip();

	/** Initializes a FLCUE with application-provided memory buffers for the LCUE.
	 *	InDCBAllocator Allocator to use for allocating DCB chunks
	 *	InResourceBufferAllocator Allocator to use for allocating LCUE Resource Buffer chunks.
	 *  CallbackFunc The callback to use when the dispatch command buffer (dcb) runs out of space for new commands.
	 *  CallbackUserData User data to pass to CallbackFunc.
	 */
	void init(TDCBAllocator& InDCBAllocator, TLCUEResourceAllocator& InResourceBufferAllocator, sce::Gnm::CommandCallbackFunc CallbackFunc, void* CallbackUserData);	

	/** Initializes a FLCUE with application-provided memory buffers for the LCUE.  Preserves any existing cached LCUE state.  Useful for mid-frame submits that re-use DCB/Resource memory.
	*	InDCBAllocator Allocator to use for allocating DCB chunks
	*	InResourceBufferAllocator Allocator to use for allocating LCUE Resource Buffer chunks.
	*  CallbackFunc The callback to use when the dispatch command buffer (dcb) runs out of space for new commands.
	*  CallbackUserData User data to pass to CallbackFunc.
	*/
	void InitPreserveState(TDCBAllocator& InDCBAllocator, TLCUEResourceAllocator& InResourceBufferAllocator, sce::Gnm::CommandCallbackFunc CallbackFunc, void* CallbackUserData);

	/**
	 * Ensures there are NumBytes bytes available contiguously in the dcb.
	 */
	void Reserve(int32 NumBytes);

	/** Submits the DrawCommandBuffer	
	 *	returns A code indicating the submission status.
	 */
	int32 submit() { check(false); return submit(ESubmitBehavior::ENormal); }

	/** Submits accumulated commands to GPU.  Valid to continue writing commands after making this call 
	 *  returns A code indicating the submission status.
	 */
	int32 submit(ESubmitBehavior Behavior);

	/** 
	 * Submits accumulated commands to GPU.  Also emits commands to have the GPU issue a flip.
	 */
	int32 submitAndFlip(uint32 VideoOutHandler, uint32 BufferToFlip, uint32 FlipMode, int64 FlipArg);

	/** 
	 * Stores off all DCB ranges to be submitted for commands enqueued up to this call.  
	 * Invalidates this FLCUE for further rendering.
	 */
	const SGnmCommandSubmission& Finalize(uint64* BeginTimestamp, uint64* EndTimestamp);

	/**
	* Returns all DCB ranges to be submitted for commands enqueued up to this call.  Clears DCB history.
	* FLCUE can be used for further rendering.
	*/
	SGnmCommandSubmission PrepareCurrentCommands(uint64* BeginTimestamp, uint64* EndTimestamp);

	uint32 GetCurrentDCBSize() const
	{
		return (m_dcb.m_cmdptr - CurrentDCBStart) * sizeof(uint32);
	}

	/** 
	 * Resets the context.  call once per frame
	 */
	void reset();

	/** 
	 * If enabled make sure that if we are in the reserve segment of the command buffer we start a new buffer
	 */
	FORCEINLINE void CommandBufferReserve()
	{
#if PS4_ENABLE_COMMANDBUFFER_RESERVE
		m_dcb.reserveSpaceInDwords( COMMANDBUFFER_RESERVE_SIZE_BYTES / sizeof( uint32 ) );
#endif
	}
private:

	static bool MasterHandleReserveFailed(sce::Gnm::CommandBuffer* DCBIn, uint32_t DwordCount, void* UserData);	
	static uint32_t* ResourceBufferReserveFailed(sce::Gnmx::BaseConstantUpdateEngine* LWCUE, uint32_t SizeInDwords, uint32_t* ResultSizeInDwords, void* UserData);

	/** Init code that is common between regular and state-preserving init.*/
	void InitCommon(uint32* DCBBuffer, uint32 VirtualDCBSizeInDwords, sce::Gnm::CommandCallbackFunc CallbackFunc, void* CallbackUserData);

	/* Sets up internal submission array ranges. */
	void PrepareForSubmit(ESubmitBehavior Behavior);

	/* Call after actually performing the submit.  Also resets SubmissionCount and the dcb according to Behavior */
	void FinalizeSubmit(ESubmitBehavior Behavior);

	// Adds current commands to the submission queue and moves the current DCB pointer
	void AddSubmissionToQueue();

	TDCBAllocator* DCBAllocator;
	TLCUEResourceAllocator* ResourceBufferAllocator;

	// Beginning of the submit currently being constructed in the DCB
	const uint32* CurrentDCBStart;

	// Actual start of the DCB data the user provided.
	const uint32* ActualDCBStart;	

	// Actual end of the DCB's data buffer.  Can be bigger than 4MB
	const uint32* ActualDCBEnd;	
	
	// buffer of last resort in case we need to do a midframe submit and there's not enough dcb left to insert the label
	// commands to wait for it properly.
	static const int ReserveSizeDwords = 64;	

	SGnmCommandSubmission StoredCommandListSubmission;

	// Invoked when DCB runs out of space.
	sce::Gnm::CommandCallback BufferFullCallback; 
};
