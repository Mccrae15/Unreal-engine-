// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmIndexBuffer.cpp: Gnm Index buffer RHI implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"


/** Constructor */
FGnmIndexBuffer::FGnmIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
	: FGnmMultiBufferResource(InSize, InUsage, CreateInfo, GET_STATID(STAT_Garlic_IndexBuffer), GET_STATID(STAT_Onion_IndexBuffer))
	, FRHIIndexBuffer(InStride, CreateInfo.bWithoutNativeResource ? 0u : InSize, InUsage)
	, IndexSize(InStride == 2 ? Gnm::kIndexSize16 : Gnm::kIndexSize32)
{
}

void FGnmIndexBuffer::Swap(FGnmIndexBuffer& Other)
{
	FGnmMultiBufferResource::Swap(Other);
	FRHIIndexBuffer::Swap(Other);
	::Swap(IndexSize, Other.IndexSize);
}

FIndexBufferRHIRef FGnmDynamicRHI::RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	return new FGnmIndexBuffer(Stride, Size, InUsage, CreateInfo);
}

void* FGnmDynamicRHI::RHILockIndexBuffer(FRHICommandListImmediate& RHICmdList, FRHIIndexBuffer* IndexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	check(Size > 0);
	return ResourceCast(IndexBufferRHI)->Lock(RHICmdList, LockMode, Size, Offset);
}

void FGnmDynamicRHI::RHIUnlockIndexBuffer(FRHICommandListImmediate& RHICmdList, FRHIIndexBuffer* IndexBufferRHI)
{
	ResourceCast(IndexBufferRHI)->Unlock(RHICmdList);
}

void FGnmDynamicRHI::RHITransferIndexBufferUnderlyingResource(FRHIIndexBuffer* DestIndexBuffer, FRHIIndexBuffer* SrcIndexBuffer)
{
	check(DestIndexBuffer);
	FGnmIndexBuffer* Dest = ResourceCast(DestIndexBuffer);
	if (!SrcIndexBuffer)
	{
		FRHIResourceCreateInfo DummyCreateInfo;
		DummyCreateInfo.bWithoutNativeResource = true;

		TRefCountPtr<FGnmIndexBuffer> DeletionProxy = new FGnmIndexBuffer(Dest->GetStride(), 0, Dest->GetUsage(), DummyCreateInfo);
		Dest->Swap(*DeletionProxy);
	}
	else
	{
		FGnmIndexBuffer* Src = ResourceCast(SrcIndexBuffer);
		Dest->Swap(*Src);
	}
}
