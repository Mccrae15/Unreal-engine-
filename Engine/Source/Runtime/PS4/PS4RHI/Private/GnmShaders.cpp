// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmShaders.cpp: Gnm shader RHI implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "Serialization/MemoryReader.h"

#if !defined(PS4_SUPPORTS_PARALLEL_RHI_EXECUTE)
#error "PS4_SUPPORTS_PARALLEL_RHI_EXECUTE must be defined."
#endif

#include "GnmxPublic.h"

#if ENABLE_GPU_DEBUGGER
#include <gpu_debugger.h>
#endif

template<typename BaseResourceType, typename ShaderType>
TGnmBaseShader<BaseResourceType, ShaderType>::TGnmBaseShader() :
	Shader(nullptr),
	bShaderNeedsGlobalConstantBuffer(false)
{
}

/** Initialization constructor. */
template<typename BaseResourceType, typename ShaderType>
void TGnmBaseShader<BaseResourceType, ShaderType>::Init(const void* InCodePtr, uint32 InCodeSize, bool bInShaderNeedsGlobalConstantBuffer)
{
	check(InCodeSize || InCodePtr);

	// cache the bytecode
	check(Microcode.Num() == 0);
	Microcode.AddUninitialized(InCodeSize);
	FMemory::Memcpy(Microcode.GetData(), InCodePtr, InCodeSize);

	// if you need to save out a compiled shader, set a breakpoint here, set this to 1 in the debugger,
	// modify the shader, recompile it, and the shader will be saved out to a known location
	static int32 HACK_SaveCodeToDisk = 0;
	if (HACK_SaveCodeToDisk)
	{
		FILE* fp = fopen("/data/shader.sb", "wb");
		fwrite(Microcode.GetData(), Microcode.Num(), 1, fp);
		fclose(fp);
	}

	// initialize the program object
	if (InCodeSize)
	{
		// bShaderNeedsGlobalConstantBuffer is stored in the last byte, see PS4ShaderCompiler.cpp
		auto Status = Program.loadFromMemory(Microcode.GetData(), Microcode.Num());
		check(Status == Shader::Binary::kStatusOk);
		bShaderNeedsGlobalConstantBuffer = bInShaderNeedsGlobalConstantBuffer;
	}
}

/** Destructor */
template<typename BaseResourceType, typename ShaderType>
TGnmBaseShader<BaseResourceType, ShaderType>::~TGnmBaseShader()
{
	FMemBlock::Free(ShaderMemory);
}

template< typename TGnmxShader, typename TShader >
static void ParseShader(TShader* InShader, Gnmx::ShaderType ShaderEnum, TGnmxShader*& OutGnmxShader, TStatId Stat, LCUE::InputResourceOffsets* OutShaderOffsets = NULL)
{
	/* parse the program into a shaderInfo object */
	Gnmx::ShaderInfo ShaderInfo;
	Gnmx::parseShader(&ShaderInfo, InShader->Microcode.GetData());
	
	/* allocate video memory */
	InShader->ShaderMemory = FMemBlock::Allocate(ShaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes, EGnmMemType::GnmMem_GPU, Stat);

	/* copy the byte code into the block */
	FMemory::Memcpy(InShader->ShaderMemory.GetPointer(), ShaderInfo.m_gpuShaderCode, ShaderInfo.m_gpuShaderCodeSize);
	
	/* set the shader object pointer */
	OutGnmxShader = (TGnmxShader*)ShaderInfo.m_shaderStruct;

	/* patch the shader object with the memory address */
	OutGnmxShader->patchShaderGpuAddress(InShader->ShaderMemory.GetPointer());
	
	Gnm::ShaderStage ShaderStage;
	switch (ShaderEnum)
	{
	case Gnmx::kComputeShader:	ShaderStage = Gnm::kShaderStageCs; break;
	case Gnmx::kPixelShader:	ShaderStage = Gnm::kShaderStagePs; break;
	case Gnmx::kVertexShader:	ShaderStage = Gnm::kShaderStageVs; break;
	case Gnmx::kGeometryShader:	ShaderStage = Gnm::kShaderStageGs; break;
	case Gnmx::kExportShader:	ShaderStage = Gnm::kShaderStageEs; break;
	case Gnmx::kHullShader:		ShaderStage = Gnm::kShaderStageHs; break;
	case Gnmx::kLocalShader:	ShaderStage = Gnm::kShaderStageLs; break;
	default:					ShaderStage = Gnm::kShaderStagePs; check(0); break;
	}
	LCUE::generateInputResourceOffsetTable(
		OutShaderOffsets ? OutShaderOffsets : &InShader->ShaderOffsets,
		ShaderStage,
		OutGnmxShader);

	//@todo-rco: Handle scratch memory?
	checkf(OutGnmxShader->m_common.m_scratchSizeInDWPerThread == 0, TEXT("Shader requires scratch memory; make sure you don't have a PSSL code gen issue"));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (InShader->ShaderName.Len() > 0)
	{
		sce::Gnm::registerResource(nullptr, GGnmManager.GetOwnerHandle(), InShader->ShaderMemory.GetPointer(), ShaderInfo.m_gpuShaderCodeSize, TCHAR_TO_ANSI(*InShader->ShaderName), sce::Gnm::kResourceTypeShaderBaseAddress, 0);
	}
#endif
}

template <typename TShader>
inline static void SetupOptionalShaderData(FShaderCodeReader& ShaderCode, TShader* Shader)
{
#if ENABLE_OPTIONAL_SHADER_INPUT_OUTPUT_INFO
	Shader->ShaderName = ShaderCode.FindOptionalData('n');
	int32 DebugInputAttributesSize = 0;
	int32 DebugOutputAttributesSize = 0;
	const uint8* DebugInputAttributesData = ShaderCode.FindOptionalDataAndSize('i', DebugInputAttributesSize);
	const uint8* DebugOutputAttributesData = ShaderCode.FindOptionalDataAndSize('o', DebugOutputAttributesSize);
	if (DebugInputAttributesData && DebugInputAttributesSize > 0)
	{
		FBufferReader Ar((void*)DebugInputAttributesData, DebugInputAttributesSize, false);
		Ar << Shader->DebugInputAttributes;
	}
	if (DebugOutputAttributesData && DebugOutputAttributesSize > 0)
	{
		FBufferReader Ar((void*)DebugOutputAttributesData, DebugOutputAttributesSize, false);
		Ar << Shader->DebugOutputAttributes;
	}
#endif
}

FGnmVertexShader::FGnmVertexShader()
	: TGnmBaseShader<FRHIVertexShader, Gnmx::VsShader>()
	, ExportShader(nullptr)
	, ShaderStage(Gnm::kShaderStageCount)
{
}

FGnmGeometryShader::~FGnmGeometryShader()
{
	// free non-base shader memory
	FMemBlock::Free(VertexShaderMemory);
}

void ExtractPlatformShader(const uint8* &PlatformShaderCode, uint32& PlatformShaderCodeSize, const TArray<uint8>& InShaderCode)
{
	PlatformShaderCode = InShaderCode.GetData();
	PlatformShaderCodeSize = InShaderCode.Num();

	// Both a base and neo version of the shader may be packed in the shader code.
	if( InShaderCode.Num() > sizeof( PackedShaderKey ) )
	{
		// Attempt to find the packed key
		if( PackedShaderKey == *reinterpret_cast<const int32*>( PlatformShaderCode ) )
		{
			// Unpack the shader code
			const uint8* pRawShaderData = PlatformShaderCode + sizeof( PackedShaderKey );

			const uint32 BaseLength = *reinterpret_cast<const uint32*>( pRawShaderData );
			pRawShaderData += sizeof( BaseLength );

			const uint32 NeoLength = *reinterpret_cast<const uint32*>( pRawShaderData );
			pRawShaderData += sizeof( NeoLength );

			const uint8* const BaseShaderCode = pRawShaderData;
			pRawShaderData += BaseLength;

			const uint8* const NeoShaderCode = pRawShaderData;

			// Set PlatformShaderCode based on neo or base
			if( sceKernelIsNeoMode() == 0 )
			{
				// Using base hardware
				PlatformShaderCode = BaseShaderCode;
				PlatformShaderCodeSize = BaseLength;
			}
			else
			{
				// Using neo hardware
				PlatformShaderCode = NeoShaderCode;
				PlatformShaderCodeSize = NeoLength;
			}
		}
	}
}

FVertexShaderRHIRef FGnmDynamicRHI::RHICreateVertexShader(const TArray<uint8>& InShaderCode)
{
	const uint8* PlatformShaderCode;
	uint32 PlatformShaderCodeSize;

	ExtractPlatformShader( PlatformShaderCode, PlatformShaderCodeSize, InShaderCode );
	
	// Shader code compatible with current hardware
	TArray<uint8> UnpackedShaderCode;
	UnpackedShaderCode.Reserve(PlatformShaderCodeSize);
	UnpackedShaderCode.Append(PlatformShaderCode, PlatformShaderCodeSize);

	FShaderCodeReader ShaderCode(UnpackedShaderCode);

	auto* Shader = new FGnmVertexShader();

	// @todo gnm: Some vertex shaders will be export shaders when we support Geometry shaders
	// Previously, I was looking for a byte in the Code array, but that's a hack. 
	// Once we figure out a way to know at precompile time if it's an export shader, we can
	// use that same logic here.
	FMemoryReader Ar( UnpackedShaderCode, true );
	
	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());

	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();

	const uint8* CodePtr = UnpackedShaderCode.GetData() + Offset;
	const uint32 CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	auto PackedResourceCounts = ShaderCode.FindOptionalData<FShaderCodePackedResourceCounts>();
	check(PackedResourceCounts);
	bool bShaderNeedsGlobalConstantBuffer = PackedResourceCounts->bGlobalUniformBufferUsed;
	SetupOptionalShaderData(ShaderCode, Shader);
	Shader->Init(CodePtr, CodeSize, bShaderNeedsGlobalConstantBuffer);
	
	uint8 ShaderVariant = Shader->Program.m_header->m_shaderTypeInfo.m_vsInfo.m_vertexShaderVariant;
	bool bIsExportShader = ShaderVariant == Shader::Binary::kVertexVariantExport || ShaderVariant == Shader::Binary::kVertexVariantExportOnChip;
	if (bIsExportShader)
	{
		// this is an export vertex shader
		Shader->ShaderStage = Gnm::kShaderStageEs;
		ParseShader(Shader, Gnmx::kExportShader, Shader->ExportShader, GET_STATID(STAT_Garlic_EsShader), &Shader->ExportShaderOffsets);

#if ENABLE_GPU_DEBUGGER
		static uint32 NumEShaders = 0;
		sceGpuDebuggerRegisterShaderCode( &Shader->ExportShader->m_esStageRegisters, Shader->ExportShader->m_common.m_shaderSize, TCHAR_TO_ANSI(*FString::Printf(TEXT("ExportShader %d"),NumEShaders++)) );
#endif
	}
	else
	{
		// this is a normal shader
		Shader->ShaderStage = Gnm::kShaderStageVs;
		ParseShader(Shader, Gnmx::kVertexShader, Shader->Shader, GET_STATID(STAT_Garlic_VsShader));
#if ENABLE_GPU_DEBUGGER
		static uint32 NumVShaders = 0;
		sceGpuDebuggerRegisterShaderCode( &Shader->Shader->m_vsStageRegisters, Shader->Shader->m_common.m_shaderSize, TCHAR_TO_ANSI(*FString::Printf(TEXT("VertexShader %d"),NumVShaders++)) );
#endif
	}
	return Shader;
}

FPixelShaderRHIRef FGnmDynamicRHI::RHICreatePixelShader(const TArray<uint8>& InShaderCode)
{
	FShaderCodeReader ShaderCode(InShaderCode);

	// initialize the shader
	auto* Shader = new FGnmPixelShader();
	FMemoryReader Ar( InShaderCode, true );
	
	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());

	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();

	const uint8* CodePtr = InShaderCode.GetData() + Offset;
	const uint32 CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;
	
	auto PackedResourceCounts = ShaderCode.FindOptionalData<FShaderCodePackedResourceCounts>();
	check(PackedResourceCounts);
	bool bShaderNeedsGlobalConstantBuffer = PackedResourceCounts->bGlobalUniformBufferUsed;
	SetupOptionalShaderData(ShaderCode, Shader);
	Shader->Init(CodePtr, CodeSize, bShaderNeedsGlobalConstantBuffer);

	ParseShader(Shader, Gnmx::kPixelShader, Shader->Shader, GET_STATID(STAT_Garlic_PsShader));

#if ENABLE_GPU_DEBUGGER
	static uint32 NumShaders = 0;
	sceGpuDebuggerRegisterShaderCode( &Shader->Shader->m_psStageRegisters, Shader->Shader->m_common.m_shaderSize, TCHAR_TO_ANSI(*FString::Printf(TEXT("PixelShader %d"),NumShaders++)) );
#endif
	return Shader;
}

FHullShaderRHIRef FGnmDynamicRHI::RHICreateHullShader(const TArray<uint8>& InShaderCode) 
{ 
	FShaderCodeReader ShaderCode(InShaderCode);

	// initialize the shader
	auto* Shader = new FGnmHullShader();
	FMemoryReader Ar( InShaderCode, true );

	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());

	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();

	const uint8* CodePtr = InShaderCode.GetData() + Offset;
	const uint32 CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	auto PackedResourceCounts = ShaderCode.FindOptionalData<FShaderCodePackedResourceCounts>();
	check(PackedResourceCounts);
	bool bShaderNeedsGlobalConstantBuffer = PackedResourceCounts->bGlobalUniformBufferUsed;
	SetupOptionalShaderData(ShaderCode, Shader);
	Shader->Init(CodePtr, CodeSize, bShaderNeedsGlobalConstantBuffer);
	ParseShader(Shader, Gnmx::kHullShader, Shader->Shader, GET_STATID(STAT_Garlic_HsShader));

#if ENABLE_GPU_DEBUGGER
	static uint32 NumShaders = 0;
	sceGpuDebuggerRegisterShaderCode( &Shader->Shader->m_hsStageRegisters, Shader->Shader->m_common.m_shaderSize, TCHAR_TO_ANSI(*FString::Printf(TEXT("HullShader %d"),NumShaders++)) );
#endif

	return Shader;
}

FDomainShaderRHIRef FGnmDynamicRHI::RHICreateDomainShader(const TArray<uint8>& InShaderCode) 
{ 
	FShaderCodeReader ShaderCode(InShaderCode);

	// initialize the shader
	FGnmDomainShader* Shader = new FGnmDomainShader();
	FMemoryReader Ar( InShaderCode, true );

	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());

	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();

	const uint8* CodePtr = InShaderCode.GetData() + Offset;
	const uint32 CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	auto PackedResourceCounts = ShaderCode.FindOptionalData<FShaderCodePackedResourceCounts>();
	check(PackedResourceCounts);
	bool bShaderNeedsGlobalConstantBuffer = PackedResourceCounts->bGlobalUniformBufferUsed;
	SetupOptionalShaderData(ShaderCode, Shader);
	Shader->Init(CodePtr, CodeSize, bShaderNeedsGlobalConstantBuffer);
	// @todo gnm shader
	// 	ParseShader(VsShader, kVertexShader, Shader->Shader);

	return Shader;
}

FGeometryShaderRHIRef FGnmDynamicRHI::RHICreateGeometryShader(const TArray<uint8>& InShaderCode) 
{ 
	FShaderCodeReader ShaderCode(InShaderCode);

	// initialize the shader
	auto* Shader = new FGnmGeometryShader();
	FMemoryReader Ar( InShaderCode, true );

	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());

	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();

	const uint8* CodePtr = InShaderCode.GetData() + Offset;
	const uint32 CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	auto PackedResourceCounts = ShaderCode.FindOptionalData<FShaderCodePackedResourceCounts>();
	check(PackedResourceCounts);
	bool bShaderNeedsGlobalConstantBuffer = PackedResourceCounts->bGlobalUniformBufferUsed;
	SetupOptionalShaderData(ShaderCode, Shader);
	Shader->Init(CodePtr, CodeSize, bShaderNeedsGlobalConstantBuffer);

	/* parse the program into a shaderInfo object */
	Gnmx::ShaderInfo GeometryShaderInfo;
	Gnmx::ShaderInfo VertexShaderInfo;
	Gnmx::parseGsShader(&GeometryShaderInfo, &VertexShaderInfo, Shader->Microcode.GetData());
	/* allocate video memory */
	Shader->ShaderMemory = FMemBlock::Allocate(GeometryShaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_GsShader));
	Shader->VertexShaderMemory = FMemBlock::Allocate(VertexShaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_GsVsShader));
	/* copy the byte code into the block */
	FMemory::Memcpy(Shader->ShaderMemory.GetPointer(), GeometryShaderInfo.m_gpuShaderCode, GeometryShaderInfo.m_gpuShaderCodeSize);
	FMemory::Memcpy(Shader->VertexShaderMemory.GetPointer(), VertexShaderInfo.m_gpuShaderCode, VertexShaderInfo.m_gpuShaderCodeSize);
	/* set the shader object pointer */
	Shader->Shader = (Gnmx::GsShader*)GeometryShaderInfo.m_shaderStruct;
	/* patch the shader object with the memory address */
	Shader->Shader->patchShaderGpuAddresses(Shader->ShaderMemory.GetPointer(), Shader->VertexShaderMemory.GetPointer());

	LCUE::generateInputResourceOffsetTable(
		&Shader->ShaderOffsets,
		Gnm::kShaderStageGs,
		Shader->Shader);	

#if ENABLE_GPU_DEBUGGER
	static uint32 NumShaders = 0;
	sceGpuDebuggerRegisterShaderCode( &Shader->Shader->m_gsStageRegisters, Shader->Shader->m_common.m_shaderSize, TCHAR_TO_ANSI(*FString::Printf(TEXT("GeometryShader %d"),NumShaders++)) );
#endif

	return Shader;
}

FGeometryShaderRHIRef FGnmDynamicRHI::RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& InShaderCode, const FStreamOutElementList& ElementList,
	uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	checkf(0, TEXT("Not supported yet"));
	return NULL;
}

/**
 * Check if the shader requires a scratch buffer.
 * A shader can have and set a scratch size without requiring an external buffer.
 */
template <class GnmxShaderType>
static bool ShaderRequiresScratchBuffer( GnmxShaderType* Shader )
{
	bool bScratchBufferRequired = false;

	if( Shader->m_common.m_scratchSizeInDWPerThread > 0 )
	{
		const sce::Gnm::InputUsageSlot* InputUsageSlots = Shader->getInputUsageSlotTable(); 
		for( uint32 SlotIndex = 0; SlotIndex < Shader->m_common.m_numInputUsageSlots; ++SlotIndex )
		{ 
			if( InputUsageSlots[SlotIndex].m_usageType == sce::Gnm::kShaderInputUsagePtrInternalGlobalTable )
			{
				bScratchBufferRequired = true;
				break;
			}
		}
	}

	return bScratchBufferRequired;
}

FComputeShaderRHIRef FGnmDynamicRHI::RHICreateComputeShader(const TArray<uint8>& InShaderCode) 
{ 
	const uint8* PlatformShaderCode;
	uint32 PlatformShaderCodeSize;

	ExtractPlatformShader( PlatformShaderCode, PlatformShaderCodeSize, InShaderCode );

	// Shader code compatible with current hardware
	TArray<uint8> UnpackedShaderCode;
	UnpackedShaderCode.Reserve(PlatformShaderCodeSize);
	UnpackedShaderCode.Append(PlatformShaderCode, PlatformShaderCodeSize);

	FShaderCodeReader ShaderCode(UnpackedShaderCode);

	// initialize the shader
	FGnmComputeShader* Shader = new FGnmComputeShader();
	FMemoryReader Ar( UnpackedShaderCode, true );

	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());

	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();

	const uint8* CodePtr = UnpackedShaderCode.GetData() + Offset;
	const uint32 CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	auto PackedResourceCounts = ShaderCode.FindOptionalData<FShaderCodePackedResourceCounts>();
	check(PackedResourceCounts);
	bool bShaderNeedsGlobalConstantBuffer = PackedResourceCounts->bGlobalUniformBufferUsed;
	SetupOptionalShaderData(ShaderCode, Shader);
	Shader->Init(CodePtr, CodeSize, bShaderNeedsGlobalConstantBuffer);
	ParseShader(Shader, Gnmx::kComputeShader, Shader->Shader, GET_STATID(STAT_Garlic_CsShader));

#if ENABLE_GPU_DEBUGGER
	static uint32 NumShaders = 0;
	sceGpuDebuggerRegisterShaderCode( &Shader->Shader->m_csStageRegisters, Shader->Shader->m_common.m_shaderSize, TCHAR_TO_ANSI(*FString::Printf(TEXT("ComputeShader %d"),NumShaders++)) );
#endif

	if( ShaderRequiresScratchBuffer( Shader->Shader ) )
	{
		uint32 ScratchSizeInDWPerThread = Shader->Shader->m_common.m_scratchSizeInDWPerThread;
		uint32 Num1KbyteScratchChunksPerWave = (ScratchSizeInDWPerThread + 3) / 4;
		uint32 ScratchBufferSize = FGnmManager::ScratchBufferMaxNumWaves * Num1KbyteScratchChunksPerWave * 1024;

			GGnmManager.UpdateShaderScratchBufferMaxSize( FGnmManager::EScratchBufferResourceType::SB_COMPUTE_RESOURCE, ScratchSizeInDWPerThread );
			UE_LOG(LogPS4, Log, TEXT("Compute Shader Scratch Usage: DWPerThread is %d, Num1KbyteScratchChunksPerWave is %d, scratch buffer size is %d"), ScratchSizeInDWPerThread, Num1KbyteScratchChunksPerWave, ScratchBufferSize );
		}

	return Shader;
}
template <class FGnmShaderType >
static void UpdateMaxScratchSize( FGnmShaderType* Shader, uint32& OutMaxScratchSizeInDw, uint32& OutMaxScratchSizeInDWNoBuffer )
{
	if( Shader )
	{
		uint32 ScratchSize = Shader->Shader->m_common.m_scratchSizeInDWPerThread;
		if( ScratchSize )
		{
			if( ShaderRequiresScratchBuffer( Shader->Shader ) )
			{
				OutMaxScratchSizeInDw = FMath::Max<uint32>(OutMaxScratchSizeInDw, ScratchSize);
			}
			else
			{
				OutMaxScratchSizeInDWNoBuffer = FMath::Max<uint32>(OutMaxScratchSizeInDWNoBuffer, ScratchSize);
			}
		}
	}
};

static void UpdateMaxScratchSize( FGnmVertexShader* Shader, uint32& OutMaxScratchSizeInDw, uint32& OutMaxScratchSizeInDWNoBuffer )
{
	if( Shader )
	{
		uint32 ScratchSize;
		bool bRequiresScratchBuffer;

		if (Shader->ExportShader)
		{
			ScratchSize = Shader->ExportShader->m_common.m_scratchSizeInDWPerThread;
			bRequiresScratchBuffer = ShaderRequiresScratchBuffer( Shader->ExportShader );
		}
		else
		{
			ScratchSize = Shader->Shader->m_common.m_scratchSizeInDWPerThread;
			bRequiresScratchBuffer = ShaderRequiresScratchBuffer( Shader->Shader );
		}

		if( bRequiresScratchBuffer )
		{
			OutMaxScratchSizeInDw = FMath::Max<uint32>(OutMaxScratchSizeInDw, ScratchSize);
		}
		else
		{
			OutMaxScratchSizeInDWNoBuffer = FMath::Max<uint32>(OutMaxScratchSizeInDWNoBuffer, ScratchSize);
		}
	}
};

FGnmBoundShaderState::FGnmBoundShaderState(
			FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
			FVertexShaderRHIParamRef InVertexShaderRHI,
			FPixelShaderRHIParamRef InPixelShaderRHI,
			FHullShaderRHIParamRef InHullShaderRHI,
			FDomainShaderRHIParamRef InDomainShaderRHI,
			FGeometryShaderRHIParamRef InGeometryShaderRHI)
	:	CacheLink(InVertexDeclarationRHI,InVertexShaderRHI,InPixelShaderRHI,InHullShaderRHI,InDomainShaderRHI,InGeometryShaderRHI,this)
{
	FGnmVertexDeclaration* InVertexDeclaration = FGnmDynamicRHI::ResourceCast(InVertexDeclarationRHI);
	FGnmVertexShader* InVertexShader = FGnmDynamicRHI::ResourceCast(InVertexShaderRHI);
	FGnmPixelShader* InPixelShader = FGnmDynamicRHI::ResourceCast(InPixelShaderRHI);
	FGnmHullShader* InHullShader = FGnmDynamicRHI::ResourceCast(InHullShaderRHI);
	FGnmDomainShader* InDomainShader = FGnmDynamicRHI::ResourceCast(InDomainShaderRHI);
	FGnmGeometryShader* InGeometryShader = FGnmDynamicRHI::ResourceCast(InGeometryShaderRHI);

	// handle fetch shader to the vertex shader
	if (InVertexShader && InVertexDeclaration)
	{
		//TArray<int32> RemapTable;
		VSMappingTable.AddUninitialized(InVertexDeclaration->Elements.Num());
		// by default, all elements point to dummy (0xFFFFFFFF) vertex inputs
		FMemory::Memset(VSMappingTable.GetData(), 0xFF, sizeof(uint32) * VSMappingTable.Num());

		// Shaders with instancing need to know how to grab vertex attributes, default all 16 possible inputs to VertexIndex
		Gnm::FetchShaderInstancingMode InstancingData[16] =
		{
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
			Gnm::kFetchShaderUseVertexIndex,
		};

		// iterate over all the vertex program inputs, finding the matching UE3 element
		for (uint32 InputIndex = 0; InputIndex < InVertexShader->Program.m_numInputAttributes; InputIndex++)
		{
			// get an input attribute object
			const Shader::Binary::Attribute& Attrib = InVertexShader->Program.m_inputAttributes[InputIndex];

			// handle most of the inputs (POSITION, etc)
			if (Attrib.m_psslSemantic == Shader::Binary::kSemanticUserDefined)
			{
				// find the named semantic in the lookup table
				uint8 StreamIndex = 0;
				bool bFoundSemantic = false;
				if (FCStringAnsi::Strcmp((ANSICHAR*)Attrib.getSemanticName(), "ATTRIBUTE") == 0)
				{
					StreamIndex = Attrib.m_semanticIndex;
					bFoundSemantic = true;
				}
				else
				{
					UE_LOG(LogPS4, Fatal, TEXT("Found unknown user attribute %s in a vertex program!"), ANSI_TO_TCHAR(Attrib.getSemanticName()));
				}

				// now find the vertex element that matches this input
				bFoundSemantic = false;
				for (uint32 ElementIndex = 0; ElementIndex < InVertexDeclaration->Elements.Num(); ElementIndex++)
				{
					const FVertexElement& Element = InVertexDeclaration->Elements[ElementIndex];
					// does it match?
					if (Element.AttributeIndex == StreamIndex)
					{
						check(VSMappingTable[ElementIndex] == 0xFFFFFFFF);

						// assign the mapping from UE3 element index to vertex input index
						VSMappingTable[ElementIndex] = Attrib.m_resourceIndex;

						// does this element use vertex index or the instance index?
						InstancingData[InputIndex] = Element.bUseInstanceIndex ? Gnm::kFetchShaderUseInstanceId : Gnm::kFetchShaderUseVertexIndex;

						bFoundSemantic = true;
						break;
					}
				}
				checkf(bFoundSemantic, TEXT("Failed to find vertex element for %s : %s"), ANSI_TO_TCHAR(Attrib.getName()), ANSI_TO_TCHAR(Attrib.getSemanticName()));
			}
		}

		// Preallocate space for a fetch shader
		TArray<uint8, TInlineAllocator<160>> CPUFetchShader;
		if (InVertexShader->ExportShader)
		{
			check(InGeometryShader != NULL);

			// Generate the fetch shader in Onion
			uint32 FetchShaderSize = Gnmx::computeEsFetchShaderSize(InVertexShader->ExportShader);
			CPUFetchShader.AddUninitialized(FetchShaderSize);
			Gnmx::generateEsFetchShader((uint32*)CPUFetchShader.GetData(), &FetchShaderModifier, InVertexShader->ExportShader, InstancingData, ARRAY_COUNT(InstancingData));
		}
		else
		{
			check(InGeometryShader == NULL);

			// Generate the fetch shader in Onion
			uint32 FetchShaderSize = Gnmx::computeVsFetchShaderSize(InVertexShader->Shader);
			CPUFetchShader.AddUninitialized(FetchShaderSize);
			Gnmx::generateVsFetchShader((uint32*)CPUFetchShader.GetData(), &FetchShaderModifier, InVertexShader->Shader, InstancingData, ARRAY_COUNT(InstancingData));
		}

		// Copy to Garlic
		FetchShader = FMemBlock::Allocate(CPUFetchShader.Num(), Gnm::kAlignmentOfFetchShaderInBytes, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_FetchShader));
		FMemory::Memcpy(FetchShader.GetPointer(), CPUFetchShader.GetData(), CPUFetchShader.Num());
	}
	
	// cache everything
	VertexDeclaration = InVertexDeclaration;
	VertexShader = InVertexShader;
	PixelShader = InPixelShader;
	HullShader = InHullShader;
	DomainShader = InDomainShader;
	GeometryShader = InGeometryShader;

	bShaderNeedsGlobalConstantBuffer[ SF_Vertex ] = InVertexShader->bShaderNeedsGlobalConstantBuffer;
	bShaderNeedsGlobalConstantBuffer[ SF_Hull ] = InHullShader ? InHullShader->bShaderNeedsGlobalConstantBuffer : false;
	bShaderNeedsGlobalConstantBuffer[ SF_Domain ] = InDomainShader ? InDomainShader->bShaderNeedsGlobalConstantBuffer : false;
	bShaderNeedsGlobalConstantBuffer[ SF_Pixel ] = InPixelShader ? InPixelShader->bShaderNeedsGlobalConstantBuffer : false;
	bShaderNeedsGlobalConstantBuffer[ SF_Geometry ] = InGeometryShader ? InGeometryShader->bShaderNeedsGlobalConstantBuffer : false;

	FMemory::Memcpy(StreamStrides, InVertexDeclaration->StreamStrides, sizeof(StreamStrides));

	// default to no scratch needed
	uint32 MaxScratchSizeInDWPerThread = 0;
	uint32 MaxScratchSizeInDWPerThreadNoBuffer = 0;

	UpdateMaxScratchSize( InGeometryShader, MaxScratchSizeInDWPerThread, MaxScratchSizeInDWPerThreadNoBuffer );
	UpdateMaxScratchSize( InVertexShader, MaxScratchSizeInDWPerThread, MaxScratchSizeInDWPerThreadNoBuffer );
	UpdateMaxScratchSize( InPixelShader, MaxScratchSizeInDWPerThread, MaxScratchSizeInDWPerThreadNoBuffer );
	UpdateMaxScratchSize( InHullShader, MaxScratchSizeInDWPerThread, MaxScratchSizeInDWPerThreadNoBuffer );
	UpdateMaxScratchSize( InDomainShader, MaxScratchSizeInDWPerThread, MaxScratchSizeInDWPerThreadNoBuffer );
	UpdateMaxScratchSize( InGeometryShader, MaxScratchSizeInDWPerThread, MaxScratchSizeInDWPerThreadNoBuffer );

	if( MaxScratchSizeInDWPerThread )
	{
		// A scratch buffer is required
		MaxScratchSizeInDWPerThread = FMath::Max<int32>( MaxScratchSizeInDWPerThread, MaxScratchSizeInDWPerThreadNoBuffer );
		Num1KbyteScratchChunksPerWave = (MaxScratchSizeInDWPerThread + 3 ) / 4;
		uint32 ScratchBufferSize = FGnmManager::ScratchBufferMaxNumWaves * Num1KbyteScratchChunksPerWave * 1024;

		// if we had any scratch buffer space, update the buffer now to handle the maximum required
		UE_LOG(LogPS4, Log, TEXT("Graphic Shader Scratch Usage: DWPerThread is %d, Num1KbyteScratchChunksPerWave is %d, scratch buffer size is %d"), MaxScratchSizeInDWPerThread, Num1KbyteScratchChunksPerWave, ScratchBufferSize );
		GGnmManager.UpdateShaderScratchBufferMaxSize( FGnmManager::SB_GRAPHIC_RESOURCE, MaxScratchSizeInDWPerThread );
	}
	else
	{
		// No scratch buffer required
		Num1KbyteScratchChunksPerWave = (MaxScratchSizeInDWPerThreadNoBuffer + 3 ) / 4;
	}
#if PS4_SUPPORTS_PARALLEL_RHI_EXECUTE
	CacheLink.AddToCache();
#endif
}

/**
 * Destructor
 */
FGnmBoundShaderState::~FGnmBoundShaderState()
{
#if PS4_SUPPORTS_PARALLEL_RHI_EXECUTE
	CacheLink.RemoveFromCache();
#endif
	// free the generated fetch shader
	FMemBlock::Free(FetchShader);
}

FBoundShaderStateRHIRef FGnmDynamicRHI::RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FHullShaderRHIParamRef HullShaderRHI, 
	FDomainShaderRHIParamRef DomainShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	FGeometryShaderRHIParamRef GeometryShaderRHI
	)
{
#if PS4_SUPPORTS_PARALLEL_RHI_EXECUTE
	// Check for an existing bound shader state which matches the parameters
	FBoundShaderStateRHIRef CachedBoundShaderStateLink = GetCachedBoundShaderState_Threadsafe(
		VertexDeclarationRHI,
		VertexShaderRHI,
		PixelShaderRHI,
		HullShaderRHI,
		DomainShaderRHI,
		GeometryShaderRHI
		);

	if(CachedBoundShaderStateLink.GetReference())
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink;
	}
#else
	bool bRenderThread = IsInRenderingThread();
	check(bRenderThread || IsInRHIThread());
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink;
	if (IsRunningRHIInSeparateThread() && bRenderThread)
	{
		// RHICreateBoundShaderState is not required to be threadsafe unless we support parallel execute
		// flush here if the RHI thread is active so that there is no concurrent call possible
		FScopedRHIThreadStaller StallRHIThread(FRHICommandListExecutor::GetImmediateCommandList());
		// Check for an existing bound shader state which matches the parameters
		CachedBoundShaderStateLink = GetCachedBoundShaderState(
			VertexDeclarationRHI,
			VertexShaderRHI,
			PixelShaderRHI,
			HullShaderRHI,
			DomainShaderRHI,
			GeometryShaderRHI
			);
	}
	else
	{
		// Check for an existing bound shader state which matches the parameters
		CachedBoundShaderStateLink = GetCachedBoundShaderState(
			VertexDeclarationRHI,
			VertexShaderRHI,
			PixelShaderRHI,
			HullShaderRHI,
			DomainShaderRHI,
			GeometryShaderRHI
			);
	}

	if (CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
#endif
	else
	{
		return new FGnmBoundShaderState(VertexDeclarationRHI,VertexShaderRHI,PixelShaderRHI,HullShaderRHI,DomainShaderRHI,GeometryShaderRHI);
	}
}