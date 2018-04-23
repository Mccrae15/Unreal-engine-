// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmCommands.cpp: D3D RHI commands implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "GlobalShader.h"
#include "OneColorShader.h"
#include "ClearReplacementShaders.h"
#include "ShaderParameterUtils.h"
#include "SceneUtils.h"
#include "EngineGlobals.h"
#include "HAL/Runnable.h"
#include "CLearQuad.h"

#if !defined(PS4_SUPPORTS_PARALLEL_RHI_EXECUTE)
#error "PS4_SUPPORTS_PARALLEL_RHI_EXECUTE must be defined."
#endif


static int32 GPS4CommandBufferSplitSize = 32 * 1024;
static FAutoConsoleVariableRef CVarPS4CommandBufferSplitSize(
	TEXT("r.PS4CommandBufferSplitSize"),
	GPS4CommandBufferSplitSize,
	TEXT("Minimum size at which to automatically split the current command buffer into a separate submit."),
	ECVF_Cheat
	);

//disable by default since we now have explicit RHI Transitions and validation in DX11.
static int32 GPS4DoComputeAutoFlush = 0;
static FAutoConsoleVariableRef CVarPS4SkipWritableTransition(
	TEXT("r.PS4DoComputeAutoFlush"),
	GPS4DoComputeAutoFlush,
	TEXT("When enabled, does a GPU wait and cache flush before and after compute dispatch to emulate DX11"),
	ECVF_Cheat
	);

#if ENABLE_OPTIONAL_SHADER_INPUT_OUTPUT_INFO
static int32 GDumpShaderExportStats = 0;
static FAutoConsoleVariableRef CVarPS4DumpExportStats(
	TEXT("r.PS4DumpExportStats"),
	GDumpShaderExportStats,
	TEXT("Dump stats parameter cache exports.."),
	ECVF_Cheat
	);

struct FShaderPairKey
{
	FShaderPairKey(uint64 InVS, uint64 InPs)
		: VertexShaderAddr(InVS)
		, PixelShaderAddr(InPs)
	{
		uint64 XOR = VertexShaderAddr ^ PixelShaderAddr;
		Hash = (uint32)(XOR >> 32) ^ (uint32)(XOR);
	}
	bool operator==(const FShaderPairKey& Other) const
	{
		return VertexShaderAddr == Other.VertexShaderAddr && PixelShaderAddr == Other.PixelShaderAddr;
	}

	uint64 VertexShaderAddr;
	uint64 PixelShaderAddr;
	uint32 Hash;
};

static uint32 GetTypeHash(const FShaderPairKey& Key)
{
	return Key.Hash;
}

struct FShaderPair
{
	TRefCountPtr<FGnmVertexShader> VertexShader;
	TRefCountPtr<FGnmPixelShader> PixelShader;
};

static TMap<FShaderPairKey, FShaderPair> GShaderPairMap;
static FCriticalSection GShaderPairMutex;

#endif


void HACK_ClearDepth(FRHICommandList_RecursiveHazardous& RHICmdList, float Depth)
{
	FIntRect ExcludeRect;
	FGnmCommandListContext& Context = (FGnmCommandListContext&)RHICmdList.GetContext();
	TShaderMapRef<FFillTextureCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
	RHICmdList.SetComputeShader(ShaderRHI);

	FTextureRHIParamRef DepthTexture = Context.GetCurrentDepthTarget();
	FGnmSurface* DepthTarget = DepthTexture ? &GetGnmSurfaceFromRHITexture(DepthTexture) : nullptr;

	uint32 Width = DepthTarget->Texture->getWidth();
	uint32 Height = DepthTarget->Texture->getHeight();
	
	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->FillValue, *(uint32*)&Depth);
	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->Params0, FVector4(Width, Height, 0.0f, 0.0f));
	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->Params1, Context.GetViewportBounds());
	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->Params2, FVector4(ExcludeRect.Min.X, ExcludeRect.Min.Y, ExcludeRect.Max.X, ExcludeRect.Max.Y));
	RHICmdList.Flush(); // always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.

	Gnm::Texture DepthTextureCopy = *DepthTarget->Texture;
	if( DepthTextureCopy.getTextureType() == Gnm::kTextureTypeCubemap )
	{
		// Treat cubemap as 2d texture array
		Gnm::TextureSpec TextureSpec;
		TextureSpec.init();
		TextureSpec.m_textureType = Gnm::kTextureType2dArray;
		TextureSpec.m_width = DepthTextureCopy.getWidth();
		TextureSpec.m_height = DepthTextureCopy.getHeight();
		TextureSpec.m_depth = 1;
		TextureSpec.m_pitch = 0;
		TextureSpec.m_numMipLevels = DepthTextureCopy.getLastMipLevel() + 1;
		TextureSpec.m_numSlices = DepthTextureCopy.getLastArraySliceIndex() + 1;
		TextureSpec.m_format = DepthTextureCopy.getDataFormat();
		TextureSpec.m_tileModeHint = DepthTextureCopy.getTileMode();
		TextureSpec.m_minGpuMode = Gnm::getGpuMode();
		TextureSpec.m_numFragments = DepthTextureCopy.getNumFragments();		
		DepthTextureCopy.init(&TextureSpec);

		DepthTextureCopy.setBaseAddress(DepthTarget->Texture->getBaseAddress());
	}

	DepthTextureCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.

	const int32 SliceBase = DepthTextureCopy.getBaseArraySliceIndex();
	const int32 SliceLast = DepthTextureCopy.getLastArraySliceIndex();

	for( int32 SliceIndex = SliceBase; SliceIndex <= SliceLast; SliceIndex++ )
	{
		DepthTextureCopy.setArrayView( SliceIndex, SliceIndex );
		Context.GetContext().setRwTextures(Gnm::kShaderStageCs, 0, 1, &DepthTextureCopy);

		DispatchComputeShader(RHICmdList, *ComputeShader, (Width + 7) / 8, (Height + 7) / 8, 1);
		RHICmdList.Flush(); // always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.
	}
	RHICmdList.Flush(); // always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.
}

void FGnmCommandListContext::RHISetStreamSource(uint32 StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI,uint32 Offset)
{
	const uint32 Stride = StreamStrides[StreamIndex];
	FGnmVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	// cache this vertex buffer into the stream slot
	if (VertexBuffer)
	{
		const uint32 Size = VertexBuffer->GetSize() - Offset;
#if PS4_USE_NEW_MULTIBUFFER
		SetPendingStream(StreamIndex, (uint8*)VertexBuffer->GetCurrentBuffer() + Offset, Size, Stride);
#else
		SetPendingStream(StreamIndex, (uint8*)VertexBuffer->GetCurrentBuffer(false, false) + Offset, Size, Stride);
#endif
	}
	else
	{
		SetPendingStream(StreamIndex, 0, 0, Stride);
	}
}

void FGnmDynamicRHI::RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets)
{

}

void FGnmCommandListContext::RHISetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	FGnmRasterizerState* NewState = ResourceCast(NewStateRHI);

	SetPrimitiveSetup(NewState->Setup);
	SetPolygonOffset(NewState->PolyScale, NewState->PolyOffset);
}

void FGnmCommandListContext::RHIWaitComputeFence(FComputeFenceRHIParamRef InFenceRHI)
{	
	FGnmComputeFence* ComputeFence = ResourceCast(InFenceRHI);
	SCOPED_RHI_DRAW_EVENTF(*this, RHIWaitComputeFence, TEXT("WaitForFence%s"), *InFenceRHI->GetName().ToString());
	ComputeFence->WaitFence(*this);
}

void FGnmCommandListContext::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	FGnmComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);

	//start setting up compute.
	SetCurrentComputeShader(ComputeShaderRHI);

	//UAVs need to be rebound when changing compute shaders just like when changing any other BoundShaderState.
	ClearAllBoundUAVs();

	GnmContextType& Context = *GnmContext;
	Context.setCsShader(ComputeShader->Shader, &ComputeShader->ShaderOffsets);

	if( ComputeShader->Shader->m_common.m_scratchSizeInDWPerThread != 0 )
	{
		uint32 Num1KbyteScratchChunksPerWave = (ComputeShader->Shader->m_common.m_scratchSizeInDWPerThread + 3) / 4;
		Context.setComputeScratchSize( FGnmManager::ScratchBufferMaxNumWaves, Num1KbyteScratchChunksPerWave );
	}
	
	Context.setAppendConsumeCounterRange(Gnm::kShaderStageCs, 0x0, FGnmContextCommon::MaxBoundUAVs * 4);
}

void FGnmCommandListContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{ 
	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.RegisterGPUWork(1);
	}

	FComputeShaderRHIParamRef ComputeShaderRHI = GetCurrentComputeShader();	
	check(ComputeShaderRHI);

	// kick off compute!
	auto& Context = GetContext();

	// flush any needed buffers that a pixel shader may have written to
	if (GPS4DoComputeAutoFlush)
	{
		FlushBeforeComputeShader();
	}
	
	PrepareForDispatch();
	
#if !NO_DRAW_CALLS
	PRE_DISPATCH;
	Context.dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	POST_DISPATCH;
#endif

	bool bDoPostFlush = bAutoFlushAfterComputeShader && (GPS4DoComputeAutoFlush != 0);

	// flush any needed buffers that the compute shader wrote to	
	if (bDoPostFlush)
	{
		FlushAfterComputeShader();
	}

	// compute shaders that write to UAVs must have the counters flushed regardless of the auto flag.
	StoreBoundUAVs(false, !bDoPostFlush);
}

// make sure the platform agnostic structure matches what we want
static_assert(sizeof(FRHIDispatchIndirectParameters) == sizeof(Gnm::DispatchIndirectArgs), "FRHIDispatchIndirectParameters size is wrong.");
static_assert(STRUCT_OFFSET(FRHIDispatchIndirectParameters, ThreadGroupCountX) == STRUCT_OFFSET(Gnm::DispatchIndirectArgs, m_dimX), "FRHIDispatchIndirectParameters X dimension is wrong.");
static_assert(STRUCT_OFFSET(FRHIDispatchIndirectParameters, ThreadGroupCountY) == STRUCT_OFFSET(Gnm::DispatchIndirectArgs, m_dimY), "FRHIDispatchIndirectParameters Y dimension is wrong.");
static_assert(STRUCT_OFFSET(FRHIDispatchIndirectParameters, ThreadGroupCountZ) == STRUCT_OFFSET(Gnm::DispatchIndirectArgs, m_dimZ), "FRHIDispatchIndirectParameters Z dimension is wrong.");

void FGnmCommandListContext::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{ 
	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.RegisterGPUWork(1);
	}

	FComputeShaderRHIParamRef ComputeShaderRHI = GetCurrentComputeShader();
	FGnmComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	FGnmVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	auto& Context = GetContext();

	// flush any needed buffers that a pixel shader may have written to
	if (GPS4DoComputeAutoFlush)
	{
		FlushBeforeComputeShader();
	}

	// @todo debug: DMA the contents to main memory and look at the structure
	// void* Temp = AllocateFromTempRingBuffer(ArgumentBuffer->GetSize());
	// Context.copyData(Gnm::translateCpuToGpuAddress(Temp), ArgumentBuffer->GetPointeress(), ArgumentBuffer->GetSize(), true);
	// GGnmManager.BlockCPUUntilGPUIdle();

	PrepareForDispatch();

	//we don't know where the arguments are coming from so we must stall the pre-fetcher to make sure it doesn't cache bad values for the upcoming draw call.
	Context.stallCommandBufferParser();

	// dispatch a compute shader, where the xyz values are stored (UE4's FRHIDispatchIndirectParameters is the same as Gnm's DispatchIndirectArgs)	
	Context.setBaseIndirectArgs(Gnm::kShaderTypeCompute, ArgumentBuffer->GetCurrentBuffer());
#if !NO_DRAW_CALLS
	PRE_DISPATCH;
	Context.dispatchIndirect(ArgumentOffset);
	POST_DISPATCH;
#endif

	// flush any needed buffers that the compute shader wrote to	
	bool bDoPostFlush = bAutoFlushAfterComputeShader && (GPS4DoComputeAutoFlush != 0);
	if (bDoPostFlush)
	{
		FlushAfterComputeShader();
	}

	// compute shaders that write to UAVs must have the counters flushed regardless of the auto flag.
	StoreBoundUAVs(false, !bDoPostFlush);
}

void FGnmCommandListContext::RHISetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
{
	SetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
}

void FGnmCommandListContext::RHISetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 LeftMinY, uint32 RightMinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 LeftMaxY, uint32 RightMaxY, float MaxZ)
{
	SetStereoViewport(LeftMinX, RightMinX, LeftMinY, RightMinY, MinZ, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, MaxZ);
}

void FGnmCommandListContext::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data)
{ 
	check(Count > 0);
	check(Data);

	// set multiple viewports
	SetViewport(Data[0].TopLeftX, Data[0].TopLeftY, Data[0].MinDepth, Data[0].TopLeftX + Data[0].Width, Data[0].TopLeftY + Data[0].Height, Data[0].MaxDepth);

	auto& Context = GetContext();
	for (uint32 Index = 1; Index < Count; Index++)
	{
		const FViewportBounds& Bounds = Data[Index];

		const FVector Scale(Bounds.Width * 0.5f, Bounds.Height * -0.5f, 0.5f );
		const FVector Bias(Bounds.TopLeftX + Bounds.Width * 0.5f, Bounds.TopLeftY + Bounds.Height * 0.5f, 0.5f);

		Context.setViewport(Index, Bounds.MinDepth, Bounds.MaxDepth, &Scale.X, &Bias.X);
		Context.setViewportScissor(Index, Data[Index].TopLeftX, Data[Index].TopLeftY, Data[Index].TopLeftX + Data[Index].Width, Data[Index].TopLeftY + Data[Index].Height, Gnm::kWindowOffsetDisable);
	}
}

void FGnmCommandListContext::RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	SetGenericScissorRect(bEnable, MinX, MinY, MaxX, MaxY);	
}

/**
* Set bound shader state. This will set the vertex decl/shader, and pixel shader
* @param BoundShaderState - state resource
*/
void FGnmCommandListContext::RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
#if ENABLE_OPTIONAL_SHADER_INPUT_OUTPUT_INFO
	if (GDumpShaderExportStats)
	{
		FScopeLock Lock(&GShaderPairMutex);
		int32 NumImperfectPairs = 0;
		int32 MaxUnusedParams = 0;
		int32 TotalUnusedParams = 0;

		int32 TotalUsedImperfectParams = 0;
		int32 TotalUsedPerfectParams = 0;

		int32 MaxUsedImperfectParams = 0;				
		int32 MaxUsedPerfectParams = 0;

		int32 TotalUsedPackingSaved = 0;

		int32 wtf = 0;
		TMap<FName, int32> UnusedCounts;

		for (auto Iter = GShaderPairMap.CreateConstIterator(); Iter; ++Iter)
		{			
			const FShaderPair& Pair = Iter->Value;
			const sce::Gnmx::VsShader& VertexShader = *Pair.VertexShader->Shader;
			const sce::Gnmx::PsShader& PixelShader = *Pair.PixelShader->Shader;
			int32 UnusedExports = VertexShader.m_numExportSemantics - PixelShader.m_numInputSemantics;
			if (UnusedExports < 0)
			{
				wtf = 1;
			}

			if (UnusedExports > 0)
			{
				++NumImperfectPairs;
				TotalUsedImperfectParams += PixelShader.m_numInputSemantics;
				MaxUsedImperfectParams = FMath::Max(MaxUsedImperfectParams, (int32)PixelShader.m_numInputSemantics);				
			}
			else
			{
				TotalUsedPerfectParams += PixelShader.m_numInputSemantics;
				MaxUsedPerfectParams = FMath::Max(MaxUsedPerfectParams, (int32)PixelShader.m_numInputSemantics);
			}

			int32 NumComponentsUsed = 0;
			int32 NumUsedExports = 0;

			int32 FoundUnused = 0;
			for (int32 ExportIndex = 0; ExportIndex < Pair.VertexShader->DebugOutputAttributes.Num(); ++ExportIndex)
			{
				const FGnmShaderAttributeInfo& ExportAttr = Pair.VertexShader->DebugOutputAttributes[ExportIndex];
				bool bFoundInInput = false;
				for (int32 InputIndex = 0; InputIndex < Pair.PixelShader->DebugInputAttributes.Num(); ++InputIndex)
				{
					const FGnmShaderAttributeInfo& InputAttr = Pair.PixelShader->DebugInputAttributes[InputIndex];
					//if (ExportAttr.ResourceIndex == InputAttr.ResourceIndex)
					if (ExportIndex == InputIndex)
					{
						bFoundInInput = true;
						break;
					}
				}

				if (!bFoundInInput)
				{
					static FName SystemPositionName = FName("S_POSITION");
					if (ExportAttr.SemanticName != SystemPositionName)
					{
						int32& Value = UnusedCounts.FindOrAdd(ExportAttr.AttrName);
						++Value;
					}
				}
				else
				{
					//only check packing stats for USED exports.  Unused ones will be ideally removed totally.
					++NumUsedExports;
					switch (ExportAttr.DataType)
					{					
						case sce::Shader::Binary::PsslType::kTypeFloat1:
						case sce::Shader::Binary::PsslType::kTypeInt1:
						case sce::Shader::Binary::PsslType::kTypeUint1:
							 NumComponentsUsed += 1;
							 break;

						case sce::Shader::Binary::PsslType::kTypeFloat2:
						case sce::Shader::Binary::PsslType::kTypeInt2:
						case sce::Shader::Binary::PsslType::kTypeUint2:
							 NumComponentsUsed += 2;
							 break;

						case sce::Shader::Binary::PsslType::kTypeFloat3:
						case sce::Shader::Binary::PsslType::kTypeInt3:
						case sce::Shader::Binary::PsslType::kTypeUint3:
							 NumComponentsUsed += 3;
							 break;

						case sce::Shader::Binary::PsslType::kTypeFloat4:
						case sce::Shader::Binary::PsslType::kTypeInt4:
						case sce::Shader::Binary::PsslType::kTypeUint4:
							  NumComponentsUsed += 4;
							  break;

						default:
							printf("Unknown datatype: %s\n", sce::Shader::Binary::getPsslTypeString(ExportAttr.DataType));
					}
				}
			}

			int32 MinParams = (NumComponentsUsed + (NumComponentsUsed % 4)) / 4;
			int32 NumExportsToSave = NumUsedExports - MinParams;
			TotalUsedPackingSaved += NumExportsToSave;

			MaxUnusedParams = FMath::Max(MaxUnusedParams, UnusedExports);
			TotalUnusedParams += UnusedExports;
		}
		GDumpShaderExportStats = 0;

		printf("Shader Param Export Stats: \n");
		printf("Total VS/PS Pairs: %i\n", GShaderPairMap.Num());
		printf("Total Imperfect Pairs: %i\n", NumImperfectPairs);
		printf("Total unused params across all imperfect pairs: %i\n", TotalUnusedParams);
		printf("Max unused params in a single pair: %i\n", MaxUnusedParams);
		printf("Average unused params per imperfect pair: %f\n", (float)TotalUnusedParams / (float)NumImperfectPairs);

		printf("Total used params across all imperfect pairs: %i\n", TotalUsedImperfectParams);
		printf("Max used params in a single imperfect pair: %i\n", MaxUsedImperfectParams);
		printf("Average used params per imperfect pair: %f\n", (float)TotalUsedImperfectParams / (float)NumImperfectPairs);

		printf("Total used params across all perfect pairs: %i\n", TotalUsedPerfectParams);
		printf("Max used params in a single perfect pair: %i\n", MaxUsedPerfectParams);
		printf("Average used params per perfect pair: %f\n", (float)TotalUsedPerfectParams / (float)(GShaderPairMap.Num() - NumImperfectPairs));

		printf("Total USED exports potentially saved by packing: %i\n", TotalUsedPackingSaved);
		printf("Average USED exports potentially saved by packing: %f\n", (float)TotalUsedPackingSaved / (float)GShaderPairMap.Num());

		printf("Unused Semantic counts\n");
		for (auto Iter = UnusedCounts.CreateConstIterator(); Iter; ++Iter)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Semantic: %s, num pairs unused: %i\n"), *Iter.Key().ToString(), Iter.Value());
		}

		GShaderPairMap.Reset();
	}
#endif
	FGnmBoundShaderState* BoundShaderState = ResourceCast(BoundShaderStateRHI);

	auto& Context = GetContext();

	// New UAVs are potentially coming in, so we need to store off the counters for any currently in use.
	StoreBoundUAVs(true, false);	

	// remember the vertex format for next draw call
	SetPendingVertexDeclaration(BoundShaderState->VertexDeclaration);

	bool bGeometryShader = false;
	// setup which shader stages are active (if there's a geometry shader, then for now, we enabled geometry with no tessellation)
	if (BoundShaderState->GeometryShader && BoundShaderState->VertexShader->ExportShader)
	{
		FGnmGeometryShader* GS = BoundShaderState->GeometryShader;
		FGnmVertexShader* VS = BoundShaderState->VertexShader;

		bGeometryShader = true;
		if( GS->Shader->isOnChip() )
		{
			// Using OnChip LDS memory for GS
			check( VS->ExportShader->isOnChip() == true );

			uint32 LDSSizeIn512Bytes = 0;
			uint32 GSPrimsPerSubGroup = 0;
			bool bFitsInOnChipGS = Gnmx::computeOnChipGsConfiguration( &LDSSizeIn512Bytes, &GSPrimsPerSubGroup, VS->ExportShader, GS->Shader, MaxLDSUsage );
			check( bFitsInOnChipGS );

			Context.setOnChipEsShader( VS->ExportShader, LDSSizeIn512Bytes, BoundShaderState->FetchShaderModifier, BoundShaderState->FetchShader.GetPointer(), &VS->ExportShaderOffsets );
			Context.setOnChipGsVsShaders( GS->Shader, GSPrimsPerSubGroup, &GS->ShaderOffsets );
		}
		else
		{
			// Using OffChip ring buffers for GS
			check( BoundShaderState->VertexShader->ExportShader->isOnChip() == false );

			Context.setEsShader(BoundShaderState->VertexShader->ExportShader, BoundShaderState->FetchShaderModifier, BoundShaderState->FetchShader.GetPointer(), &BoundShaderState->VertexShader->ExportShaderOffsets);
			Context.setGsVsShaders(GS->Shader, &GS->ShaderOffsets);
		}

		// setup vertex shader as an export shader for a geometry shader
		SetupVertexShaderMode( VS->ExportShader, GS->Shader );
	}
	else
	{
		// optional verification
		StartVertexShaderVerification(BoundShaderState->VertexShader);

		SetupVertexShaderMode(NULL, NULL);

		if (BoundShaderState->VertexShader)
		{			
			Context.setVsShader(BoundShaderState->VertexShader->Shader, BoundShaderState->FetchShaderModifier, BoundShaderState->FetchShader.GetPointer(), &BoundShaderState->VertexShader->ShaderOffsets);
		}
	}

	if (BoundShaderState->PixelShader)
	{
		// optional verification
		StartPixelShaderVerification(BoundShaderState->PixelShader);
		RequestRenderTargetVerification();

		Context.setPsShader(BoundShaderState->PixelShader->Shader, &BoundShaderState->PixelShader->ShaderOffsets);
		SetDeferredPixelShaderUAVs();

		if( CachedCbMode != Gnm::kCbModeNormal )
		{
			Context.setCbControl( Gnm::kCbModeNormal, Gnm::kRasterOpCopy );
			CachedCbMode = Gnm::kCbModeNormal;
		}
	}
	else
	{
		// is there ever a case where we would need a fake null pixel shader? if so, see p4 history of this block
		Context.setPsShader(nullptr, nullptr);

		if( CachedCbMode != Gnm::kCbModeDisable )
		{
			Context.setCbControl(Gnm::kCbModeDisable, Gnm::kRasterOpCopy);
			CachedCbMode = Gnm::kCbModeDisable;
		}
	}

	if (BoundShaderState->Num1KbyteScratchChunksPerWave > 0)
	{
		Context.setGraphicsScratchSize(FGnmManager::ScratchBufferMaxNumWaves, BoundShaderState->Num1KbyteScratchChunksPerWave);
	}

	FMemory::Memcpy(StreamStrides, BoundShaderState->StreamStrides, sizeof(StreamStrides));

#if ENABLE_OPTIONAL_SHADER_INPUT_OUTPUT_INFO
	if (!bGeometryShader && BoundShaderState->PixelShader)
	{
		FScopeLock Lock(&GShaderPairMutex);
		FShaderPairKey Key((uint64)BoundShaderState->VertexShader->Shader->getBaseAddress(), (uint64)BoundShaderState->PixelShader->Shader->getBaseAddress());
		if (!GShaderPairMap.Contains(Key))
		{
			FShaderPair Value;
			Value.VertexShader = BoundShaderState->VertexShader;
			Value.PixelShader = BoundShaderState->PixelShader;
			GShaderPairMap.Add(Key, Value);
		}		
	}
#endif

// 	if (BoundShaderState->HullShader != NULL && BoundShaderState->DomainShader != NULL)
// 	{
// 		bUsingTessellation = true;
// 	}
// 	else
// 	{
// 		bUsingTessellation = false;
// 	}

	// Prevent transient bound shader states from being recreated for each use by keeping a history of the most recently used bound shader states.
	// The history keeps them alive, and the bound shader state cache allows them to be reused if needed.
	RememberBoundShaderState(BoundShaderState);

	CurrentBoundShaderState = BoundShaderState;

	//streams must be reset for LCUE when shaders change.
	bPendingStreamsAreDirty = true;

	// Shader changed so all resource tables are dirty (20 bits)
//@todo-rco: Revisit when using SRO
	const uint32 DirtyValue = (1 << FGnmContextCommon::MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE) - 1;
	DirtyUniformBuffers[SF_Vertex] = DirtyValue;
	DirtyUniformBuffers[SF_Pixel] = DirtyValue;
	DirtyUniformBuffers[SF_Hull] = DirtyValue;
	DirtyUniformBuffers[SF_Domain] = DirtyValue;
	DirtyUniformBuffers[SF_Geometry] = DirtyValue;
}

/** 
 * Set the shader resource view of a surface. This is used for binding UAV to the compute shader.
 */
void FGnmCommandListContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI)
{
	FGnmUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	check(UAVIndex < FGnmContextCommon::MaxBoundUAVs);

	if (UAV)
	{
		//check it's safe for r/w for this UAV
		const EResourceTransitionAccess CurrentUAVAccess = UAV->GetResourceAccess();
		const bool UAVDirty = UAV->IsResourceDirty();
		ensureMsgf(!UAVDirty || (CurrentUAVAccess == EResourceTransitionAccess::ERWNoBarrier), TEXT("UAV: %i is in unsafe state for GPU R/W: %s, Dirty: %i"), UAVIndex, *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)CurrentUAVAccess], (int32)UAVDirty);

		//UAVs always dirty themselves. If a shader wanted to just read, it should use an SRV.
		UAV->SetResourceDirty(true, GGnmManager.GetFrameCount());
	}

	BindUAV(UAV, UAVIndex, Gnm::kShaderStageCs);
}

void FGnmCommandListContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount)
{
	FGnmUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	check(UAVIndex < FGnmContextCommon::MaxBoundUAVs);

	if (UAV)
	{
		//check it's safe for r/w for this UAV
		const EResourceTransitionAccess CurrentUAVAccess = UAV->GetResourceAccess();
		const bool UAVDirty = UAV->IsResourceDirty();
		ensureMsgf(!UAVDirty || (CurrentUAVAccess == EResourceTransitionAccess::ERWNoBarrier), TEXT("UAV: %i is in unsafe state for GPU R/W: %s, Dirty: %i"), UAVIndex, *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)CurrentUAVAccess], (int32)UAVDirty);

		//UAVs always dirty themselves. If a shader wanted to just read, it should use an SRV.
		UAV->SetResourceDirty(true, GGnmManager.GetFrameCount());
	}
	
	// Store here and trigger a flush because counters might be getting cleared in sequence on the same UAV Index without a draw call or a dispatch to trigger the 
	// other automatic stores.  This problem will go away when we move to static GDS allocation for UAV groupings.
	StoreBoundUAVs(true, false);
	BindUAV(UAV, UAVIndex, Gnm::kShaderStageCs, InitialCount);
}

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
void FGnmCommandListContext::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	FGnmVertexShader* VertexShader = ResourceCast(VertexShaderRHI);
	SetTextureForStage(NewTextureRHI, TextureIndex, VertexShader->ShaderStage);
}

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
void FGnmCommandListContext::RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	SetTextureForStage(NewTextureRHI, TextureIndex, Gnm::kShaderStageHs);
}

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
void FGnmCommandListContext::RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	// @todo gnm - This is probably the wrong stage!
	SetTextureForStage(NewTextureRHI, TextureIndex, Gnm::kShaderStageEs);
}

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
void FGnmCommandListContext::RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	SetTextureForStage(NewTextureRHI, TextureIndex, Gnm::kShaderStageGs);
}

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
void FGnmCommandListContext::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	SetTextureForStage(NewTextureRHI, TextureIndex, Gnm::kShaderStagePs);
}

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
void FGnmCommandListContext::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	SetTextureForStage(NewTextureRHI, TextureIndex, Gnm::kShaderStageCs);
}


void FGnmCommandListContext::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	FGnmVertexShader* VertexShader = ResourceCast(VertexShaderRHI);
	SetSRVForStage(SRVRHI, TextureIndex, VertexShader->ShaderStage);
}

void FGnmCommandListContext::RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	SetSRVForStage(SRVRHI, TextureIndex, Gnm::kShaderStageHs);
}

void FGnmCommandListContext::RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	// @todo gnm this is probably the wrong stage
	SetSRVForStage(SRVRHI, TextureIndex, Gnm::kShaderStageEs);
}

void FGnmCommandListContext::RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	SetSRVForStage(SRVRHI, TextureIndex, Gnm::kShaderStageGs);
}

void FGnmCommandListContext::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	SetSRVForStage(SRVRHI, TextureIndex, Gnm::kShaderStagePs);
}

void FGnmCommandListContext::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	SetSRVForStage(SRVRHI, TextureIndex, Gnm::kShaderStageCs);
}


void FGnmCommandListContext::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FGnmSamplerState* NewState = ResourceCast(NewStateRHI);
	FGnmVertexShader* VertexShader = ResourceCast(VertexShaderRHI);

	// set the texture into the texture register
	GetContext().setSamplers(VertexShader->ShaderStage, SamplerIndex, 1, &NewState->Sampler);
}

void FGnmCommandListContext::RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FGnmSamplerState* NewState = ResourceCast(NewStateRHI);

	// set the texture into the texture register
	GetContext().setSamplers(Gnm::kShaderStageHs, SamplerIndex, 1, &NewState->Sampler);
}

void FGnmCommandListContext::RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FGnmSamplerState* NewState = ResourceCast(NewStateRHI);

	// set the texture into the texture register
	UE_LOG(LogPS4, Fatal, TEXT("Need to figure this one out, is it vertex?"));
//	GGnmManager.GetContext().setSamplers(Gnm::kShaderStageVs, SamplerIndex, 1, &NewState->Sampler);
}

void FGnmCommandListContext::RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FGnmSamplerState* NewState = ResourceCast(NewStateRHI);

	// set the texture into the texture register
	GetContext().setSamplers(Gnm::kShaderStageGs, SamplerIndex, 1, &NewState->Sampler);
}

void FGnmCommandListContext::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FGnmSamplerState* NewState = ResourceCast(NewStateRHI);

	// set the texture into the texture register
	GetContext().setSamplers(Gnm::kShaderStagePs, SamplerIndex, 1, &NewState->Sampler);
}

void FGnmCommandListContext::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FGnmSamplerState* NewState = ResourceCast(NewStateRHI);

	// set the texture into the texture register
	GetContext().setSamplers(Gnm::kShaderStageCs, SamplerIndex, 1, &NewState->Sampler);
}


void FGnmCommandListContext::RHISetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	UpdateVSConstant(BufferIndex, NewValue, BaseIndex, NumBytes);
}

void FGnmCommandListContext::RHISetShaderParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	UpdateHSConstant(BufferIndex, NewValue, BaseIndex, NumBytes);
}

void FGnmCommandListContext::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	UpdatePSConstant(BufferIndex, NewValue, BaseIndex, NumBytes);
}

void FGnmCommandListContext::RHISetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	UpdateDSConstant(BufferIndex, NewValue, BaseIndex, NumBytes);
}

void FGnmCommandListContext::RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	UpdateGSConstant(BufferIndex, NewValue, BaseIndex, NumBytes);
}

void FGnmCommandListContext::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	UpdateCSConstant(BufferIndex, NewValue, BaseIndex, NumBytes);
}

void FGnmCommandListContext::RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FGnmUniformBuffer* Buffer = ResourceCast(BufferRHI);
	FGnmVertexShader* VertexShader = ResourceCast(VertexShaderRHI);
	if (Buffer && Buffer->IsConstantBuffer())
	{
		Buffer->Set(*this, VertexShader->ShaderStage, BufferIndex);
	}
	else
	{
		GGnmManager.DummyConstantBuffer->Set(*this, VertexShader->ShaderStage, BufferIndex);
	}

	BoundUniformBuffers[SF_Vertex][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Vertex] |= (1 << BufferIndex);
}

void FGnmCommandListContext::RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FGnmUniformBuffer* Buffer = ResourceCast(BufferRHI);
	if (Buffer && Buffer->IsConstantBuffer())
	{
		Buffer->Set(*this, Gnm::kShaderStageHs, BufferIndex);
	}
	else
	{
		GGnmManager.DummyConstantBuffer->Set(*this, Gnm::kShaderStageHs, BufferIndex);
	}

	BoundUniformBuffers[SF_Hull][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Hull] |= (1 << BufferIndex);
}

void FGnmCommandListContext::RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FGnmUniformBuffer* Buffer = ResourceCast(BufferRHI);
	if (Buffer && Buffer->IsConstantBuffer())
	{
		UE_LOG(LogPS4, Fatal, TEXT("Need to figure this out"));
		//Buffer->Set(Gnm::kShaderStageEs, BufferIndex);
	}

	BoundUniformBuffers[SF_Domain][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Domain] |= (1 << BufferIndex);
}

void FGnmCommandListContext::RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FGnmUniformBuffer* Buffer = ResourceCast(BufferRHI);
	if (Buffer && Buffer->IsConstantBuffer())
	{
		Buffer->Set(*this, Gnm::kShaderStageGs, BufferIndex);
	}
	else
	{
		GGnmManager.DummyConstantBuffer->Set(*this, Gnm::kShaderStageGs, BufferIndex);
	}

	BoundUniformBuffers[SF_Geometry][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Geometry] |= (1 << BufferIndex);
}

void FGnmCommandListContext::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FGnmUniformBuffer* Buffer = ResourceCast(BufferRHI);
	if (Buffer && Buffer->IsConstantBuffer())
	{
		Buffer->Set(*this, Gnm::kShaderStagePs, BufferIndex);
	}
	else
	{
		GGnmManager.DummyConstantBuffer->Set(*this, Gnm::kShaderStagePs, BufferIndex);
	}

	BoundUniformBuffers[SF_Pixel][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Pixel] |= (1 << BufferIndex);
}

void FGnmCommandListContext::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FGnmUniformBuffer* Buffer = ResourceCast(BufferRHI);
	if (Buffer && Buffer->IsConstantBuffer())
	{
		Buffer->Set(*this, Gnm::kShaderStageCs, BufferIndex);
	}
	else
	{
		GGnmManager.DummyConstantBuffer->Set(*this, Gnm::kShaderStageCs, BufferIndex);
	}

	BoundUniformBuffers[SF_Compute][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Compute] |= (1 << BufferIndex);
}

void FGnmCommandListContext::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef)
{
	FGnmDepthStencilState* NewState = ResourceCast(NewStateRHI);
	SetDepthStencilState(NewState,  (uint8)StencilRef);
}

void FGnmCommandListContext::RHISetStencilRef(uint32 StencilRef)
{
	SetStencilRef((uint8)StencilRef);
}

void FGnmCommandListContext::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI, const FLinearColor& BlendFactor)
{
	FGnmBlendState* NewState = ResourceCast(NewStateRHI);

	// set constant blend factor
	SetBlendColor(BlendFactor);

	// Set blend control and render target mask
	SetBlendState(NewState);
}

void FGnmCommandListContext::RHISetBlendFactor(const FLinearColor& BlendFactor)
{
	// set constant blend factor
	SetBlendColor(BlendFactor);
}

void FGnmCommandListContext::SetRenderTargets_Impl(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs, bool bHWColorRequired, bool bHWDepthRequired)
{
	RequestRenderTargetVerification();

	//Rendertarget changes tend to mark large rendering transitions.  It's a reasonable time to split off a chunk of rendering commands for an early submit if
	//we have enough commands queued.
	SubmitCurrentCommands(GPS4CommandBufferSplitSize);	

	SetNumRenderTargets(NumSimultaneousRenderTargets);

	// set all render targets to valid or NULL
	CachedActiveRenderTargetMask = 0;
	for (uint32 RTIndex = 0; RTIndex < MaxSimultaneousRenderTargets; RTIndex++)
	{
		if (RTIndex < NumSimultaneousRenderTargets && NewRenderTargets[RTIndex].Texture != NULL)
		{

			// ArraySliceIndex support not implemented yet
			SetCurrentRenderTarget(RTIndex, NewRenderTargets[RTIndex].Texture, bHWColorRequired, NewRenderTargets[RTIndex].MipIndex, NewRenderTargets[RTIndex].ArraySliceIndex);

			// mark how to flush this surface when we CopyToResolveTarget
			// TODO: mw this isn't threadsafe.
			FGnmSurface& RenderTarget = GetGnmSurfaceFromRHITexture(NewRenderTargets[RTIndex].Texture);
			static Gnm::WaitTargetSlot CachesByIndex[] = { Gnm::kWaitTargetSlotCb0, Gnm::kWaitTargetSlotCb1, Gnm::kWaitTargetSlotCb2, Gnm::kWaitTargetSlotCb3, Gnm::kWaitTargetSlotCb4, Gnm::kWaitTargetSlotCb5, Gnm::kWaitTargetSlotCb6, Gnm::kWaitTargetSlotCb7 };
			RenderTarget.CacheToFlushOnResolve = CachesByIndex[RTIndex];
			CachedActiveRenderTargetMask |= (0xF) << (RTIndex * 4);
		}
		else
		{
			SetCurrentRenderTarget(RTIndex, nullptr, false);
		}
	}

	SetRenderTargetMask( CachedRenderTargetColorWriteMask & CachedActiveRenderTargetMask );

	FGnmSurface* DepthStencilSurface = nullptr;

	// set depth target if requested
	if (NewDepthStencilTarget && NewDepthStencilTarget->Texture)
	{
		DepthStencilSurface = &GetGnmSurfaceFromRHITexture(NewDepthStencilTarget->Texture);
		SetCurrentDepthTarget(NewDepthStencilTarget->Texture, bHWDepthRequired, !NewDepthStencilTarget->GetDepthStencilAccess().IsDepthWrite());

		//todo: mw this won't work in parallel on color buffers.
		// mark how to flush this guy
		DepthStencilSurface->CacheToFlushOnResolve |= Gnm::kWaitTargetSlotDb;
	}
	else
	{
		SetCurrentDepthTarget(nullptr, false, false);
	}

	// Set the viewport to the full size of render target 0 if valid, or the full size of the depth target
	uint32 ScissorWidth = 0;
	uint32 ScissorHeight = 0;

	if (NewRenderTargets && NewRenderTargets[0].Texture)
	{
		ScissorWidth = CurrentRenderTargets[0].getWidth();
		ScissorHeight = CurrentRenderTargets[0].getHeight();
	}
	else if (DepthStencilSurface)
	{
		ScissorWidth = CurrentDepthRenderTarget.getWidth();
		ScissorHeight = CurrentDepthRenderTarget.getHeight();
	}

	if( ScissorWidth != 0 )
	{
		RHISetViewport( 0, 0, 0.0f, ScissorWidth, ScissorHeight, 1.0f );
		GnmContext->setScreenScissor( 0, 0, ScissorWidth, ScissorHeight );
		GnmContext->setWindowScissor( 0, 0, ScissorWidth, ScissorHeight, Gnm::kWindowOffsetDisable );
		SetGenericScissorRect( true, 0, 0, ScissorWidth, ScissorHeight );
	}


	// new render target, new set of UAVs.  Old ones not required to be set for pixel shaders anymore.
	ClearPixelShaderUAVs();

	uint32 CurrentFrame = GGnmManager.GetFrameCount();

	// now set any UAVs for a pixel shader
	for (uint32 UAVIndex = 0; UAVIndex < NumUAVs; UAVIndex++)
	{
		FGnmUnorderedAccessView* UAV = (FGnmUnorderedAccessView*)UAVs[UAVIndex];

		if (UAV)
		{
			//check it's safe for r/w for this UAV
			const EResourceTransitionAccess CurrentUAVAccess = UAV->GetResourceAccess();
			const bool UAVDirty = UAV->IsResourceDirty();
			ensureMsgf((CurrentUAVAccess == EResourceTransitionAccess::ERWBarrier && !UAVDirty) || (CurrentUAVAccess == EResourceTransitionAccess::ERWNoBarrier && !UAVDirty), TEXT("UAV: %i is in unsafe state for GPU R/W: %s"), UAVIndex, *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)CurrentUAVAccess]);

			//UAVs get set to dirty.  If the shader just wanted to read it should have used an SRV.
			UAV->SetResourceDirty(true, CurrentFrame);

			TrackResource(sce::Shader::Binary::kInternalBufferTypeUav, UAVIndex, Gnm::kShaderStagePs);

			// the index into the UAVs array is guaranteed to be the resource index from the shader
			// and this function is only used for pixel shaders

			// track UAV for setting when shaders change.
			AddDeferredPixelShaderUAV(UAVIndex, UAV);

			// track the buffer for fnnlushing later
			AddPixelShaderUAVAddress(UAV->Buffer.getBaseAddress(), UAV->Buffer.getSize());
		}
	}
}

void FGnmCommandListContext::RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets,
	const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs)
{
	SetRenderTargets_Impl(NumSimultaneousRenderTargets, NewRenderTargets, NewDepthStencilTarget, NumUAVs, UAVs, false, false);
}

void FGnmDynamicRHI::RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
}

void FGnmCommandListContext::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	FLinearColor ClearColors[MaxSimultaneousRenderTargets];
	float DepthClear = 0.0;
	uint32 StencilClear = 0;

	if (RenderTargetsInfo.bClearColor)
	{
		for (int32 i = 0; i < RenderTargetsInfo.NumColorRenderTargets; ++i)
		{
			const FClearValueBinding& ClearValue = RenderTargetsInfo.ColorRenderTarget[i].Texture->GetClearBinding();
			checkf(ClearValue.ColorBinding == EClearBinding::EColorBound, TEXT("Texture: %s does not have a color bound for fast clears"), *RenderTargetsInfo.ColorRenderTarget[i].Texture->GetName().GetPlainNameString());
			ClearColors[i] = ClearValue.GetClearColor();
		}
	}
	if (RenderTargetsInfo.bClearDepth || RenderTargetsInfo.bClearStencil)
	{
		const FClearValueBinding& ClearValue = RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetClearBinding();
		checkf(ClearValue.ColorBinding == EClearBinding::EDepthStencilBound, TEXT("Texture: %s does not have a DS value bound for fast clears"), *RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetName().GetPlainNameString());
		ClearValue.GetDepthStencil(DepthClear, StencilClear);
	}

	SetRenderTargets_Impl(RenderTargetsInfo.NumColorRenderTargets,
		RenderTargetsInfo.ColorRenderTarget,
		&RenderTargetsInfo.DepthStencilRenderTarget,
		0,
		nullptr,
		RenderTargetsInfo.bClearColor,
		RenderTargetsInfo.bClearDepth);

	const bool bDoClear =
		RenderTargetsInfo.bClearColor ||
		RenderTargetsInfo.bClearStencil ||
		RenderTargetsInfo.bClearDepth;

	if (bDoClear)
	{
		TryClearMRTByHardware(RenderTargetsInfo.bClearColor, RenderTargetsInfo.NumColorRenderTargets, ClearColors, RenderTargetsInfo.bClearDepth, DepthClear, RenderTargetsInfo.bClearStencil, StencilClear);
	}
}


// Occlusion/Timer queries.
void FGnmCommandListContext::RHIBeginRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	FGnmRenderQuery* Query = ResourceCast(QueryRHI);
	// reset the query
	Query->bResultIsCached = false;
	Query->Result = 0;
	Query->Begin(*this);
}

void FGnmCommandListContext::RHIEndRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	FGnmRenderQuery* Query = ResourceCast(QueryRHI);

	Query->End(*this);
}

void FGnmCommandListContext::RHIBeginOcclusionQueryBatch()
{
}

void FGnmCommandListContext::RHIEndOcclusionQueryBatch()
{
}

void FGnmCommandListContext::RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures)
{	
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;	

	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResources, bShowTransitionEvents, TEXT("TransitionTo: %s: %i Textures"), *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)TransitionType], NumTextures);
	if (TransitionType == EResourceTransitionAccess::EReadable || TransitionType == EResourceTransitionAccess::ERWBarrier || TransitionType == EResourceTransitionAccess::ERWSubResBarrier)
	{
		Gnm::CacheAction AllCacheAction = Gnm::kCacheActionWriteBackAndInvalidateL1andL2;
		uint32 AllExtendedCacheAction = 0;
		for (int32 i = 0; i < NumTextures; ++i)
		{
			FTextureRHIParamRef RenderTarget = InTextures[i];

			if (RenderTarget)
			{
				SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResourcesLoop, bShowTransitionEvents, TEXT("To:%i - %s"), i, *RenderTarget->GetName().ToString());

				FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(RenderTarget);
				Surface.SetCurrentGPUAccess(TransitionType);

				void* BaseAddress = Surface.Texture->getBaseAddress();

				bool bIsColorBuffer = Surface.ColorBuffer != NULL;

				//only invalidate caches on the final wait
				bool bFlushCaches = i == (NumTextures - 1);

				// tell Gnm to flush caches so the GPU can read from what it just wrote to
				AllExtendedCacheAction |= bIsColorBuffer ? Gnm::kExtendedCacheActionFlushAndInvalidateCbCache : Gnm::kExtendedCacheActionFlushAndInvalidateDbCache;
				uint32 ExtendedCacheAction = bFlushCaches ? AllExtendedCacheAction : 0;
				Gnm::CacheAction CacheAction = bFlushCaches ? AllCacheAction : Gnm::kCacheActionNone;

				const uint32 CbTargetMask = Gnm::kWaitTargetSlotCb0 |
					Gnm::kWaitTargetSlotCb1 |
					Gnm::kWaitTargetSlotCb2 |
					Gnm::kWaitTargetSlotCb3 |
					Gnm::kWaitTargetSlotCb4 |
					Gnm::kWaitTargetSlotCb5 |
					Gnm::kWaitTargetSlotCb6 |
					Gnm::kWaitTargetSlotCb7;

				const uint32 DbTargetMask = Gnm::kWaitTargetSlotDb;

				//even if there's no fast clears, we still need to wait for graphics writes to ensure subsequent gpu reads from this surface are cache coherent.
				//only wait on the memory range of the current texture.
				GetContext().waitForGraphicsWrites((PTRINT)(BaseAddress) >> 8, (Surface.GetMemorySize(false) >> 8),
					bIsColorBuffer ? CbTargetMask : DbTargetMask,
					CacheAction,
					ExtendedCacheAction,
					Gnm::kStallCommandBufferParserDisable);
			}
		}

		for (int32 i = 0; i < NumTextures; ++i)
		{
			FTextureRHIParamRef RenderTarget = InTextures[i];

			if (RenderTarget)
			{
				FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(RenderTarget);
				void* BaseAddress = Surface.Texture->getBaseAddress();

				bool bIsColorBuffer = Surface.ColorBuffer != NULL;

				if (bIsColorBuffer)
				{
#if USE_CMASK
					if (Surface.ColorBuffer && Surface.ColorBuffer->getCmaskAddress() != nullptr)
					{
						if (Surface.ColorBuffer->getFmaskCompressionEnable())
						{
							DecompressFmaskSurface(RenderTarget);
						}
						else
						{
#if 0
							if (!IsRunningRHIInSeparateThread() && Surface.GetFastCleared() != GGnmManager.GetFrameCount())
							{
								printf("target resolved but never cleared.\n");
							}
#endif
							if (TargetsNeedingEliminateFastClear.Contains(RenderTarget))
							{
								if (!IsRunningRHIInSeparateThread() && !Surface.bNeedsFastClearResolve)
								{
									UE_LOG(LogRHI, Warning, TEXT("Buffer doing unnecessary fast clear."));
								}
								EliminateFastClear(RenderTarget);
							}
						}
					}
#endif
				}
				else
				{
#if USE_HTILE
					if (Surface.DepthBuffer && Surface.DepthBuffer->getHtileAddress() != nullptr)
					{
						if (TargetsNeedingEliminateFastClear.Contains(RenderTarget))
						{
							if (!IsRunningRHIInSeparateThread() && !Surface.bNeedsFastClearResolve)
							{
								//UE_LOG(LogRHI, Warning, TEXT("Buffer doing unnecessary fast clear on depth."));
							}

							// we need to decompress the HTile buffer to the full depth so we can read it				
							DecompressDepthTarget(RenderTarget);
						}
					}
#endif
				}

				if (Surface.ColorBuffer == nullptr && Surface.DepthBuffer == nullptr )
				{
					// This warning is disabled while we're allowing the RHI to non-RT cubemap faces
					// We may want to revisit if we add an explicit method to copy these resources
					if (!GRHISupportsResolveCubemapFaces)
					{
						UE_LOG(LogPS4, Warning, TEXT("Attempting to resolve non-rendertarget texture"));
					}
				}
				if (!IsRunningRHIInSeparateThread())
				{
					Surface.bNeedsFastClearResolve = false;
				}

				// note that we have resolved the render target, and are clear to use it as a texture
				SetTargetAsResolved(BaseAddress);
			}
		}
	}

	if ((TransitionType == EResourceTransitionAccess::EWritable || TransitionType == EResourceTransitionAccess::ERWBarrier || TransitionType == EResourceTransitionAccess::ERWSubResBarrier))
	{		
		for (int32 i = 0; i < NumTextures; ++i)
		{
			FTextureRHIParamRef RenderTarget = InTextures[i];
			if (RenderTarget)
			{
				SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResourcesLoop, bShowTransitionEvents, TEXT("To:%i - %s"), i, *RenderTarget->GetName().ToString());

				FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(RenderTarget);
				Surface.SetCurrentGPUAccess(TransitionType);
			}
		}

		GnmContextType& Context = GetContext();

		volatile uint32_t* label = (volatile uint32_t*)Context.allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
		*label = 0x0;
		// NOTE: kEopCbDbReadsDone and kEopCsDone are two names for the same value, so this EOP event does cover both graphics and compute
		// use cases.
		Context.writeAtEndOfPipe(Gnm::kEopCbDbReadsDone, Gnm::kEventWriteDestMemory, const_cast<uint32_t*>(label),
			Gnm::kEventWriteSource32BitsImmediate, 0x1, Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru);
		Context.waitOnAddress(const_cast<uint32_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
	}

	if (TransitionType == EResourceTransitionAccess::EMetaData)
	{
		FlushCBMetaData();
	}
}

void FGnmCommandListContext::RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFenceRHI)
{
	const uint32 CurrentFrame = GGnmManager.GetFrameCount();
	SCOPED_RHI_DRAW_EVENTF(*this, RHITransitionResources, TEXT("TransitionTo%i: %i UAVs"), (int32)TransitionType, NumUAVs);
	for (int32 i = 0; i < NumUAVs; ++i)
	{
		if (InUAVs[i])
		{
			FGnmUnorderedAccessView* UAV = ResourceCast(InUAVs[i]);
			if (UAV)
			{
				UAV->SetResourceAccess(TransitionType);

				if (TransitionType != EResourceTransitionAccess::ERWNoBarrier)
				{
					UAV->SetResourceDirty(false, CurrentFrame);
				}
			}
		}
	}

	if (TransitionType != EResourceTransitionAccess::ERWNoBarrier)
	{
		GnmContextType& Context = GetContext();

		volatile uint32_t* label = (volatile uint32_t*)Context.allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
		*label = 0x0;

		Context.flushShaderCachesAndWait(
			Gnm::kCacheActionWriteBackAndInvalidateL1andL2,
			Gnm::kExtendedCacheActionInvalidateKCache,
			Gnm::kStallCommandBufferParserDisable);

		// NOTE: kEopCbDbReadsDone and kEopCsDone are two names for the same value, so this EOP event does cover both graphics and compute use cases.
		Context.writeAtEndOfPipe(
			Gnm::kEopCbDbReadsDone,
			Gnm::kEventWriteDestMemory,
			const_cast<uint32_t*>(label),
			Gnm::kEventWriteSource32BitsImmediate, 0x1,
			Gnm::kCacheActionNone, Gnm::kCachePolicyLru);

		Context.waitOnAddress(const_cast<uint32_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
	}

	FGnmComputeFence* WriteComputeFence = ResourceCast(WriteComputeFenceRHI);
	if (WriteComputeFence)
	{
		WriteComputeFence->WriteFenceGPU(*this, false);
	}
}

/**
 * Convert UE3 primitive type for drawing to the Gnm equivalent
 */
static Gnm::PrimitiveType TranslatePrimitiveType(EPrimitiveType PrimitiveType)
{
	// @todo gnm: See D3D11 for tesselation support
	switch (PrimitiveType)
	{
		case PT_TriangleList:	return Gnm::kPrimitiveTypeTriList;
		case PT_TriangleStrip:	return Gnm::kPrimitiveTypeTriStrip;
		case PT_LineList:		return Gnm::kPrimitiveTypeLineList;
		case PT_QuadList:		return Gnm::kPrimitiveTypeQuadList;
//		case PT_GeomtryShaderInput: return Gnm::kPrimitiveTypePointList;
		default:				UE_LOG(LogPS4, Fatal, TEXT("Unsupported primitive type %d"), (int32)PrimitiveType); return Gnm::kPrimitiveTypeTriList;
	}
}

void FGnmCommandListContext::RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	RHI_DRAW_CALL_STATS(PrimitiveType,NumInstances*NumPrimitives);
//	INC_DWORD_STAT(STAT_GnmDrawPrimitiveCalls);
//	INC_DWORD_STAT_BY(STAT_GnmTriangles,(uint32)(PrimitiveType != PT_LineList ? (NumPrimitives*NumInstances) : 0));
//	INC_DWORD_STAT_BY(STAT_GnmLines,(uint32)(PrimitiveType == PT_LineList ? (NumPrimitives*NumInstances) : 0));
	SCOPE_CYCLE_COUNTER(STAT_GnmDrawCallTime);

	uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
	
	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}

	// bind all late-bound things (vertex buffer pointers, constant buffers, etc)
	PrepareForDrawCall( 0 );

	auto& Context = GetContext();
	SetPrimitiveType(TranslatePrimitiveType((EPrimitiveType)PrimitiveType));

	SetIndexOffset( BaseVertexIndex );

	SetNumInstances(NumInstances);

#if !NO_DRAW_CALLS
	PRE_DRAW;
	SetIndexSize(Gnm::kIndexSize16);
	Context.drawIndexAuto(NumVertices);
	POST_DRAW;	
#endif

	// reset
	SetNumInstances(1);
	SetIndexOffset( 0 );
}

// make sure what the hardware expects matches what we give it for indirect arguments
static_assert(sizeof(FRHIDrawIndirectParameters) == sizeof(Gnm::DrawIndirectArgs), "FRHIDrawIndirectParameters size is wrong.");
static_assert(STRUCT_OFFSET(FRHIDrawIndirectParameters, VertexCountPerInstance) == STRUCT_OFFSET(Gnm::DrawIndirectArgs, m_vertexCountPerInstance), "Wrong offset of FRHIDrawIndirectParameters::VertexCountPerInstance.");
static_assert(STRUCT_OFFSET(FRHIDrawIndirectParameters, InstanceCount) == STRUCT_OFFSET(Gnm::DrawIndirectArgs, m_instanceCount), "Wrong offset of FRHIDrawIndirectParameters::InstanceCount.");
static_assert(STRUCT_OFFSET(FRHIDrawIndirectParameters, StartVertexLocation) == STRUCT_OFFSET(Gnm::DrawIndirectArgs, m_startVertexLocation), "Wrong offset of FRHIDrawIndirectParameters::StartVertexLocation.");
static_assert(STRUCT_OFFSET(FRHIDrawIndirectParameters, StartInstanceLocation) == STRUCT_OFFSET(Gnm::DrawIndirectArgs, m_startInstanceLocation), "Wrong offset of FRHIDrawIndirectParameters::StartInstanceLocation.");

void FGnmCommandListContext::RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
//	INC_DWORD_STAT(STAT_GnmDrawPrimitiveCalls);
	SCOPE_CYCLE_COUNTER(STAT_GnmDrawCallTime);

	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.RegisterGPUWork(0);
	}

	FGnmVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	auto& Context = GetContext();

	PrepareForDrawCall( 0 );

	//we don't know where the arguments are coming from so we must stall the pre-fetcher to make sure it doesn't cache bad values for the upcoming draw call.
	Context.stallCommandBufferParser();

	// set location of indirect buffer
	Context.stallCommandBufferParser();
	Context.setBaseIndirectArgs(Gnm::kShaderTypeGraphics, ArgumentBuffer->GetCurrentBuffer());

	// set drawing info
	SetPrimitiveType(TranslatePrimitiveType((EPrimitiveType)PrimitiveType));

	// draw it from an indirect block in GPU memory (the index is into an array of indexed indirect args
#if !NO_DRAW_CALLS
	PRE_DRAW;
	Context.drawIndirect(ArgumentOffset);
	POST_DRAW;	
#endif

	// Reset num instances after indirect call
	SetNumInstances(1);
	GnmContext->setNumInstances(1);

}


void FGnmCommandListContext::RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
	uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	RHI_DRAW_CALL_STATS(PrimitiveType,NumInstances*NumPrimitives);
//	INC_DWORD_STAT(STAT_GnmDrawPrimitiveCalls);
//	INC_DWORD_STAT_BY(STAT_GnmTriangles,(uint32)(PrimitiveType != PT_LineList ? (NumPrimitives*NumInstances) : 0));
//	INC_DWORD_STAT_BY(STAT_GnmLines,(uint32)(PrimitiveType == PT_LineList ? (NumPrimitives*NumInstances) : 0));
	SCOPE_CYCLE_COUNTER(STAT_GnmDrawCallTime);

	FGnmIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	uint64 IndexBufferOffset = 0;
	// if we want to start at a later index, calculate the offset in bytes
	if (StartIndex != 0)
	{
		IndexBufferOffset = StartIndex * (IndexBuffer->IndexSize == Gnm::kIndexSize16 ? 2 : 4);
	}

	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}
	
	// bind all late-bound things (vertex buffer pointers, constant buffers, etc)
	PrepareForDrawCall( FirstInstance );

	auto& Context = GetContext();
	SetPrimitiveType(TranslatePrimitiveType((EPrimitiveType)PrimitiveType));
	SetNumInstances(NumInstances);

	SetIndexSize(IndexBuffer->IndexSize);
	SetIndexOffset(BaseVertexIndex);
#if !NO_DRAW_CALLS
	PRE_DRAW;
	Context.drawIndex(GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType), (uint8*)IndexBuffer->GetCurrentBuffer() + IndexBufferOffset);
	POST_DRAW;
#endif

	// reset
	SetNumInstances(1);
	SetIndexOffset(0);
}

// UE4 assumes indexed indirect args are 5 UINTs
static_assert(sizeof(FRHIDrawIndexedIndirectParameters) == sizeof(Gnm::DrawIndexIndirectArgs), "FRHIDrawIndexedIndirectParameters size is wrong.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, IndexCountPerInstance) == STRUCT_OFFSET(Gnm::DrawIndexIndirectArgs, m_indexCountPerInstance), "Wrong offset of FRHIDrawIndexedIndirectParameters::IndexCountPerInstance.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, InstanceCount) == STRUCT_OFFSET(Gnm::DrawIndexIndirectArgs, m_instanceCount), "Wrong offset of FRHIDrawIndexedIndirectParameters::InstanceCount.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, StartIndexLocation) == STRUCT_OFFSET(Gnm::DrawIndexIndirectArgs, m_startIndexLocation), "Wrong offset of FRHIDrawIndexedIndirectParameters::StartIndexLocation.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, BaseVertexLocation) == STRUCT_OFFSET(Gnm::DrawIndexIndirectArgs, m_baseVertexLocation), "Wrong offset of FRHIDrawIndexedIndirectParameters::BaseVertexLocation.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, StartInstanceLocation) == STRUCT_OFFSET(Gnm::DrawIndexIndirectArgs, m_startInstanceLocation), "Wrong offset of FRHIDrawIndexedIndirectParameters::StartInstanceLocation.");

void FGnmCommandListContext::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
//	INC_DWORD_STAT(STAT_GnmDrawPrimitiveCalls);
	SCOPE_CYCLE_COUNTER(STAT_GnmDrawCallTime);

	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.RegisterGPUWork(0);
	}

	checkf(NumInstances > 1, TEXT("D3D expects NumInstances to be > 1, but doesn't use it, as the number of instances is in the indirect params..."));

	FGnmIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FGnmStructuredBuffer* ArgumentsBuffer = ResourceCast(ArgumentsBufferRHI);

	auto& Context = GetContext();

	PrepareForDrawCall( 0 );

	//we don't know where the arguments are coming from so we must stall the pre-fetcher to make sure it doesn't cache bad values for the upcoming draw call.
	Context.stallCommandBufferParser();

	// set location of indirect buffer
	Context.stallCommandBufferParser();
	Context.setBaseIndirectArgs(Gnm::kShaderTypeGraphics, ArgumentsBuffer->Buffer.getBaseAddress());

	// set drawing info
	SetPrimitiveType(TranslatePrimitiveType((EPrimitiveType)PrimitiveType));

	// set up index buffer
	SetIndexSize(IndexBuffer->IndexSize);
	Context.setIndexBuffer(IndexBuffer->GetCurrentBuffer());
	Context.setIndexCount(IndexBuffer->GetSize() / IndexBuffer->GetStride());

	// draw it from an indirect block in GPU memory (the index is into an array of indexed indirect args
#if !NO_DRAW_CALLS
	PRE_DRAW;
	Context.drawIndexIndirect(DrawArgumentsIndex * sizeof(Gnm::DrawIndexIndirectArgs));
	POST_DRAW;	
#endif

	// Reset num instances after indirect call
	SetNumInstances(1);
	GnmContext->setNumInstances(1);
}

void FGnmCommandListContext::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBufferRHI, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
//	INC_DWORD_STAT(STAT_GnmDrawPrimitiveCalls);
	SCOPE_CYCLE_COUNTER(STAT_GnmDrawCallTime);

	FGnmIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FGnmVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	auto& Context = GetContext();

	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.RegisterGPUWork(0);
	}

	PrepareForDrawCall( 0 );

	//we don't know where the arguments are coming from so we must stall the pre-fetcher to make sure it doesn't cache bad values for the upcoming draw call.
	Context.stallCommandBufferParser();

	// set location of indirect buffer
	Context.stallCommandBufferParser();
	Context.setBaseIndirectArgs(Gnm::kShaderTypeGraphics, ArgumentBuffer->GetCurrentBuffer());

	// set drawing info
	SetPrimitiveType(TranslatePrimitiveType((EPrimitiveType)PrimitiveType));

	// set up index buffer
	SetIndexSize(IndexBuffer->IndexSize);
	Context.setIndexBuffer(IndexBuffer->GetCurrentBuffer());
	Context.setIndexCount(IndexBuffer->GetSize() / IndexBuffer->GetStride());

	// draw it from an indirect block in GPU memory (the index is into an array of indexed indirect args
#if !NO_DRAW_CALLS
	PRE_DRAW;
	Context.drawIndexIndirect(ArgumentOffset);
	POST_DRAW;	
#endif

	// Reset num instances after indirect call
	SetNumInstances(1);
	GnmContext->setNumInstances(1);
}


void FGnmCommandListContext::RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
	checkSlow(PendingDrawPrimitiveUPVertexData == NULL);
	PendingDrawPrimitiveUPVertexData = AllocateFromTempFrameBuffer(NumVertices * VertexDataStride);
	check(PendingDrawPrimitiveUPVertexData);

	OutVertexData = PendingDrawPrimitiveUPVertexData;

	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;

	bPendingStreamsAreDirty = true;
}


/**
	* Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
	*/
void FGnmCommandListContext::RHIEndDrawPrimitiveUP()
{
	RHI_DRAW_CALL_STATS(PendingPrimitiveType,PendingNumPrimitives);
//	INC_DWORD_STAT(STAT_GnmDrawPrimitiveCalls);
//	INC_DWORD_STAT_BY(STAT_GnmTriangles,(uint32)(PendingPrimitiveType != PT_LineList ? PendingNumPrimitives : 0));
//	INC_DWORD_STAT_BY(STAT_GnmLines,(uint32)(PendingPrimitiveType == PT_LineList ? PendingNumPrimitives : 0));
	SCOPE_CYCLE_COUNTER(STAT_GnmDrawCallTime);

	checkSlow(PendingDrawPrimitiveUPVertexData != NULL);

	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.RegisterGPUWork(PendingNumPrimitives, PendingNumVertices);
	}

	// update everything, and tell Gnm about the GPU-view of the immediate data

	PrepareForDrawCallUP(PendingNumVertices, PendingDrawPrimitiveUPVertexData, PendingVertexDataStride);

	SetPrimitiveType(TranslatePrimitiveType((EPrimitiveType)PendingPrimitiveType));
#if !NO_DRAW_CALLS
	PRE_DRAW;
	SetIndexSize(Gnm::kIndexSize16);
	GetContext().drawIndexAuto(PendingNumVertices);
	POST_DRAW;
#endif

	// free used mem
	PendingDrawPrimitiveUPVertexData = NULL;
}

void FGnmCommandListContext::RHIBeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
{
	checkSlow(PendingDrawPrimitiveUPVertexData == NULL);
	checkSlow(PendingDrawPrimitiveUPIndexData == NULL);

	PendingDrawPrimitiveUPVertexData = AllocateFromTempFrameBuffer(NumVertices * VertexDataStride);
	OutVertexData = PendingDrawPrimitiveUPVertexData;
	check(OutVertexData);

	PendingDrawPrimitiveUPIndexData = AllocateFromTempFrameBuffer(NumIndices * IndexDataStride);
	OutIndexData = PendingDrawPrimitiveUPIndexData;
	check(OutIndexData);

	checkSlow((sizeof(uint16) == IndexDataStride) || (sizeof(uint32) == IndexDataStride));

	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingMinVertexIndex = MinVertexIndex;
	PendingIndexDataStride = IndexDataStride;

	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;

	bPendingStreamsAreDirty = true;
}

void FGnmCommandListContext::RHIEndDrawIndexedPrimitiveUP()
{
	RHI_DRAW_CALL_STATS(PendingPrimitiveType,PendingNumPrimitives);
//	INC_DWORD_STAT(STAT_GnmDrawPrimitiveCalls);
//	INC_DWORD_STAT_BY(STAT_GnmTriangles,(uint32)(PendingPrimitiveType != PT_LineList ? PendingNumPrimitives : 0));
//	INC_DWORD_STAT_BY(STAT_GnmLines,(uint32)(PendingPrimitiveType == PT_LineList ? PendingNumPrimitives : 0));
	SCOPE_CYCLE_COUNTER(STAT_GnmDrawCallTime);

	checkSlow(PendingDrawPrimitiveUPVertexData != NULL);
	checkSlow(PendingDrawPrimitiveUPIndexData != NULL);

	// update everything, and tell Gnm about the GPU-view of the immediate data
	PrepareForDrawCallUP(PendingNumVertices, PendingDrawPrimitiveUPVertexData, PendingVertexDataStride);

	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.RegisterGPUWork(PendingNumPrimitives, PendingNumVertices);
	}

	// draw it!
	SetPrimitiveType(TranslatePrimitiveType((EPrimitiveType)PendingPrimitiveType));
	SetIndexSize(PendingIndexDataStride == 2 ? Gnm::kIndexSize16 : Gnm::kIndexSize32);
#if !NO_DRAW_CALLS
	PRE_DRAW;
	GetContext().drawIndex(	GetVertexCountForPrimitiveCount(PendingNumPrimitives, PendingPrimitiveType), PendingDrawPrimitiveUPIndexData);
	POST_DRAW;
#endif

	// free used mem
	PendingDrawPrimitiveUPIndexData = NULL;
	PendingDrawPrimitiveUPVertexData = NULL;
}

/** Vertex declaration for just one FVector4 position. */
// @todo gnm: This is also in D3D11, can we move it up into the engine somewhere? Not sure where is best
class FVector4VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0,0,VET_Float4,0,sizeof(FVector4)));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

void FGnmCommandListContext::ClearColorTargetsByHardware(FRHICommandList_RecursiveHazardous& RHICmdList, int32 NumClearColors, const FLinearColor* ClearColorArray)
{
	auto& Context = GetContext();
	Context.pushMarker("BeginClearCMASK");
	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	int32 NumActiveRenderTargets = GetNumRenderTargets();

	// Using CMASK clear
	Context.triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta);

	
	// Setup the compute shader
	TShaderMapRef<FClearBufferReplacementCS> ComputeShader(ShaderMap);
	uint32 BaseIndex = ComputeShader->ClearBufferRW.GetBaseIndex();
	FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
	FGnmComputeShader* GnmComputeShader = (FGnmComputeShader*)ShaderRHI;
	RHICmdList.SetComputeShader(ShaderRHI);

	// UINT32_MAX means let the number of thread group to decide how many dwords to clear
	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->ClearBufferCSParams, FIntVector4(0, UINT32_MAX, 0, 0));
	
	RHICmdList.Flush(); // always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.

	PrepareForDispatch();

	bool bNonCMaskTargetsToClear = false;
	for (int32 RTIndex = 0; RTIndex < NumActiveRenderTargets; RTIndex++)
	{
		
		Gnm::RenderTarget* RTSurface = GetCurrentRenderTarget(RTIndex);

		if (RTSurface->getCmaskAddress() != NULL)
		{

			// Clear the cmask bits
			uint32 NumSlices = RTSurface->getLastArraySliceIndex() - RTSurface->getBaseArraySliceIndex() + 1;
			uint32 NumDwords = RTSurface->getCmaskSliceSizeInBytes() * NumSlices / sizeof(uint32);
			Gnm::Buffer DestinationBuffer;
			DestinationBuffer.initAsDataBuffer(RTSurface->getCmaskAddress(), Gnm::kDataFormatR32Uint, NumDwords);
			DestinationBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
			Context.setRwBuffers(Gnm::kShaderStageCs, BaseIndex, 1, &DestinationBuffer);

#if !NO_DRAW_CALLS
			PRE_DISPATCH;
			Context.dispatch((NumDwords + Gnm::kThreadsPerWavefront - 1) / Gnm::kThreadsPerWavefront, 1, 1);
			POST_DISPATCH;
#endif			

			// remember that we were cleared via CMASK, and we need to eliminate it later
			// only useful for debugging in single-threaded mode.
			if (!IsRunningRHIInSeparateThread())
			{
				FTextureRHIParamRef RT = GetCurrentRenderTargetTexture(RTIndex);
				FGnmSurface& GnmSurface = GetGnmSurfaceFromRHITexture(RT);
				GnmSurface.SetFastCleared(GGnmManager.GetFrameCount());
				GnmSurface.bNeedsFastClearResolve = true;
			}
		}
		else
		{
			bNonCMaskTargetsToClear = true;
		}
	}

	// Clear any targets which weren't CMask cleared
	if (bNonCMaskTargetsToClear)
	{
		for (int32 RTIndex = 0; RTIndex < NumActiveRenderTargets; RTIndex++)
		{
			Gnm::RenderTarget* RTSurface = GetCurrentRenderTarget(RTIndex);
			if (RTSurface->getCmaskAddress() == NULL)
			{
				FRHITexture* UAVTexture = GetCurrentRenderTargetTexture(RTIndex);
				FGnmUnorderedAccessView* UAV = new FGnmUnorderedAccessView(UAVTexture);
				FUnorderedAccessViewRHIRef UAVRef(UAV);

				ClearTexture2DUAV(RHICmdList, UAVRef, RTSurface->getWidth(), RTSurface->getHeight(), ClearColorArray[RTIndex]);
			}
		}
	}
	Context.popMarker();
}

void FGnmCommandListContext::ClearDepthTargetByHardware(FRHICommandList_RecursiveHazardous& RHICmdList, float Depth)
{
	auto& Context = GetContext();
	Context.pushMarker("BeginClearHTILE");

	FTextureRHIParamRef DepthTexture = GetCurrentDepthTarget();
	FGnmSurface* DepthTarget = DepthTexture ? &GetGnmSurfaceFromRHITexture(DepthTexture) : nullptr;
	
	Context.triggerEvent(Gnm::kEventTypeFlushAndInvalidateDbMeta);
	Gnm::Htile HTile = {};
	HTile.m_hiZ.m_zMask = 0;
	HTile.m_hiZ.m_minZ = static_cast<uint32_t>(floorf(Depth * Gnm::Htile::kMaximumZValue));
	HTile.m_hiZ.m_maxZ = static_cast<uint32_t>(ceilf(Depth * Gnm::Htile::kMaximumZValue));

	uint32 NumSlices = DepthTarget->DepthBuffer->getLastArraySliceIndex() - DepthTarget->DepthBuffer->getBaseArraySliceIndex() + 1;
	FillMemoryWithDword(RHICmdList, DepthTarget->DepthBuffer->getHtileAddress(),
		DepthTarget->DepthBuffer->getHtileSliceSizeInBytes() * NumSlices / sizeof(uint32), HTile.m_asInt);

	DepthTarget->SetFastCleared(GGnmManager.GetFrameCount());

	// when the system pulls from HTILE to depth buffer and clears the neighboring pixels, this is what it will fill with
	SetDepthClearValue(Depth);

	// mark that we need to have the HTILE decompressed later
	if (!IsRunningRHIInSeparateThread())
	{
		DepthTarget->bNeedsFastClearResolve = true;	
	}
	Context.popMarker();
}

void FGnmCommandListContext::ClearColorTargetsByShader(FRHICommandList_RecursiveHazardous& RHICmdList, int32 NumClearColors, const FLinearColor* ClearColorArray)
{
	FGnmCommandListContext& CommandListContext = (FGnmCommandListContext&)RHICmdList.GetContext();

	int32 NumActiveRenderTargets = CommandListContext.GetNumRenderTargets();
	auto& Context = CommandListContext.GetContext();

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	//take the most restrictive bounds between the viewport and the scissor to emulate how a pixel shader would be clipped.
	FVector4 Bounds = GetViewportBounds();
	Bounds.X = FMath::Max((float)CachedViewportScissorMinX, Bounds.X);
	Bounds.Y = FMath::Max((float)CachedViewportScissorMinY, Bounds.Y);
	Bounds.Z = FMath::Min((float)CachedViewportScissorMaxX, Bounds.Z);
	Bounds.W = FMath::Min((float)CachedViewportScissorMaxX, Bounds.W);	

	// make a texture out of the render target we are clearing
	for (int32 RTIndex = 0; RTIndex < NumActiveRenderTargets; RTIndex++)
	{
		FTextureRHIParamRef RT = CommandListContext.GetCurrentRenderTargetTexture(RTIndex);
		FGnmSurface& RTSurface = GetGnmSurfaceFromRHITexture(RT);

		bool bIsCubeMap = RTSurface.Texture->getTextureType() == Gnm::kTextureTypeCubemap;
		bool bIsVolumeTexture = RTSurface.Texture->getTextureType() == Gnm::kTextureType3d;

		const FGnmCommandListContext::FRenderTargetViewInfo& RTViewInfo = CommandListContext.GetCurrentRenderTargetViewInfo(RTIndex);

		//for cubemaps the array slice index will be the face.
		int32 EffectiveArraySliceIndex = RTViewInfo.ArraySliceIndex == -1 ? 0 : RTViewInfo.ArraySliceIndex;


		Gnm::Texture TextureCopy = *RTSurface.Texture;

		if (bIsVolumeTexture)
		{
			TShaderMapRef< FClearVolumeReplacementCS<float> > ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
			RHICmdList.SetComputeShader(ShaderRHI);
			
			SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->GetClearColorParameter(), ClearColorArray[RTIndex]);			
			uint32 x = (RTSurface.Texture->getWidth() + 3) / 4;
			uint32 y = (RTSurface.Texture->getHeight() + 3) / 4;
			uint32 z = (RTSurface.Texture->getDepth() + 3) / 4;

			RHICmdList.Flush(); // always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.			
			
			TextureCopy.setBaseAddress(RTSurface.Texture->getBaseAddress());
			TextureCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
			TextureCopy.setMipLevelRange(RTViewInfo.MipIndex, RTViewInfo.MipIndex);
			TextureCopy.setArrayView(EffectiveArraySliceIndex, EffectiveArraySliceIndex);
			Context.setRwTextures(Gnm::kShaderStageCs, ComputeShader->GetClearTextureRWParameter().GetBaseIndex(), 1, &TextureCopy);

			CommandListContext.PrepareForDispatch();			

#if !NO_DRAW_CALLS
			PRE_DISPATCH;
			Context.dispatch(x, y, z);
			POST_DISPATCH;
#endif
		}
		else
		{
			TShaderMapRef<FClearTexture2DReplacementScissorCS> ComputeShader(ShaderMap);
			CommandListContext.RHISetComputeShader(ComputeShader->GetComputeShader());

			FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
			FGnmComputeShader* GnmComputeShader = (FGnmComputeShader*)ShaderRHI;
			RHICmdList.SetComputeShader(ShaderRHI);

			SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->GetClearColorParameter(), ClearColorArray[RTIndex]);
			SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->GetTargetBoundsParameter(), Bounds);
			RHICmdList.Flush(); // always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.

			if (bIsCubeMap)
			{
				Gnm::TextureSpec TextureSpec;
				TextureSpec.init();
				TextureSpec.m_textureType = Gnm::kTextureType2dArray;
				TextureSpec.m_width = TextureCopy.getWidth();
				TextureSpec.m_height = TextureCopy.getHeight();
				TextureSpec.m_depth = 1;
				TextureSpec.m_pitch = 0;
				TextureSpec.m_numMipLevels = TextureCopy.getLastMipLevel() + 1;
				TextureSpec.m_numSlices = TextureCopy.getLastArraySliceIndex() + 1;
				TextureSpec.m_format = TextureCopy.getDataFormat();
				TextureSpec.m_tileModeHint = TextureCopy.getTileMode();
				TextureSpec.m_minGpuMode = Gnm::getGpuMode();
				TextureSpec.m_numFragments = TextureCopy.getNumFragments();
				TextureCopy.init(&TextureSpec);
			}

			TextureCopy.setBaseAddress(RTSurface.Texture->getBaseAddress());
			TextureCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
			TextureCopy.setMipLevelRange(RTViewInfo.MipIndex, RTViewInfo.MipIndex);
			TextureCopy.setArrayView(EffectiveArraySliceIndex, EffectiveArraySliceIndex);
			Context.setRwTextures(Gnm::kShaderStageCs, ComputeShader->GetClearTextureRWParameter().GetBaseIndex(), 1, &TextureCopy);

			CommandListContext.PrepareForDispatch();
#if !NO_DRAW_CALLS
			PRE_DISPATCH;
			const uint32 MipWidth = FMath::Max<uint32>(TextureCopy.getWidth() >> RTViewInfo.MipIndex, 1);
			const uint32 MipHeight = FMath::Max<uint32>(TextureCopy.getHeight() >> RTViewInfo.MipIndex, 1);

			Context.dispatch((MipWidth + 7) / 8, (MipHeight + 7) / 8, 1);
			POST_DISPATCH;
#endif
		}
	}

	// flush any needed buffers that the compute shader wrote to
	CommandListContext.FlushAfterComputeShader();
}

void FGnmCommandListContext::ClearDepthTargetByShader(FRHICommandList_RecursiveHazardous& RHICmdList, float Depth)
{
	FTextureRHIParamRef DepthTexture = GetCurrentDepthTarget();
	if( DepthTexture == nullptr )
	{
		// No depth target to clear
		return;
	}

	HACK_ClearDepth(RHICmdList, Depth);
	if (!IsRunningRHIInSeparateThread())
	{
		FGnmSurface* DepthTarget = &GetGnmSurfaceFromRHITexture(DepthTexture);
		DepthTarget->bNeedsFastClearResolve = false;
	}
}

void FGnmCommandListContext::ClearStencilTarget(FRHICommandList_RecursiveHazardous& RHICmdList, uint32 Stencil)
{
	auto& Context = GetContext();
	FTextureRHIParamRef DepthTexture = GetCurrentDepthTarget();
	if( DepthTexture == nullptr )
	{
		// No depth target
		return;
	}

	FGnmSurface* DepthTarget = &GetGnmSurfaceFromRHITexture(DepthTexture);
	if( DepthTarget->DepthBuffer->getStencilWriteAddress() == nullptr )
	{
		// No stencil target to clear
		return;
	}

	// Wait for the draw to be finished, and flush the Cb/Db caches:
	volatile uint32* Label = (uint32*)Context.allocateFromCommandBuffer(sizeof(uint32), Gnm::kEmbeddedDataAlignment8);
	*Label = 0;
	Context.writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushAndInvalidateCbDbCaches, (void*)Label, 1, Gnm::kCacheActionWriteBackAndInvalidateL1andL2);
	Context.waitOnAddress((void*)Label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);

	Context.triggerEvent(Gnm::kEventTypeFlushAndInvalidateDbMeta);

	uint32 NumSlices = DepthTarget->DepthBuffer->getLastArraySliceIndex() - DepthTarget->DepthBuffer->getBaseArraySliceIndex() + 1;
	FillMemoryWithDword(RHICmdList, DepthTarget->DepthBuffer->getStencilWriteAddress(),
		DepthTarget->DepthBuffer->getStencilSliceSizeInBytes() * NumSlices / sizeof(uint32), Stencil);
}

void FGnmCommandListContext::ClearStencilTargetByGfxShader( uint32 Stencil )
{
	auto& Context = GetContext();
	sce::Gnmx::GnmxDrawCommandBuffer* DCB = &GnmContext->m_dcb;

	FTextureRHIParamRef DepthTexture = GetCurrentDepthTarget();
	if( DepthTexture == nullptr )
	{
		// No depth target
		return;
	}

	FGnmSurface* DepthTarget = &GetGnmSurfaceFromRHITexture(DepthTexture);
	if( DepthTarget->DepthBuffer->getStencilWriteAddress() == nullptr )
	{
		// No stencil target to clear
		return;
	}

	Gnm::DbRenderControl DBRenderControl;
	DBRenderControl.init();
	DBRenderControl.setDepthClearEnable( false );
	DBRenderControl.setStencilClearEnable( true );
	DCB->setDbRenderControl( DBRenderControl );

	Gnm::DepthStencilControl DepthControl;
	DepthControl.init();
	DepthControl.setDepthControl( Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncAlways );
	DepthControl.setStencilFunction( Gnm::kCompareFuncAlways );
	DepthControl.setDepthEnable( false );
	DepthControl.setStencilEnable( true );
	DCB->setDepthStencilControl( DepthControl );

	Gnm::StencilOpControl StencilOpControl;
	StencilOpControl.init();
	StencilOpControl.setStencilOps( Gnm::kStencilOpReplaceTest, Gnm::kStencilOpReplaceTest, Gnm::kStencilOpReplaceTest );
	DCB->setStencilOpControl( StencilOpControl );
	const Gnm::StencilControl StencilControl = { 0xff, 0xff, 0xff, 0xff };
	DCB->setStencil( StencilControl );
	DCB->setStencilClearValue( Stencil );

	Gnm::PrimitiveSetup PrimitiveSetup;
	PrimitiveSetup.init();
	PrimitiveSetup.setCullFace(sce::Gnm::kPrimitiveSetupCullFaceBack);
	PrimitiveSetup.setPolygonMode(sce::Gnm::kPrimitiveSetupPolygonModeFill, sce::Gnm::kPrimitiveSetupPolygonModeFill);
	DCB->setPrimitiveSetup(PrimitiveSetup);

	SetNumInstances(1);
	DCB->setRenderTargetMask(0x0);

	TShaderMapRef<FClearReplacementVS> ClearVertexShader( GetGlobalShaderMap( GMaxRHIFeatureLevel ) );
	FVertexShaderRHIParamRef VertexShaderRHI = ClearVertexShader->GetVertexShader();
	FGnmVertexShader* VertexShader = ResourceCast(VertexShaderRHI);
	Gnmx::VsShader* VSB = VertexShader->Shader;
	DCB->setPsShader( NULL );
	DCB->setVsShader(&VSB->m_vsStageRegisters, 0);
	DCB->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
	
	DCB->disableGsMode();
	DCB->setIndexSize(Gnm::kIndexSize16, Gnm::kCachePolicyBypass);
	DCB->setPrimitiveType(Gnm::kPrimitiveTypeRectList);

	PRE_DRAW;
	DCB->drawIndexAuto(3);
	POST_DRAW;

	DBRenderControl.init();
	DCB->setDbRenderControl( DBRenderControl );

	RestoreCachedDCBState();
}

void FGnmCommandListContext::TryClearMRTByHardware(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	FRHICommandList_RecursiveHazardous RHICmdList(this);

	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.RegisterGPUWork();
	}

	const int32 NumActiveRenderTargets = GetNumRenderTargets();
	// Must specify enough clear colors for all active RTs
	check(!bClearColor || NumClearColors >= NumActiveRenderTargets);

	auto& Context = GetContext();

	// we must flush the render targets the compute shader will write to, even though they haven't been written 
	// to yet... this is not exactly clear why, but it's REQUIRED!!!
	FlushBeforeComputeShader();	

	if (bClearColor)
	{
#if USE_CMASK
		FVector4 Bounds = GetViewportBounds();		
		Gnm::RenderTarget* RTSurface = GetCurrentRenderTarget(0);

		check(RTSurface);

		bool bCanUseCMASK = RTSurface->getCmaskAddress() != nullptr;

		// We can only use the CMASK fast clear when clearing the full screen
		bool bFullScreenClear = (Bounds.X == 0 && Bounds.Y == 0 && Bounds.Z == RTSurface->getWidth() && Bounds.W == RTSurface->getHeight());

		if (bCanUseCMASK && bFullScreenClear)
		{
			ClearColorTargetsByHardware(RHICmdList, NumClearColors, ClearColorArray);
		}
		else
#endif
		{
			ClearColorTargetsByShader(RHICmdList, NumClearColors, ClearColorArray);
		}
	}

	FTextureRHIParamRef DepthTexture = GetCurrentDepthTarget();
	FGnmSurface* DepthTarget = DepthTexture ? &GetGnmSurfaceFromRHITexture(DepthTexture) : nullptr;

	bool bRequireDepthTarget = (bClearDepth || bClearStencil);
	if (bRequireDepthTarget && DepthTarget)
	{
		if (bClearDepth)
		{
#if USE_HTILE
			// We can only use the HTILE fast clear when clearing the full screen
			FVector4 Bounds = GetViewportBounds();
			const bool bIsHTileAvailable = DepthTarget->DepthBuffer->getHtileAddress() != NULL;
			const bool bIsFullScreenClear = Bounds.X == 0 && Bounds.Y == 0 && Bounds.Z == DepthTarget->DepthBuffer->getWidth() && Bounds.W == DepthTarget->DepthBuffer->getHeight();
			if (bIsHTileAvailable && bIsFullScreenClear)
			{
				ClearDepthTargetByHardware(RHICmdList, Depth);
			}
			else
#endif
			{
				ClearDepthTargetByShader(RHICmdList, Depth);
			}
		}

		if (bClearStencil)
		{
			ClearStencilTarget(RHICmdList, Stencil);
		}
	}
	
	RHICmdList.Flush(); // always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.

	FlushAfterComputeShader();

	// Flush the CB color cache
	volatile uint32_t* Label = (uint32_t*)Context.allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
	*Label = 0;
	Context.writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (uint32_t*)Label, 1, Gnm::kCacheActionNone);
	Context.waitOnAddress((uint32_t*)Label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);

	//disable 'error' check.  Now that we bind the clear color on texture creation we don't need this error and there's no contract
	//on the high level to require a resolve before deciding to clear again.
	BindClearMRTValues_Impl(bClearColor, bClearDepth, bClearStencil, false);	
}

void FGnmCommandListContext::ClearMRTByShader(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	float ViewportMaxX = CachedViewportBounds.Z;
	float ViewportMaxY = CachedViewportBounds.W;

	FRHICommandList_RecursiveHazardous RHICmdList(this);
	auto& Context = GetContext();
	Context.pushMarker("ShaderClear");

	// we must flush the render targets the compute shader will write to, even though they haven't been written 
	// to yet... this is not exactly clear why, but it's REQUIRED!!!
	if( bClearColor || bClearDepth )
	{
		FlushBeforeComputeShader();
	}

	if (bClearColor)
	{
		ClearColorTargetsByShader(RHICmdList, NumClearColors, ClearColorArray);
	}

	if (bClearDepth)
	{
		ClearDepthTargetByShader(RHICmdList, Depth);
	}

	if (bClearStencil)
	{
		ClearStencilTargetByGfxShader( Stencil );
	}

	if( bClearColor || bClearDepth )
	{
		FlushAfterComputeShader();

		// Flush the CB color cache
		volatile uint32_t* Label = (uint32_t*)Context.allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
		*Label = 0;
		Context.writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (uint32_t*)Label, 1, Gnm::kCacheActionNone);
		Context.waitOnAddress((uint32_t*)Label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);
	}
	Context.popMarker();
}


void FGnmCommandListContext::RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	if (!GIsRHIInitialized)
	{
		return;
	}

	// loose RHIClear calls always go through the slow path because allowing loose hardware clears adds unnecessary complexity to the fastclear tracking when using parallel rendering.
	// high level code should be written to use SetAndClear whenever possible.
	ClearMRTByShader(bClearColor, NumClearColors, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil);
}

void FGnmCommandListContext::RHIBindClearMRTValues(bool bClearColor, bool bClearDepth, bool bClearStencil)
{
	//don't check for errors.  these bindings are generally coming from calls to propagate data across parallel command list boundaries.  When testing parallel algorithms in 
	//single threaded mode, these binding calls get made after the SetRT calls, but before the finalize.  Since these bindings aren't associated with their own clear calls,
	//the checks are benign.
	BindClearMRTValues_Impl(bClearColor, bClearDepth, bClearStencil, false);
}

void FGnmCommandListContext::BindClearMRTValues_Impl(bool bClearColor, bool bClearDepth, bool bClearStencil, bool bCheckErrors)
{
	int32 NumActiveRenderTargets = GetNumRenderTargets();
	GnmContext->Reserve(4096);
	if (bClearColor)
	{
		for (int32 RTIndex = 0; RTIndex < NumActiveRenderTargets; RTIndex++)
		{
			FTextureRHIParamRef RT = GetCurrentRenderTargetTexture(RTIndex);
			FGnmSurface& RTSurface = GetGnmSurfaceFromRHITexture(RT);
			if (RT->HasClearValue() && RTSurface.ColorBuffer->getCmaskAddress() != nullptr)
			{				
				ensureMsgf(!bCheckErrors || !TargetsNeedingEliminateFastClear.Contains(RT), TEXT("Binding texture: %s for clear before it was resolved."), *(RT->GetName().GetPlainNameString()));
				TargetsNeedingEliminateFastClear.Add(RT);

				//when using RHIBindClearMRTValues to propagate information between parallel contexts, the SetRenderTarget calls come first, so they won't have the information necessary to set up cmask
				//usage properly.  Set it up properly now. 
				CurrentRenderTargets[RTIndex].setCmaskFastClearEnable(true);
				GnmContext->setRenderTarget(RTIndex, &(CurrentRenderTargets[RTIndex]));							
			}
		}
	}

	if (bClearDepth)
	{
		check(CurrentDepthTarget);
		
		FGnmSurface& RTSurface = GetGnmSurfaceFromRHITexture(CurrentDepthTarget);
		if (CurrentDepthTarget->HasClearValue() && RTSurface.DepthBuffer->getHtileAddress() != nullptr)
		{
			ensureMsgf(!bCheckErrors || !TargetsNeedingEliminateFastClear.Contains(CurrentDepthTarget), TEXT("Binding depth target: %s for clear before it was resolved."), *(CurrentDepthTarget->GetName().GetPlainNameString()));
			TargetsNeedingEliminateFastClear.Add(CurrentDepthTarget);			
		}
	}
}

// Blocks the CPU until the GPU catches up and goes idle.
void FGnmDynamicRHI::RHIBlockUntilGPUIdle()
{
	check(IsInRenderingThread() || IsInRHIThread());
	GGnmManager.WaitForGPUIdleNoReset();	
}

/**
* After the reflections are captured, we need to kick the command buffer, otherwise too many captures will
* overflow the command buffer.
* Syncing may also be needed here to read from SceneColor
*/
void FGnmDynamicRHI::RHISubmitCommandsAndFlushGPU()
{
	//may cause problems if resources allocated via tempframe buffers span rendering into cubefaces,
	//but anything that does span is a bug given the resource lifetime semantics involved.  No such resources
	//detected at this time.
	GGnmManager.EndFrame(false, true);
}

/**
 * Returns the total GPU time taken to render the last frame. Same metric as appCycles().
 */
uint32 FGnmDynamicRHI::RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

void FGnmCommandListContext::RHIAutomaticCacheFlushAfterComputeShader(bool bEnable)
{
	AutomaticCacheFlushAfterComputeShader( bEnable );
}

void FGnmCommandListContext::RHIFlushComputeShaderCache()
{
	FlushAfterComputeShader();
}

void FGnmDynamicRHI::RHIExecuteCommandList(FRHICommandList*)
{
	check(0);
}

void FGnmCommandListContext::RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
{
	EnableDepthBoundsTest( bEnable, MinDepth, MaxDepth );
}

void FGnmCommandListContext::RHISubmitCommandsHint()
{
	//break off a submit as long as we have any.
	SubmitCurrentCommands(1);
}

#if PLATFORM_USES_FIXED_RHI_CLASS
#define INTERNAL_DECORATOR(Method) ((FGnmCommandListContext&)CmdList.GetContext()).FGnmCommandListContext::Method
#include "RHICommandListCommandExecutes.inl"
#endif

