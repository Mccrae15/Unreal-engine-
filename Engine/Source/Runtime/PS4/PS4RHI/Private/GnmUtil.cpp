// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmUtil.h: Gnm RHI utility implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "GnmBridge.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFilemanager.h"

#include <razor_gpu_thread_trace.h>

DEFINE_STAT(STAT_GnmDrawPrimitiveCalls);
DEFINE_STAT(STAT_GnmTriangles);
DEFINE_STAT(STAT_GnmLines);
DEFINE_STAT(STAT_GnmDrawCallTime);
DEFINE_STAT(STAT_GnmDrawCallPrepareTime);
DEFINE_STAT(STAT_GnmCreateUniformBufferTime);
DEFINE_STAT(STAT_GnmTexturesAllocated);
DEFINE_STAT(STAT_GnmTexturesReleased);
DEFINE_STAT(STAT_GnmUpdateUniformBufferTime);
DEFINE_STAT(STAT_GnmGPUFlipWaitTime);
DEFINE_STAT(STAT_GnmGPUSubmitWaitTime);
DEFINE_STAT(STAT_GnmGPUTotalTime);
DEFINE_STAT(STAT_GnmNumSubmissions);
DEFINE_STAT(STAT_Garlic_Label);
DEFINE_STAT(STAT_Garlic_Event);
DEFINE_STAT(STAT_Garlic_FetchShader);
DEFINE_STAT(STAT_Garlic_GsShader);
DEFINE_STAT(STAT_Garlic_EsShader);
DEFINE_STAT(STAT_Garlic_VsShader);
DEFINE_STAT(STAT_Garlic_CsShader);
DEFINE_STAT(STAT_Garlic_GsVsShader);
DEFINE_STAT(STAT_Garlic_PsShader);
DEFINE_STAT(STAT_Garlic_HsShader);
DEFINE_STAT(STAT_Garlic_ShaderHelperMem);
DEFINE_STAT(STAT_Garlic_HTile);
DEFINE_STAT(STAT_Garlic_StencilBuffer);
DEFINE_STAT(STAT_Garlic_CMask);
DEFINE_STAT(STAT_Garlic_MultiuseUniformBuffer);
DEFINE_STAT(STAT_Garlic_StructuredBuffer);
DEFINE_STAT(STAT_Garlic_RenderQuery);
DEFINE_STAT(STAT_Garlic_RingBuffer);
DEFINE_STAT(STAT_TempBlockAllocator);
DEFINE_STAT(STAT_Garlic_DrawCommandBuffer);
DEFINE_STAT(STAT_Garlic_ConstantCommandBuffer);
DEFINE_STAT(STAT_Garlic_IndexBuffer);
DEFINE_STAT(STAT_Garlic_VertexBuffer);
DEFINE_STAT(STAT_Garlic_RenderTarget);
DEFINE_STAT(STAT_Garlic_DepthRenderTarget);
DEFINE_STAT(STAT_Garlic_Texture);
DEFINE_STAT(STAT_Garlic_DefragHeap);
DEFINE_STAT(STAT_Garlic_DuplicateUntracked);
DEFINE_STAT(STAT_Onion_Label);
DEFINE_STAT(STAT_Onion_RazorCPU);
DEFINE_STAT(STAT_Onion_Event);
DEFINE_STAT(STAT_Onion_FetchShader);
DEFINE_STAT(STAT_Onion_GsShader);
DEFINE_STAT(STAT_Onion_EsShader);
DEFINE_STAT(STAT_Onion_VsShader);
DEFINE_STAT(STAT_Onion_CsShader);
DEFINE_STAT(STAT_Onion_GsVsShader);
DEFINE_STAT(STAT_Onion_PsShader);
DEFINE_STAT(STAT_Onion_HsShader);
DEFINE_STAT(STAT_Onion_ShaderHelperMem);
DEFINE_STAT(STAT_Onion_HTile);
DEFINE_STAT(STAT_Onion_StencilBuffer);
DEFINE_STAT(STAT_Onion_CMask);
DEFINE_STAT(STAT_Onion_MultiuseUniformBuffer);
DEFINE_STAT(STAT_Onion_StructuredBuffer);
DEFINE_STAT(STAT_Onion_RenderQuery);
DEFINE_STAT(STAT_Onion_RingBuffer);
DEFINE_STAT(STAT_Onion_CUECpRam);
DEFINE_STAT(STAT_Onion_DrawCommandBuffer);
DEFINE_STAT(STAT_Onion_ConstantCommandBuffer);
DEFINE_STAT(STAT_Onion_IndexBuffer);
DEFINE_STAT(STAT_Onion_VertexBuffer);
DEFINE_STAT(STAT_Onion_RenderTarget);
DEFINE_STAT(STAT_Onion_DepthRenderTarget);
DEFINE_STAT(STAT_Onion_Texture);
DEFINE_STAT(STAT_Onion_UavCounters);
DEFINE_STAT(STAT_Onion_DuplicateUntracked);

#if ENABLE_GNM_VERIFICATION

static TMap<uint32, TMap<uint32, FString> > GVertexShaderResources;
static TMap<uint32, TMap<uint32, FString> > GPixelShaderResources;

static TMap<uint32, TSet<uint32> > GBoundVertexResources;
static TMap<uint32, TSet<uint32> > GBoundPixelResources;


void ResetBoundResources()
{
	GBoundVertexResources.Empty();
	GBoundPixelResources.Empty();
}

/**
 * Looks through a program looking for resources that need to be tracked
 */
static void GetShaderResources(TMap<uint32, TMap<uint32, FString> >& Resources, sce::Shader::Binary::Program& Program)
{
	Resources.Empty();
	for (uint32 i = 0; i < Program.m_numSamplerStates; ++i)
	{
		auto& SamplerState = Program.m_samplerStates[i];
		TMap<uint32, FString>& Map = Resources.FindOrAdd((uint32)sce::Shader::Binary::kInternalBufferTypeTextureSampler);
		Map.Add(SamplerState.m_resourceIndex, ANSI_TO_TCHAR(SamplerState.getName()));
	}

	for (uint32 i = 0; i < Program.m_numBuffers; ++i)
	{
		auto& Buffer = Program.m_buffers[i];
		TMap<uint32, FString>& Map = Resources.FindOrAdd((uint32)Buffer.m_internalType);
		Map.Add(Buffer.m_resourceIndex, ANSI_TO_TCHAR(Buffer.getName()));
	}
}

void StartVertexShaderVerification(FGnmVertexShader* Shader)
{
	// look into the bytes to make sure the suffix isn't overwritten
	uint64 GPUCode = (((uint64)Shader->Shader->m_vsStageRegisters.m_spiShaderPgmHiVs) << 32) | Shader->Shader->m_vsStageRegisters.m_spiShaderPgmLoVs;
	uint8* Code = (uint8*)Gnm::translateGpuToCpuAddress(GPUCode << 8);
	uint8* SuffixLoc = Code + Shader->Shader->m_common.m_shaderSize - 28; //sizeof(Gnmx::ShaderBinaryInfo)
	checkf(	SuffixLoc[0] == 'O' && SuffixLoc[1] == 'r' && SuffixLoc[2] == 'b' && SuffixLoc[3] == 'S' && 
		SuffixLoc[4] == 'h' && SuffixLoc[5] == 'd' && SuffixLoc[6] == 'r',
		TEXT("The vertex program's suffix was overwritten"));

	// get all the resources we want to set
	GetShaderResources(GVertexShaderResources, Shader->Program);
}

void StartPixelShaderVerification(FGnmPixelShader* Shader)
{
	// @todo: Remove this when we are confident the shaders are valid
	uint64 GPUCode = (((uint64)Shader->Shader->m_psStageRegisters.m_spiShaderPgmHiPs) << 32) | Shader->Shader->m_psStageRegisters.m_spiShaderPgmLoPs;
	uint8* Code = (uint8*)Gnm::translateGpuToCpuAddress(GPUCode << 8);
	uint8* SuffixLoc = Code + Shader->Shader->m_common.m_shaderSize - 28; //sizeof(Gnmx::ShaderBinaryInfo)
	checkf(	SuffixLoc[0] == 'O' && SuffixLoc[1] == 'r' && SuffixLoc[2] == 'b' && SuffixLoc[3] == 'S' && 
		SuffixLoc[4] == 'h' && SuffixLoc[5] == 'd' && SuffixLoc[6] == 'r',
		TEXT("The pixel program's suffix was overwritten"));

	// get all the resources we want to set
	GetShaderResources(GPixelShaderResources, Shader->Program);
}



void TrackResource(uint32 ResourceType, uint32 ResourceIndex, Gnm::ShaderStage Stage)
{
	// remove this index, it's been set, we no longer care about it
	if (Stage == Gnm::kShaderStageVs)
	{
		GBoundVertexResources.FindOrAdd(ResourceType).Add(ResourceIndex);
	}
	else if (Stage == Gnm::kShaderStagePs)
	{
		GBoundPixelResources.FindOrAdd(ResourceType).Add(ResourceIndex);
	}
};


void EndShaderVerification()
{
	// was anything unset?
	for (TMap<uint32, TMap<uint32, FString> >::TIterator It(GVertexShaderResources); It; ++It)
	{
		for (TMap<uint32, FString>::TIterator It2(It.Value()); It2; ++It2)
		{
			checkf(GBoundVertexResources.FindOrAdd(It.Key()).Find(It2.Key()) != NULL, TEXT("Failed to set vertex shader resource %s, type %d, in slot %d"), *It2.Value(), It.Key(), It2.Key());
		}
	}
	for (TMap<uint32, TMap<uint32, FString> >::TIterator It(GPixelShaderResources); It; ++It)
	{
		for (TMap<uint32, FString>::TIterator It2(It.Value()); It2; ++It2)
		{
			checkf(GBoundPixelResources.FindOrAdd(It.Key()).Find(It2.Key()) != NULL, TEXT("Failed to set pixel shader resource %s, type %d, in slot %d"), *It2.Value(), It.Key(), It2.Key());
		}
	}
};

#endif

#if ENABLE_RENDERTARGET_VERIFICATION

static bool GDoRenderTargetVerification = false;

/**
 * Indicate that the render target / pixel shader pair should be verified at the next draw call
 */
void RequestRenderTargetVerification()
{
	GDoRenderTargetVerification = true;
}

FString GetTargetOutputModeString( sce::Gnm::PsTargetOutputMode TargetOutputMode )
{
	FString OutputModeString;
	#define TARGETOUTPUTMODECASE(x,y) case sce::Gnm::x: OutputModeString = TEXT(#y); break;

	switch( TargetOutputMode )
	{
		TARGETOUTPUTMODECASE(kPsTargetOutputModeR32,FMT_32_R)
		TARGETOUTPUTMODECASE(kPsTargetOutputModeG32R32,FMT_32_GR)
		TARGETOUTPUTMODECASE(kPsTargetOutputModeA32R32,FMT_32_AR)
		TARGETOUTPUTMODECASE(kPsTargetOutputModeA16B16G16R16Float,FMT_FP16_ABGR)
		TARGETOUTPUTMODECASE(kPsTargetOutputModeA16B16G16R16Unorm,FMT_UNORM16_ABGR)
		TARGETOUTPUTMODECASE(kPsTargetOutputModeA16B16G16R16Snorm,FMT_SNORM16_ABGR)
		TARGETOUTPUTMODECASE(kPsTargetOutputModeA16B16G16R16Uint,FMT_UINT16_ABGR)
		TARGETOUTPUTMODECASE(kPsTargetOutputModeA16B16G16R16Sint,FMT_SINT16_ABGR)
		TARGETOUTPUTMODECASE(kPsTargetOutputModeA32B32G32R32,FMT_32_ABGR)
		default:
			OutputModeString = TEXT("INVALID");
	}
	return OutputModeString;
}

FString GetRenderTargetFormatString( sce::Gnm::RenderTargetFormat RenderTargetFormat )
{
	FString RenderTargetFormatString;

	#define RENDERTARGETFORMATCASE(x) case sce::Gnm::x: RenderTargetFormatString = TEXT(#x); break;

	switch( RenderTargetFormat )
	{
		RENDERTARGETFORMATCASE(kRenderTargetFormatInvalid)
		RENDERTARGETFORMATCASE(kRenderTargetFormat8)
		RENDERTARGETFORMATCASE(kRenderTargetFormat16)
		RENDERTARGETFORMATCASE(kRenderTargetFormat8_8)
		RENDERTARGETFORMATCASE(kRenderTargetFormat32)
		RENDERTARGETFORMATCASE(kRenderTargetFormat16_16)
		RENDERTARGETFORMATCASE(kRenderTargetFormat10_11_11)
		RENDERTARGETFORMATCASE(kRenderTargetFormat11_11_10)
		RENDERTARGETFORMATCASE(kRenderTargetFormat10_10_10_2)
		RENDERTARGETFORMATCASE(kRenderTargetFormat2_10_10_10)
		RENDERTARGETFORMATCASE(kRenderTargetFormat8_8_8_8)
		RENDERTARGETFORMATCASE(kRenderTargetFormat32_32)
		RENDERTARGETFORMATCASE(kRenderTargetFormat16_16_16_16)
		RENDERTARGETFORMATCASE(kRenderTargetFormat32_32_32_32)
		RENDERTARGETFORMATCASE(kRenderTargetFormat5_6_5)
		RENDERTARGETFORMATCASE(kRenderTargetFormat1_5_5_5)
		RENDERTARGETFORMATCASE(kRenderTargetFormat5_5_5_1)
		RENDERTARGETFORMATCASE(kRenderTargetFormat4_4_4_4)
		RENDERTARGETFORMATCASE(kRenderTargetFormat8_24)
		RENDERTARGETFORMATCASE(kRenderTargetFormat24_8)
		RENDERTARGETFORMATCASE(kRenderTargetFormatX24_8_32)
		default:
			RenderTargetFormatString = FString::Printf(TEXT("Unknown 0x%x"), RenderTargetFormat);
	}

	return RenderTargetFormatString;
}


/**
 * Checks if the target format is one of the valid target formats
 */
bool IsValidRenderTargetFormat( sce::Gnm::RenderTargetFormat TargetFormat, sce::Gnm::RenderTargetFormat* ValidRenderTargetFormats, uint32 NumFormats )
{
	bool bValid = false;
	for(  uint32 RenderTargetFormatIndex = 0; RenderTargetFormatIndex < NumFormats; RenderTargetFormatIndex++ )
	{
		if( ValidRenderTargetFormats[ RenderTargetFormatIndex ] == TargetFormat )
		{
			bValid = true;
			break;
		}
	}
	return bValid;
}

/**
 * Checks whether the output mode of the bound pixel shader is compatible with the
 * currently bound render targets.
 */
void VerifyRenderTargets(FGnmCommandListContext& GnmCommandContext, FGnmBoundShaderState* BoundShaderState)
{
	if( GDoRenderTargetVerification == true && BoundShaderState->PixelShader )
	{
		GDoRenderTargetVerification = false;

		Gnmx::PsShader* PShader = BoundShaderState->PixelShader->Shader;
		uint32 NumRenderTargets = GnmCommandContext.GetNumRenderTargets();
		uint32 ShaderOutputTargetIndex = 0;
		uint32 ShaderOutputMask = 0xf;
		for( uint32 RenderTargetIndex = 0; RenderTargetIndex < MaxSimultaneousRenderTargets; RenderTargetIndex++, ShaderOutputMask <<= 4 )
		{
			if( ( PShader->m_psStageRegisters.m_cbShaderMask & ShaderOutputMask ) == 0 )
			{
				// Shader doesn't write to this target
				continue;
			}

			FTextureRHIParamRef RT = GnmCommandContext.GetCurrentRenderTargetTexture(RenderTargetIndex);			
			if (RT == NULL)
			{
				continue;
			}
			FGnmSurface* RenderTarget = &(GetGnmSurfaceFromRHITexture(RT));

			// Information about the render target
			sce::Gnm::DataFormat RenderTargetDataFormat = RenderTarget->ColorBuffer->getDataFormat();
			sce::Gnm::RenderTargetFormat RenderTargetFormat = RenderTargetDataFormat.getRenderTargetFormat();
			sce::Gnm::RenderTargetChannelOrder TargetChannelOrder;
			RenderTargetDataFormat.getRenderTargetChannelOrder( &TargetChannelOrder );
			sce::Gnm::RenderTargetChannelType TargetChannelType;
			RenderTargetDataFormat.getRenderTargetChannelType( &TargetChannelType );

			// Output mode that the shader is expecting
			sce::Gnm::PsTargetOutputMode TargetOutputMode = PShader->m_psStageRegisters.getTargetOutputMode( ShaderOutputTargetIndex );
			ShaderOutputTargetIndex++;

			bool bIsValid = true;

			switch( TargetOutputMode )
			{
			case sce::Gnm::kPsTargetOutputModeR32:
				{
					static sce::Gnm::RenderTargetFormat ValidRenderTargetFormats[] = {Gnm::kRenderTargetFormat8, Gnm::kRenderTargetFormat16, Gnm::kRenderTargetFormat32};

					bIsValid = IsValidRenderTargetFormat( RenderTargetFormat, ValidRenderTargetFormats, ARRAY_COUNT( ValidRenderTargetFormats ));
					bIsValid &= TargetChannelOrder == sce::Gnm::kRenderTargetChannelOrderStandard;
					break;
				}
			case sce::Gnm::kPsTargetOutputModeG32R32:
				{
					static sce::Gnm::RenderTargetFormat ValidRenderTargetFormats[] = {Gnm::kRenderTargetFormat8_8, Gnm::kRenderTargetFormat16_16, Gnm::kRenderTargetFormat32_32};

					bIsValid = IsValidRenderTargetFormat( RenderTargetFormat, ValidRenderTargetFormats, ARRAY_COUNT( ValidRenderTargetFormats ));
					bIsValid &= ( TargetChannelOrder == sce::Gnm::kRenderTargetChannelOrderStandard ) ||
								( TargetChannelOrder == sce::Gnm::kRenderTargetChannelOrderReversed );
					break;
				}
			case sce::Gnm::kPsTargetOutputModeA32R32:
				{
					static sce::Gnm::RenderTargetFormat ValidRenderTargetFormats[] = {Gnm::kRenderTargetFormat8, Gnm::kRenderTargetFormat16, Gnm::kRenderTargetFormat32,
																			Gnm::kRenderTargetFormat8_8, Gnm::kRenderTargetFormat16_16, Gnm::kRenderTargetFormat32_32};

					bIsValid = IsValidRenderTargetFormat( RenderTargetFormat, ValidRenderTargetFormats, ARRAY_COUNT( ValidRenderTargetFormats ));
					bIsValid &= ( TargetChannelOrder == sce::Gnm::kRenderTargetChannelOrderStandard ) ||
								( TargetChannelOrder == sce::Gnm::kRenderTargetChannelOrderReversed ) ||
								( TargetChannelOrder == sce::Gnm::kRenderTargetChannelOrderAltReversed ) ;
					break;
				}
			case sce::Gnm::kPsTargetOutputModeA16B16G16R16Float:
				{
					if( TargetChannelType == sce::Gnm::kRenderTargetChannelTypeUNorm || TargetChannelType == sce::Gnm::kRenderTargetChannelTypeSNorm )
					{
						// Only supported on 10bit channel size or lower
						static sce::Gnm::RenderTargetFormat ValidRenderTargetFormats[] = {Gnm::kRenderTargetFormat8, Gnm::kRenderTargetFormat8_8, Gnm::kRenderTargetFormat8_8_8_8,
																				Gnm::kRenderTargetFormat5_6_5, Gnm::kRenderTargetFormat1_5_5_5, Gnm::kRenderTargetFormat4_4_4_4,
																				Gnm::kRenderTargetFormat5_5_5_1,	Gnm::kRenderTargetFormat2_10_10_10, Gnm::kRenderTargetFormat10_10_10_2};
						bIsValid = IsValidRenderTargetFormat( RenderTargetFormat, ValidRenderTargetFormats, ARRAY_COUNT( ValidRenderTargetFormats ));
					}
					else if( TargetChannelType == sce::Gnm::kRenderTargetChannelTypeUInt || TargetChannelType == sce::Gnm::kRenderTargetChannelTypeSInt )
					{
						bIsValid = false;
					}
					else
					{
						static sce::Gnm::RenderTargetFormat ValidRenderTargetFormats[] = {Gnm::kRenderTargetFormat8, Gnm::kRenderTargetFormat16,
																				Gnm::kRenderTargetFormat8_8, Gnm::kRenderTargetFormat16_16,
																				Gnm::kRenderTargetFormat8_8_8_8, Gnm::kRenderTargetFormat16_16_16_16,
																				Gnm::kRenderTargetFormat5_6_5, Gnm::kRenderTargetFormat1_5_5_5, Gnm::kRenderTargetFormat4_4_4_4,
																				Gnm::kRenderTargetFormat5_5_5_1, Gnm::kRenderTargetFormat10_11_11, Gnm::kRenderTargetFormat11_11_10,
																				Gnm::kRenderTargetFormat2_10_10_10, Gnm::kRenderTargetFormat10_10_10_2};
						bIsValid = IsValidRenderTargetFormat( RenderTargetFormat, ValidRenderTargetFormats, ARRAY_COUNT( ValidRenderTargetFormats ));
					}
					break;
				}
			case sce::Gnm::kPsTargetOutputModeA16B16G16R16Unorm:
				{
					static sce::Gnm::RenderTargetFormat ValidRenderTargetFormats[] = {Gnm::kRenderTargetFormat16, Gnm::kRenderTargetFormat16_16, Gnm::kRenderTargetFormat16_16_16_16};

					bIsValid = IsValidRenderTargetFormat( RenderTargetFormat, ValidRenderTargetFormats, ARRAY_COUNT( ValidRenderTargetFormats ));
					bIsValid &= TargetChannelType == sce::Gnm::kRenderTargetChannelTypeUNorm;
					break;
				}
			case sce::Gnm::kPsTargetOutputModeA16B16G16R16Snorm:
				{
					static sce::Gnm::RenderTargetFormat ValidRenderTargetFormats[] = {Gnm::kRenderTargetFormat16, Gnm::kRenderTargetFormat16_16, Gnm::kRenderTargetFormat16_16_16_16};

					bIsValid = IsValidRenderTargetFormat( RenderTargetFormat, ValidRenderTargetFormats, ARRAY_COUNT( ValidRenderTargetFormats ));
					bIsValid &= TargetChannelType == sce::Gnm::kRenderTargetChannelTypeSNorm;
					break;
				}
			case sce::Gnm::kPsTargetOutputModeA16B16G16R16Uint:
				{
					static sce::Gnm::RenderTargetFormat ValidRenderTargetFormats[] = {Gnm::kRenderTargetFormat8, Gnm::kRenderTargetFormat16,
																			Gnm::kRenderTargetFormat8_8, Gnm::kRenderTargetFormat16_16,
																			Gnm::kRenderTargetFormat8_8_8_8, Gnm::kRenderTargetFormat16_16_16_16,
																			Gnm::kRenderTargetFormat2_10_10_10, Gnm::kRenderTargetFormat10_10_10_2};

					bIsValid = IsValidRenderTargetFormat( RenderTargetFormat, ValidRenderTargetFormats, ARRAY_COUNT( ValidRenderTargetFormats ));
					bIsValid &= TargetChannelType == sce::Gnm::kRenderTargetChannelTypeUInt;
					break;
				}
			case sce::Gnm::kPsTargetOutputModeA16B16G16R16Sint:
				{
					static sce::Gnm::RenderTargetFormat ValidRenderTargetFormats[] = {Gnm::kRenderTargetFormat8, Gnm::kRenderTargetFormat16,
																			Gnm::kRenderTargetFormat8_8, Gnm::kRenderTargetFormat16_16,
																			Gnm::kRenderTargetFormat8_8_8_8, Gnm::kRenderTargetFormat16_16_16_16};

					bIsValid = IsValidRenderTargetFormat( RenderTargetFormat, ValidRenderTargetFormats, ARRAY_COUNT( ValidRenderTargetFormats ));
					bIsValid &= TargetChannelType == sce::Gnm::kRenderTargetChannelTypeSInt;
					break;
				}
			case sce::Gnm::kPsTargetOutputModeA32B32G32R32:
				{
					if( TargetChannelType == sce::Gnm::kRenderTargetChannelTypeUNorm || TargetChannelType == sce::Gnm::kRenderTargetChannelTypeSNorm )
					{
						// Only supported on 16bit channel size
						static sce::Gnm::RenderTargetFormat ValidRenderTargetFormats[] = {Gnm::kRenderTargetFormat16, Gnm::kRenderTargetFormat16_16, Gnm::kRenderTargetFormat16_16_16_16};
						bIsValid = IsValidRenderTargetFormat( RenderTargetFormat, ValidRenderTargetFormats, ARRAY_COUNT( ValidRenderTargetFormats ));
					}
					else
					{
						static sce::Gnm::RenderTargetFormat ValidRenderTargetFormats[] = {Gnm::kRenderTargetFormat8, Gnm::kRenderTargetFormat16, Gnm::kRenderTargetFormat32,
																				Gnm::kRenderTargetFormat8_8, Gnm::kRenderTargetFormat16_16, Gnm::kRenderTargetFormat32_32,
																				Gnm::kRenderTargetFormat8_8_8_8, Gnm::kRenderTargetFormat16_16_16_16, Gnm::kRenderTargetFormat32_32_32_32,
																				Gnm::kRenderTargetFormat2_10_10_10, Gnm::kRenderTargetFormat10_10_10_2};
						bIsValid = IsValidRenderTargetFormat( RenderTargetFormat, ValidRenderTargetFormats, ARRAY_COUNT( ValidRenderTargetFormats ));
					}
					break;
				}
			}

			checkf(bIsValid, TEXT("Incompatible surface format [%s] with PS output [%s] detected for render target slot %d.\nThe shader must indicate the correct output format in ModifyCompilationEnvironment with OutEnvironment.SetRenderTargetOutputFormat\n"), *GetRenderTargetFormatString(RenderTargetDataFormat.getRenderTargetFormat()), *GetTargetOutputModeString(TargetOutputMode), RenderTargetIndex);

		}
	}
}

#endif

/**
 * Initializes the static variables, if necessary.
 */
void FGnmGPUTiming::PlatformStaticInitialize(void* UserData)
{
	// Are the static variables initialized?
	check( !GAreGlobalsInitialized );

	// Get the GPU timestamp frequency.
	GTimingFrequency = SCE_GNM_GPU_CORE_CLOCK_FREQUENCY;
}

/**
 * Initializes all Gnm resources and if necessary, the static variables.
 */
void FGnmGPUTiming::Initialize()
{
	StaticInitialize(NULL, PlatformStaticInitialize);

	bIsTiming = false;

	// Now initialize the queries for this timing object.
	if ( GIsSupported )
	{
		StartTimestamp = FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Event));
		EndTimestamp = FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Event));

		// Initialize to 0 so we can tell when the GPU has written to it (completed the query)
		*((uint64*)StartTimestamp.GetPointer()) = 0;
		*((uint64*)EndTimestamp.GetPointer()) = 0;
	}
}

/**
 * Releases all Gnm resources.
 */
void FGnmGPUTiming::Release()
{
	FMemBlock::Free(StartTimestamp);
	FMemBlock::Free(EndTimestamp);
}

/**
 * Start a GPU timing measurement.
 */
void FGnmGPUTiming::StartTiming()
{
	// Issue a timestamp query for the 'start' time.
	if ( GIsSupported && !bIsTiming )
	{
		GGnmManager.GetImmediateContext().GetContext().writeTimestampAtEndOfPipe(Gnm::kEopFlushCbDbCaches, StartTimestamp.GetPointer(), Gnm::kCacheActionNone);
		bIsTiming = true;
	}
}

/**
 * End a GPU timing measurement.
 * The timing for this particular measurement will be resolved at a later time by the GPU.
 */
void FGnmGPUTiming::EndTiming()
{
	// Issue a timestamp query for the 'end' time.
	if ( GIsSupported && bIsTiming )
	{
		GGnmManager.GetImmediateContext().GetContext().writeTimestampAtEndOfPipe(Gnm::kEopFlushCbDbCaches, EndTimestamp.GetPointer(), Gnm::kCacheActionNone);
		bIsTiming = false;
		bEndTimestampIssued = true;
	}
}

/**
 * Retrieves the most recently resolved timing measurement.
 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
 *
 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
 */
uint64 FGnmGPUTiming::GetTiming(bool bGetCurrentResultsAndBlock)
{
	if ( GIsSupported )
	{
		if (bGetCurrentResultsAndBlock)
		{
			// Block until the GPU has finished the last query
			while (!IsComplete())
			{
				FPlatformProcess::Sleep(0);
			}
		}

		uint32 Start = (*(uint64*)StartTimestamp.GetPointer()) & 0xFFFFFFFF;
		uint32 End = (*(uint64*)EndTimestamp.GetPointer()) & 0xFFFFFFFF;
		return End - Start;
	}

	return 0;
}

void FGnmGPUProfiler::PushEvent(const TCHAR* Name, FColor Color)
{
	GGnmManager.GetImmediateContext().PushMarker(TCHAR_TO_ANSI(Name));
	FGPUProfiler::PushEvent(Name, Color);
}

void FGnmGPUProfiler::PopEvent()
{
	if (!bCommandlistSubmitted)
	{
		GGnmManager.GetImmediateContext().PopMarker();
		FGPUProfiler::PopEvent();
	}
}

/** Start this frame of per tracking */
void FGnmEventNodeFrame::StartFrame()
{
	EventTree.Reset();
	RootEventTiming.StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FGnmEventNodeFrame::EndFrame()
{
	RootEventTiming.EndTiming();
}

float FGnmEventNodeFrame::GetRootTimingResults()
{
	double RootResult = 0.0f;
	if (RootEventTiming.IsSupported())
	{
		const uint64 GPUTiming = RootEventTiming.GetTiming(true);
		const uint64 GPUFreq = RootEventTiming.GetTimingFrequency();

		RootResult = double(GPUTiming) / double(GPUFreq);
	}

	return (float)RootResult;
}

float FGnmEventNode::GetTiming()
{
	float Result = 0;

	if (Timing.IsSupported())
	{
		const uint64 GPUTiming = Timing.GetTiming(true);
		const uint64 GPUFreq = Timing.GetTimingFrequency();

		Result = double(GPUTiming) / double(GPUFreq);
	}

	return Result;
}

void FGnmGPUProfiler::BeginTrace()
{
#if !UE_BUILD_SHIPPING && !SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS
	bool bIsPaDebugEnabled = sce::Gnm::isUserPaEnabled();

	if (bIsPaDebugEnabled)
	{
		// initialize thread trace parameters
		sce::Gnm::SqPerfCounter	counters[] =
		{
			sce::Gnm::kSqPerfCounterWaveCycles,
			sce::Gnm::kSqPerfCounterWaveReady,
			sce::Gnm::kSqPerfCounterInsts,
			sce::Gnm::kSqPerfCounterInstsValu,
			sce::Gnm::kSqPerfCounterWaitCntAny,
			sce::Gnm::kSqPerfCounterWaitCntVm,
			sce::Gnm::kSqPerfCounterWaitCntExp,
			sce::Gnm::kSqPerfCounterWaitExpAlloc,
			sce::Gnm::kSqPerfCounterWaitAny,
			sce::Gnm::kSqPerfCounterIfetch,
			sce::Gnm::kSqPerfCounterWaitIfetch,
			sce::Gnm::kSqPerfCounterSurfSyncs,
			sce::Gnm::kSqPerfCounterEvents,
			sce::Gnm::kSqPerfCounterInstsBranch,
			sce::Gnm::kSqPerfCounterValuDepStall,
			sce::Gnm::kSqPerfCounterCbranchFork
		};

		uint32_t numCounters = sizeof(counters) / sizeof(counters[0]);

		SceRazorGpuThreadTraceParams params;
		memset(&params, 0, sizeof(SceRazorGpuThreadTraceParams));
		params.sizeInBytes = sizeof(SceRazorGpuThreadTraceParams);
		memcpy(params.counters, counters, sizeof(counters));
		params.numCounters = numCounters;
		params.counterRate = SCE_RAZOR_GPU_THREAD_TRACE_COUNTER_RATE_HIGH;
		params.enableInstructionIssueTracing = false;
		params.shaderEngine0ComputeUnitIndex = 0;
		params.shaderEngine1ComputeUnitIndex = 9;

		SceRazorGpuErrorCode ret = 0;
		uint32_t *streamingCounters = params.streamingCounters;

		// ----- global streaming counters

		if (bIsPaDebugEnabled)
		{
			// TCC
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterMcWrreq);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterWrite);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterMcRdreq);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterRead);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterHit);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterMiss);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterReq);

			// TCA
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTcaPerfCounterCycle);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTcaPerfCounterBusy);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTcaPerfCounterReqTcs);

			// IA
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kIaPerfCounterIaBusy);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kIaPerfCounterIaDmaReturn);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kIaPerfCounterIaStalled);

			// ----- SE streaming counters

			// SX
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterPaIdleCycles, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterPaReq, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterPaPos, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterClock, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterShPosStarve, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterShColorStarve, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterShPosStall, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterShColorStall, SCE_RAZOR_GPU_BROADCAST_ALL);

			// PA-SU
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kPaSuPerfCounterPaInputPrim, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kPaSuPerfCounterPasxReq, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kPaSuPerfCounterPaInputEndOfPacket, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kPaSuPerfCounterPaInputNullPrim, SCE_RAZOR_GPU_BROADCAST_ALL);

			// VGT
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kVgtPerfCounterVgtSpiVsvertSend, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kVgtPerfCounterVgtSpiVsvertEov, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kVgtPerfCounterVgtSpiVsvertStalled, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kVgtPerfCounterVgtSpiVsvertStarvedBusy, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kVgtPerfCounterVgtSpiVsvertStarvedIdle, SCE_RAZOR_GPU_BROADCAST_ALL);

			// TA
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTaPerfCounterBufferWavefronts, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTaPerfCounterImageReadWavefronts, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTaPerfCounterTaBusy, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTaPerfCounterShFifoBusy, SCE_RAZOR_GPU_BROADCAST_ALL);

			// CB
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kCbPerfCounterCcCacheHit, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kCbPerfCounterCcMcWriteRequest, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kCbPerfCounterCcMcWriteRequestsInFlight, SCE_RAZOR_GPU_BROADCAST_ALL);
			ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kCbPerfCounterCcMcReadRequest, SCE_RAZOR_GPU_BROADCAST_ALL);
		}

		check(ret == SCE_OK);

		params.numStreamingCounters = streamingCounters - params.streamingCounters;
		params.streamingCounterRate = SCE_RAZOR_GPU_THREAD_TRACE_STREAMING_COUNTER_RATE_HIGH;

		//Done initializing thread trace parameters.			

		ret = sceRazorGpuThreadTraceInit(&params);
		if (ret != SCE_OK)
		{
			UE_LOG(LogRHI, Warning, TEXT("Error initializing Razor GPU Trace! 0X%d"), ret);
		}

		FGnmCommandListContext& Context = GGnmManager.GetImmediateContext();
		ret = sceRazorGpuThreadTraceStart(&Context.GetContext().m_dcb);
		if (ret != SCE_OK)
		{
			UE_LOG(LogRHI, Warning, TEXT("Error Begining Razor GPU Trace! 0X%d"), ret);
		}
	}
	else
	{
		UE_LOG(LogRHI, Warning, TEXT("Cannot perform GPU Trace!\nPlease Enable \"Settings->Debug Settings->Graphics->PA Debug\" on the target device."));
	}
#endif//!UE_BUILD_SHIPPING
}

void FGnmGPUProfiler::EndTrace()
{
#if !UE_BUILD_SHIPPING && !SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS
	FGnmCommandListContext& Context = GGnmManager.GetImmediateContext();
	SceRazorGpuErrorCode Error = sceRazorGpuThreadTraceStop(&Context.GetContext().m_dcb);
	if (Error != SCE_OK)
	{
		UE_LOG(LogRHI, Warning, TEXT("Error Stopping Razor GPU Trace! 0X%d"), Error);
	}

	GPUTraceLabel = (uint64_t*)Context.GetContext().m_dcb.allocateFromCommandBuffer(8, Gnm::kEmbeddedDataAlignment8);
	*GPUTraceLabel = 0;
	Context.GetContext().writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)GPUTraceLabel, 1, Gnm::kCacheActionNone);
#endif//!UE_BUILD_SHIPPING
}

void FGnmGPUProfiler::FinalizeTrace()
{
#if !UE_BUILD_SHIPPING
	if (!GGPUTraceFileName.IsEmpty())
	{
		// wait for the trace to finish
		uint32_t wait = 0;
		while (*GPUTraceLabel != 1)
			++wait;

		FPS4PlatformFile* PlatformFile = (FPS4PlatformFile*)FPlatformFileManager::Get().GetPlatformFile().GetLowerLevel();
		FString NormalizedFileName = PlatformFile->NormalizeFileName(*GGPUTraceFileName);

		FGnmCommandListContext& Context = GGnmManager.GetImmediateContext();
		SceRazorGpuErrorCode Error = sceRazorGpuThreadTraceSave(TCHAR_TO_UTF8(*NormalizedFileName));
		if (Error != SCE_OK)
		{
			switch ((uint32)Error)
			{
			case SCE_RAZOR_GPU_THREAD_TRACE_INVALID_POINTER: UE_LOG(LogRHI, Warning, TEXT("GPU Trace save error!\nNULL filename given.")); break;
			case SCE_RAZOR_GPU_THREAD_TRACE_ERROR_FILE_IO: UE_LOG(LogRHI, Warning, TEXT("GPU Trace save error!\nCould not open file for writing %s."), *GGPUTraceFileName); break;
			case SCE_RAZOR_GPU_THREAD_TRACE_INSUFFICIENT_BUFFER_SIZE: UE_LOG(LogRHI, Warning, TEXT("GPU Trace save error!\nData exceeded the available buffer size(327MB)")); break;
			case SCE_RAZOR_GPU_THREAD_TRACE_DRIVER_ERROR: UE_LOG(LogRHI, Warning, TEXT("GPU Trace save error!\nThere was an error in the driver layer.")); break;
			case SCE_RAZOR_GPU_THREAD_TRACE_MEMORY_ALLOCATION_ERROR: UE_LOG(LogRHI, Warning, TEXT("GPU Trace save error!\nError when allocating memory for the trace buffer.")); break;
			case SCE_RAZOR_GPU_THREAD_TRACE_ERROR_UNINITIALIZED: UE_LOG(LogRHI, Warning, TEXT("GPU Trace save error!\nTrace was not initialized.")); break;
			case SCE_RAZOR_GPU_THREAD_TRACE_ERROR_TRACE_NOT_STARTED: UE_LOG(LogRHI, Warning, TEXT("GPU Trace save error!\nTrace was not started.")); break;
			case SCE_RAZOR_GPU_THREAD_TRACE_ERROR_TRACE_NOT_STOPPED: UE_LOG(LogRHI, Warning, TEXT("GPU Trace save error!\nTrace was not stopped.")); break;
			default: UE_LOG(LogRHI, Warning, TEXT("GPU Trace save error!\nUnknown Error."));
			};
		}

		Error = sceRazorGpuThreadTraceShutdown();
		if (Error != SCE_OK)
		{
			UE_LOG(LogRHI, Warning, TEXT("Error shutting down Razor GPU Trace! 0X%d"), Error);
		}

		GGPUTraceFileName.Empty();
	}
#endif//!UE_BUILD_SHIPPING
}

void FGnmGPUProfiler::BeginFrame()
{
	bCommandlistSubmitted = false;
	CurrentEventNode = NULL;
	check(!bTrackingEvents);
	check(!CurrentEventNodeFrame); // this should have already been cleaned up and the end of the previous frame

	// latch the bools from the game thread into our private copy
	bLatchedGProfilingGPU = GTriggerGPUProfile;
	bLatchedGProfilingGPUHitches = GTriggerGPUHitchProfile;
	if (bLatchedGProfilingGPUHitches)
	{
		bLatchedGProfilingGPU = false; // we do NOT permit an ordinary GPU profile during hitch profiles
	}

	if (bLatchedGProfilingGPU)
	{
		// Issue a bunch of GPU work at the beginning of the frame, to make sure that we are GPU bound
		// We can't isolate idle time from GPU timestamps
		//@todo PS4 - need to implement this if we get partial command buffer execution to avoid measuring GPU bubbles
		//InRHI->IssueLongGPUTask();
	}

	if (!GGPUTraceFileName.IsEmpty())
	{
		bTracing = true;
	} 

	// if we are starting a hitch profile or this frame is a gpu profile, then save off the state of the draw events
	if (bLatchedGProfilingGPU || (!bPreviousLatchedGProfilingGPUHitches && bLatchedGProfilingGPUHitches) || bTracing)
	{
		bOriginalGEmitDrawEvents = GetEmitDrawEvents();
	}

	if (bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches)
	{
		if (bLatchedGProfilingGPUHitches && GPUHitchDebounce)
		{
			// if we are doing hitches and we had a recent hitch, wait to recover
			// the reasoning is that collecting the hitch report may itself hitch the GPU
			GPUHitchDebounce--; 
		}
		else
		{
			SetEmitDrawEvents(true);  // thwart an attempt to turn this off on the game side
			bTrackingEvents = true;
			CurrentEventNodeFrame = new FGnmEventNodeFrame();
			CurrentEventNodeFrame->StartFrame();
		}
	}
	else if (bPreviousLatchedGProfilingGPUHitches)
	{
		// hitch profiler is turning off, clear history and restore draw events
		GPUHitchEventNodeFrames.Empty();
		SetEmitDrawEvents(bOriginalGEmitDrawEvents);
	}
	else if (bTracing)
	{
		SetEmitDrawEvents(true);
		BeginTrace();
	}
	bPreviousLatchedGProfilingGPUHitches = bLatchedGProfilingGPUHitches;

	if (GetEmitDrawEvents())
	{
		PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255));
	}
}

void FGnmGPUProfiler::EndFrameBeforeSubmit()
{
	if (GetEmitDrawEvents())
	{
		// Finish all open nodes
		// This is necessary because timestamps must be issued before SubmitDone(), and SubmitDone() happens in RHIEndDrawingViewport instead of RHIEndFrame
		while (CurrentEventNode)
		{
			PopEvent();
		}

		bCommandlistSubmitted = true;
	}

	// if we have a frame open, close it now.
	if (CurrentEventNodeFrame)
	{
		CurrentEventNodeFrame->EndFrame();
	}

	//Stop the trace
	if (bTracing)
	{
		EndTrace();
	}
}

void FGnmGPUProfiler::EndFrame()
{
	check(!bTrackingEvents || bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches);
	check(!bTrackingEvents || CurrentEventNodeFrame);
	if (bLatchedGProfilingGPU)
	{
		if (bTrackingEvents)
		{
			SetEmitDrawEvents(bOriginalGEmitDrawEvents);
			UE_LOG(LogRHI, Warning, TEXT(""));
			UE_LOG(LogRHI, Warning, TEXT(""));
			CurrentEventNodeFrame->DumpEventTree();
			GTriggerGPUProfile = false;
			bLatchedGProfilingGPU = false;
		}
	}
	else if (bLatchedGProfilingGPUHitches)
	{
		UE_LOG(LogRHI, Warning, TEXT("GPU hitch tracking not implemented on PS4"));
	}
	bTrackingEvents = false;
	delete CurrentEventNodeFrame;
	CurrentEventNodeFrame = NULL;

	if (bTracing)
	{
		//Should roll this up more closely with existing code and bTrackingEvents?
		SetEmitDrawEvents(bOriginalGEmitDrawEvents);

		FinalizeTrace();
		bTracing = false;
	}
}


FMemBlock GGnmMemoryCheck;
uint8 GGnmMemoryFill;
void AllocateMemoryCheckBlock(uint64 Size, uint8 Fill)
{
#if ENABLE_MEMORY_CHECKING
	// in case there was one already, free it
	FMemBlock::Free(GGnmMemoryCheck);

	// free a chunk in CPU accessible memory
	GGnmMemoryCheck = FMemBlock::Allocate(Size ? Size : 128 * 1024, Gnm::kBufferAlignment, GET_STATID(GnmMem_SystemShared), TEXT("MemCheck"));

	// fill it
	GGnmMemoryFill = Fill;
	FMemory::Memset(GGnmMemoryCheck.CPUPointer, GPS4MemoryFill, GPS4MemoryCheck.Size);
#endif
}

void ValidateMemoryCheckBlock(bool bBlockCPUUntilGPUIdle)
{
#if ENABLE_MEMORY_CHECKING
	// make sure there's a buffer to check
	if (GGnmMemoryCheck.Size > 0)
	{
		return;
	}

	// block until GPU is idle if requested
	if (bBlockCPUUntilGPUIdle)
	{
		GGnmManager.BlockCPUUntilGPUIdle();
	}

	// walk over the buffer looking for written bytes
	uint8* Buffer = (uint8*)GPS4MemoryCheck.CPUPointer;
	for (int64 Index = 0; Index < GPS4MemoryCheck.Size; Index++)
	{
		if (Buffer[Index] != GPS4MemoryFill)
		{
			UE_DEBUG_BREAK();
			break;
		}
	}
#endif
}

namespace GnmBridge {

PS4RHI_API void SetReprojectionSamplerWrapMode(sce::Gnm::WrapMode InReprojectionSamplerWrapMode)
{
	GGnmManager.SetReprojectionSamplerWrapMode(InReprojectionSamplerWrapMode);
}

PS4RHI_API void CreateSocialScreenBackBuffers()
{
	GGnmManager.CreateSocialScreenBackBuffers();
}

void ChangeOutputMode(EPS4OutputMode InMode)
{
	GGnmManager.ChangeOutputMode(InMode);
}

void ChangeSocialScreenOutputMode(EPS4SocialScreenOutputMode InMode)
{
	GGnmManager.ChangeSocialScreenOutputMode(InMode);
}

void CacheReprojectionData(GnmBridge::MorpheusDistortionData& DistortionData)
{
	GGnmManager.CacheReprojectionData(DistortionData);
}

void StartMorpheus2DVRReprojection(Morpheus2DVRReprojectionData& ReprojectionData)
{
	GGnmManager.StartMorpheus2DVRReprojection(ReprojectionData);
}

void StopMorpheus2DVRReprojection()
{
	GGnmManager.StopMorpheus2DVRReprojection();
}

EPS4OutputMode GetOutputMode()
{
	return GGnmManager.GetOutputMode();
}

EPS4SocialScreenOutputMode GetSocialScreenOutputMode()
{
	return GGnmManager.GetSocialScreenOutputMode();
}

FTexture2DRHIRef GetSocialScreenRenderTarget()
{
	return GGnmManager.GetSocialScreenRenderTarget();
}

bool ShouldRenderSocialScreenThisFrame()
{
	return GGnmManager.ShouldRenderSocialScreenThisFrame();
}

void TranslateSocialScreenOutput(FRHICommandListImmediate& RHICmdList)
{
	GGnmManager.TranslateSocialScreenOutput(RHICmdList);
}

uint64 GetLastFlipTime()
{
	return GGnmManager.GetLastFlipTime();
}

uint32 GetVideoOutPort()
{
	return GGnmManager.GetVideoOutPort();
}

void GetLastApplyReprojectionInfo(GnmBridge::MorpheusApplyReprojectionInfo& OutInfo)
{
	return GGnmManager.GetLastApplyReprojectionInfo(OutInfo);
}

} //Gnm Bridge