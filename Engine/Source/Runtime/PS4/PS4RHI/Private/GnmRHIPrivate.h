// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmRHIPrivate.h: Private Gnm RHI definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Containers/ResourceArray.h"
#include "GnmRHI.h"

#ifndef USE_DEFRAG_ALLOCATOR
	#define USE_DEFRAG_ALLOCATOR 0
#endif

//neither parallel rendering, nor async compute supports append/consume buffers for now.
#if PS4_SUPPORTS_PARALLEL_RHI_EXECUTE
	#define SUPPORTS_APPEND_CONSUME_BUFFERS 0
#else
	#define SUPPORTS_APPEND_CONSUME_BUFFERS 0
#endif

#define USE_CMASK 1
#define USE_HTILE 1

#if !UE_BUILD_SHIPPING
#define ENABLE_SUBMITWAIT_TIMING 1
#else
#define ENABLE_SUBMITWAIT_TIMING 0
#endif



/** Samples align their buffers (VB, IB, etc) in video memory to 1024 */
const int DEFAULT_VIDEO_ALIGNMENT = 1024;

/** Samples align the shader video memory to 256 bytes */
const int SHADER_VIDEO_ALIGNMENT = 256;

/** Samples align the occlusion query video memory to 256 bytes */
const int QUERY_VIDEO_ALIGNMENT = 16;

/** General buffer alignment */
const int BUFFER_VIDEO_ALIGNMENT = 16;

/** Constant buffers need to be aligned */
const int CONSTANT_BUFFER_VIDEO_ALIGNMENT = 1024;

/** Max number of pending vertex streams and vertex attributes */
const int MAX_VERTEX_ATTRIBUTES = 16;

/** Max GPU allocation that will go into a pooled alloc */
const int MAX_SIZE_GPU_ALLOC_POOL = 16 * 1024;

#define NO_DRAW_CALLS	0
#include <pm4_dump.h>
//#define PRE_DRAW		//SCOPE_CYCLE_COUNTER(STAT_GnmDrawCallTime) // GGnmManager.FlushAfterComputeShader(); GGnmManager.FlushBeforeComputeShader();	
 #define PRE_DRAW		 //{ uint32 Res = GGnmManager.GetContext().validate(); if (Res != 0) { FILE* log = fopen("/hostapp/pm4dump.txt", "w"); Gnm::Pm4Dump::dumpPm4PacketStream(log, &GGnmManager.GetContext().m_dcb); fclose(log); } checkf(Res == 0, TEXT("DRAW_PRE validate failed: %x"), Res); }  
// #define PRE_DRAW		{ uint32 Offset = (GGnmManager.GetContext().m_dcb.m_cmdptr - GGnmManager.GetContext().m_dcb.m_beginptr) * 4; printf("Offset = %d\n", Offset); if (Offset == 0x302d4) UE_DEBUG_BREAK(); }
//#define PRE_DRAW
#define POST_DRAW		 //{ uint32 Res = GGnmManager.GetContext().validate(); if (Res != 0) { FILE* log = fopen("/hostapp/pm4dump.txt", "w"); Gnm::Pm4Dump::dumpPm4PacketStream(log, &GGnmManager.GetContext().m_dcb); fclose(log); } checkf(Res == 0, TEXT("DRAW_POST validate failed: %x"), Res); }
//#define POST_DRAW		 GGnmManager.BlockCPUUntilGPUIdle(false); // GGnmManager.FlushAfterComputeShader(); GGnmManager.FlushBeforeComputeShader(); 
//#define POST_DRAW


// if this is 1, extra memory is allocated, and then during a GPU crash, will be checked for any overwrites
#define USE_GPU_OVERWRITE_CHECKING	0
//#define OVERWRITE_CHECKING_MAGIC	0xDEADBEEF

#define PRE_DISPATCH	
#define POST_DISPATCH	POST_DRAW


// Dependencies
#include "GnmRHI.h"
#include "RHI.h"
#include "GPUProfiler.h"
#include "GnmUtil.h"
#include "GnmConstantBuffer.h"
#include "GnmContext.h"
#include "GnmComputeContext.h"
#include "GnmManager.h"
#include <gnm.h>

#pragma pack(push,8)
//#include <addrlib/addrlib.h>
#pragma pack(pop)

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("DrawPrimitive calls"),STAT_GnmDrawPrimitiveCalls,STATGROUP_PS4RHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Triangles drawn"),STAT_GnmTriangles,STATGROUP_PS4RHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Lines drawn"),STAT_GnmLines,STATGROUP_PS4RHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures Allocated"),STAT_GnmTexturesAllocated,STATGROUP_PS4RHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures Released"),STAT_GnmTexturesReleased,STATGROUP_PS4RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Draw call time"), STAT_GnmDrawCallTime, STATGROUP_PS4RHIVERBOSE, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Draw call prep time"), STAT_GnmDrawCallPrepareTime, STATGROUP_PS4RHIVERBOSE, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Create uniform buffer time"),STAT_GnmCreateUniformBufferTime,STATGROUP_PS4RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update uniform buffer"),STAT_GnmUpdateUniformBufferTime,STATGROUP_PS4RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GPU Flip Wait Time"), STAT_GnmGPUFlipWaitTime, STATGROUP_PS4RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GPU Submit Wait Time"), STAT_GnmGPUSubmitWaitTime, STATGROUP_PS4RHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GPU Total Time"), STAT_GnmGPUTotalTime, STATGROUP_PS4RHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Submissions"), STAT_GnmNumSubmissions, STATGROUP_PS4RHI, );

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Garlic Pool Free Bytes"), STAT_Garlic_Pool_FreeSize, STATGROUP_GPUDEFRAG, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Largest Garlic Pool Fragment"), STAT_Largest_Fragment, STATGROUP_GPUDEFRAG, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Garlic Pool Fragments"), STAT_Num_Fragments, STATGROUP_GPUDEFRAG, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num GPU Relocations"), STAT_NumRelocations, STATGROUP_GPUDEFRAG, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Size GPU Relocations"), STAT_SizeRelocations, STATGROUP_GPUDEFRAG, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Locked Chunks"), STAT_NunLockedChunks, STATGROUP_GPUDEFRAG, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Padding Waste"), STAT_PaddingWaste, STATGROUP_GPUDEFRAG, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GPU Defrag Time"), STAT_GPUDefragTime, STATGROUP_GPUDEFRAG, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("CPU Defrag Time"), STAT_CPUDefragTime, STATGROUP_GPUDEFRAG, );



// 	STAT_GnmCreateTextureTime,
// 	STAT_GnmLockTextureTime,
// 	STAT_GnmUnlockTextureTime,
// 	STAT_GnmCopyTextureTime,
// 	STAT_GnmCreateBoundShaderStateTime,
// 	STAT_GnmGlobalConstantBufferUpdateTime,
// 	STAT_GnmCleanUniformBufferTime,
// 	STAT_GnmClearShaderResourceTime,
// 	STAT_GnmNumFreeUniformBuffers,
// 	STAT_GnmFreeUniformBufferMemory,
// 	STAT_GnmUpdateUniformBufferTime,

// 	STAT_GnmRenderTargetMemory2D,
// 	STAT_GnmRenderTargetMemory3D,
// 	STAT_GnmRenderTargetMemoryCube,
// 	STAT_GnmTextureMemory2D,
// 	STAT_GnmTextureMemory3D,
// 	STAT_GnmTextureMemoryCube,
// 	STAT_GnmRWBufferMemory,
// 	STAT_GnmBufferMemory,
// 	STAT_GnmTexturesAllocated,
// 	STAT_GnmTexturesReleased,
// 	STAT_GnmTexturePoolMemory
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic Label"), STAT_Garlic_Label, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic Event"), STAT_Garlic_Event, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic FetchShader"), STAT_Garlic_FetchShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic GsShader"), STAT_Garlic_GsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic EsShader"), STAT_Garlic_EsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic VsShader"), STAT_Garlic_VsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic CsShader"), STAT_Garlic_CsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic GsVsShader"), STAT_Garlic_GsVsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic PsShader"), STAT_Garlic_PsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic HsShader"), STAT_Garlic_HsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic ShaderHelperMem"), STAT_Garlic_ShaderHelperMem, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic HTile"), STAT_Garlic_HTile, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic StencilBuffer"), STAT_Garlic_StencilBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic CMask"), STAT_Garlic_CMask, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic MultiuseUniformBuffer"), STAT_Garlic_MultiuseUniformBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic StructuredBuffer"), STAT_Garlic_StructuredBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic RenderQuery"), STAT_Garlic_RenderQuery, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic RingBuffer"), STAT_Garlic_RingBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic TempBlockAllocator"), STAT_TempBlockAllocator, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic DrawCommandBuffer"), STAT_Garlic_DrawCommandBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic ConstantCommandBuffer"), STAT_Garlic_ConstantCommandBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic IndexBuffer"), STAT_Garlic_IndexBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic VertexBuffer"), STAT_Garlic_VertexBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic RenderTarget"), STAT_Garlic_RenderTarget, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic DepthRenderTarget"), STAT_Garlic_DepthRenderTarget, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic Texture"), STAT_Garlic_Texture, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic Defrag Heap"), STAT_Garlic_DefragHeap, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic Duplicate_Untracked"), STAT_Garlic_DuplicateUntracked, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );

DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion Label"), STAT_Onion_Label, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion RazorCPU"), STAT_Onion_RazorCPU, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion Event"), STAT_Onion_Event, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion FetchShader"), STAT_Onion_FetchShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion GsShader"), STAT_Onion_GsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion EsShader"), STAT_Onion_EsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion VsShader"), STAT_Onion_VsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion CsShader"), STAT_Onion_CsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion GsVsShader"), STAT_Onion_GsVsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion PsShader"), STAT_Onion_PsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion HsShader"), STAT_Onion_HsShader, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion ShaderHelperMem"), STAT_Onion_ShaderHelperMem, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion HTile"), STAT_Onion_HTile, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion StencilBuffer"), STAT_Onion_StencilBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion CMask"), STAT_Onion_CMask, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion MultiuseUniformBuffer"), STAT_Onion_MultiuseUniformBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion StructuredBuffer"), STAT_Onion_StructuredBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion RenderQuery"), STAT_Onion_RenderQuery, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion RingBuffer"), STAT_Onion_RingBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion CUECpRam"), STAT_Onion_CUECpRam, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion DrawCommandBuffer"), STAT_Onion_DrawCommandBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion ConstantCommandBuffer"), STAT_Onion_ConstantCommandBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion IndexBuffer"), STAT_Onion_IndexBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion VertexBuffer"), STAT_Onion_VertexBuffer, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion RenderTarget"), STAT_Onion_RenderTarget, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion DepthRenderTarget"), STAT_Onion_DepthRenderTarget, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion Texture"), STAT_Onion_Texture, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion Uav Counters"), STAT_Onion_UavCounters, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion Duplicate_Untracked"), STAT_Onion_DuplicateUntracked, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );

/**
* Convert from ECubeFace to Gnm::CubemapFace type
* @param Face - ECubeFace type to convert
* @return Gnm cube face enum value
*/
FORCEINLINE Gnm::CubemapFace GetGnmCubeFace(ECubeFace Face)
{
	switch (Face)
	{
		case CubeFace_NegX:	return Gnm::kCubemapFaceNegativeX;
		case CubeFace_NegY:	return Gnm::kCubemapFaceNegativeY;
		case CubeFace_NegZ:	return Gnm::kCubemapFaceNegativeZ;
		case CubeFace_PosX:	return Gnm::kCubemapFacePositiveX;
		case CubeFace_PosY:	return Gnm::kCubemapFacePositiveY;
		case CubeFace_PosZ:	return Gnm::kCubemapFacePositiveZ;
		default:			return Gnm::kCubemapFaceNone;
	}
}
