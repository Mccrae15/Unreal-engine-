// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GPUDefragAllocator.h"

class FPS3RHITexture;

class FPS4GPUDefragAllocator : public FGPUDefragAllocator
{
public:
	static const int64 MemMoveTempAreaSize = 16 * 1024 * 1024;

	FPS4GPUDefragAllocator();
	~FPS4GPUDefragAllocator();


	virtual void Initialize(uint8* PoolMemory, int64 PoolSize) override;
	
	virtual void* Allocate(int64 Size, int32 Alignment, TStatId Stat, bool bAllowFailure) override;	

	/** 
	 * Allocates memory of the given size in a state that is NOT relocatable.  Done inside the call so as to be threadsafe compared to subsequent calls to Allocate, then Lock.
	 */
	void* AllocateLocked(int64 Size, int32 Alignment, TStatId Stat, bool bAllowFailure);
	

	virtual void Free(void* Pointer) override;

	virtual int32 Tick(FRelocationStats& Stats, bool bPanicDefrag) override;
	void LockWithFence(void* Pointer, FComputeFenceRHIParamRef LockFence);

#if 0
	/**
	* Requests an async allocation or reallocation.
	* The caller must hold on to the request until it has been completed or canceled.
	*
	* @param ReallocationRequest	The request
	* @param bForceRequest			If TRUE, the request will be accepted even if there's currently not enough free space
	* @return						TRUE if the request was accepted
	*/
	bool	AsyncReallocate(FAsyncReallocationRequest* ReallocationRequest, bool bForceRequest)
	{
		return FGPUDefragAllocator::AsyncReallocate(ReallocationRequest, bForceRequest);
	}
#endif	

#if 0
	/**
	* Blocks the calling thread until the specified request has been completed.
	*
	* @param Request	Request to wait for. Must be a valid request.
	*/
	virtual void	BlockOnAsyncReallocation(FAsyncReallocationRequest* Request) override
	{
		FGPUDefragAllocator::BlockOnAsyncReallocation(Request);
	}
#endif		

	/**
	* Notify the texture pool that a new texture has been created.
	* This allows its memory to be relocated during defragmentation.
	*
	* @param Texture	The new texture object that has been created
	*/
	void	RegisterDefraggable(FGnmDefraggable* Resource);

	/**
	* Notify the texture pool that a texture has been deleted.
	*
	* @param Texture	The texture object that is being deleted
	*/
	void	UnregisterDefraggable(FGnmDefraggable* Resource);



	/**
	* Method to safely do a full defrag, flush pending frees, and flush the GPU. 
	* Must be called from renderthread with RHI thread flushed.
	*/
	void FullDefragAndFlushGPU();

#if 0
	/**
	* Defragment the texture memory. This function can be called from both gamethread and renderthread.
	* Texture memory is shuffled around primarily using GPU transfers. Texture memory that can't be associated
	* with a tracked texture object will not be relocated. Locked textures will not be relocated either.
	*/
	void	DefragmentTextureMemory();
#endif

protected:

	void FullDefragAndFlushGPU_Internal();

	float EstimateGPUTimeForMove(int32 Size, bool bOverlapped);
	virtual void	PlatformRelocate(void* Dest, const void* Source, int64 Size, void* UserPayload) override;
	virtual uint64	PlatformInsertFence() override;
	virtual void	PlatformBlockOnFence(uint64 Fence) override;
	virtual bool	PlatformCanRelocate(const void* Source, void* UserPayload) const override;
	virtual void	PlatformNotifyReallocationFinished(FAsyncReallocationRequest* FinishedRequest, void* UserPayload) override;

	void PS4MemMoveWithCompute(const uint8* Dest, const uint8* Src, const uint64 NumBytes);

#if VALIDATE_MEMORY_PROTECTION	
	virtual void	PlatformSetStandardMemoryPrivileges(const FMemProtectTracker& Block) override;
	virtual void	PlatformSetNoMemoryPrivileges(const FMemProtectTracker& Block) override;

	virtual void	PlatformSetStaticMemoryPrivileges(const FMemProtectTracker& Block) override;
	virtual void	PlatformSetRelocationMemoryPrivileges(const FMemProtectTracker& Block) override;
	virtual void	PlatformSetRelocationMemoryPrivileges(const TArray<FMemProtectTracker>& BlocksToRemove) override;
#endif

	//when moving memory by less than the resource size we have to move in non-overlapped chunks for correctness.
	//with compute shaders this can require far too many gpu flushes to be efficient.  Instead we move to temporary location in large chunks
	//and then shift back.  This doubles the bandwidth required but that's cheaper than all the flushing would be.
	uint8* MemMoveTempArea;	
	FMemBlock MemMoveTempAreaBlock;

	volatile uint64* FenceLabel;

	//3 frames to account for renderthread/rhi thread overlap + GPU.
	static const int32 NUM_BUFFERED_FRAMES_FOR_FREES = 4;
		
	TMultiMap<void*, FComputeFenceRHIRef> FenceLocks;
	TMultiMap<void*, FComputeFenceRHIRef> FenceFrees;
	TArray<void*> FreesWithLocks;
	TArray<void*> DeferredFrees[NUM_BUFFERED_FRAMES_FOR_FREES];

	int32 CurrentFreeBuffer;
};


/**
* Defragment the texture memory. This function can be called from both gamethread and renderthread.
* Texture memory is shuffled around primarily using GPU transfers. Texture memory that can't be associated
* with a tracked texture object will not be relocated. Locked textures will not be relocated either.
*/
void appDefragmentTexturePool();