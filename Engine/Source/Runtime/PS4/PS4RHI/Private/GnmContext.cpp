// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GnmContext.cpp: Class to generate gnm command buffers from RHI CommandLists
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "GnmContext.h"
#include "GnmContextCommon.h"
#include "ClearReplacementShaders.h"
#include "ShaderParameterUtils.h"

#if !defined(PS4_SUPPORTS_PARALLEL_RHI_EXECUTE)
#error "PS4_SUPPORTS_PARALLEL_RHI_EXECUTE must be defined."
#endif

template<> struct TRHIShaderToEnum < FGnmVertexShader > { enum { ShaderFrequency = SF_Vertex }; };
template<> struct TRHIShaderToEnum < FGnmHullShader > { enum { ShaderFrequency = SF_Hull }; };
template<> struct TRHIShaderToEnum < FGnmDomainShader > { enum { ShaderFrequency = SF_Domain }; };
template<> struct TRHIShaderToEnum < FGnmPixelShader > { enum { ShaderFrequency = SF_Pixel }; };
template<> struct TRHIShaderToEnum < FGnmGeometryShader > { enum { ShaderFrequency = SF_Geometry }; };
template<> struct TRHIShaderToEnum < FGnmComputeShader > { enum { ShaderFrequency = SF_Compute }; };

FGnmCommandListContext::FGnmCommandListContext(bool bInIsImmediate)
	: bIsImmediate(bInIsImmediate)
	, bCurrentDepthTargetReadOnly(false)
	, CachedViewportBounds(0, 0, 0, 0)
	, CachedStereoViewportBounds(0, 0, 0, 0)
	, CachedViewportMinZ(0)
	, CachedViewportMaxZ(0)	
	, bAnySetUAVs(false)
	, bUpdateAnySetUAVs(false)
	, CachedAAEnabled(Gnm::kScanModeControlAaDisable)
	, CachedScissorEnabled(Gnm::kScanModeControlViewportScissorDisable)
	, CachedViewportScissorMinX(0)
	, CachedViewportScissorMinY(0)
	, CachedViewportScissorMaxX(0)
	, CachedViewportScissorMaxY(0)
	, CachedGenericScissorMinX(0)
	, CachedGenericScissorMinY(0)
	, CachedGenericScissorMaxX(16384)
	, CachedGenericScissorMaxY(16384)
	, CachedDepthTestEnabled(false)
	, CachedStencilTestEnabled(false)
	, CachedDepthBoundsTestEnabled(false)
	, CachedPrimitiveType(Gnm::kPrimitiveTypeNone)
	, CachedIndexSize(Gnm::kIndexSize16)
	, CachedIndexOffset(0)
	, CachedNumInstances(0)
	, CachedPolyScale(0.0f)
	, CachedPolyOffset(0.0f)
	, CachedActiveShaderStages(Gnm::kActiveShaderStagesVsPs)
	, CachedFirstInstance(0)
	, bPendingStreamsAreDirty(true)
	, MarkerStackLevel(0)	
	, bAutoFlushAfterComputeShader(true)
{
	for (int32 i = 0; i < FGnmContextCommon::MaxBoundUAVs; ++i)
	{
		BoundUAVs.Add(nullptr);
	}

	// how big should the buffer be for all shader types?
	uint32 Size = Align(MAX_GLOBAL_CONSTANT_BUFFER_SIZE, 16);

	// make a buffer for each shader type
	VSConstantBuffer = new FGnmConstantBuffer(Size);
	HSConstantBuffer = new FGnmConstantBuffer(Size);
	DSConstantBuffer = new FGnmConstantBuffer(Size);
	PSConstantBuffer = new FGnmConstantBuffer(Size);
	GSConstantBuffer = new FGnmConstantBuffer(Size);
	CSConstantBuffer = new FGnmConstantBuffer(Size);

	for (int32 Frequency = 0; Frequency < SF_NumFrequencies; ++Frequency)
	{
		DirtyUniformBuffers[Frequency] = 0;
	}

	PendingDrawPrimitiveUPVertexData = nullptr;
	PendingNumVertices = 0;
	PendingVertexDataStride = 0;

	PendingDrawPrimitiveUPIndexData = nullptr;
	PendingPrimitiveType = 0;
	PendingNumPrimitives = 0;
	PendingMinVertexIndex = 0;
	PendingIndexDataStride = 0;

	GnmContext = new FLCUE();
}

static bool DrawBufferFullCallback(Gnm::CommandBuffer* Buffer, uint32_t SizeInDwords, void* Param)
{
	uint32 UsedSize = Buffer->getSizeInBytes();
	uint32 TotalSize = 4 * Buffer->getRemainingBufferSpaceInDwords() + UsedSize;
	UE_LOG(LogRHI, Warning, TEXT("GNM Draw/Constant command buffer is full; Total size is %d, used size %d, failed while requesting %d bytes"), TotalSize, UsedSize, SizeInDwords * 4);

	//Ran out of command buffer.  Let's just submit and stall.  Better than crashing...
	GGnmManager.WaitForGPUIdleNoReset();

	return true;
}

FGnmCommandListContext::~FGnmCommandListContext()
{

}

void FGnmCommandListContext::InitContextBuffers(bool bReuseDCBMem, bool bPreserveState)
{
	if (bReuseDCBMem)
	{
		DCBAllocator.Reset();
		ResourceBufferAllocator.Reset();
	}
	else
	{
		DCBAllocator.Clear();
		ResourceBufferAllocator.Clear();
	}	

	if (bPreserveState)
	{
		GnmContext->InitPreserveState(DCBAllocator, ResourceBufferAllocator, &DrawBufferFullCallback, this);
	}
	else
	{
		// set current context	
		GnmContext->init(DCBAllocator, ResourceBufferAllocator, &DrawBufferFullCallback, this);
	}

	TempFrameAllocator.Clear();

	StartOfSubmissionTimestamp = nullptr;
	EndOfSubmissionTimestamp = nullptr;

	//add marker at beginning of next context section.
	GGnmManager.TimeSubmitOnCmdListBegin(this);
}

void FGnmCommandListContext::ClearState()
{	
	Gnm::DbRenderControl DBRenderControl;
	DBRenderControl.init();
	GnmContext->setDbRenderControl(DBRenderControl);
	
	// Original values
	CachedPrimitiveType = Gnm::kPrimitiveTypeTriList;
	CachedIndexSize = Gnm::kIndexSize16;
	CachedPrimitiveSetup.init();
	CachedIndexOffset = 0;
	CachedNumInstances = 0;
	CachedPolyScale = 0.0f;
	CachedPolyOffset = 0.0f;
	CachedActiveShaderStages = Gnm::kActiveShaderStagesVsPs;
	CachedAAEnabled = Gnm::kScanModeControlAaDisable;
	CachedScissorEnabled = Gnm::kScanModeControlViewportScissorDisable;
	CachedViewportScissorMinX = 0;
	CachedViewportScissorMinY = 0;
	CachedViewportScissorMaxX = 0;
	CachedViewportScissorMaxY = 0;
	CachedGenericScissorMinX = 0;
	CachedGenericScissorMinY = 0;
	CachedGenericScissorMaxX = 16384;
	CachedGenericScissorMaxY = 16384;
	CachedViewportBounds.Set(0, 0, 0, 0);
	CachedStereoViewportBounds.Set(0, 0, 0, 0);
	CachedViewportMinZ = 0;
	CachedViewportMaxZ = 0;
	CachedDepthStencilState.init();
	CachedStencilControl.init();
	CachedStencilOpControl.init();
	CachedRenderTargetMask = 0;
	CachedRenderTargetColorWriteMask = 0;
	CachedActiveRenderTargetMask = 0;
	CachedBlendColor = FLinearColor::White;
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		CachedBlendControlRT[Index].init();
	}
	CachedDepthClearValue = 0.0f;
	CachedDepthBoundsTestEnabled = false;
	CachedDepthBoundsMin = 0.0f;
	CachedDepthBoundsMax = 1.0f;
	bAnySetUAVs = false;
	bUpdateAnySetUAVs = false;
	CachedCbMode = Gnm::kCbModeNormal;
	bCurrentDepthTargetReadOnly = false;
	CachedEsGsVertexInfo.Init();

	CurrentDepthTarget = nullptr;
	FMemory::Memzero(CurrentRenderTargetTextures);

	CachedFirstInstance = 0;
	bPendingStreamsAreDirty = true;

	PendingDrawPrimitiveUPVertexData = nullptr;
	PendingNumVertices = 0;
	PendingVertexDataStride = 0;

	PendingDrawPrimitiveUPIndexData = nullptr;
	PendingPrimitiveType = 0;
	PendingNumPrimitives = 0;
	PendingMinVertexIndex = 0;
	PendingIndexDataStride = 0;

	// Apply them
	GnmContext->setPrimitiveType(CachedPrimitiveType);
	GnmContext->setIndexSize(CachedIndexSize);
	GnmContext->setIndexOffset(CachedIndexOffset);
	GnmContext->setNumInstances(CachedNumInstances);
	GnmContext->setInstanceStepRate(1, 1);
	GnmContext->setPolygonOffsetZFormat(Gnm::kZFormat16);
	GnmContext->setPrimitiveSetup(CachedPrimitiveSetup);
	GnmContext->setPolygonOffsetFront(CachedPolyScale, CachedPolyOffset);
	GnmContext->setPolygonOffsetBack(CachedPolyScale, CachedPolyOffset);
	GnmContext->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
	GnmContext->m_lwcue.m_drawCommandBuffer->disableGsMode();
	GnmContext->setScanModeControl(CachedAAEnabled, CachedScissorEnabled);	
	GnmContext->setDepthStencilControl(CachedDepthStencilState);
	GnmContext->setDepthBoundsRange(CachedDepthBoundsMin, CachedDepthBoundsMax);
	GnmContext->setStencilOpControl(CachedStencilOpControl);
	GnmContext->setStencil(CachedStencilControl);
	GnmContext->setRenderTargetMask(CachedRenderTargetMask);
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		GnmContext->setBlendControl(Index, CachedBlendControlRT[Index]);
	}
	GnmContext->setBlendColor(CachedBlendColor.R, CachedBlendColor.G, CachedBlendColor.B, CachedBlendColor.A);
	GnmContext->setDepthClearValue(CachedDepthClearValue);

	GnmContext->setCbControl(CachedCbMode, Gnm::kRasterOpCopy);

	GnmContext->setGenericScissor(CachedGenericScissorMinX, CachedGenericScissorMinY, CachedGenericScissorMaxX, CachedGenericScissorMaxY, Gnm::kWindowOffsetDisable);
	GnmContext->setWindowScissor(0, 0, 0, 0, Gnm::kWindowOffsetDisable);
	GnmContext->setScreenScissor(0, 0, 0, 0);
	GnmContext->setViewportScissor(0, CachedViewportScissorMinX, CachedViewportScissorMinY, CachedViewportScissorMaxX, CachedViewportScissorMaxY, Gnm::kWindowOffsetDisable);

	GnmContext->setPsShader(nullptr, nullptr);
	CurrentBoundShaderState.SafeRelease();

	TargetsNeedingEliminateFastClear.Reset();	
	UnresolvedTargets.Empty();

	ClearAllBoundUAVs();

	PixelShaderUAVToFlush.Reset();
	PixelShaderUAVToSet.Reset();

	AllocateGlobalResourceTable();	
}

void FGnmCommandListContext::InitializeStateForFrameStart()
{
	//printf("InitForFrameStart\n");
	check(IsImmediate());
	InitContextBuffers();

	GnmContext->initializeDefaultHardwareState();

	// set up some once per frame stuff
	Gnm::ClipControl Clip;
	Clip.init();
	Clip.setClipSpace(Gnm::kClipControlClipSpaceDX);
	
	// ISR multi-view writes to S_VIEWPORT_INDEX in vertex shaders
	static const auto CVarInstancedStereo = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.InstancedStereo"));
	static const auto CVarMultiView = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MultiView"));

	if (CVarInstancedStereo && 
		CVarInstancedStereo->GetValueOnAnyThread() && 
		CVarMultiView && 
		CVarMultiView->GetValueOnAnyThread())
	{
		Clip.setForceViewportIndexFromVsEnable(true);
	}
	
	GnmContext->setClipControl(Clip);
	GnmContext->setLineWidth(10);

	ClearState();
}

void FGnmCommandListContext::AllocateGlobalResourceTable()
{
	// Allocate the Global Resource table from the command buffer
	void* GlobalResourceTable = GnmContext->allocateFromCommandBuffer(SCE_GNM_SHADER_GLOBAL_TABLE_SIZE, Gnm::kEmbeddedDataAlignment4);
	GnmContext->setGlobalResourceTableAddr(GlobalResourceTable);

	// Set initial values
	GGnmManager.GraphicShaderScratchBuffer.Commit(*GnmContext);
	GGnmManager.ComputeShaderScratchBuffer.Commit(*GnmContext);
}

void FGnmCommandListContext::SetupVertexShaderMode(Gnmx::EsShader* ES, Gnmx::GsShader* GS)
{
	if (ES)
	{
		check(GS != NULL);

		// (comments taken from the geometry shader sample)
		// The HW stages used for geometry shading (when tessellation is disabled) are
		// ES -> GS -> VS -> PS and the shaders assigned to these stages are:
		// Vertex Shader --> Geometry Shader --> (Copy VS shader) --> Pixel Shader
		if( CachedActiveShaderStages != Gnm::kActiveShaderStagesEsGsVsPs )
		{
			GnmContext->setActiveShaderStages( Gnm::kActiveShaderStagesEsGsVsPs );
		CachedActiveShaderStages = Gnm::kActiveShaderStagesEsGsVsPs;
		}

		// set the vertex shader mode as export
		CurrentVertexShaderStage = Gnm::kShaderStageEs;

		if (GS->isOnChip())
		{
			// The shaders assigned to the ES and GS stages both write their data to LDS on-chip memory.
			check(ES->isOnChip() == true);
			GnmContext->setOnChipEsGsLdsLayout(ES->m_memExportVertexSizeInDWord);
			GnmContext->setOnChipGsVsLdsLayout(GS->m_memExportVertexSizeInDWord, GS->m_maxOutputVertexCount);
		}
		else
		{
			// The shaders assigned to the ES and GS stages both write their data to off-chip memory.
			// Setup the ES/GS GS/VS ring buffers
			check(ES->isOnChip() == false);

			if( CachedEsGsVertexInfo.Equals( ES, GS ) == false )
			{
			AllocateGlobalResourceTable();

				GnmContext->setEsGsRingBuffer( GGnmManager.ESGSRingBuffer.GetPointer(), Gnm::kGsRingSizeSetup4Mb, ES->m_memExportVertexSizeInDWord );
				GnmContext->setGsVsRingBuffers( GGnmManager.GSVSRingBuffer.GetPointer(), Gnm::kGsRingSizeSetup4Mb, GS->m_memExportVertexSizeInDWord, GS->m_maxOutputVertexCount );
				
				CachedEsGsVertexInfo.Set( ES, GS );
			}
		}
	}
	else
	{
		// make sure we are doing normal VS/PS rendering
		if (CachedActiveShaderStages != Gnm::kActiveShaderStagesVsPs)
		{
			GnmContext->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
			CachedActiveShaderStages = Gnm::kActiveShaderStagesVsPs;
		}

		// set the vertex shader mode as normal
		CurrentVertexShaderStage = Gnm::kShaderStageVs;
	}
}

/** Cache the info for a stream - will be bound to Gnm next draw call */
void FGnmCommandListContext::SetPendingVertexDeclaration(TRefCountPtr<FGnmVertexDeclaration> InVertexDeclaration)
{
	// remember the vertex format
	PendingVertexDeclaration = InVertexDeclaration;

	// mark as dirty
	bPendingStreamsAreDirty = true;

	// @todo gnm: really should only discard the constants if the shader state has actually changed
	// this is copied from D3D11
	bDiscardSharedConstants = true;
}

/**
* Convert a UE4 data type to Gnm type
*/
static inline Gnm::DataFormat TranslateElementType(EVertexElementType Type)
{
	switch (Type)
	{
	case VET_Float1:		return Gnm::kDataFormatR32Float;
	case VET_Float2:		return Gnm::kDataFormatR32G32Float;
	case VET_Float3:		return Gnm::kDataFormatR32G32B32Float;
	case VET_Float4:		return Gnm::kDataFormatR32G32B32A32Float;
	case VET_PackedNormal:	return Gnm::kDataFormatR8G8B8A8Unorm;
	case VET_UByte4:		return Gnm::kDataFormatR8G8B8A8Uint;
	case VET_UByte4N:		return Gnm::kDataFormatR8G8B8A8Unorm;
	case VET_Color:			return Gnm::kDataFormatB8G8R8A8Unorm;
	case VET_Short2:		return Gnm::kDataFormatR16G16Sint;
	case VET_Short4:		return Gnm::kDataFormatR16G16B16A16Sint;
	case VET_Short2N:		return Gnm::kDataFormatR16G16Snorm;			// 16 bit word normalized to (value/32767.0,value/32767.0,0,0,1)
	case VET_Half2:			return Gnm::kDataFormatR16G16Float;			// 16 bit float using 1 bit sign, 5 bit exponent, 10 bit mantissa 
	case VET_Half4:			return Gnm::kDataFormatR16G16B16A16Float;
	case VET_Short4N:		return Gnm::kDataFormatR16G16B16A16Snorm;	// 16 bit word normalized to (value/32767.0,value/32767.0,0,0,1)
	case VET_UShort2:		return Gnm::kDataFormatR16G16Uint;
	case VET_UShort4:		return Gnm::kDataFormatR16G16B16A16Uint;
	case VET_UShort2N:		return Gnm::kDataFormatR16G16Unorm;
	case VET_UShort4N:		return Gnm::kDataFormatR16G16B16A16Unorm;
	case VET_URGB10A2N:		return Gnm::kDataFormatR10G10B10A2Unorm;
	default:				return Gnm::kDataFormatInvalid;
	};
}

// Handle immediate (UP) data, which has no PendingStreams
void FGnmCommandListContext::SetupVertexBuffersUP(uint32 NumVertices, void* ImmediateVertexMemory, uint32 ImmediateVertexStride)
{
	TArray<int32> & rVSRemapTable = CurrentBoundShaderState->VSMappingTable;

	// walk the elements
	for( uint32 ElementIndex = 0; ElementIndex < PendingVertexDeclaration->Elements.Num(); ElementIndex++ )
	{
		int32 nRealIndex = rVSRemapTable[ElementIndex];
		if (nRealIndex >= 0)
		{
			// get the element
			FVertexElement& Element = PendingVertexDeclaration->Elements[ElementIndex];

			// buffer descriptor
			Gnm::Buffer VertexBuffer;

			auto VElementType = ( EVertexElementType )Element.Type;
			auto ElementType = TranslateElementType( VElementType );

			check( ImmediateVertexStride );
			check( Element.StreamIndex == 0 );

			// combine the two into a Gnm vertex buffer object
			VertexBuffer.initAsVertexBuffer(
				( uint8* )ImmediateVertexMemory + Element.Offset, // location of the element in the first vertex
				ElementType,	// data type
				ImmediateVertexStride,	// offset to next vertex
				NumVertices );	// how many vertices in this buffer?
			check( VertexBuffer.getDataFormat().m_asInt != Gnm::kDataFormatInvalid.m_asInt );

			// bind to Gnm
			GnmContext->setVertexBuffers(CurrentVertexShaderStage, nRealIndex, 1, &VertexBuffer);
		}
	}
}
		
// Handle PendingStreams data
void FGnmCommandListContext::SetupVertexBuffers(uint32 FirstInstance)
{
	TArray<int32> & rVSRemapTable = CurrentBoundShaderState->VSMappingTable;

	// walk the elements
	for( uint32 ElementIndex = 0; ElementIndex < PendingVertexDeclaration->Elements.Num(); ElementIndex++ )
	{
		int32 nRealIndex = rVSRemapTable[ElementIndex];
		if (nRealIndex >= 0)
		{
			// get the element
			FVertexElement& Element = PendingVertexDeclaration->Elements[ElementIndex];

			// get the matching stream
			FGnmPendingStream& Stream = PendingStreams[Element.StreamIndex];

			// if nothing to set, just leave it as is
			if (Stream.VertexBufferMem == 0)
			{
				continue;
			}

			// buffer descriptor
			Gnm::Buffer VertexBuffer;

			auto VElementType = ( EVertexElementType )Element.Type;
			auto ElementType = TranslateElementType( VElementType );

			// compute the proper base if we're using instancing.
			int32 FirstInstanceOffset = Element.bUseInstanceIndex ? (Element.Stride * FirstInstance) : 0;

			// location of the element in the first vertex
			void* ReadAddress = (uint8*)Stream.VertexBufferMem + Element.Offset + FirstInstanceOffset;

			// combine the two into a Gnm vertex buffer object 
			check(Stream.Stride || Stream.NumElementsOrBufferSize >= ElementType.getBytesPerElement());
			const uint32 NumElementsOrElementSizeForZeroStride = Stream.Stride ? Stream.NumElementsOrBufferSize : ElementType.getBytesPerElement();
			VertexBuffer.initAsVertexBuffer(
				ReadAddress,
				ElementType,	// data type
				Stream.Stride,	// offset to next vertex
				NumElementsOrElementSizeForZeroStride);	// how many vertices in this buffer or element size in bytes
			check( VertexBuffer.getDataFormat().m_asInt != Gnm::kDataFormatInvalid.m_asInt );

			// bind to Gnm
			GnmContext->setVertexBuffers(CurrentVertexShaderStage, nRealIndex, 1, &VertexBuffer);
		}
	}
}

/** Bind the vertex buffer addresses to the GPU for all vertices, and update constant buffer */
void FGnmCommandListContext::PrepareForDrawCallUP(uint32 NumVertices, void* ImmediateVertexMemory, uint32 ImmediateVertexStride)
{
	SCOPE_CYCLE_COUNTER(STAT_GnmDrawCallPrepareTime);

	// update streams if they are dirty	
	if (bPendingStreamsAreDirty)
	{
		SetupVertexBuffersUP( NumVertices, ImmediateVertexMemory, ImmediateVertexStride );
		bPendingStreamsAreDirty = false;
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	// optional validation
	VerifyRenderTargets(*this, CurrentBoundShaderState);
	EndShaderVerification();
}

/** Bind the vertex buffer addresses to the GPU for all vertices, and update constant buffer */
void FGnmCommandListContext::PrepareForDrawCall( uint32 FirstInstance )
{
	SCOPE_CYCLE_COUNTER(STAT_GnmDrawCallPrepareTime);

	bPendingStreamsAreDirty |= (FirstInstance != CachedFirstInstance);

	// update streams if they are dirty	
	if (bPendingStreamsAreDirty)
	{
		CachedFirstInstance = FirstInstance;
		SetupVertexBuffers( FirstInstance );
		bPendingStreamsAreDirty = false;
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	// optional validation
	VerifyRenderTargets(*this, CurrentBoundShaderState);
	EndShaderVerification();
}

void FGnmCommandListContext::PrepareForDispatch()
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetCurrentComputeShader();
	FGnmComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);

	check(ComputeShader);
	if (ComputeShader->bShaderNeedsGlobalConstantBuffer)
	{
		CommitComputeConstants();
	}

	CommitComputeResourceTables(ComputeShader);
}

void FGnmCommandListContext::CommitComputeConstants()
{
	CSConstantBuffer->CommitConstantsToDevice(*this, Gnm::kShaderStageCs, 0, true);
}

void FGnmCommandListContext::SetupMSAARenderState(FGnmSurface* RT)
{
	check(RT);

	sce::Gnm::NumSamples numSamples = RT->NumSamples;
	Gnm::DepthEqaaControl eqaaReg;
	eqaaReg.init();
	eqaaReg.setPsSampleIterationCount(Gnm::kNumSamples1);
	eqaaReg.setMaskExportNumSamples(numSamples);
	eqaaReg.setMaxAnchorSamples(numSamples);
	eqaaReg.setAlphaToMaskSamples(numSamples);
	if (numSamples > Gnm::kNumSamples1)
	{
		eqaaReg.setStaticAnchorAssociations(true);
		SetScanModeControl(Gnm::kScanModeControlAaEnable, Gnm::kScanModeControlViewportScissorEnable);
	}
	else
	{
		eqaaReg.setStaticAnchorAssociations(false);
		SetScanModeControl(Gnm::kScanModeControlAaDisable, Gnm::kScanModeControlViewportScissorEnable);
	}
	GnmContext->setDepthEqaaControl(eqaaReg);
	GnmContext->setAaDefaultSampleLocations(numSamples);
}

void FGnmCommandListContext::SetCurrentRenderTarget(uint32 RTIndex, FTextureRHIParamRef RTTexture, bool CMASKRequired, uint32 MipIndex, uint32 SliceIndex)
{
	FGnmSurface* RT = nullptr;
	if (RTTexture)
	{
		RT = &GetGnmSurfaceFromRHITexture(RTTexture);

		if (RT)
		{
			SetupMSAARenderState(RT);
		}
	}

	if (!IsRunningRHIInSeparateThread() && RT)
	{
		uint32 CurrentFrame = GGnmManager.GetFrameCount();
		const EResourceTransitionAccess CurrentAccess = RT->GetCurrentGPUAccess();
		const uint32 LastFrameWritten = RT->GetLastFrameWritten();
		const bool bReadable = CurrentAccess == EResourceTransitionAccess::EReadable;		
		const bool bNewFrame = LastFrameWritten != CurrentFrame;
		const bool bAccessValid = !bReadable ||	bNewFrame;

		ensureMsgf(bAccessValid, TEXT("RenderTarget %s is not GPU writable."), *RTTexture->GetName().ToString());

		//switch to writable state if this is the first render of the frame. or if the access invalid so we can catch subsequent errors.
		if (!bAccessValid || bReadable)
		{
			RT->SetCurrentGPUAccess(EResourceTransitionAccess::EWritable);
		}
		RT->SetDirty(true, CurrentFrame);
	}

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
	check(CVar);
	const bool bUseVSync = CVar->GetValueOnRenderThread() != 0 
		// Remove the block on vsync completion while profiling the GPU
		&& !GGnmManager.GPUProfilingData.bLatchedGProfilingGPU;

	bool bBackBufferRender = (RT == &GGnmManager.GetBackBuffer(false)->Surface);

	//solve this for parallel rendering so that we don't get duplicate calls when we do multi-threaded commandbuffer generation
	if (bUseVSync && bBackBufferRender)
	{

#if !PS4_SUPPORTS_PARALLEL_RHI_EXECUTE
		if (!GGnmManager.bGPUFlipWaitedThisFrame)
#endif
		{
			GGnmManager.WaitForBackBufferSafety(*GnmContext);
		}
#if !PS4_SUPPORTS_PARALLEL_RHI_EXECUTE
		GGnmManager.bGPUFlipWaitedThisFrame = true;
#endif
	}

	//always set to a local copy of the Gnm::RenderTarget because we will modify it for CMASK usage and other things.
	Gnm::RenderTarget* ColorBuffer = NULL;

	// if we are rendering to a mip, then we need to make a temp render target
	// also we may need to adjust the CMASK usage
	Gnm::RenderTarget FinalColorBuffer;	

	if (MipIndex > 0 || SliceIndex != -1)
	{
		// Trying to compute the baseaddress of the mip ourselves is very tricky, as there's a lot of padding and tilemode related
		// issues that make it hard to get right.  Fortunately, Sony made a function to simplify aliasing RenderTargets correctly,
		// so we can go through that function to get a correct result in all supported cases.		
		int32 RTInitStatus = FinalColorBuffer.initFromTexture(RT->Texture, MipIndex);
		check(RTInitStatus == 0);
		
		if (SliceIndex != -1)
		{
			FinalColorBuffer.setArrayView(SliceIndex, SliceIndex);
		}

		ColorBuffer = &FinalColorBuffer;
	}
	else if (RT)
	{		
		FinalColorBuffer = *RT->ColorBuffer;
		ColorBuffer = &FinalColorBuffer;
	}
	else
	{
		FMemory::Memzero(FinalColorBuffer);
	}

	if (ColorBuffer)
	{
#if USE_CMASK
		bool bWasFastCleared = TargetsNeedingEliminateFastClear.Contains(RTTexture);
		CMASKRequired |= bWasFastCleared;

		bool bHasCMASKBuffer = ColorBuffer->getCmaskAddress() != nullptr;
		
		checkf(!CMASKRequired || RTTexture->HasClearValue(), TEXT("CMASK is required, but texture \"%s\" does not have a clear color binding."), *RTTexture->GetName().GetPlainNameString());

		//checking for bHasCMASK seems silly, but certain texture formats don't allow it.
		if (CMASKRequired && bHasCMASKBuffer)
		{
			//we only enable fastclears when actually used.  assume it was always originally set to false.
			ColorBuffer->setCmaskFastClearEnable(true);
		}
#endif

		// remember the exact address for flushing
		RT->LastUsedRenderTargetAddr = ColorBuffer->getBaseAddress();

		// Buffer may have a partial view of the slices.
		int32 NumSlices = ((int32)ColorBuffer->getLastArraySliceIndex() - (int32)ColorBuffer->getBaseArraySliceIndex()) + 1;
		check(NumSlices >= 1);

		// track the buffer for flushing later
		int32 BufferSize = ColorBuffer->getSliceSizeInBytes() * NumSlices;
		AddPixelShaderUAVAddress(ColorBuffer->getBaseAddress(), BufferSize);

		// remember this target as having been bound
		// these tests cannot be performed in parallel
		if (!IsRunningRHIInSeparateThread())
		{
			UnresolvedTargets.Add(RT->ColorBuffer->getBaseAddress());
		}
	}

	// set it
	GnmContext->setRenderTarget(RTIndex, ColorBuffer);

	// remember for later
	CurrentRenderTargets[RTIndex] = FinalColorBuffer;
	CurrentRenderTargetsViewInfo[RTIndex].MipIndex = MipIndex;
	CurrentRenderTargetsViewInfo[RTIndex].ArraySliceIndex = SliceIndex;

	CurrentRenderTargetTextures[RTIndex] = RTTexture;
}

void FGnmCommandListContext::SetCurrentDepthTarget(FTextureRHIParamRef DepthTexture, bool HTILERequired, bool bReadOnly)
{
	FGnmSurface* Depth = nullptr;
	Gnm::DepthRenderTarget* DepthBuffer = nullptr;
	Gnm::DepthRenderTarget FinalDepthBuffer;	

	if (DepthTexture)
	{
		Depth = &GetGnmSurfaceFromRHITexture(DepthTexture);
		FinalDepthBuffer = *Depth->DepthBuffer;
		DepthBuffer = &FinalDepthBuffer;

		SetupMSAARenderState(Depth);
	}
	else
	{
		FMemory::Memzero(FinalDepthBuffer);
	}

	if (!IsRunningRHIInSeparateThread() && Depth)
	{		
		uint32 CurrentFrame = GGnmManager.GetFrameCount();
		const EResourceTransitionAccess CurrentAccess = Depth->GetCurrentGPUAccess();
		const uint32 LastFrameWritten = Depth->GetLastFrameWritten();
		const bool bReadable = CurrentAccess == EResourceTransitionAccess::EReadable;
		const bool bDepthWrite = !bReadOnly;
		const bool bNewFrame = LastFrameWritten != CurrentFrame;
		const bool bAccessValid = !bReadable || !bDepthWrite || bNewFrame;

		ensureMsgf(bAccessValid, TEXT("DepthTarget %s is not GPU writable."), *DepthTexture->GetName().ToString());

		//switch to writable state if this is the first render of the frame. or if the access invalid so we can catch subsequent errors.
		if (!bAccessValid || (bReadable && bDepthWrite))
		{
			Depth->SetCurrentGPUAccess(EResourceTransitionAccess::EWritable);
		}

		if (bDepthWrite)
		{
			Depth->SetDirty(true, CurrentFrame);
		}
	}


	if (DepthBuffer)
	{
#if USE_HTILE

		bool bWasFastCleared = TargetsNeedingEliminateFastClear.Contains(DepthTexture);
		HTILERequired |= bWasFastCleared;

		// Set the depth clear value that was originally used to clear this target
		// (another target may have been cleared and changed the currently set depth clear value)
		check(!HTILERequired || DepthTexture->HasClearValue());
		if (DepthTexture->HasClearValue())
		{
			SetDepthClearValue(DepthTexture->GetDepthClearValue());
		}
#endif

		if (!bReadOnly)
		{
			// remember this target as having been bound
			// these tests cannot be performed in parallel
			if (!IsRunningRHIInSeparateThread())
			{
				UnresolvedTargets.Add(Depth->DepthBuffer->getZReadAddress());
			}
		}
	}

	// set it
	GnmContext->setDepthRenderTarget(DepthBuffer);

	// remember for later
	CurrentDepthTarget = DepthTexture;
	CurrentDepthRenderTarget = FinalDepthBuffer;

	bCurrentDepthTargetReadOnly = bReadOnly;
}

void FGnmCommandListContext::SetTargetAsResolved(void* GPUAddr)
{
	// once the target is resolved, we are free to use it as a texture
	// these tests cannot be performed in parallel
	if (!IsRunningRHIInSeparateThread())
	{
		UnresolvedTargets.Remove(GPUAddr);
	}
}

void FGnmCommandListContext::SetTextureForStage(FGnmSurface& Surface, uint32 TextureIndex, Gnm::ShaderStage Stage, FName TextureName)
{

	if (!IsRunningRHIInSeparateThread())
	{
		bool bNeedsResolve = UnresolvedTargets.Contains(Surface.Texture->getBaseAddress());
		const EResourceTransitionAccess CurrentAccess = Surface.GetCurrentGPUAccess();
		const bool bAccessPass = CurrentAccess == EResourceTransitionAccess::EReadable || (CurrentAccess == EResourceTransitionAccess::ERWBarrier && !Surface.IsDirty()) || CurrentAccess == EResourceTransitionAccess::ERWSubResBarrier;

		ensure(!bNeedsResolve && !Surface.bNeedsFastClearResolve);
		ensureMsgf(bAccessPass || Surface.GetLastFrameWritten() != GGnmManager.GetFrameCount(), TEXT("ATtempting to set texture: %s that was not transitioned to readable"), *TextureName.ToString());
	}

	// set the texture into the texture register
	GnmContext->setTextures(Stage, TextureIndex, 1, Surface.Texture);
	TrackResource(sce::Shader::Binary::kInternalBufferTypeTextureSampler, TextureIndex, Stage);
}

void FGnmCommandListContext::SetTextureForStage(FTextureRHIParamRef NewTextureRHI, uint32 TextureIndex, Gnm::ShaderStage Stage)
{
	if (NewTextureRHI)
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(NewTextureRHI);

		//these tests can't currently be properly done with parallel rhi.
		if (!IsRunningRHIInSeparateThread())
		{
			bool bNeedsResolve = UnresolvedTargets.Contains(Surface.Texture->getBaseAddress());
			const EResourceTransitionAccess CurrentAccess = Surface.GetCurrentGPUAccess();
			const bool bAccessPass = CurrentAccess == EResourceTransitionAccess::EReadable || (CurrentAccess == EResourceTransitionAccess::ERWBarrier && !Surface.IsDirty()) || CurrentAccess == EResourceTransitionAccess::ERWSubResBarrier;

			ensureMsgf(!bNeedsResolve, TEXT("%s needs resolve. %p"), *NewTextureRHI->GetName().ToString(), Surface.Texture->getBaseAddress());
			ensureMsgf(!Surface.bNeedsFastClearResolve, TEXT("%s needs fast clear resolve. %p"), *NewTextureRHI->GetName().ToString(), Surface.Texture->getBaseAddress());
			ensureMsgf(bAccessPass || Surface.GetLastFrameWritten() != GGnmManager.GetFrameCount(), TEXT("ATtempting to set: %s that was not transitioned to readable"), *NewTextureRHI->GetName().ToString());
		}

		// set the texture into the texture register
		GnmContext->setTextures(Stage, TextureIndex, 1, Surface.Texture);
		TrackResource(sce::Shader::Binary::kInternalBufferTypeTextureSampler, TextureIndex, Stage);
	}
}

void FGnmCommandListContext::SetSRVForStage(FShaderResourceViewRHIParamRef SRVRHI, uint32 TextureIndex, Gnm::ShaderStage Stage)
{
	FGnmShaderResourceView* SRV = ResourceCast(SRVRHI);
	if (SRV)
	{
		ensureMsgf(FGnmContextCommon::ValidateSRVForSet(SRV), TEXT("ValidateSRVForSet failed for %s."), SRV->SourceTexture ? *SRV->SourceTexture->GetName().ToString() : TEXT("None"));
		if (!IsRunningRHIInSeparateThread())
		{
			const EResourceTransitionAccess CurrentAccess = SRV->GetResourceAccess();
			const bool bAccessPass = CurrentAccess == EResourceTransitionAccess::EReadable || (CurrentAccess == EResourceTransitionAccess::ERWBarrier && !SRV->IsResourceDirty()) || CurrentAccess == EResourceTransitionAccess::ERWSubResBarrier;

			ensureMsgf(UnresolvedTargets.Contains(SRV->Buffer.getBaseAddress()) == false, TEXT("Attempting to use a render/depth target %s (as an SRV) that was not resolved via RHICopyToResolveTarget"), SRV->SourceTexture ? *SRV->SourceTexture->GetName().ToString() : TEXT("None"));
			ensureMsgf(bAccessPass || SRV->GetLastFrameWritten() != GGnmManager.GetFrameCount(), TEXT("ATtempting to use resource %s that was not transitioned to readable."), SRV->SourceTexture ? *SRV->SourceTexture->GetName().ToString() : TEXT("None"));
		}

		if (IsValidRef(SRV->SourceTexture))
		{
			GnmContext->setTextures(Stage, TextureIndex, 1, &SRV->Texture);
		}
		else
		{

			// make sure the GPU address matches the source VB if there is one
			if (IsValidRef(SRV->SourceVertexBuffer))
			{
				void* BufferData = SRV->SourceVertexBuffer->GetCurrentBuffer();
#if PS4_USE_NEW_MULTIBUFFER == 0
				if (BufferData == nullptr)
				{
					UE_LOG(LogRHI, Warning, TEXT("Volatile Vertex buffer trying to use data before any data has been provided. Adding memory to avoid crash."));
					BufferData = SRV->SourceVertexBuffer->GetCurrentBuffer(true, true);
					FMemory::Memset(BufferData, 0, SRV->SourceVertexBuffer->GetSize());					
				}
#endif

				SRV->Buffer.setBaseAddress(BufferData);
			}
			// set the texture into the texture register
			GnmContext->setBuffers(Stage, TextureIndex, 1, &SRV->Buffer);
		}

		TrackResource(sce::Shader::Binary::kInternalBufferTypeTextureSampler, TextureIndex, Stage);
	}
}

void FGnmCommandListContext::AddPixelShaderUAVAddress(void* UAVAddress, uint32 Size)
{
	// cache it
	FPixelShaderUAVToFlushPair Pair = { UAVAddress, Size };
	PixelShaderUAVToFlush.Add(Pair);
}

void FGnmCommandListContext::AddDeferredPixelShaderUAV(int32 UAVIndex, FGnmUnorderedAccessView* UAV)
{
	PixelShaderUAVToSet.Add(FDeferredUAV(UAVIndex, UAV));
}

void FGnmCommandListContext::BindUAV(FGnmUnorderedAccessView* InUAV, int32 UAVIndex, Gnm::ShaderStage Stage, int32 CounterValue, bool bOverrideCounter)
{	
	FGnmUnorderedAccessView* BoundUAV = (FGnmUnorderedAccessView*)BoundUAVs[UAVIndex];
	bool bUpload = false;
	if (BoundUAV != InUAV)
	{		
		bUpload = true;
	}
	BoundUAVs[UAVIndex] = InUAV;

	// always need to make the set call for LCUE, but don't always need to DMA the counter value.
	if (InUAV)
	{
		if (bOverrideCounter)
		{
			InUAV->SetAndClearCounter(*this, Stage, UAVIndex, CounterValue);
		}
		else
		{
			InUAV->Set(*this, Stage, UAVIndex, bUpload);
		}
	}
	bUpdateAnySetUAVs = true;
}

void FGnmCommandListContext::ClearBoundUAV(FGnmUnorderedAccessView* InUAV)
{
	for (int i = 0; i < BoundUAVs.Num(); ++i)
	{
		if (BoundUAVs[i] == InUAV)
		{
			BoundUAVs[i] = nullptr;
		}
	}
}

void FGnmCommandListContext::ClearAllBoundUAVs()
{
	for (int i = 0; i < BoundUAVs.Num(); ++i)
	{
		BoundUAVs[i] = nullptr;		
	}
}

void FGnmCommandListContext::StoreBoundUAVs(bool bFlushGfx, bool bFlushCompute)
{
	if (bUpdateAnySetUAVs)
	{
		bAnySetUAVs = false;
		for (int i = 0; i < BoundUAVs.Num(); ++i)
		{
			if (BoundUAVs[i] != nullptr && BoundUAVs[i]->bUseUAVCounters)
			{
				bAnySetUAVs = true;				
				break;
			}
		}
		bUpdateAnySetUAVs = false;
	}

	// temporary workaround to avoid flushing when we don't need to.  What we REALLY need to know is whether any set UAV
	// is actually using the append/consume counters and then we can pare this back even more.
	if (!bAnySetUAVs)
	{
		return;
	}

	//we don't have a good mechanism to do minimal flushing for these.  To manage these conservatively causes huge performance penalites.
	//so for now we just don't support append/consume buffers in parallel rhi.
#if SUPPORTS_APPEND_CONSUME_BUFFERS
	GnmContextType& Context = GetContext();

	if (bFlushGfx)
	{
		// allocate a label in the command buffer and initialize to 0
		volatile uint64* PSLabel = (volatile uint64*)Context.allocateFromCommandBuffer(sizeof(uint64), Gnm::kEmbeddedDataAlignment8);
		*PSLabel = 0;

		// tell CP to wait until pixel shader is complete (it will write 1 to the label, then block until it's 1)
		Context.writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches,
			Gnm::kEventWriteDestMemory, (void*)(PSLabel),
			Gnm::kEventWriteSource64BitsImmediate, 0x1,
			Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru);
		Context.waitOnAddress((void*)PSLabel, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1); // tell the CP to wait until the memory has the value 1

		// tell the CP to flush the L1$ and L2$	
		Context.flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, 0, Gnm::kStallCommandBufferParserEnable);
	}

	if (bFlushCompute)
	{
		// allocate a label in the command buffer and initialize to 0
		volatile uint64* CSLabel = (volatile uint64*)Context.allocateFromCommandBuffer(sizeof(uint64), Gnm::kEmbeddedDataAlignment8);
		*CSLabel = 0;

		// tell CP to wait until pixel shader is complete (it will write 1 to the label, then block until it's 1)
		Context.writeAtEndOfPipe(Gnm::kEopCsDone,
			Gnm::kEventWriteDestMemory, (void*)(CSLabel),
			Gnm::kEventWriteSource64BitsImmediate, 0x1,
			Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru);
		Context.waitOnAddress((void*)CSLabel, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1); // tell the CP to wait until the memory has the value 1

		// tell the CP to flush the L1$ and L2$	
		Context.flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, 0, Gnm::kStallCommandBufferParserEnable);
	}

	for (int i = 0; i < BoundUAVs.Num(); ++i)
	{
		if (BoundUAVs[i])
		{			
			BoundUAVs[i]->StoreCurrentCounterValue(*this, i);
		}
	}
#endif
}

void FGnmCommandListContext::SetDeferredPixelShaderUAVs()
{
	GnmContext->setAppendConsumeCounterRange(Gnm::kShaderStagePs, 0x0, PixelShaderUAVToSet.Num() * 4);
	for (int i = 0; i < PixelShaderUAVToSet.Num(); ++i)
	{
		const FDeferredUAV& UAV = PixelShaderUAVToSet[i];
		BindUAV(UAV.UAV, UAV.UAVIndex, Gnm::kShaderStagePs);
	}
}

void FGnmCommandListContext::ClearPixelShaderUAVs()
{
	PixelShaderUAVToSet.Empty();
}

void FGnmCommandListContext::FlushBeforeComputeShader()
{
	{
		// allocate a label in the command buffer and initialize to 0
		volatile uint64* Label = (volatile uint64*)GnmContext->allocateFromCommandBuffer(sizeof(uint64), Gnm::kEmbeddedDataAlignment8);
		*Label = 0;

		// tell CP to wait until pixel shader is complete (it will write 1 to the label, then block until it's 1)
		GnmContext->writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches,
			Gnm::kEventWriteDestMemory, (void*)(Label),
			Gnm::kEventWriteSource64BitsImmediate, 0x1,
			Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru);

		GnmContext->waitOnAddress((void*)Label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1); // tell the CP to wait until the memory has the value 1

		for (int32 Index = 0; Index < PixelShaderUAVToFlush.Num(); Index++)
		{
			// flush the cache of the UAVs that a pixel shader wrote to
			GnmContext->waitForGraphicsWrites((uint32)((uint64)PixelShaderUAVToFlush[Index].Pointer >> 8), PixelShaderUAVToFlush[Index].Size >> 8,
				Gnm::kWaitTargetSlotCb0 | Gnm::kWaitTargetSlotCb1 | Gnm::kWaitTargetSlotCb2 | Gnm::kWaitTargetSlotCb3 |
				Gnm::kWaitTargetSlotCb4 | Gnm::kWaitTargetSlotCb5 | Gnm::kWaitTargetSlotCb6 | Gnm::kWaitTargetSlotCb7,
				Gnm::kCacheActionNone, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache,
				// @todo gnm: do we actually need to stall the CB?
				Gnm::kStallCommandBufferParserEnable);
		}
	}

	// tell the CP to flush the L1$ and L2$
	// @todo gnm: do we actually need to stall the CB?
	GnmContext->flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, 0, Gnm::kStallCommandBufferParserEnable);

	// no more need for these
	PixelShaderUAVToFlush.Empty();
}

void FGnmCommandListContext::FlushAfterComputeShader()
{
	// allocate a label in the command buffer and initialize to 0
	volatile uint64* Label = (volatile uint64*)GnmContext->allocateFromCommandBuffer(sizeof(uint64), Gnm::kEmbeddedDataAlignment8);
	*Label = 0;
	// tell CP to wait until compute shader is complete (it will write 1 to the label, then block until it's 1)
	GnmContext->writeAtEndOfShader(Gnm::kEosCsDone, (void*)Label, 1);
	GnmContext->waitOnAddress((void*)Label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1); // tell the CP to wait until the memory has the value 1

	// tell the CP to flush the L1$ and L2$
	// @todo gnm: do we actually need to stall the CB?
	GnmContext->flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, 0, Gnm::kStallCommandBufferParserEnable);
}

void FGnmCommandListContext::FillMemoryWithDword(FRHICommandList_RecursiveHazardous& RHICmdList, void* Address, uint32 NumDwords, uint32 Value)
{
	TShaderMapRef<FClearBufferReplacementCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
	RHICmdList.SetComputeShader(ShaderRHI);
	RHICmdList.Flush(); // always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.

	Gnm::Buffer DestinationBuffer;
	DestinationBuffer.initAsDataBuffer(Address, Gnm::kDataFormatR32Uint, NumDwords);
	DestinationBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
	uint32 BaseIndex = ComputeShader->ClearBufferRW.GetBaseIndex();
	GetContext().setRwBuffers(Gnm::kShaderStageCs, BaseIndex, 1, &DestinationBuffer);

	SetShaderValue(RHICmdList, ShaderRHI, ComputeShader->ClearBufferCSParams, FIntVector4(Value, NumDwords, 0, 0));

	DispatchComputeShader(RHICmdList, *ComputeShader, (NumDwords + Gnm::kThreadsPerWavefront - 1) / Gnm::kThreadsPerWavefront, 1, 1);
	RHICmdList.Flush(); // always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.
}

/**
*  Resolve the CMASK fast clear buffer
*
*  This function was taken from the Toolkit sample code and fixed up for UE4.
*	The main differences are we bypass the CUE and reset the states after the draw
*  so we don't trash the current state.
*/
void FGnmCommandListContext::EliminateFastClear(FTextureRHIParamRef Target)
{	
#if USE_CMASK
	// We are bypassing the cue so make sure there is enough space for the upcoming commands
	GnmContext->CommandBufferReserve();

	FGnmSurface* Surface = &GetGnmSurfaceFromRHITexture(Target);

	Gnm::RenderTarget TempRenderTarget = *Surface->ColorBuffer;
	TempRenderTarget.setCmaskFastClearEnable(true);
	Gnm::RenderTarget* RenderTarget = &TempRenderTarget;

	void* TargetAddress = RenderTarget->getBaseAddress();

	GnmContextType& Context = GetContext();

	Context.pushMarker("EliminateFastClear");

	volatile uint32* Label = (uint32*)Context.allocateFromCommandBuffer(sizeof(uint32), Gnm::kEmbeddedDataAlignment8);
	*Label = 0;

	// Bypass the CUE and write directly to the dcb to avoid overwriting the current CUE state
	sce::Gnmx::GnmxDrawCommandBuffer* DCB = &Context.m_dcb;
	DCB->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta);

	if (!Surface->GetSkipEliminate())
	{
		Gnm::BlendControl OpaqueBlending;
		OpaqueBlending.init();

		Gnm::DepthStencilControl DSC;
		DSC.init();
		DSC.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncAlways);
		DSC.setDepthEnable(true);

		Gnm::PrimitiveSetup PrimitiveSetup;
		PrimitiveSetup.init();
		PrimitiveSetup.setCullFace(sce::Gnm::kPrimitiveSetupCullFaceBack);
		PrimitiveSetup.setPolygonMode(sce::Gnm::kPrimitiveSetupPolygonModeFill, sce::Gnm::kPrimitiveSetupPolygonModeFill);

	DCB->setRenderTarget(0, RenderTarget);
	DCB->setDepthRenderTarget( nullptr );

		//make sure to set both scissors.
		uint32 Width = RenderTarget->getWidth();
		uint32 Height = RenderTarget->getHeight();
		SetViewportUncached(0, 0, 0, Width, Height, 1.0f, 0.5f, 0.5f);
		DCB->setScreenScissor(0, 0, Width, Height);
		DCB->setWindowScissor(0, 0, Width, Height, Gnm::kWindowOffsetDisable);
		DCB->setGenericScissor(0, 0, Width, Height, Gnm::kWindowOffsetDisable);

		DCB->setBlendControl(0, OpaqueBlending); // disable blending
		DCB->setRenderTargetMask(0x0000000F); // enable MRT0 output only	

		Context.setCbControl(Gnm::kCbModeEliminateFastClear, Gnm::kRasterOpCopy);

		DCB->setDepthStencilControl(DSC);
		DCB->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
		DCB->setPrimitiveSetup(PrimitiveSetup);

		// draw full screen quad using the provided color-clear shader.
		TShaderMapRef<FClearReplacementVS> ClearVertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FVertexShaderRHIParamRef VertexShaderRHI = ClearVertexShader->GetVertexShader();
		FGnmVertexShader* VertexShader = ResourceCast(VertexShaderRHI);
		Gnmx::VsShader* VSB = VertexShader->Shader;

		DCB->setPsShader(NULL);
		DCB->setVsShader(&VSB->m_vsStageRegisters, 0);
		DCB->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);

		DCB->disableGsMode();
		DCB->setIndexSize(Gnm::kIndexSize16, Gnm::kCachePolicyBypass);

		DCB->setNumInstances(1);

		PRE_DRAW;
		DCB->drawIndexAuto(3);
		POST_DRAW;
	}

	// Wait for the draw to be finished, and flush the Cb/Db caches:
	DCB->writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches,
		Gnm::kEventWriteDestMemory, (void*)Label,
		Gnm::kEventWriteSource32BitsImmediate, 1,
		Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
	DCB->waitOnAddress((void*)Label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);

	RestoreCachedDCBState();

	if (!Surface->GetSkipEliminate())
	{
		//after an eliminatefastclear we can safely remove the target to avoid duplicate fastclears.	
		//if the target is cleared again, it will be re-added to this list.
		TargetsNeedingEliminateFastClear.Remove(Target);
		if (!IsRunningRHIInSeparateThread())
		{
			Surface->bNeedsFastClearResolve = false;
		}
	}

	Context.popMarker();
#endif
}


void FGnmCommandListContext::FlushCBMetaData()
{
	GnmContextType& Context = GetContext();

	sce::Gnmx::GnmxDrawCommandBuffer* DCB = &Context.m_dcb;

	volatile uint32* Label = (uint32*)Context.allocateFromCommandBuffer(sizeof(uint32), Gnm::kEmbeddedDataAlignment8);
	*Label = 0;

	// Wait for the draw to be finished, and flush the Cb/Db caches:
	DCB->writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches,
		Gnm::kEventWriteDestMemory, (void*)Label,
		Gnm::kEventWriteSource32BitsImmediate, 1,
		Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru);
	DCB->waitOnAddress((void*)Label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);
	DCB->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta);
}

void FGnmCommandListContext::DecompressFmaskSurface(FTextureRHIParamRef Target)
{
	// We are bypassing the cue so make sure there is enough space for the upcoming commands
	GnmContext->CommandBufferReserve();

	FGnmSurface* Surface = &GetGnmSurfaceFromRHITexture(Target);

	Gnm::RenderTarget* RenderTarget = Surface->ColorBuffer;

	GnmContextType& Context = GetContext();

	Context.pushMarker("Decompress FMask Surface");

	// Bypass the CUE and write directly to the dcb to avoid overwriting the current CUE state
	sce::Gnmx::GnmxDrawCommandBuffer* DCB = &Context.m_dcb;

	SCE_GNM_ASSERT_MSG(DCB, "DCB must not be NULL.");
	SCE_GNM_ASSERT_MSG(RenderTarget, "target must not be NULL.");
	if (!RenderTarget->getFmaskCompressionEnable())
		return; // nothing to do.
	SCE_GNM_ASSERT_MSG(RenderTarget->getFmaskAddress() != nullptr, "Compressed surface must have an FMASK surface");

	// Required sequence of events for FMASK decompression:
	// 1. Flush the CB data and metadata caches.
	// 2. Fully enable slot 0 with setRenderTargetMask(0xF).  Output to all other MRTs must be disabled.
	// 3. Bind the render target whose FMASK should be decompressed to slot 0.
	// 4. Disable blending.
	// 5. Set the CB mode to kCbModeFmaskDecompress, with a rasterOp of kRasterOpCopy.
	// 6. Bind a pixel shader that outputs to MRT slot 0. Do *NOT* use setPsShader(NULL). The shader code will not be run by this pass,
	//    but the shader state must be configured correctly. The recommended approach is to use setEmbeddedPsShader(kEmbeddedPsShaderDummy).
	// 7. Draw over the screen region(s) that should be modified.  Usually, this will just be a large rectangle the size of the surface.  Do not draw the same pixel twice.
	// 8. Flush the color cache for all the modes.

	DCB->setRenderTarget(0, RenderTarget);

	// Validation
	DCB->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta); // FLUSH_AND_INV_CB_META		// Flush the FMask cache

	uint32 Width = RenderTarget->getWidth();
	uint32 Height = RenderTarget->getHeight();
	SetViewportUncached(0, 0, 0, Width, Height, 1.0f, 0.5f, 0.5f);
	DCB->setScreenScissor(0, 0, Width, Height);
	DCB->setWindowScissor(0, 0, Width, Height, Gnm::kWindowOffsetDisable);
	DCB->setGenericScissor(0, 0, Width, Height, Gnm::kWindowOffsetDisable);

	Gnm::BlendControl BlendControl;
	BlendControl.init();
	BlendControl.setBlendEnable(false);
	DCB->setBlendControl(0, BlendControl);
	DCB->setRenderTargetMask(0x0000000F); // enable MRT0 output only
	DCB->setCbControl(Gnm::kCbModeFmaskDecompress, Gnm::kRasterOpCopy);
	Gnm::DepthStencilControl DSC;
	DSC.init();
	DSC.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncAlways);
	DSC.setDepthEnable(true);
	DCB->setDepthStencilControl(DSC);

	// draw full screen quad.
	DCB->setPsShader(nullptr);

	// Render fullscreen quad
	SCE_GNM_ASSERT_MSG(DCB, "DCB must not be NULL.");
	DCB->setEmbeddedVsShader(Gnm::kEmbeddedVsShaderFullScreen, 0);
	DCB->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
	DCB->setIndexSize(Gnm::kIndexSize16, Gnm::kCachePolicyLru);
	DCB->drawIndexAuto(3, Gnm::kDrawModifierDefault);

	// Restore CB mode to normal
	DCB->setCbControl(Gnm::kCbModeNormal, Gnm::kRasterOpCopy);

	volatile uint32* Label = (uint32*)Context.allocateFromCommandBuffer(sizeof(uint32), Gnm::kEmbeddedDataAlignment8);
	*Label = 0;

	// Flush caches again
	if (RenderTarget->getCmaskAddress() == nullptr)
	{
		// Flush the CB color cache
		DCB->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbPixelData); // FLUSH_AND_INV_CB_PIXEL_DATA
	}
	DCB->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta); // FLUSH_AND_INV_CB_META										

	// Wait for the draw to be finished, and flush the Cb/Db caches:
	DCB->writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches,
		Gnm::kEventWriteDestMemory, (void*)Label,
		Gnm::kEventWriteSource32BitsImmediate, 1,
		Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
	DCB->waitOnAddress((void*)Label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);

	RestoreCachedDCBState();

	//after an eliminatefastclear we can safely remove the target to avoid duplicate fastclears.	
	//if the target is cleared again, it will be re-added to this list.
	TargetsNeedingEliminateFastClear.Remove(Target);
	if (!IsRunningRHIInSeparateThread())
	{
		Surface->bNeedsFastClearResolve = false;
	}

	Context.popMarker();
}

/**
*  Resolve the HTILE fast clear buffer
*
*  This function was taken from the Toolkit sample code and fixed up for UE4.
*	The main differences are we bypass the CUE and reset the states after the draw
*  so we don't trash the current state.
*/
void FGnmCommandListContext::DecompressDepthTarget(FTextureRHIParamRef DepthTexture)
{
#if USE_HTILE
	// We are bypassing the cue so make sure there is enough space for the upcoming commands
	GnmContext->CommandBufferReserve();

	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(DepthTexture);

	check(Surface.DepthBuffer->getHtileAccelerationEnable());
	Gnm::DepthRenderTarget* DepthTarget = Surface.DepthBuffer;
	void* ZReadAddress = DepthTarget->getZReadAddress();
	const float DepthClearValue = DepthTexture->GetDepthClearValue();

	GnmContext->pushMarker("DecompressDepth");

	volatile uint32* Label = (uint32*)GnmContext->allocateFromCommandBuffer(sizeof(uint32), Gnm::kEmbeddedDataAlignment8);
	*Label = 0;

	// Bypass the CUE and write directly to the dcb to avoid overwriting the current CUE state
	sce::Gnmx::GnmxDrawCommandBuffer* DCB = &GnmContext->m_dcb;

	SetupMSAARenderState(&Surface);

	DCB->triggerEvent(Gnm::kEventTypeFlushAndInvalidateDbMeta);

	
	DCB->setDepthRenderTarget(DepthTarget);
	uint32 Width = DepthTarget->getWidth();
	uint32 Height = DepthTarget->getHeight();
	SetViewportUncached(0, 0, 0, Width, Height, 1.0f, 0.5f, 0.5f);
	DCB->setScreenScissor(0, 0, Width, Height);
	DCB->setWindowScissor(0, 0, Width, Height, Gnm::kWindowOffsetDisable);
	DCB->setGenericScissor(0, 0, Width, Height, Gnm::kWindowOffsetDisable);

	DCB->setRenderTargetMask(0x0);

	Gnm::PrimitiveSetup PrimitiveSetup;
	PrimitiveSetup.init();
	PrimitiveSetup.setCullFace(sce::Gnm::kPrimitiveSetupCullFaceBack);
	PrimitiveSetup.setPolygonMode(sce::Gnm::kPrimitiveSetupPolygonModeFill, sce::Gnm::kPrimitiveSetupPolygonModeFill);
	DCB->setPrimitiveSetup(PrimitiveSetup);

	// Z_ENABLE = 0, STENCIL_ENABLE = 0
	DCB->setDepthStencilDisable();

	// DEPTH_COMPRESS_DISABLE = 1, STENCIL_COMPRESS_DISABLE = 1
	Gnm::DbRenderControl DBRenderControl;
	DBRenderControl.init();
	DBRenderControl.setStencilTileWriteBackPolicy(Gnm::kDbTileWriteBackPolicyCompressionForbidden);
	DBRenderControl.setDepthTileWriteBackPolicy(Gnm::kDbTileWriteBackPolicyCompressionForbidden);
	DCB->setDbRenderControl(DBRenderControl);

	// DB_RENDER_OVERRIDE.DECOMPRESS_Z_ON_FLUSH = 0
	DCB->setCbControl(Gnm::kCbModeDisable, Gnm::kRasterOpCopy);

	DCB->setDepthClearValue(DepthClearValue);

	if (CachedIndexSize != Gnm::kIndexSize16)
	{
		DCB->setIndexSize(Gnm::kIndexSize16, Gnm::kCachePolicyBypass);
	}

	// draw full screen quad
	TShaderMapRef<FClearReplacementVS> ClearVertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FVertexShaderRHIParamRef VertexShaderRHI = ClearVertexShader->GetVertexShader();
	FGnmVertexShader* VertexShader = ResourceCast(VertexShaderRHI);
	Gnmx::VsShader* VSB = VertexShader->Shader;

	DCB->setPsShader(NULL);
	DCB->setVsShader(&VSB->m_vsStageRegisters, 0);
	DCB->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);

	DCB->disableGsMode();

	DCB->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
	DCB->setNumInstances(1);

	Gnm::DepthRenderTarget DepthTargetCopy = *DepthTarget;

	const int32 SliceBase = DepthTargetCopy.getBaseArraySliceIndex();
	const int32 SliceLast = DepthTargetCopy.getLastArraySliceIndex();

	for (int32 SliceIndex = SliceBase; SliceIndex <= SliceLast; SliceIndex++)
	{
		DepthTargetCopy.setArrayView(SliceIndex, SliceIndex);
		GnmContext->setDepthRenderTarget(&DepthTargetCopy);

		PRE_DRAW;
		DCB->drawIndexAuto(3);
		POST_DRAW;
	}

	// Wait for the draw to be finished, and flush the Cb/Db caches:
	DCB->writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches,
		Gnm::kEventWriteDestMemory, (void*)Label,
		Gnm::kEventWriteSource32BitsImmediate, 1,
		Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
	//DCB->writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)Label, 1, Gnm::kCacheActionNone);
	DCB->waitOnAddress((void*)Label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);

	// Restore state
	RestoreCachedDCBState();

	TargetsNeedingEliminateFastClear.Remove(DepthTexture);
	if (!IsRunningRHIInSeparateThread())
	{
		Surface.bNeedsFastClearResolve = false;
	}

	DBRenderControl.init();
	GnmContext->setDbRenderControl(DBRenderControl);
	GnmContext->popMarker();
#endif
}

void FGnmCommandListContext::RestoreCachedDCBState()
{
	// We are bypassing the cue so make sure there is enough space for the upcoming commands
	GnmContext->CommandBufferReserve();

	sce::Gnmx::GnmxDrawCommandBuffer* DCB = &GnmContext->m_dcb;

	// Restore state

	uint32 ScreenScissorX = 0;
	uint32 ScreenScissorY = 0;

	// Restore state
	if (CurrentRenderTargetTextures[0])
	{
		Gnm::RenderTarget* ColorBuffer = &CurrentRenderTargets[0];
		DCB->setRenderTarget(0, ColorBuffer);
		ScreenScissorX = ColorBuffer->getWidth();
		ScreenScissorY = ColorBuffer->getHeight();
	}

	if (CurrentDepthTarget)
	{
		Gnm::DepthRenderTarget* DepthTarget = &CurrentDepthRenderTarget;
		DCB->setDepthRenderTarget(DepthTarget);

		ScreenScissorX = DepthTarget->getWidth();
		ScreenScissorY = DepthTarget->getHeight();
	}

	SetViewportUncached(CachedViewportBounds.X, CachedViewportBounds.Y, CachedViewportMinZ, CachedViewportBounds.Z, CachedViewportBounds.W, CachedViewportMaxZ, 1.0f, 0.0f);
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		DCB->setBlendControl(Index, CachedBlendControlRT[Index]);
	}
	DCB->setRenderTargetMask(CachedRenderTargetMask);
	DCB->setViewportScissor(0, CachedViewportScissorMinX, CachedViewportScissorMinY, CachedViewportScissorMaxX, CachedViewportScissorMaxY, Gnm::kWindowOffsetDisable);
	DCB->setGenericScissor(CachedGenericScissorMinX, CachedGenericScissorMinY, CachedGenericScissorMaxX, CachedGenericScissorMaxY, Gnm::kWindowOffsetDisable);
	DCB->setScreenScissor(0, 0, ScreenScissorX, ScreenScissorY);
	DCB->setWindowScissor(0, 0, ScreenScissorX, ScreenScissorY, Gnm::kWindowOffsetDisable);

	DCB->setCbControl(CachedCbMode, Gnm::kRasterOpCopy);

	Gnm::DepthStencilControl DepthStencilState = CachedDepthStencilState;
	DepthStencilState.setDepthBoundsEnable(CachedDepthBoundsTestEnabled);
	DCB->setDepthStencilControl(DepthStencilState);
	DCB->setDepthClearValue(CachedDepthClearValue);
	DCB->setStencil(CachedStencilControl);
	DCB->setStencilOpControl(CachedStencilOpControl);

	DCB->setPrimitiveType(CachedPrimitiveType);
	DCB->setPrimitiveSetup(CachedPrimitiveSetup);

	DCB->setNumInstances(CachedNumInstances);
	
	if (CurrentBoundShaderState)
	{
		if (CurrentBoundShaderState->PixelShader)
		{
			DCB->setPsShader(&CurrentBoundShaderState->PixelShader->Shader->m_psStageRegisters);
		}
		else
		{
			DCB->setPsShader(nullptr);
		}

		FGnmGeometryShader* const CurrentGeometryShader = CurrentBoundShaderState->GeometryShader;
		FGnmVertexShader* const CurrentVertexShader = CurrentBoundShaderState->VertexShader;

		if (CurrentGeometryShader)
		{
			check(CurrentVertexShader);

			const sce::Gnmx::GsShader* gsShader = CurrentGeometryShader->Shader;
			const sce::Gnmx::VsShader* vsShader = gsShader->getCopyShader();

			DCB->setVsShader(&vsShader->m_vsStageRegisters, CurrentBoundShaderState->FetchShaderModifier);
			Gnm::GsMaxOutputPrimitiveDwordSize RequiredGsMaxOutput = gsShader->getGsMaxOutputPrimitiveDwordSize();

			if (CurrentGeometryShader->Shader->isOnChip())
			{
				// Using OnChip LDS memory for GS
				check(CurrentVertexShader->ExportShader->isOnChip());

				uint32 LDSSizeIn512Bytes = 0;
				uint32 GSPrimsPerSubGroup = 0;
				bool bFitsInOnChipGS = Gnmx::computeOnChipGsConfiguration(&LDSSizeIn512Bytes, &GSPrimsPerSubGroup, CurrentVertexShader->ExportShader, CurrentGeometryShader->Shader, MaxLDSUsage);
				check(bFitsInOnChipGS);

				// Do what Sony do here, in LightweightGraphicsConstantUpdateEngine::setOnChipGsVsShaders
				uint16_t ESVerticesPerSubGroup = (uint16_t)((gsShader->m_inputVertexCountMinus1 + 1) * GSPrimsPerSubGroup);
				check(ESVerticesPerSubGroup <= 2047);

				DCB->enableGsModeOnChip(RequiredGsMaxOutput, ESVerticesPerSubGroup, GSPrimsPerSubGroup);
			}
			else
			{
				DCB->enableGsMode(RequiredGsMaxOutput);
			}
		}
		else
		{
			DCB->setVsShader(&CurrentBoundShaderState->VertexShader->Shader->m_vsStageRegisters, CurrentBoundShaderState->FetchShaderModifier);
		}

		DCB->setActiveShaderStages(CachedActiveShaderStages);
	}

	DCB->setIndexSize(CachedIndexSize, Gnm::kCachePolicyBypass);
}

template <class ShaderType>
void FGnmCommandListContext::SetResourcesFromTables(const ShaderType* RESTRICT Shader, Gnm::ShaderStage ShaderStage)
{
	checkSlow(Shader);

	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = Shader->ShaderResourceTable.ResourceTableBits & DirtyUniformBuffers[TRHIShaderToEnum<ShaderType>::ShaderFrequency];
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits)& (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;
		auto* Buffer = (FGnmUniformBuffer*)BoundUniformBuffers[TRHIShaderToEnum<ShaderType>::ShaderFrequency][BufferIndex].GetReference();
		check(Buffer &&
			BufferIndex < Shader->ShaderResourceTable.ResourceTableLayoutHashes.Num() &&
			Buffer->GetLayout().GetHash() == Shader->ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);

		// todo: could make this two pass: gather then set
		FGnmContextCommon::SetShaderResourcesFromBuffer_Surface(*this, ShaderStage, Buffer, Shader->ShaderResourceTable.TextureMap.GetData(), BufferIndex);
		FGnmContextCommon::SetShaderResourcesFromBuffer_SRV(*this, ShaderStage, Buffer, Shader->ShaderResourceTable.ShaderResourceViewMap.GetData(), BufferIndex);
		FGnmContextCommon::SetShaderResourcesFromBuffer_Sampler(*this, ShaderStage, Buffer, Shader->ShaderResourceTable.SamplerMap.GetData(), BufferIndex);
	}
	DirtyUniformBuffers[TRHIShaderToEnum<ShaderType>::ShaderFrequency] = 0;
}

void FGnmCommandListContext::CommitGraphicsResourceTables()
{	
	check(CurrentBoundShaderState);

	if (auto* Shader = CurrentBoundShaderState->VertexShader.GetReference())
	{
		SetResourcesFromTables(Shader, Shader->ShaderStage);
	}
	if (auto* Shader = CurrentBoundShaderState->PixelShader.GetReference())
	{
		SetResourcesFromTables(Shader, Gnm::kShaderStagePs);
	}
	if (auto* Shader = CurrentBoundShaderState->HullShader.GetReference())
	{
		SetResourcesFromTables(Shader, Gnm::kShaderStageHs);
		auto* DomainShader = CurrentBoundShaderState->DomainShader.GetReference();
		check(DomainShader);
		check(0);
		/*
		SetResourcesFromTables(DomainShader, Gnm::kShaderStageDs);
		*/
	}
	if (auto* Shader = CurrentBoundShaderState->GeometryShader.GetReference())
	{
		SetResourcesFromTables(Shader, Gnm::kShaderStageGs);
	}
}

void FGnmCommandListContext::CommitNonComputeShaderConstants()
{	
	check(CurrentBoundShaderState);

	// update constant buffers from local shadow memory, and send to GPU
	if (CurrentBoundShaderState->bShaderNeedsGlobalConstantBuffer[SF_Vertex])
	{
		VSConstantBuffer->CommitConstantsToDevice(*this, CurrentVertexShaderStage, 0, bDiscardSharedConstants);
	}
	if (CurrentBoundShaderState->bShaderNeedsGlobalConstantBuffer[SF_Pixel])
	{
		PSConstantBuffer->CommitConstantsToDevice(*this, Gnm::kShaderStagePs, 0, bDiscardSharedConstants);
	}
	if (CurrentBoundShaderState->bShaderNeedsGlobalConstantBuffer[SF_Geometry])
	{
		GSConstantBuffer->CommitConstantsToDevice(*this, Gnm::kShaderStageGs, 0, bDiscardSharedConstants);
	}
	/*
	// Skip HS/DS CB updates in cases where tessellation isn't being used
	// Note that this is *potentially* unsafe because bDiscardSharedConstants is cleared at the
	// end of the function, however we're OK for now because bDiscardSharedConstants
	// is always reset whenever bUsingTessellation changes in SetBoundShaderState()
	if (bUsingTessellation)
	{
	if (CurrentBoundShaderState->bShaderNeedsGlobalConstantBuffer[ SF_Hull ] )
	{
	HSConstantBuffer->CommitConstantsToDevice(Gnm::kShaderStageHs, 0, bDiscardSharedConstants);
	}
	if (CurrentBoundShaderState->bShaderNeedsGlobalConstantBuffer[ SF_Domain ] )
	{
	check(0);
	//DSConstantBuffer->CommitConstantsToDevice(Gnm::kShaderStageDs, 0, bDiscardSharedConstants);
	}
	}
	*/

	// don't need to discard again until next shader is set
	bDiscardSharedConstants = false;
}

void FGnmCommandListContext::CommitComputeResourceTables(FGnmComputeShader* ComputeShader)
{
	check(ComputeShader);
	SetResourcesFromTables(ComputeShader, Gnm::kShaderStageCs);
}

bool FGnmCommandListContext::SubmitCurrentCommands(uint32 MinimumCommandByes)
{
	if (IsImmediate())
	{
		const int32 CommandBytes = GnmContext->GetCurrentDCBSize();
		if (CommandBytes >= MinimumCommandByes)
		{
			GGnmManager.TimeSubmitOnCmdListEnd(this);
			GGnmManager.AddSubmission(GnmContext->PrepareCurrentCommands(StartOfSubmissionTimestamp, EndOfSubmissionTimestamp));

			//clear timestamps and add a begin stamp for the next immediate context section.
			StartOfSubmissionTimestamp = nullptr;
			EndOfSubmissionTimestamp = nullptr;			
			GGnmManager.TimeSubmitOnCmdListBegin(this);
			return true;
		}		
	}
	return false;
}

static int32 GPS4StallsOnMarkers = 0;
static FAutoConsoleVariableRef CVarStallsOnMarkers(
	TEXT("r.PS4StallsOnMarkers"),
	GPS4StallsOnMarkers,
	TEXT("Adds Color, Depth, and compute shader write stalls around push/pop markers.  Gives perfect timings for leafnode markers.  Items will line up properly in threadtrace."),
	ECVF_RenderThreadSafe | ECVF_Cheat
	);

void FGnmCommandListContext::PushMarker(const char* DebugString)
{
	if (GPS4StallsOnMarkers)
	{
		//wait for outstanding gpu work to finish before starting the next section so we get a clean timing.
		uint64* CBDBWait = (uint64*)GnmContext->allocateFromCommandBuffer(sizeof(uint64), sce::Gnm::EmbeddedDataAlignment::kEmbeddedDataAlignment8);
		uint64* CSWait = (uint64*)GnmContext->allocateFromCommandBuffer(sizeof(uint64), sce::Gnm::EmbeddedDataAlignment::kEmbeddedDataAlignment8);
		*CBDBWait = 0;
		*CSWait = 0;
		GnmContext->writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)CBDBWait, 1, Gnm::kCacheActionNone);
		GnmContext->writeImmediateDwordAtEndOfPipe(Gnm::kEopCsDone, (void*)CSWait, 1, Gnm::kCacheActionNone);
		GnmContext->waitOnAddress(CBDBWait, 0xFFFFFFFF, sce::Gnm::WaitCompareFunc::kWaitCompareFuncNotEqual, 0);
		GnmContext->waitOnAddress(CSWait, 0xFFFFFFFF, sce::Gnm::WaitCompareFunc::kWaitCompareFuncNotEqual, 0);
	}

	GnmContext->pushMarker(DebugString);
	MarkerStackLevel++;
}

void FGnmCommandListContext::PopMarker()
{
	if (MarkerStackLevel)
	{
		if (GPS4StallsOnMarkers)
		{
			//wait for outstanding gpu work to finish before stopping the current so we get a clean timing.
			uint64* CBDBWait = (uint64*)GnmContext->allocateFromCommandBuffer(sizeof(uint64), sce::Gnm::EmbeddedDataAlignment::kEmbeddedDataAlignment8);
			uint64* CSWait = (uint64*)GnmContext->allocateFromCommandBuffer(sizeof(uint64), sce::Gnm::EmbeddedDataAlignment::kEmbeddedDataAlignment8);
			*CBDBWait = 0;
			*CSWait = 0;
			GnmContext->writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)CBDBWait, 1, Gnm::kCacheActionNone);
			GnmContext->writeImmediateDwordAtEndOfPipe(Gnm::kEopCsDone, (void*)CSWait, 1, Gnm::kCacheActionNone);
			GnmContext->waitOnAddress(CBDBWait, 0xFFFFFFFF, sce::Gnm::WaitCompareFunc::kWaitCompareFuncNotEqual, 0);
			GnmContext->waitOnAddress(CSWait, 0xFFFFFFFF, sce::Gnm::WaitCompareFunc::kWaitCompareFuncNotEqual, 0);
		}
		GnmContext->popMarker();
		MarkerStackLevel--;
	}
}

void* FGnmCommandListContext::AllocateFromTempFrameBuffer(uint32 Size)
{
	//if the allocation is small enough use the local allocator that doesn't require a mutex
	if (Size <= TempFrameAllocator.GetChunkSize())
	{
		return TempFrameAllocator.Allocate(Size);
	}
	else
	{
		return GGnmManager.AllocateFromTempRingBuffer(Size);
	}
}
uint64* FGnmCommandListContext::AllocateBeginCmdListTimestamp()
{
	ensure(StartOfSubmissionTimestamp == nullptr);
	StartOfSubmissionTimestamp = Align((uint64*)AllocateFromTempFrameBuffer(sizeof(uint64) * 2), 8);
	return StartOfSubmissionTimestamp;
}

uint64* FGnmCommandListContext::AllocateEndCmdListTimestamp()
{
	ensure(EndOfSubmissionTimestamp == nullptr);
	EndOfSubmissionTimestamp = Align((uint64*)AllocateFromTempFrameBuffer(sizeof(uint64) * 2), 8);
	return EndOfSubmissionTimestamp;
}
