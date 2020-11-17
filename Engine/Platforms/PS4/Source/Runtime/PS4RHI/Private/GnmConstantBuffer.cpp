// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmConstantBuffer.cpp: Gnm Constant buffer implementation.
=============================================================================*/

#include "GnmConstantBuffer.h"
#include "GnmRHIPrivate.h"
#include "GnmContext.h"
#include "GnmComputeContext.h"

FGnmConstantBuffer::FGnmConstantBuffer(uint32 InSize) 
	: MaxSize(Align(InSize, ConstantBufferAlignmentSize))
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

	// Align size to 16 bytes
	TotalUpdateSize = FPlatformMath::Min(MaxSize, Align(TotalUpdateSize, ConstantBufferAlignmentSize));

	// copy the constants into the command buffer or ring buffer (command buffer growing too big can be an issue)	
	void* CommandBufferConstants = CommandListContext.AllocateFromTempFrameBuffer(TotalUpdateSize, sce::Gnm::kAlignmentOfBufferInBytes);
	
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
FGnmUniformBuffer::FGnmUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& InLayout, EUniformBufferUsage InUsage, EUniformBufferValidation InValidation)
	: FRHIUniformBuffer(InLayout)
	, Layout(InLayout)
	, Usage(InUsage)
	, ConstantBufferSize(InLayout.ConstantBufferSize)
#if ENABLE_BUFFER_INTEGRITY_CHECK
	, AllocationFrame(0xFFFFFFFF)
#endif
{
	Update(nullptr, Contents, InValidation);
}

void FGnmUniformBuffer::Update(FRHICommandListImmediate* RHICmdList, const void* Contents, EUniformBufferValidation Validation)
{
	// Allocate a new GPU buffer and copy the data to it.
	void* BufferPtr = nullptr;
	if (ConstantBufferSize > 0)
		{
		// Single use uniform buffers can simply be allocated from the command buffer, since they won't be needed after this frame.
		if ((Usage == UniformBuffer_SingleDraw || Usage == UniformBuffer_SingleFrame) && FGnmManager::SafeToAllocateFromTempRingBuffer())
		{
			// Allocate temporary memory for single use buffers
			BufferPtr = GGnmManager.AllocateFromTempRingBuffer(ConstantBufferSize, sce::Gnm::kAlignmentOfBufferInBytes);
#if ENABLE_BUFFER_INTEGRITY_CHECK
			AllocationFrame = GGnmManager.GetFrameCount();
#endif
		}
		else
		{
			// Allocate from permanent storage
			UniformBufferMemoryRT = FMemBlock::Allocate(ConstantBufferSize, BUFFER_VIDEO_ALIGNMENT, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_MultiuseUniformBuffer));
			BufferPtr = UniformBufferMemoryRT.GetPointer();
		}

		FMemory::Memcpy(BufferPtr, Contents, ConstantBufferSize);
	}

	// Update the resource table. If we're deferring this to the RHI thread, then
	// we'll create a new TArray instead, which we hand to the RHI thread later.
	FRHIResource** CmdListResources = nullptr;
	int32 NumResources = Layout.Resources.Num();
	if (NumResources)
	{
		TArray<TRefCountPtr<FRHIResource>>* ResourceTableRef = RHICmdList ? nullptr : &ResourceTable;
		CmdListResources = RHICmdList ? (FRHIResource**)RHICmdList->Alloc(sizeof(FRHIResource*) * NumResources, alignof(FRHIResource*)) : nullptr;

		if (ResourceTableRef)
		{
			ResourceTableRef->Empty(NumResources);
			ResourceTableRef->AddZeroed(NumResources);
		}

		for (int32 i = 0; i < NumResources; ++i)
		{
			FRHIResource* Resource = *(FRHIResource**)((uint8*)Contents + Layout.Resources[i].MemberOffset);
#if DO_CHECK
			if (Validation == EUniformBufferValidation::ValidateResources)
			{
				checkf(Resource, TEXT("Invalid resource entry creating uniform buffer, %s.Resources[%u], ResourceType 0x%x."), *Layout.GetDebugName(), i, (uint8)Layout.Resources[i].MemberType);
			}
#endif
			if (ResourceTableRef)
			{
				(*ResourceTableRef)[i] = Resource;
			}
			else
			{
				CmdListResources[i] = Resource;
			}
		}
	}

	if (RHICmdList)
	{
		RHICmdList->EnqueueLambda([this, BufferPtr, CmdListResources, NumResources, NewBlock = UniformBufferMemoryRT](FRHICommandList&)
		{
			// Defer free any old data
			FMemBlock::Free(UniformBufferMemoryRHIT);
			UniformBufferMemoryRHIT = NewBlock;

			Buffer.initAsConstantBuffer(BufferPtr, ConstantBufferSize);
			Buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

			if (CmdListResources)
			{
				ResourceTable.Empty(NumResources);
				ResourceTable.AddZeroed(NumResources);
				for (int32 Index = 0; Index < NumResources; ++Index)
		{
					ResourceTable[Index] = CmdListResources[Index];
		}
	}
		});
		RHICmdList->RHIThreadFence(true);
	}
	else
	{
		// Update the buffer immediately.
		Buffer.initAsConstantBuffer(BufferPtr, ConstantBufferSize);
		Buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

		FMemBlock::Free(UniformBufferMemoryRHIT);
		UniformBufferMemoryRHIT = UniformBufferMemoryRT;
	}
}

// Destructor 
FGnmUniformBuffer::~FGnmUniformBuffer()
{
	FMemBlock::Free(UniformBufferMemoryRHIT);
}

void FGnmUniformBuffer::Set(FGnmCommandListContext& CommandListContext, Gnm::ShaderStage Stage, uint32 BufferIndex)
{
#if ENABLE_BUFFER_INTEGRITY_CHECK
	uint32 CurrentFrame = GGnmManager.GetFrameCount();
	bool bValidAllocation = AllocationFrame == 0xFFFFFFFF || AllocationFrame == CurrentFrame;
	checkf(bValidAllocation, TEXT("SingleFrame/SingleDraw buffer is used over multiple frames"));
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

FUniformBufferRHIRef FGnmDynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage, EUniformBufferValidation Validation)
{
	SCOPE_CYCLE_COUNTER(STAT_GnmCreateUniformBufferTime);

#if DO_CHECK
	uint32 NumBytes = Layout.ConstantBufferSize;
	if (NumBytes > 0)
	{
		// Constant buffers must also be 16-byte aligned.
		check(Align(NumBytes, 16) == NumBytes);
		check(Align(Contents, 16) == Contents);
	}
#endif

	return new FGnmUniformBuffer(Contents, Layout, Usage, Validation);
}

void FGnmDynamicRHI::RHIUpdateUniformBuffer(FRHIUniformBuffer* UniformBufferRHI, const void* Contents)
{
	check(IsInRenderingThread());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FGnmUniformBuffer* UniformBuffer = ResourceCast(UniformBufferRHI);

	UniformBuffer->Update(RHICmdList.Bypass() ? nullptr : &RHICmdList, Contents, EUniformBufferValidation::ValidateResources);
}
