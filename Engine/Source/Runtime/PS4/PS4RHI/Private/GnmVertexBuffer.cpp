// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmVertexBuffer.cpp: Gnm vertex buffer RHI implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"


/** Constructor */
FGnmVertexBuffer::FGnmVertexBuffer(uint32 InSize, uint32 InUsage)
	: FGnmMultiBufferResource(InSize, InUsage, GET_STATID(STAT_Garlic_VertexBuffer), GET_STATID(STAT_Onion_VertexBuffer)) 
	, FRHIVertexBuffer(InSize, InUsage)
{
}

FVertexBufferRHIRef FGnmDynamicRHI::RHICreateVertexBuffer(uint32 Size, uint32 InUsage,FRHIResourceCreateInfo& CreateInfo)
{
	// make the RHI object, which will allocate memory
	FGnmVertexBuffer* VertexBuffer = new FGnmVertexBuffer(Size, InUsage);

	if(CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());

		// make a buffer usable by CPU
		void* Buffer = RHILockVertexBuffer(VertexBuffer, 0, Size, RLM_WriteOnly);

		// copy the contents of the given data into the buffer
		FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);

		RHIUnlockVertexBuffer(VertexBuffer);

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return VertexBuffer;
}

void* FGnmDynamicRHI::RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FGnmVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	check(Size > 0);

	// default to vertex buffer memory
#if PS4_USE_NEW_MULTIBUFFER
	return VertexBuffer->Lock(FRHICommandListExecutor::GetImmediateCommandList(), LockMode, Size, Offset);
#else
	return VertexBuffer->Lock(LockMode, Size, Offset);
#endif
}

void FGnmDynamicRHI::RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI)
{
	FGnmVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

#if PS4_USE_NEW_MULTIBUFFER
	VertexBuffer->Unlock(FRHICommandListExecutor::GetImmediateCommandList());
#else
	VertexBuffer->Unlock();
#endif
}

void FGnmDynamicRHI::RHICopyVertexBuffer(FVertexBufferRHIParamRef SourceBufferRHI,FVertexBufferRHIParamRef DestBufferRHI)
{
	// @todo gnm: this would be relatively easy (copyData), but the tricky part is ensuring the cache flushing is good, so we need 
	// a usecase to test it on - this Fatality will make it easy to find a use case!
	UE_LOG(LogPS4, Fatal, TEXT("RHICopyVertexBuffer is not supported yet in Gnm"));
}