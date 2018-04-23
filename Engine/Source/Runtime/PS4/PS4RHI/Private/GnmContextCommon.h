// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GnmContextCommon.h: Functions common between gfx and compute contexts
=============================================================================*/

#pragma once 

#include "GnmRHIPrivate.h"
#include "GnmContextCommon.h"
#include "ShaderParameterUtils.h"

class FGnmContextCommon
{
public:
	/** GNM defines a maximum of 20 constant buffers per shader stage. */
	static const int32 MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE = 20;	
	static const int32 MaxBoundUAVs = 16;

	template <typename TGnmContext>
	static FORCEINLINE int32 SetShaderResourcesFromBuffer_Surface(TGnmContext& CommandListContext, Gnm::ShaderStage ShaderStage, FGnmUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex);
	template <typename TGnmContext>
	static FORCEINLINE int32 SetShaderResourcesFromBuffer_SRV(TGnmContext& CommandListContext, Gnm::ShaderStage ShaderStage, FGnmUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex);
	template <typename TGnmContext>
	static FORCEINLINE int32 SetShaderResourcesFromBuffer_Sampler(TGnmContext& CommandListContext, Gnm::ShaderStage ShaderStage, FGnmUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex);

	static bool ValidateSRVForSet(FGnmShaderResourceView* SRV);
};

template <typename TGnmContext>
inline int32 FGnmContextCommon::SetShaderResourcesFromBuffer_Surface(TGnmContext& CommandListContext, Gnm::ShaderStage ShaderStage, FGnmUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	float CurrentTimeForTextureTimes = FApp::GetCurrentTime();
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			FRHITexture* TextureRHI = (FRHITexture*)Resources[ResourceIndex].GetReference();
			TextureRHI->SetLastRenderTime(CurrentTimeForTextureTimes);
			FGnmSurface* ResourcePtr = &GetGnmSurfaceFromRHITexture(TextureRHI);

			CommandListContext.SetResource(ShaderStage, BindIndex, ResourcePtr, TextureRHI->GetName());

			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}

	return NumSetCalls;
}

template <typename TGnmContext>
inline int32 FGnmContextCommon::SetShaderResourcesFromBuffer_SRV(TGnmContext& CommandListContext, Gnm::ShaderStage ShaderStage, FGnmUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			FGnmShaderResourceView* ResourcePtr = (FGnmShaderResourceView*)Resources[ResourceIndex].GetReference();
			CommandListContext.SetResource(ShaderStage, BindIndex, ResourcePtr);

			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}

	return NumSetCalls;
}

template <typename TGnmContext>
inline int32 FGnmContextCommon::SetShaderResourcesFromBuffer_Sampler(TGnmContext& CommandListContext, Gnm::ShaderStage ShaderStage, FGnmUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			Gnm::Sampler* ResourcePtr = &((FGnmSamplerState*)Resources[ResourceIndex].GetReference())->Sampler;

			CommandListContext.SetResource(ShaderStage, BindIndex, ResourcePtr);

			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}

	return NumSetCalls;
}

