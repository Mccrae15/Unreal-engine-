// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmRHI.cpp: Gnm device RHI implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "ClearReplacementShaders.h"
#include "UpdateTextureShaders.h"
#include "ShaderParameterUtils.h"
#include "Modules/ModuleManager.h"
#include "RenderUtils.h"
#include "ShaderCompiler.h"
#include "ShaderCodeLibrary.h"

#if !defined(PS4_SUPPORTS_PARALLEL_RHI_EXECUTE)
#error "PS4_SUPPORTS_PARALLEL_RHI_EXECUTE must be defined."
#endif


DEFINE_LOG_CATEGORY(LogPS4);

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
uint32 PS4DoStandardWriteLocks = 0;
static TAutoConsoleVariable<int32> CVarPS4StandardWriteLocks(
	TEXT("r.PS4StandardWriteLocks"),
	0,
	TEXT("Toggle on standard cross-platform write lock behavior for debugging."));
#endif

static struct FGnmLockTracker
{
	struct FLockParams
	{
		void* RHIBuffer;
		void* Buffer;
		uint32 BufferSize;
		uint32 Offset;
		EResourceLockMode LockMode;
		bool bDirectLock; //did we call the normal flushing/updating lock?
		bool bCreateLock; //did we lock to immediately initialize a newly created buffer?

		FORCEINLINE_DEBUGGABLE FLockParams(void* InRHIBuffer, void* InBuffer, uint32 InOffset, uint32 InBufferSize, EResourceLockMode InLockMode, bool bInbDirectLock, bool bInCreateLock)
			: RHIBuffer(InRHIBuffer)
			, Buffer(InBuffer)
			, BufferSize(InBufferSize)
			, Offset(InOffset)
			, LockMode(InLockMode)
			, bDirectLock(bInbDirectLock)
			, bCreateLock(bInCreateLock)
		{
		}
	};

	struct FUnlockFenceParams
	{
		FUnlockFenceParams(void* InRHIBuffer, FGraphEventRef InUnlockEvent)
		: RHIBuffer(InRHIBuffer)
		, UnlockEvent(InUnlockEvent)
		{

		}
		void* RHIBuffer;
		FGraphEventRef UnlockEvent;
	};

	TArray<FLockParams, TInlineAllocator<16> > OutstandingLocks;
	uint32 TotalMemoryOutstanding;
	TArray<FUnlockFenceParams, TInlineAllocator<16> > OutstandingUnlocks;

	FGnmLockTracker()
	{
		TotalMemoryOutstanding = 0;
	}

	FORCEINLINE_DEBUGGABLE void Lock(void* RHIBuffer, void* Buffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode, bool bInDirectBufferWrite = false, bool bInCreateLock = false)
	{
#if DO_CHECK
		for (auto& Parms : OutstandingLocks)
		{
			check((Parms.RHIBuffer != RHIBuffer) || (Parms.bDirectLock && bInDirectBufferWrite));
		}
#endif
		OutstandingLocks.Add(FLockParams(RHIBuffer, Buffer, Offset, SizeRHI, LockMode, bInDirectBufferWrite, bInCreateLock));
		TotalMemoryOutstanding += SizeRHI;
	}
	FORCEINLINE_DEBUGGABLE FLockParams Unlock(void* RHIBuffer)
	{
		for (int32 Index = 0; Index < OutstandingLocks.Num(); Index++)
		{
			if (OutstandingLocks[Index].RHIBuffer == RHIBuffer)
			{
				FLockParams Result = OutstandingLocks[Index];
				OutstandingLocks.RemoveAtSwap(Index, 1, false);
				return Result;
			}
		}
		check(!"Mismatched RHI buffer locks.");
		return FLockParams(nullptr, nullptr, 0, 0, RLM_WriteOnly, false, false);
	}

	template<class TIndexOrVertexBufferPointer>
	FORCEINLINE_DEBUGGABLE void AddUnlockFence(TIndexOrVertexBufferPointer* Buffer, FRHICommandListImmediate& RHICmdList, const FLockParams& LockParms)
	{
		if (LockParms.LockMode != RLM_WriteOnly || !(Buffer->GetUsage() & BUF_Volatile))
		{
			OutstandingUnlocks.Emplace(Buffer, RHICmdList.RHIThreadFence(true));
		}
	}

	FORCEINLINE_DEBUGGABLE void WaitForUnlock(void* RHIBuffer)
	{
		for (int32 Index = 0; Index < OutstandingUnlocks.Num(); Index++)
		{
			if (OutstandingUnlocks[Index].RHIBuffer == RHIBuffer)
			{
				FRHICommandListExecutor::WaitOnRHIThreadFence(OutstandingUnlocks[Index].UnlockEvent);
				OutstandingUnlocks.RemoveAtSwap(Index, 1, false);
				return;
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void FlushCompleteUnlocks()
	{
		uint32 Count = OutstandingUnlocks.Num();
		for (int32 Index = 0; Index < Count; Index++)
		{
			if (OutstandingUnlocks[Index].UnlockEvent->IsComplete())
			{
				OutstandingUnlocks.RemoveAt(Index, 1);
				--Count;
				--Index;
			}
		}
	}

} GGnmLockTracker;


bool FGnmDynamicRHIModule::IsSupported()
{
	// figure out a way to detect the only allowable video card
	return true;
}

FDynamicRHI* FGnmDynamicRHIModule::CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel)
{
	return new FGnmDynamicRHI();
}

// Use "PS4" here as that's the publically accessible name
IMPLEMENT_MODULE(FGnmDynamicRHIModule, PS4RHI);

FGnmDynamicRHI::FGnmDynamicRHI()
{
	// This should be called once at the start 
	check( IsInGameThread() );
	check( !GIsThreadedRendering );

	FPS4PlatformMemory::RegisterGPUMemStatDump(FMemBlock::Dump);

	// Initialize the RHI capabilities.
	GMaxRHIShaderPlatform = SP_PS4;

	GMaxTextureDimensions = 16384;
	GRHIAdapterName = sceKernelIsNeoMode() ? TEXT("PlayStation4 Neo GPU") : TEXT("PlayStation4 GPU");
	GRHIVendorId = 0x1002;	// AMD GPU
	GSupportsRenderTargetFormat_PF_G8 = false;
	GSupportsQuads = true;
	GRHISupportsTextureStreaming = true;
	GMaxShadowDepthBufferSizeX = GMaxTextureDimensions;
	GMaxShadowDepthBufferSizeY = GMaxTextureDimensions;	
	GSupportsSeparateRenderTargetBlendState = true;
	GSupportsDepthBoundsTest = true;
	GSupportsEfficientAsyncCompute = true;
	GRHISupportsRHIThread = true;
	GRHISupportsParallelRHIExecute = PS4_SUPPORTS_PARALLEL_RHI_EXECUTE;
	GSupportsParallelOcclusionQueries = true;
	GRHISupportsFirstInstance = true;
	GRHISupportsMSAADepthSampleAccess = true;
	GSupportsTimestampRenderQueries = true;
	GSupportsRenderTargetWriteMask = true;
	GRHISupportsResolveCubemapFaces = true;
	GRHISupportsGPUTimestampBubblesRemoval = true;
	GRHISupportsDynamicResolution = true;
	GRHISupportsFrameCyclesBubblesRemoval = true;


	GMaxTextureMipCount = FPlatformMath::CeilLogTwo( GMaxTextureDimensions ) + 1;
	GMaxTextureMipCount = FPlatformMath::Min<int32>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );
	GMaxCubeTextureDimensions = 16384;
	GMaxTextureArrayLayers = 8192;

	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_PS4;

	GDrawUPVertexCheckCount = MAX_uint16;

	// Initialize the platform pixel format map.
	GPixelFormats[PF_Unknown			].PlatformFormat	= Gnm::kDataFormatInvalid.m_asInt;
	// @todo gnm: 96 and 128 bit render targets aren't supported yet, so always use 16 bits when 32 are requested
//	GPixelFormats[PF_A32B32G32R32F		].PlatformFormat	= Gnm::DataFormat::build(Gnm::kSurfaceFormat16_16_16_16, Gnm::kTextureChannelTypeFloat, Gnm::kTextureChannelZ, Gnm::kTextureChannelY, Gnm::kTextureChannelX, Gnm::kTextureChannelW).m_asInt;
	GPixelFormats[PF_A32B32G32R32F		].PlatformFormat	= Gnm::kDataFormatR32G32B32A32Float.m_asInt;
	GPixelFormats[PF_B8G8R8A8			].PlatformFormat	= Gnm::kDataFormatB8G8R8A8Unorm.m_asInt;
	GPixelFormats[PF_G8					].PlatformFormat	= Gnm::kDataFormatR8Unorm.m_asInt;
	GPixelFormats[PF_G16				].PlatformFormat	= Gnm::kDataFormatR16Unorm.m_asInt;
	GPixelFormats[PF_DXT1				].PlatformFormat	= Gnm::kDataFormatBc1Unorm.m_asInt;
	GPixelFormats[PF_DXT3				].PlatformFormat	= Gnm::kDataFormatBc2Unorm.m_asInt;
	GPixelFormats[PF_DXT5				].PlatformFormat	= Gnm::kDataFormatBc3Unorm.m_asInt;
	GPixelFormats[PF_UYVY				].PlatformFormat	= Gnm::kDataFormatInvalid.m_asInt;	// @todo gnm: Is it supported? I don't think so. Maybe kFormatBG_RG ?
	GPixelFormats[PF_FloatRGB			].PlatformFormat	= Gnm::kDataFormatR11G11B10Float.m_asInt;
	GPixelFormats[PF_FloatRGB			].BlockBytes		= 4;
	GPixelFormats[PF_FloatRGBA			].PlatformFormat	= Gnm::kDataFormatR16G16B16A16Float.m_asInt;
	GPixelFormats[PF_FloatRGBA			].BlockBytes		= 8;
	GPixelFormats[PF_DepthStencil		].PlatformFormat	= Gnm::kDataFormatR32Float.m_asInt;
	GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
	GPixelFormats[PF_X24_G8				].PlatformFormat	= Gnm::kDataFormatR8Uint.m_asInt;
	GPixelFormats[PF_X24_G8				].BlockBytes		= 1;
	GPixelFormats[PF_ShadowDepth		].PlatformFormat	= Gnm::kDataFormatR16Unorm.m_asInt;
	GPixelFormats[PF_ShadowDepth		].BlockBytes		= 2;
	GPixelFormats[PF_R32_FLOAT			].PlatformFormat	= Gnm::kDataFormatR32Float.m_asInt;
	GPixelFormats[PF_G16R16				].PlatformFormat	= Gnm::kDataFormatR16G16Unorm.m_asInt;
	GPixelFormats[PF_G16R16F			].PlatformFormat	= Gnm::kDataFormatR16G16Float.m_asInt;
	GPixelFormats[PF_G16R16F_FILTER		].PlatformFormat	= Gnm::kDataFormatR16G16Float.m_asInt;
	GPixelFormats[PF_G32R32F			].PlatformFormat	= Gnm::kDataFormatR32G32Float.m_asInt;
	GPixelFormats[PF_A2B10G10R10		].PlatformFormat    = Gnm::kDataFormatR10G10B10A2Unorm.m_asInt;
	GPixelFormats[PF_ETC1				].PlatformFormat	= Gnm::kDataFormatB10G10R10A2Unorm.m_asInt;
	GPixelFormats[PF_A16B16G16R16		].PlatformFormat    = Gnm::kDataFormatR16G16B16A16Unorm.m_asInt;
	// @todo gnm: Do we need this?
	GPixelFormats[PF_D24				].PlatformFormat	= Gnm::kDataFormatInvalid.m_asInt;
	GPixelFormats[PF_R16F				].PlatformFormat	= Gnm::kDataFormatR16Float.m_asInt;
	GPixelFormats[PF_R16F_FILTER		].PlatformFormat	= Gnm::kDataFormatR16Float.m_asInt;
	GPixelFormats[PF_BC5				].PlatformFormat	= Gnm::kDataFormatBc5Unorm.m_asInt;
	GPixelFormats[PF_V8U8				].PlatformFormat	= Gnm::kDataFormatR8G8Snorm.m_asInt;
	GPixelFormats[PF_A1					].PlatformFormat	= Gnm::kDataFormatInvalid.m_asInt; // Not supported for rendering.
	GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= Gnm::kDataFormatR11G11B10Float.m_asInt;
	GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 4;
	GPixelFormats[PF_FloatR11G11B10		].Supported			= true;
	GPixelFormats[PF_L8					].PlatformFormat 	= Gnm::kDataFormatL8Unorm.m_asInt;
	GPixelFormats[PF_A8					].PlatformFormat	= Gnm::kDataFormatA8Unorm.m_asInt;
	GPixelFormats[PF_R32_UINT			].PlatformFormat	= Gnm::kDataFormatR32Uint.m_asInt;
	GPixelFormats[PF_R32_SINT			].PlatformFormat	= Gnm::kDataFormatR32Sint.m_asInt;
	GPixelFormats[PF_R16G16B16A16_UINT	].PlatformFormat	= Gnm::kDataFormatR16G16B16A16Uint.m_asInt;
	GPixelFormats[PF_R16G16B16A16_SINT	].PlatformFormat	= Gnm::kDataFormatR16G16B16A16Sint.m_asInt;
	GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= Gnm::kDataFormatB5G6R5Unorm.m_asInt;
	GPixelFormats[PF_R8G8B8A8			].PlatformFormat	= Gnm::kDataFormatR8G8B8A8Unorm.m_asInt;
	GPixelFormats[PF_R8G8B8A8_UINT		].PlatformFormat	= Gnm::kDataFormatR8G8B8A8Uint.m_asInt;
	GPixelFormats[PF_R8G8B8A8_SNORM		].PlatformFormat	= Gnm::kDataFormatR8G8B8A8Snorm.m_asInt;
	GPixelFormats[PF_R8G8				].PlatformFormat	= Gnm::kDataFormatR8G8Unorm.m_asInt;
	GPixelFormats[PF_BC4				].PlatformFormat	= Gnm::kDataFormatBc4Unorm.m_asInt;
	GPixelFormats[PF_R16G16_UINT		].PlatformFormat	= Gnm::kDataFormatR16G16Uint.m_asInt;
	GPixelFormats[PF_R32G32B32A32_UINT	].PlatformFormat	= Gnm::kDataFormatR32G32B32A32Uint.m_asInt;
	GPixelFormats[PF_R16_UINT			].PlatformFormat	= Gnm::kDataFormatR16Uint.m_asInt;
	GPixelFormats[PF_R16_SINT			].PlatformFormat	= Gnm::kDataFormatR16Sint.m_asInt;
	GPixelFormats[PF_BC6H				].PlatformFormat	= Gnm::kDataFormatBc6Unorm.m_asInt;
	GPixelFormats[PF_BC7				].PlatformFormat	= Gnm::kDataFormatBc7Unorm.m_asInt;
	GPixelFormats[PF_R8_UINT			].PlatformFormat	= Gnm::kDataFormatR8Uint.m_asInt;
	GPixelFormats[PF_R16G16B16A16_UNORM	].PlatformFormat	= Gnm::kDataFormatR16G16B16A16Unorm.m_asInt;
	GPixelFormats[PF_R16G16B16A16_SNORM	].PlatformFormat	= Gnm::kDataFormatR16G16B16A16Snorm.m_asInt;

	// @todo gnm: When this because a static RHI, we won't need this, and we can perform initialization init RHIInit
	GDynamicRHI = this;

	// initialize Gnm
	GGnmManager.Initialize();

	for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
	{
		ResourceIt->InitRHI();
	}

	// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitDynamicRHI();
	}

	GRHISupportsHDROutput = false; // This is determined after creating the VideoOut handle (required for the query)
	GRHIHDRDisplayOutputFormat = PF_ETC1;

	GIsRHIInitialized = true;

	static const auto CVarUseLegacyTexturePool = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PS4UseLegacyTexturePool"));
	if (CVarUseLegacyTexturePool->GetValueOnAnyThread() != 0)
	{
		UE_LOG(LogRHI, Warning, TEXT("PS4 - Legacy texture pool is enabled (r.PS4UseLegacyTexturePool). This behaviour is deprecated. Please see UE 4.18 PS4 release notes."));
	}
}

void FGnmDynamicRHI::Init()
{
#if 0
	// these can't be dynamically initialized or it is a deadlock with the screwy recursive RHI methods
	{TShaderMapRef<FFillTextureCS> ComputeShader(GetGlobalShaderMap());}
	{TShaderMapRef<FClearBufferReplacementCS> ComputeShader(GetGlobalShaderMap());}
	{TShaderMapRef<FClearTexture2DReplacementCS> ComputeShader(GetGlobalShaderMap());}
	{TShaderMapRef<FClearTexture2DReplacementCS> ComputeShader(GetGlobalShaderMap());}
	{TShaderMapRef<FClearTexture2DReplacementCS> ComputeShader(GetGlobalShaderMap());}
	{TShaderMapRef<FClearBufferReplacementCS> ComputeShader(GetGlobalShaderMap());}
	{TShaderMapRef<FClearReplacementVS> ClearVertexShader(GetGlobalShaderMap());}
	{TShaderMapRef<FClearReplacementVS> ClearVertexShader(GetGlobalShaderMap());}
	{TShaderMapRef<FCopyTexture2DCS> ComputeShader( GetGlobalShaderMap() );}
#endif

	// Need to load shader library (if packaged with) before creating shaders
	FShaderCodeLibrary::InitForRuntime(GMaxRHIShaderPlatform);

	extern int32 GCreateShadersOnLoad;
	TGuardValue<int32> Guard(GCreateShadersOnLoad, 1);
	CompileGlobalShaderMap(false);
}

void FGnmDynamicRHI::Shutdown()
{
	check(IsInGameThread() && IsInRenderingThread());
	RHIShutdownFlipTracking();
}

void FGnmCommandListContext::RHIBeginFrame()
{
	check(IsImmediate());
	GGnmManager.GPUProfilingData.BeginFrame();
}

void FGnmCommandListContext::RHIEndFrame()
{
	check(IsImmediate());

	UnresolvedTargets.Empty();

	GGnmManager.GPUProfilingData.EndFrame();
}

void FGnmCommandListContext::RHIBeginScene()
{	
	check(IsImmediate());
	GGnmManager.BeginScene();
}

void FGnmCommandListContext::RHIEndScene()
{
	check(IsImmediate());
	GGnmManager.EndScene();
}

void FGnmCommandListContext::RHIPushEvent(const TCHAR* Name, FColor Color)
{
	//only valid on immediate context currently.  needs to be fixed for parallel rhi execute
	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.PushEvent(Name, Color);
	}
}

void FGnmCommandListContext::RHIPopEvent()
{	
	//only valid on immediate context currently.  needs to be fixed for parallel rhi execute
	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.PopEvent();
	}
}

void FGnmDynamicRHI::RHIGetSupportedResolution( uint32 &Width, uint32 &Height )
{
	Width = 1280;
	Height = 720;
}

bool FGnmDynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	FScreenResolutionRHI Res;
	Res.Width = 1280;
	Res.Height = 720;
	Res.RefreshRate = 60;
	Resolutions.Add(Res);

	return true;
}

void FGnmDynamicRHI::RHIFlushResources()
{	
#if USE_NEW_PS4_MEMORY_SYSTEM

	// We free pending blocks twice, either side of a GPU flush, to ensure
	// there are no outstanding blocks remaining once the GPU is idle.

	GGnmManager.FreePendingMemBlocks();
	GGnmManager.WaitForGPUIdleReset();
	GGnmManager.FreePendingMemBlocks();

	// Ensure all pending frees have been cleared.
	check(FMemBlock::HasPendingFrees() == false);

#else

	// wait for GPU to finish all outstanding commands.
	GGnmManager.WaitForGPUIdleReset();

	// Free all pending GPU memory.
	FMemBlock::ProcessDelayedFrees();
	FMemBlock::ProcessDelayedFrees();

#endif

#if USE_DEFRAG_ALLOCATOR
	FPS4GPUDefragAllocator& DefragAllocator = GGnmManager.GetDefragAllocator();
	DefragAllocator.FullDefragAndFlushGPU();	
#endif

	GGnmLockTracker.FlushCompleteUnlocks();
}

void FGnmDynamicRHI::RHIAcquireThreadOwnership()
{
	// Nothing to do
}
void FGnmDynamicRHI::RHIReleaseThreadOwnership()
{
	// Nothing to do
}

void* FGnmDynamicRHI::RHIGetNativeDevice()
{
	return NULL;
}

IRHICommandContext* FGnmDynamicRHI::RHIGetDefaultContext()
{
	return &GGnmManager.GetImmediateContext();
}

IRHIComputeContext* FGnmDynamicRHI::RHIGetDefaultAsyncComputeContext()
{
	return &GGnmManager.GetImmediateComputeContext();
}

#if PS4_USE_NEW_MULTIBUFFER == 0

struct FRHICommandUnlockBufferUpdate final : public FRHICommand<FRHICommandUnlockBufferUpdate>
{
	FGnmMultiBufferResource* Buffer;
	void* UpdateBuffer;
	uint32 Offset;
	uint32 BufferSize;

	FORCEINLINE_DEBUGGABLE FRHICommandUnlockBufferUpdate(FGnmMultiBufferResource* InBuffer, void* InUpdateBuffer, uint32 InOffset, uint32 InBufferSize)
		: Buffer(InBuffer)
		, UpdateBuffer(InUpdateBuffer)
		, Offset(InOffset)
		, BufferSize(InBufferSize)
	{
		
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		Buffer->UnlockDeferredUpdate(UpdateBuffer);		
	}
};

#endif

struct FRHICommandUnlockTexture2DUpdate final : public FRHICommand<FRHICommandUnlockTexture2DUpdate>
{
	FGnmTexture2D* Texture;
	void* UpdateData;
	uint32 MipIndex;
	bool bLockWithinMiptail;

	FORCEINLINE_DEBUGGABLE FRHICommandUnlockTexture2DUpdate(FGnmTexture2D* InTexture, void* InUpdateData, uint32 InMipIndex, bool InLockWithinMiptail)
		: Texture(InTexture)
		, UpdateData(InUpdateData)
		, MipIndex(InMipIndex)
		, bLockWithinMiptail(InLockWithinMiptail)
	{
		
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		Texture->Surface.UnlockDeferred(UpdateData, MipIndex, 0);		
	}
};

FVertexBufferRHIRef FGnmDynamicRHI::CreateAndLockVertexBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
{
	check(IsInRenderingThread());

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST && (PS4_USE_NEW_MULTIBUFFER == 0)
	if (PS4DoStandardWriteLocks)
	{
		return FDynamicRHI::CreateAndLockVertexBuffer_RenderThread(RHICmdList, Size, InUsage, CreateInfo, OutDataBuffer);		
	}
#endif
	
	FVertexBufferRHIRef VertexBuffer = RHICreateVertexBuffer(Size, InUsage, CreateInfo);
	OutDataBuffer = RHILockVertexBuffer(VertexBuffer, 0, Size, RLM_WriteOnly);
	GGnmLockTracker.Lock(VertexBuffer, OutDataBuffer, 0, Size, RLM_WriteOnly, true);
	return VertexBuffer;
}

FIndexBufferRHIRef FGnmDynamicRHI::CreateAndLockIndexBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
{
	check(IsInRenderingThread());

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST && (PS4_USE_NEW_MULTIBUFFER == 0)
	if (PS4DoStandardWriteLocks)
	{
		return FDynamicRHI::CreateAndLockIndexBuffer_RenderThread(RHICmdList, Stride, Size, InUsage, CreateInfo, OutDataBuffer);		
	}
#endif
	
	FIndexBufferRHIRef IndexBuffer = RHICreateIndexBuffer(Stride, Size, InUsage, CreateInfo);
	OutDataBuffer = RHILockIndexBuffer(IndexBuffer, 0, Size, RLM_WriteOnly);
	GGnmLockTracker.Lock(IndexBuffer, OutDataBuffer, 0, Size, RLM_WriteOnly, true);
	
	return IndexBuffer;
}

void* FGnmDynamicRHI::LockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FGNMDynamicRHI_LockVertexBuffer_RenderThread);
	check(IsInRenderingThread());

#if PS4_USE_NEW_MULTIBUFFER

	FGnmVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	return VertexBuffer->Lock(RHICmdList, LockMode, SizeRHI, Offset);

#else

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (PS4DoStandardWriteLocks)
	{
		return FDynamicRHI::LockVertexBuffer_RenderThread(RHICmdList, VertexBufferRHI, Offset, SizeRHI, LockMode);		
	}
#endif
	
	static IConsoleVariable* BufferWriteLocksVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmdBufferWriteLocks"));

	//we must wait for any previous locks to copmlete their deferred unlock.  If we don't, the lock() could return the wrong write-buffer.
	GGnmLockTracker.WaitForUnlock(VertexBufferRHI);

	bool bDirectLock = BufferWriteLocksVar->GetInt() == 0;
	void* BufferData = nullptr;
	FGnmVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	if (bDirectLock)
	{
		//unbuffered locks simply flush and update the buffer object.
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockVertexBuffer_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		BufferData = RHILockVertexBuffer(VertexBufferRHI, Offset, SizeRHI, LockMode);
	}
	else
	{
		//buffered locks return the correct buffer to write without requiring memcopies later, but modify the bufferobject's current pointer later
		//in an RHIThread unlock
		BufferData = VertexBuffer->LockDeferredUpdate(LockMode, SizeRHI, Offset);		
	}	

	GGnmLockTracker.Lock(VertexBufferRHI, BufferData, Offset, SizeRHI, LockMode, bDirectLock);
	return BufferData;

#endif
}

void FGnmDynamicRHI::UnlockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBufferRHI)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FGNMDynamicRHI_UnlockVertexBuffer_RenderThread);
	check(IsInRenderingThread());

#if PS4_USE_NEW_MULTIBUFFER

	FGnmVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	VertexBuffer->Unlock(RHICmdList);

#else

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (PS4DoStandardWriteLocks)
	{
		FDynamicRHI::UnlockVertexBuffer_RenderThread(RHICmdList, VertexBufferRHI);
		return;
	}
#endif

	FGnmLockTracker::FLockParams Params = GGnmLockTracker.Unlock(VertexBufferRHI);
	const bool bDoDirectUnlock = Params.bDirectLock;	
	const bool bUnlockForCreate = Params.bCreateLock;

	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread() || bDoDirectUnlock)
	{
		//if we locked for create, there's no way draw commands using this buffer could be in flight.  So we don't need to flush.
		if (!bUnlockForCreate)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockVertexBuffer_Flush);
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		}
		
		FGnmVertexBuffer* PlatformVertexBuffer = ResourceCast(VertexBufferRHI);

		if (bDoDirectUnlock)
		{			
			RHIUnlockVertexBuffer(VertexBufferRHI);
		}
		else
		{
			FRHICommandUnlockBufferUpdate UnlockCommand(static_cast<FGnmMultiBufferResource*>(PlatformVertexBuffer), Params.Buffer, Params.Offset, Params.BufferSize);
			UnlockCommand.Execute(RHICmdList);
		}		
				
		GGnmLockTracker.TotalMemoryOutstanding = 0;
	}
	else
	{
		FGnmVertexBuffer* PlatformVertexBuffer = ResourceCast(VertexBufferRHI);
		new (RHICmdList.AllocCommand<FRHICommandUnlockBufferUpdate>()) FRHICommandUnlockBufferUpdate(static_cast<FGnmMultiBufferResource*>(PlatformVertexBuffer), Params.Buffer, Params.Offset, Params.BufferSize);
		GGnmLockTracker.AddUnlockFence(VertexBufferRHI, RHICmdList, Params);

		if (GGnmLockTracker.TotalMemoryOutstanding > 256 * 1024)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockVertexBuffer_FlushForMem);
			// we could be loading a level or something, lets get this stuff going
			RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
			GGnmLockTracker.TotalMemoryOutstanding = 0;
		}
	}

#endif

}

void* FGnmDynamicRHI::LockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef IndexBufferRHI, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FGNMDynamicRHI_LockIndexBuffer_RenderThread);
	check(IsInRenderingThread());

#if PS4_USE_NEW_MULTIBUFFER

	FGnmIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	return IndexBuffer->Lock(RHICmdList, LockMode, SizeRHI, Offset);

#else

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (PS4DoStandardWriteLocks)
	{
		return FDynamicRHI::LockIndexBuffer_RenderThread(RHICmdList, IndexBufferRHI, Offset, SizeRHI, LockMode);
	}
#endif

	static IConsoleVariable* BufferWriteLocksVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmdBufferWriteLocks"));

	//we must wait for any previous locks to copmlete their deferred unlock.  If we don't, the lock() could return the wrong write-buffer.
	GGnmLockTracker.WaitForUnlock(IndexBufferRHI);

	bool bDirectLock = BufferWriteLocksVar->GetInt() == 0;
	void* BufferData = nullptr;
	FGnmIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	if (bDirectLock)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockIndexBuffer_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		BufferData = RHILockIndexBuffer(IndexBufferRHI, Offset, SizeRHI, LockMode);
	}
	else
	{		
		BufferData = IndexBuffer->LockDeferredUpdate(LockMode, SizeRHI, Offset);
	}
	
	GGnmLockTracker.Lock(IndexBufferRHI, BufferData, Offset, SizeRHI, LockMode, bDirectLock);
	return BufferData;

#endif
}

void FGnmDynamicRHI::UnlockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef IndexBufferRHI)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FGNMDynamicRHI_UnlockIndexBuffer_RenderThread);
	check(IsInRenderingThread());

#if PS4_USE_NEW_MULTIBUFFER

	FGnmIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	IndexBuffer->Unlock(RHICmdList);

#else

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (PS4DoStandardWriteLocks)
	{
		FDynamicRHI::UnlockIndexBuffer_RenderThread(RHICmdList, IndexBufferRHI);
		return;
	}
#endif

	FGnmLockTracker::FLockParams Params = GGnmLockTracker.Unlock(IndexBufferRHI);
	bool bDoDirectUnlock = Params.bDirectLock;
	const bool bUnlockForCreate = Params.bCreateLock;

	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread() || bDoDirectUnlock)
	{
		//if we locked for create, there's no way draw commands using this buffer could be in flight.  So we don't need to flush.
		if (!bUnlockForCreate)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockVertexBuffer_Flush);
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		}

		FGnmIndexBuffer* PlatformIndexBuffer = ResourceCast(IndexBufferRHI);

		if (bDoDirectUnlock)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockIndexBuffer_Flush);
			RHIUnlockIndexBuffer(IndexBufferRHI);
		}
		else
		{
			FRHICommandUnlockBufferUpdate UpdateCommand(static_cast<FGnmMultiBufferResource*>(PlatformIndexBuffer), Params.Buffer, Params.Offset, Params.BufferSize);
			UpdateCommand.Execute(RHICmdList);
		}		
		
		GGnmLockTracker.TotalMemoryOutstanding = 0;
	}
	else
	{
		FGnmIndexBuffer* PlatformIndexBuffer = ResourceCast(IndexBufferRHI);
		new (RHICmdList.AllocCommand<FRHICommandUnlockBufferUpdate>()) FRHICommandUnlockBufferUpdate(static_cast<FGnmMultiBufferResource*>(PlatformIndexBuffer), Params.Buffer, Params.Offset, Params.BufferSize);
		GGnmLockTracker.AddUnlockFence(IndexBufferRHI, RHICmdList, Params);
		
		if (GGnmLockTracker.TotalMemoryOutstanding > 256 * 1024)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockVertexBuffer_FlushForMem);
			// we could be loading a level or something, lets get this stuff going
			RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
			GGnmLockTracker.TotalMemoryOutstanding = 0;
		}
	}

#endif
}

struct FRHICommandUpdateTexture2D final : public FRHICommand<FRHICommandUpdateTexture2D>
{
	FUpdateTextureRegion2D UpdateRegion;
	FTexture2DRHIParamRef Texture;
	uint8* SourceData;
	uint32 MipIndex;
	uint32 SourcePitch;	

	FORCEINLINE_DEBUGGABLE FRHICommandUpdateTexture2D(FTexture2DRHIParamRef InTexture, const FUpdateTextureRegion2D& InUpdateRegion, uint32 InMipIndex, uint32 InSourcePitch, uint8* InSourceData)
		: UpdateRegion(InUpdateRegion)
		, Texture(InTexture)
		, SourceData(InSourceData)
		, MipIndex(InMipIndex)
		, SourcePitch(InSourcePitch)
		
	{

	}

	void Execute(FRHICommandListBase& CmdList)
	{
		GDynamicRHI->RHIUpdateTexture2D(Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
		FMemory::Free(SourceData);
	}
};

void FGnmDynamicRHI::UpdateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	check(IsInRenderingThread());
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		GDynamicRHI->RHIUpdateTexture2D(Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
	}
	else
	{
		const uint32 DataSize = SourcePitch * UpdateRegion.Height;
		uint8* SourceCopy = (uint8*)FMemory::Malloc(DataSize);
		FMemory::Memcpy(SourceCopy, SourceData, DataSize);
		new (RHICmdList.AllocCommand<FRHICommandUpdateTexture2D>()) FRHICommandUpdateTexture2D(Texture, UpdateRegion, MipIndex, SourcePitch, SourceCopy);
	}
}

//For the first attempt we only allow non-flushing Write-Only locks on non-rendertarget textures with a single mip.  Under the hood this method simply allocates new memory and updates the resource pointer.
//Handling multiple mips could be handled, but would require a GPU assisted copy of the mips that WEREN'T written and is generally much more complicated.
bool GnmIsDeferredTextureLockAllowed(const FGnmTexture2D* GnmTexture, EResourceLockMode LockMode)
{
	const FGnmSurface& Surface = GnmTexture->Surface;
	return (LockMode == RLM_WriteOnly) && 
		(Surface.bNeedsToTileOnUnlock || Surface.Texture->getTileMode() == Gnm::TileMode::kTileModeDisplay_LinearAligned) && 
		(Surface.Texture->getLastMipLevel() == 0) && 
		(Surface.ColorBuffer == nullptr) && 
		(Surface.DepthBuffer == nullptr);
}

void* FGnmDynamicRHI::LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FGNMDynamicRHI_LockTexture2D_RenderThread);
	check(IsInRenderingThread());
	
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (PS4DoStandardWriteLocks)
	{
		return FDynamicRHI::LockTexture2D_RenderThread(RHICmdList, TextureRHI, MipIndex, LockMode, DestStride, bLockWithinMiptail, bNeedsDefaultRHIFlush);
	}
#endif

	//don't need unlock fences since we only support RLM_WRITEONLY
	//GGnmLockTracker.WaitForUnlock(TextureRHI);

	FGnmTexture2D* GnmTexture = ResourceCast(TextureRHI);

	//if deferred locks aren't allowed then just call lock directly and safely with a flush.
	bool bDirectLock = !GnmIsDeferredTextureLockAllowed(GnmTexture, LockMode);
	void* BufferData = nullptr;
	if (bDirectLock)
	{
		if (bNeedsDefaultRHIFlush)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTexture2D_Flush);
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		}
		BufferData = RHILockTexture2D(TextureRHI, MipIndex, LockMode, DestStride, bLockWithinMiptail);
	}
	else
	{
		BufferData = GnmTexture->Surface.LockDeferred(MipIndex, 0, LockMode, DestStride);
	}	

	GGnmLockTracker.Lock(TextureRHI, BufferData, MipIndex, 0, LockMode, bDirectLock);
	return BufferData;
}

void FGnmDynamicRHI::UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FGNMDynamicRHI_UnlockTexture2D_RenderThread);
	check(IsInRenderingThread());

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (PS4DoStandardWriteLocks)
	{
		FDynamicRHI::UnlockTexture2D_RenderThread(RHICmdList, TextureRHI, MipIndex, bLockWithinMiptail, bNeedsDefaultRHIFlush);
		return;
	}
#endif

	FGnmLockTracker::FLockParams Params = GGnmLockTracker.Unlock(TextureRHI);
	bool bDoDirectUnlock = Params.bDirectLock;
	const bool bUnlockForCreate = Params.bCreateLock;

	//texture streaming causes multiple locks for multiple mips to be in flight at once.  However these go through direct locks where this is safe.
	checkf((MipIndex == Params.Offset) || bDoDirectUnlock, TEXT("Texture: %s unlock mip mismatch"), *TextureRHI->GetName().ToString());
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread() || bDoDirectUnlock)
	{
		FGnmTexture2D* PlatformTexture = ResourceCast(TextureRHI);

		if (bDoDirectUnlock)
		{			
			RHIUnlockTexture2D(TextureRHI, MipIndex, bLockWithinMiptail);
		}
		else
		{
			FRHICommandUnlockTexture2DUpdate UpdateCommand(PlatformTexture, Params.Buffer, Params.Offset, bLockWithinMiptail);
			UpdateCommand.Execute(RHICmdList);
		}

		GGnmLockTracker.TotalMemoryOutstanding = 0;
	}
	else
	{
		FGnmTexture2D* PlatformTexture = ResourceCast(TextureRHI);
		new (RHICmdList.AllocCommand<FRHICommandUnlockTexture2DUpdate>()) FRHICommandUnlockTexture2DUpdate(PlatformTexture, Params.Buffer, Params.Offset, bLockWithinMiptail);

		//don't need unlock fences since we only support RLM_WRITEONLY
		//GGnmLockTracker.AddUnlockFence(TextureRHI, RHICmdList, Params);

		if (GGnmLockTracker.TotalMemoryOutstanding > 256 * 1024)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockTexture2D_FlushForMem);
			// we could be loading a level or something, lets get this stuff going
			RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
			GGnmLockTracker.TotalMemoryOutstanding = 0;
		}
	}
}

struct FRHICommandUpdate3DTexture final : public FRHICommand<FRHICommandUpdate3DTexture>
{
	FTexture3DRHIParamRef Texture;
	uint32 MipIndex;
	FUpdateTextureRegion3D UpdateRegion;
	uint32 SourceRowPitch;
	uint32 SourceDepthPitch;
	uint8* SourceData;

	FORCEINLINE_DEBUGGABLE FRHICommandUpdate3DTexture(FTexture3DRHIParamRef InTexture, uint32 InMipIndex, const FUpdateTextureRegion3D& InUpdateRegion, uint32 InSourceRowPitch, uint32 InSourceDepthPitch, uint8* InSourceData)
		: Texture(InTexture)
		, MipIndex(InMipIndex)
		, UpdateRegion(InUpdateRegion)
		, SourceRowPitch(InSourceRowPitch)
		, SourceDepthPitch(InSourceDepthPitch)
		, SourceData(InSourceData)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		GDynamicRHI->RHIUpdateTexture3D(Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
		FMemory::Free(SourceData);
	}
};

void FGnmDynamicRHI::UpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture, uint32 MipIndex, const FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	check(IsInRenderingThread());
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		GDynamicRHI->RHIUpdateTexture3D(Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
	}
	else
	{
		SIZE_T MemorySize = SourceDepthPitch * UpdateRegion.Depth;
		uint8* Data = (uint8*)FMemory::Malloc(MemorySize);
		FMemory::Memcpy(Data, SourceData, MemorySize);

		new (RHICmdList.AllocCommand<FRHICommandUpdate3DTexture>()) FRHICommandUpdate3DTexture(Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, Data);
	}
}

struct FUpdateTexture3DGnmData
{
	FUpdateTexture3DGnmData(void* Alloc)
		: RingAlloc(Alloc)
	{
	}

	FUpdateTexture3DGnmData(FMemBlock& InBlock)
		: MemBlock(InBlock)
		, RingAlloc(nullptr)
	{
	}

	FMemBlock MemBlock;
	void* RingAlloc;
};

struct FRHICommandGnmEndUpdate3DTexture final : public FRHICommand<FRHICommandGnmEndUpdate3DTexture>
{
	FUpdateTexture3DData UpdateData;

	FORCEINLINE_DEBUGGABLE FRHICommandGnmEndUpdate3DTexture(FUpdateTexture3DData& InUpdateData)
		: UpdateData(InUpdateData)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		((FGnmDynamicRHI*)GDynamicRHI)->RHIUpdateTexture3D_Internal(UpdateData.Texture, UpdateData.MipIndex, UpdateData.UpdateRegion, UpdateData.RowPitch, UpdateData.DepthPitch, UpdateData.Data);

		FUpdateTexture3DGnmData* AllocData = (FUpdateTexture3DGnmData*)(&UpdateData.PlatformData[0]);
		if (!AllocData->RingAlloc)
		{
			FMemBlock::Free(AllocData->MemBlock);
		}
	}
};



FUpdateTexture3DData FGnmDynamicRHI::BeginUpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion)
{
	check(IsInRenderingThread());

	const int32 FormatSize = GPixelFormats[Texture->GetFormat()].BlockBytes;
	const int32 RowPitch = UpdateRegion.Width * FormatSize;
	const int32 DepthPitch = UpdateRegion.Width * UpdateRegion.Height * FormatSize;

	SIZE_T MemorySize = DepthPitch * UpdateRegion.Depth;
	void* RingAlloc = GGnmManager.AllocateFromTempRingBuffer(MemorySize, FGnmManager::ETempRingBufferAllocateMode::AllowFailure);
	void* FinalAlloc = RingAlloc;

	FUpdateTexture3DData UpdateData(Texture, MipIndex, UpdateRegion, RowPitch, DepthPitch, nullptr, MemorySize, GFrameNumberRenderThread);
	
	static_assert(sizeof(FUpdateTexture3DGnmData) < sizeof(UpdateData.PlatformData), "Platform data in FUpdateTexture3DData too small to support PS4");
	if (!RingAlloc)
	{
		// If we can't allocate from the temporary store, fall back to main heap.
		FMemBlock MemBlock = FMemBlock::Allocate(MemorySize, DEFAULT_VIDEO_ALIGNMENT, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_Texture));
		FinalAlloc = MemBlock.GetPointer();		

		new (&UpdateData.PlatformData[0]) FUpdateTexture3DGnmData(MemBlock);
	}
	else
	{
		new (&UpdateData.PlatformData[0]) FUpdateTexture3DGnmData(FinalAlloc);
	}
	UpdateData.Data = (uint8*)FinalAlloc;
	
	return UpdateData;
}

void FGnmDynamicRHI::EndUpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FUpdateTexture3DData& UpdateData)
{
	check(IsInRenderingThread());
	check(GFrameNumberRenderThread == UpdateData.FrameNumber);
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		FRHICommandGnmEndUpdate3DTexture Command(UpdateData);
		Command.Execute(RHICmdList);
	}
	else
	{
		new (RHICmdList.AllocCommand<FRHICommandGnmEndUpdate3DTexture>()) FRHICommandGnmEndUpdate3DTexture(UpdateData);
	}
}

void FGnmDynamicRHI::RHIAdvanceFrameForGetViewportBackBuffer(FViewportRHIParamRef Viewpor)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	//do a full flush so we don't trigger asserts if this changes in the middle of some lock/unlocks.
	if (PS4DoStandardWriteLocks != CVarPS4StandardWriteLocks.GetValueOnRenderThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		PS4DoStandardWriteLocks = CVarPS4StandardWriteLocks.GetValueOnRenderThread();
	}
#endif

	GGnmManager.Advance_CurrentBackBuffer_RenderThread();
	GGnmLockTracker.FlushCompleteUnlocks();
}

void FGnmDynamicRHI::EnableIdealGPUCaptureOptions(bool bEnable)
{
	FDynamicRHI::EnableIdealGPUCaptureOptions(bEnable);

	static IConsoleVariable* ContinuousSubmitsCVAR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PS4ContinuousSubmits"));
	const bool bContinuousSubmits = ContinuousSubmitsCVAR && ContinuousSubmitsCVAR->GetInt();
	const bool bShouldDoContinuousSubmits = !bEnable;

	if (bContinuousSubmits != bShouldDoContinuousSubmits && ContinuousSubmitsCVAR)
	{
		UE_LOG(LogRHI, Display, TEXT("Toggling continuous submits: %i"), bShouldDoContinuousSubmits ? 1 : 0);
		ContinuousSubmitsCVAR->Set(bShouldDoContinuousSubmits ? 1 : 0);
	}
}

#if PS4_SUPPORTS_PARALLEL_RHI_EXECUTE

class FGnmCommandContextContainer : public IRHICommandContextContainer
{	
	FGnmCommandListContext* CmdContext;
	SGnmCommandSubmission FinalCommandList;	

public:

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

	FGnmCommandContextContainer()
		: CmdContext(nullptr)		
	{		
	}

	virtual ~FGnmCommandContextContainer() override
	{
	}

	virtual IRHICommandContext* GetContext() override
	{
		check(!CmdContext && FinalCommandList.SubmissionAddrs.Num() == 0);
		FPlatformTLS::SetTlsValue(GGnmManager.GetParallelTranslateTLS(), (void*)1);		

		// these are expensive and we don't want to worry about allocating them on the fly, so they should only be allocated while actually used, and it should not be possible to have more than we preallocated, based on the number of task threads
		CmdContext = GGnmManager.AcquireDeferredContext();
		CmdContext->InitContextBuffers();
		CmdContext->ClearState();		
		return CmdContext;
	}
	virtual void FinishContext() override
	{
		check(CmdContext && FinalCommandList.SubmissionAddrs.Num() == 0);
		GGnmManager.TimeSubmitOnCmdListEnd(CmdContext);

		//store off all memory ranges for DCBs to be submitted to the GPU.
		FinalCommandList = CmdContext->GetContext().Finalize(CmdContext->GetBeginCmdListTimestamp(), CmdContext->GetEndCmdListTimestamp());
		
		GGnmManager.ReleaseDeferredContext(CmdContext);
		CmdContext = nullptr;
		check(!CmdContext && FinalCommandList.SubmissionAddrs.Num() > 0);

		FPlatformTLS::SetTlsValue(GGnmManager.GetParallelTranslateTLS(), (void*)0);		
	}
	virtual void SubmitAndFreeContextContainer(int32 Index, int32 Num) override
	{		
		if (!Index)
		{
			//printf("BeginParallelContext: %i, %i\n", Index, Num);
			GGnmManager.BeginParallelContexts();
		}
		GGnmManager.AddSubmission(FinalCommandList);
		check(!CmdContext && FinalCommandList.SubmissionAddrs.Num() != 0);
		if (Index == Num - 1)
		{
			//printf("EndParallelContexts: %i, %i\n", Index, Num);
			GGnmManager.EndParallelContexts();
		}
		FinalCommandList.Reset();
		delete this;
	}
};

void* FGnmCommandContextContainer::operator new(size_t Size)
{
	return FMemory::Malloc(Size);
}

/**
* Custom delete
*/
void FGnmCommandContextContainer::operator delete(void *RawMemory)
{
	FMemory::Free(RawMemory);
}

IRHICommandContextContainer* FGnmDynamicRHI::RHIGetCommandContextContainer(int32 Index, int32 Num)
{
	return new FGnmCommandContextContainer();
}

#else

IRHICommandContextContainer* FGnmDynamicRHI::RHIGetCommandContextContainer(int32 Index, int32 Num)
{
	return nullptr;
}

#endif
