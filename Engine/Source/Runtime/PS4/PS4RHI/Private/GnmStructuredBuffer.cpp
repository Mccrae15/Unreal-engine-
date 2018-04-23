// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "GnmRHIPrivate.h"


FGnmStructuredBuffer::FGnmStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 Usage)
	: FRHIStructuredBuffer(Stride, Size, Usage)
	, bOwnsMemory(true)
{
	check((Size % Stride) == 0);

	EGnmMemType MemoryType = EGnmMemType::GnmMem_GPU;

	if (Usage & BUF_KeepCPUAccessible)
	{
		MemoryType = EGnmMemType::GnmMem_CPU;
	}

	// allocate memory
	BufferMemory = FMemBlock::Allocate(Size, BUFFER_VIDEO_ALIGNMENT, MemoryType, GET_STATID(STAT_Onion_StructuredBuffer));

	// byte buffer has a specific initialization.
	// if you use initAsRegularBuffer instead, writes won't be allowed to the full buffer by the shader.
	if ((Usage & BUF_ByteAddressBuffer) != 0)
	{
		Buffer.initAsByteBuffer(BufferMemory.GetPointer(), Size);
	}
	else
	{
		// initialize the buffer (with GPU address)
		Buffer.initAsRegularBuffer(BufferMemory.GetPointer(), Stride, Size / Stride);
	}

	// copy any resources to the CPU address
	if (ResourceArray)
	{
		FMemory::Memcpy(BufferMemory.GetPointer(), ResourceArray->GetResourceData(), Size);
		ResourceArray->Discard();
	}
	SetCurrentGPUAccess(EResourceTransitionAccess::ERWBarrier);
}


FGnmStructuredBuffer::FGnmStructuredBuffer(uint32 Stride, uint32 Size, void *Ptr, uint32 Usage)
	: FRHIStructuredBuffer(Stride, Size, Usage)
	, bOwnsMemory(false)
{
	check((Size % Stride) == 0);

	// byte buffer has a specific initialization.
	// if you use initAsRegularBuffer instead, writes won't be allowed to the full buffer by the shader.
	if ((Usage & BUF_ByteAddressBuffer) != 0)
	{
		Buffer.initAsByteBuffer(Ptr, Size);
		Buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
	}
	else
	{
		Gnm::DataFormat BufferFormat = Gnm::DataFormat::build(Gnm::kBufferFormat8, Gnm::kBufferChannelTypeUInt);
		check(BufferFormat.supportsBuffer());
		Buffer.initAsDataBuffer(Ptr, Gnm::kDataFormatR8Uint, Size / Stride);
		Buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
		check(Buffer.isBuffer());
	}

	SetCurrentGPUAccess(EResourceTransitionAccess::ERWBarrier);

}

FGnmStructuredBuffer::~FGnmStructuredBuffer()
{
	if (bOwnsMemory)
	{
		FMemBlock::Free(BufferMemory);
	}
}




FStructuredBufferRHIRef FGnmDynamicRHI::RHICreateStructuredBuffer(uint32 Stride,uint32 Size,uint32 InUsage,FRHIResourceCreateInfo& CreateInfo)
{
	return new FGnmStructuredBuffer(Stride, Size, CreateInfo.ResourceArray, InUsage);
}

void* FGnmDynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	FGnmStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	// just return a pointer into the buffer
	// @todo: We may actually need to copy the buffer so the CPU can read/write the buffer, since the GPU could be reading/writing to it now
	return StructuredBuffer->GetCPUAddress(Offset);
}

void FGnmDynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	// nothing to do in unlock
}
