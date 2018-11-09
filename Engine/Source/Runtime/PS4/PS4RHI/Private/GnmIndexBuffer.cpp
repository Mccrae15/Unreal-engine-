// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmIndexBuffer.cpp: Gnm Index buffer RHI implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"


/** Constructor */
FGnmIndexBuffer::FGnmIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage)
	: FGnmMultiBufferResource(InSize, InUsage, GET_STATID(STAT_Garlic_IndexBuffer), GET_STATID(STAT_Onion_IndexBuffer))
	, FRHIIndexBuffer(InStride, InSize, InUsage)
	, IndexSize(InStride == 2 ? Gnm::kIndexSize16 : Gnm::kIndexSize32)
{
}

FIndexBufferRHIRef FGnmDynamicRHI::RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// make the RHI object, which will allocate memory
	FGnmIndexBuffer* IndexBuffer = new FGnmIndexBuffer(Stride, Size, InUsage);
	
	if(CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());

		// make a buffer usable by CPU
		void* Buffer = RHILockIndexBuffer(IndexBuffer, 0, Size, RLM_WriteOnly);

		// copy the contents of the given data into the buffer
		FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);

		RHIUnlockIndexBuffer(IndexBuffer);

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return IndexBuffer;
}

void* FGnmDynamicRHI::RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FGnmIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	check(Size > 0);

	// return memory usable by CPU
#if PS4_USE_NEW_MULTIBUFFER
	return (uint8*)IndexBuffer->Lock(FRHICommandListExecutor::GetImmediateCommandList(), LockMode, Size, Offset);
#else
	return (uint8*)IndexBuffer->Lock(LockMode, Size, Offset);
#endif
}

void FGnmDynamicRHI::RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	FGnmIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

#if PS4_USE_NEW_MULTIBUFFER
	IndexBuffer->Unlock(FRHICommandListExecutor::GetImmediateCommandList());
#else
	IndexBuffer->Unlock();
#endif
}
