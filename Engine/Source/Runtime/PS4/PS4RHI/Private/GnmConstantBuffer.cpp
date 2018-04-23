// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmConstantBuffer.cpp: Gnm Constant buffer implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "GnmContext.h"
#include "GnmComputeContext.h"

FGnmConstantBuffer::FGnmConstantBuffer(uint32 InSize) 
	: MaxSize(Align(InSize, CONSTANT_BUFFER_VIDEO_ALIGNMENT))
	, bIsDirty(false)
	, ShadowData(NULL)
	, CurrentUpdateSize(0)
	, TotalUpdateSize(0)
{
	InitResource();
}

FGnmConstantBuffer::~FGnmConstantBuffer()
{
	ReleaseResource();
}

/**
* Creates a constant buffer on the device
*/
void FGnmConstantBuffer::InitDynamicRHI()
{
	// allocate the local shadow copy of the data
	ShadowData = (uint8*)FMemory::Malloc(MaxSize);
	FMemory::Memzero(ShadowData, MaxSize);
}

void FGnmConstantBuffer::ReleaseDynamicRHI()
{
	// free local shadow copy
	FMemory::Free(ShadowData);
}

/**
* Updates a variable in the constant buffer.
* @param Data - The data to copy into the constant buffer
* @param Offset - The offset in the constant buffer to place the data at
* @param Size - The size of the data being copied
*/
void FGnmConstantBuffer::UpdateConstant(const uint8* Data, uint16 Offset, uint16 Size)
{
	// copy the constant into the buffer
	FMemory::Memcpy(ShadowData + Offset, Data, Size);
	
	// mark the highest point used in the buffer
	CurrentUpdateSize = FPlatformMath::Max<uint32>(Offset + Size, CurrentUpdateSize);
	
	// this buffer is now dirty
	bIsDirty = true;
}

template<typename TGnmCommandContext>
bool FGnmConstantBuffer::SetupCommitConstantsToDevice(TGnmCommandContext& CommandListContext, Gnm::ShaderStage Stage, uint32 BufferIndex, bool bDiscardSharedConstants, Gnm::Buffer& OutBuffer)
{
	// is there anything that needs to be pushed to GPU?
	if (!bIsDirty)
	{
		return false;
	}

	if (bDiscardSharedConstants)
	{
		// If we're discarding shared constants, just use constants that have been updated since the last Commit.
		TotalUpdateSize = CurrentUpdateSize;
	}
	else
	{
		// If we're re-using shared constants, use all constants.
		TotalUpdateSize = FPlatformMath::Max(CurrentUpdateSize, TotalUpdateSize);
	}

	// copy the constants into the command buffer or ring buffer (command buffer growing too big can be an issue)	
	void* CommandBufferConstants = CommandListContext.AllocateFromTempFrameBuffer(TotalUpdateSize);

	FMemory::Memcpy(CommandBufferConstants, ShadowData, TotalUpdateSize);

	// create a constant buffer object to tell GPU about it	
	OutBuffer.initAsConstantBuffer(CommandBufferConstants, TotalUpdateSize);
	OutBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

	// assign this memory as the constant memory
	check(OutBuffer.getDataFormat().m_asInt != Gnm::kDataFormatInvalid.m_asInt);

	return true;
}

/**
 * Pushes the outstanding buffer data to the GPU
 */
#if 0
template<typename TGnmCommandContext>
bool FGnmConstantBuffer::CommitConstantsToDevice(TGnmCommandContext& CommandListContext, Gnm::ShaderStage Stage, uint32 BufferIndex, bool bDiscardSharedConstants)
{
	check(0);
}
#endif

bool FGnmConstantBuffer::CommitConstantsToDevice(FGnmCommandListContext& CommandListContext, Gnm::ShaderStage Stage, uint32 BufferIndex, bool bDiscardSharedConstants)
{
	Gnm::Buffer Buffer;
	if (!SetupCommitConstantsToDevice<FGnmCommandListContext>(CommandListContext, Stage, BufferIndex, bDiscardSharedConstants, Buffer))
	{
		return false;
	}
	CommandListContext.GetContext().setConstantBuffers(Stage, BufferIndex, 1, &Buffer);
	TrackResource(sce::Shader::Binary::kInternalBufferTypeCbuffer, BufferIndex, Stage);

	// no longer dirty, and we are cleared out
	bIsDirty = false;
	CurrentUpdateSize = 0;
	return true;
}

bool FGnmConstantBuffer::CommitConstantsToDevice(FGnmComputeCommandListContext& CommandListContext, Gnm::ShaderStage Stage, uint32 BufferIndex, bool bDiscardSharedConstants)
{
	Gnm::Buffer Buffer;
	if (!SetupCommitConstantsToDevice<FGnmComputeCommandListContext>(CommandListContext, Stage, BufferIndex, bDiscardSharedConstants, Buffer))
	{
		return false;
	}
	CommandListContext.GetContext().setConstantBuffers(BufferIndex, 1, &Buffer);	

	// no longer dirty, and we are cleared out
	bIsDirty = false;
	CurrentUpdateSize = 0;
	return true;
}




/*-----------------------------------------------------------------------------
	Uniform buffer RHI object
-----------------------------------------------------------------------------*/

// Constructor
FGnmUniformBuffer::FGnmUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& InLayout, EUniformBufferUsage Usage)
	: FRHIUniformBuffer(InLayout)
	, ConstantBufferSize(0)
#if ENABLE_BUFFER_INTEGRITY_CHECK
	, AllocationFrame(0xFFFFFFFF)
#endif
{
	// CB Size may be 0 if the uniform buffer just contains textures/samplers
	if (InLayout.ConstantBufferSize > 0)
	{	
		bool bCanFastAllocate = FGnmManager::SafeToAllocateFromTempRingBuffer();

		void* BufferPtr;
		// single use uniform buffers can be simply allocated from the command buffer, since they won't be
		// needed after this frame
		if (bCanFastAllocate && (Usage == UniformBuffer_SingleDraw || Usage == UniformBuffer_SingleFrame))
		{
			// allocate temporary memory for single use buffers		
			BufferPtr = GGnmManager.AllocateFromTempRingBuffer(InLayout.ConstantBufferSize);
#if ENABLE_BUFFER_INTEGRITY_CHECK
			AllocationFrame = GGnmManager.GetFrameCount();
#endif
		}
		else
		{
			// allocate permanent storage
			UniformBufferMemory = FMemBlock::Allocate(InLayout.ConstantBufferSize, BUFFER_VIDEO_ALIGNMENT, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_MultiuseUniformBuffer));
			// pull the ptr out
			BufferPtr = UniformBufferMemory.GetPointer();
		}

		// initialize the buffer
		if (Contents)
		{
			FMemory::Memcpy(BufferPtr, Contents, InLayout.ConstantBufferSize);
		}
	
		// setup the Gnm descriptor
		Buffer.initAsConstantBuffer(BufferPtr, InLayout.ConstantBufferSize);
		Buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);		
	}
	ConstantBufferSize = InLayout.ConstantBufferSize;
}

// Destructor 
FGnmUniformBuffer::~FGnmUniformBuffer()
{
	// free the memory if it was allocated
	FMemBlock::Free(UniformBufferMemory);
}

void FGnmUniformBuffer::Set(FGnmCommandListContext& CommandListContext, Gnm::ShaderStage Stage, uint32 BufferIndex)
{
#if ENABLE_BUFFER_INTEGRITY_CHECK
	uint32 CurrentFrame = GGnmManager.GetFrameCount();
	bool bValidAllocation = AllocationFrame == 0xFFFFFFFF || AllocationFrame == CurrentFrame;
	checkf( bValidAllocation, TEXT( "SingleFrame/SingleDraw buffer is used over multiple frames" ) );
#endif

	if (ConstantBufferSize > 0)
	{	
		check(Buffer.getDataFormat().m_asInt != Gnm::kDataFormatInvalid.m_asInt);
		CommandListContext.GetContext().setConstantBuffers(Stage, BufferIndex, 1, &Buffer);
		TrackResource(sce::Shader::Binary::kInternalBufferTypeCbuffer, BufferIndex, Stage);
	}
}

void FGnmUniformBuffer::Set(FGnmComputeCommandListContext& CommandListContext, uint32 BufferIndex)
{
	if (ConstantBufferSize > 0)
	{
		check(Buffer.getDataFormat().m_asInt != Gnm::kDataFormatInvalid.m_asInt);
		CommandListContext.GetContext().setConstantBuffers(BufferIndex, 1, &Buffer);
	}
}

FUniformBufferRHIRef FGnmDynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
{
	SCOPE_CYCLE_COUNTER(STAT_GnmCreateUniformBufferTime);
	FGnmUniformBuffer* NewUniformBuffer = nullptr;
	uint32 NumBytes = Layout.ConstantBufferSize;

	if (NumBytes > 0)
	{
		// Constant buffers must also be 16-byte aligned.
		check(Align(NumBytes,16) == NumBytes);
		check(Align(Contents,16) == Contents);

		SCOPE_CYCLE_COUNTER(STAT_GnmUpdateUniformBufferTime);

		NewUniformBuffer = new FGnmUniformBuffer(Contents, Layout, Usage);
	}
	else
	{
		// This uniform buffer contains no constants, only a resource table.
		NewUniformBuffer = new FGnmUniformBuffer(nullptr, Layout, Usage);
	}

	if (Layout.Resources.Num())
	{
		int32 NumResources = Layout.Resources.Num();
		FRHIResource** InResources = (FRHIResource**)((uint8*)Contents + Layout.ResourceOffset);
		NewUniformBuffer->ResourceTable.Empty(NumResources);
		NewUniformBuffer->ResourceTable.AddZeroed(NumResources);
		for (int32 i = 0; i < NumResources; ++i)
		{
			check(InResources[i]);
			NewUniformBuffer->ResourceTable[i] = InResources[i];
		}
	}

	return NewUniformBuffer;
}
