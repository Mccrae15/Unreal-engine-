// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmVertexBuffer.cpp: Gnm vertex buffer RHI implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"


/** Constructor */
FGnmVertexBuffer::FGnmVertexBuffer(uint32 InSize, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
	: FGnmMultiBufferResource(InSize, InUsage, CreateInfo, GET_STATID(STAT_Garlic_VertexBuffer), GET_STATID(STAT_Onion_VertexBuffer)) 
	, FRHIVertexBuffer(CreateInfo.bWithoutNativeResource ? 0u : InSize, InUsage)
{
}

void FGnmVertexBuffer::Swap(FGnmVertexBuffer& Other)
{
	FGnmMultiBufferResource::Swap(Other);
	FRHIVertexBuffer::Swap(Other);
}

FVertexBufferRHIRef FGnmDynamicRHI::RHICreateVertexBuffer(uint32 Size, uint32 InUsage,FRHIResourceCreateInfo& CreateInfo)
{
	return new FGnmVertexBuffer(Size, InUsage, CreateInfo);
}

void* FGnmDynamicRHI::RHILockVertexBuffer(FRHICommandListImmediate& RHICmdList, FRHIVertexBuffer* VertexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	check(Size > 0);
	return ResourceCast(VertexBufferRHI)->Lock(RHICmdList, LockMode, Size, Offset);
}

void FGnmDynamicRHI::RHIUnlockVertexBuffer(FRHICommandListImmediate& RHICmdList, FRHIVertexBuffer* VertexBufferRHI)
{
	ResourceCast(VertexBufferRHI)->Unlock(RHICmdList);
}

void FGnmDynamicRHI::RHICopyVertexBuffer(FRHIVertexBuffer* SourceBufferRHI, FRHIVertexBuffer* DestBufferRHI)
{
	// @todo gnm: this would be relatively easy (copyData), but the tricky part is ensuring the cache flushing is good, so we need 
	// a usecase to test it on - this Fatality will make it easy to find a use case!
	UE_LOG(LogSony, Fatal, TEXT("RHICopyVertexBuffer is not supported yet in Gnm"));
}
void FGnmDynamicRHI::RHITransferVertexBufferUnderlyingResource(FRHIVertexBuffer* DestVertexBuffer, FRHIVertexBuffer* SrcVertexBuffer)
{
	check(DestVertexBuffer);
	FGnmVertexBuffer* Dest = ResourceCast(DestVertexBuffer);
	if (!SrcVertexBuffer)
	{
		FRHIResourceCreateInfo DummyCreateInfo;
		DummyCreateInfo.bWithoutNativeResource = true;

		TRefCountPtr<FGnmVertexBuffer> DeletionProxy = new FGnmVertexBuffer(0, Dest->GetUsage(), DummyCreateInfo);
		Dest->Swap(*DeletionProxy);
	}
	else
	{
		FGnmVertexBuffer* Src = ResourceCast(SrcVertexBuffer);
		Dest->Swap(*Src);
	}
}
