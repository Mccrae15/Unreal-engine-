// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmShaderCompiler.cpp: Gnm shader compiler implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

using namespace std;

#include "PS4ShaderFormat.h"
#include "ShaderCore.h"
#include "ShaderPreprocessor.h"
#include "ShaderCompilerCommon.h"

#pragma pack(push,8)
#include <shader/binary.h>
#include <gnm/constants.h>
#include <shader/wave_psslc.h>
#include <shaderperf.h>
#pragma pack(pop)

#include "GnmShaderResources.h"

using namespace sce;
using namespace sce::Shader;

static int32 GUseExternalShaderCompiler = 0;
static FAutoConsoleVariableRef CVarPS4UseExternalShaderCompiler(
	TEXT("r.PS4UseExternalShaderCompiler"),
	GUseExternalShaderCompiler,
	TEXT("Whether to use the internal shader compiling library or the external orbis-psslc compiler.\n")
	TEXT(" 0: Internal compiler\n")
	TEXT(" 1: External compiler)"),
	ECVF_Default
	);

static int32 GPS4PackParameters = 0;
static FAutoConsoleVariableRef CVarPS4PackParameters(
	TEXT("r.PS4PackParameters"),
	GPS4PackParameters,
	TEXT("When using the wave compiler, enable -packing-parameter.\n")
	TEXT(" 0: Disable packing\n")
	TEXT(" 1: Enable packing)"),
	ECVF_Default
	);

static int32 GPS4StripExtraShaderBinaryData = 1;
static FAutoConsoleVariableRef CVarPS4StripExtraShaderBinaryData(
	TEXT("r.PS4StripExtraShaderBinaryData"),
	GPS4StripExtraShaderBinaryData,
	TEXT("Enable stripping of unused data in the shader binary.\n")
	TEXT(" 0: No stripping\n")
	TEXT(" 1: Stripping enabled)"),
	ECVF_Default
	);


static TAutoConsoleVariable<int32> CVarPS4UseTTrace(
	TEXT("r.PS4UseTTrace"),
	0,
	TEXT("Enables adding thread trace information on the shader binary.\n")
	TEXT(" 0: No extra info (default)\n")
	TEXT(" 1: Adds basic ttrace info (around 16 bytes)\n")
	TEXT(" 2: Adds constant ttrace info (same as 1 plus metadata per constant\n")
	TEXT(" 3: Adds max ttrace info (undocumented!)"),
	ECVF_Default
	);

static bool bMixedModeShaderDebugInfo = false;
static bool bDumpShaderSDB = false;

static bool bUseFastMath = true;
static bool bCheckedSDK = false;

/**
 * Handle resources that can be arrays (textures, samplerstates, etc)
 */
static void AddResource(FShaderCompilerOutput& Output, const FString& ResourceName, uint16 BufferIndex, uint16 ResourceIndex, uint16 ElementSize)
{
	// is it an array?
	if (ResourceName.Contains(TEXT("[")))
	{
		TArray<FString> Tokens;

		// if so, break it into name and array
		ResourceName.ParseIntoArray(Tokens, TEXT("["), false);

		// Calculate the total size of the array in bytes,
		// Multi-dimensional arrays are handled by multiplying the number of elements in each array together along with the element size.
		// The number of elements in each array comes from the array indexes used in the resource name which will give an incorrect result
		// until we process the resource name that indexes into the last element of each array.

		uint16 ArrayTotalSize = ElementSize;
		for( int32 TokenIndex = 1; TokenIndex < Tokens.Num(); TokenIndex++ )
		{
			uint16 NumElementsInArray = (uint16)FCString::Atoi(*Tokens[ TokenIndex ]) + 1;
			ArrayTotalSize *= NumElementsInArray;
		}
		uint16 BaseIndex = ResourceIndex - ( ArrayTotalSize - ElementSize );

		// The array resources should be processed in increasing order but just in case make sure
		// that we are using the smallest base index and largest total size to guarantee we get the whole array
		{
			uint16 CurrentBufferIndex;
			uint16 CurrentBaseIndex;
			uint16 CurrentSize;
			bool bFoundParameter = Output.ParameterMap.FindParameterAllocation( *Tokens[0], CurrentBufferIndex, CurrentBaseIndex, CurrentSize );
			if( bFoundParameter )
			{
				check( BufferIndex == CurrentBufferIndex );
				BaseIndex = FMath::Min<uint16>( CurrentBaseIndex, BaseIndex );
				ArrayTotalSize = FMath::Max<uint16>( CurrentSize, ArrayTotalSize );
			}
		}

		// add it to the list of parameters
		Output.ParameterMap.AddParameterAllocation(	*Tokens[0], BufferIndex, BaseIndex, ArrayTotalSize );

	}
	// handle non-arrays
	else
	{
		// add it to the list of parameters
		Output.ParameterMap.AddParameterAllocation(
			*ResourceName,
			BufferIndex,
			ResourceIndex,
			ElementSize
			);
	}
}

static inline uint32_t GetAttributeRelativeMappedNameIndex(const void* BaseAttributeNameOffsetAddr, const int32 CurrentStringOffset, const void* BaseStringTableAddr)
{
	return (uint32_t)((uint64)BaseStringTableAddr - (uint64)BaseAttributeNameOffsetAddr) + CurrentStringOffset;
}

static inline void RemapAttributeNames(Shader::Binary::Attribute& Attribute, TMap<void*, int32>& RemappedStringIndices, int32& CurrentStringOffset, char*& CurrentTableString, const Shader::Binary::Program& Program, const int32 TotalTableSize)
{		
	uint8_t* BaseStringTableAddr = Program.m_stringTable;
	{
		const char* OriginalName = Attribute.getName();

		//resuse existing string table entries whenever possible.
		if (RemappedStringIndices.Contains((void*)OriginalName))
		{
			Attribute.m_nameOffset = GetAttributeRelativeMappedNameIndex(&Attribute.m_nameOffset, *RemappedStringIndices.Find(OriginalName), BaseStringTableAddr);
		}
		else
		{
			Attribute.m_nameOffset = GetAttributeRelativeMappedNameIndex(&Attribute.m_nameOffset, CurrentStringOffset, BaseStringTableAddr);
			FCStringAnsi::Strcpy(CurrentTableString, TotalTableSize - CurrentStringOffset, OriginalName);
			RemappedStringIndices.Add((void*)OriginalName, CurrentStringOffset);

			int32 StringAdvance = FCStringAnsi::Strlen(CurrentTableString) + 1;
			CurrentStringOffset += StringAdvance;
			CurrentTableString += StringAdvance;
		}
		check((uint32_t)CurrentStringOffset <= Program.m_stringTableSize);
	}	

	{
		const char* OriginalSemanticName = Attribute.getSemanticName();		
		if (RemappedStringIndices.Contains(OriginalSemanticName))
		{
			Attribute.m_semanticNameOffset = GetAttributeRelativeMappedNameIndex(&Attribute.m_semanticNameOffset, *RemappedStringIndices.Find(OriginalSemanticName), BaseStringTableAddr);
		}
		else
		{
			Attribute.m_semanticNameOffset = GetAttributeRelativeMappedNameIndex(&Attribute.m_semanticNameOffset, CurrentStringOffset, BaseStringTableAddr);
			FCStringAnsi::Strcpy(CurrentTableString, TotalTableSize - CurrentStringOffset, OriginalSemanticName);
			RemappedStringIndices.Add((void*)OriginalSemanticName, CurrentStringOffset);

			int32 StringAdvance = FCStringAnsi::Strlen(CurrentTableString) + 1;
			CurrentStringOffset += StringAdvance;
			CurrentTableString += StringAdvance;
		}
		check((uint32_t)CurrentStringOffset <= Program.m_stringTableSize);
	}	
}

// Converts kSemanticSPosition to S_POSITION, or kSemanticSVertexId to S_VERTEX_ID
static FString GetSystemSemanticName(const char* PSSLEnumName)
{
	FString EnumName = ANSI_TO_TCHAR(PSSLEnumName);
	if (!EnumName.StartsWith("kSemanticS"))
	{
		return TEXT("");
	}
	// 10 is strlen("kSemanticS")
	EnumName = EnumName.RightChop(10);
	FString SemanticName = TEXT("S");
	for (int32 Index = 0; Index < EnumName.Len(); ++Index)
	{
		TCHAR Current = EnumName[Index];

		// Current naming scheme has upper case letters in the enum name, and that means an _ in the semantic name
		if ((Current >= 'A' && Current <= 'Z'))
		{
			SemanticName += "_";
		}

		SemanticName += Current;
	}

	SemanticName.ToUpperInline();
	return SemanticName;
}

// Hold information to be able to call the compilers
struct FCompilerInfo
{
	const FShaderCompilerInput& Input;
	FString WorkingDirectory;
	FString Profile;
	Wave::Psslc::Options WaveOptions;

	FCompilerInfo(const FShaderCompilerInput& InInput, const FString& InWorkingDirectory) :
		Input(InInput),
		WorkingDirectory(InWorkingDirectory)
	{
	}
};

static void CallCompileShader(bool bSecondPass, FCompilerInfo& CompilerInfo, FString& PreprocessedShaderSource, FString& EntryPointName, FShaderCompilerOutput& Output);

/** Processes the results of a Gnm shader compilation. */
static void ProcessShaderInformation(FCompilerInfo& CompilerInfo, bool bSecondPass, const TArray<uint8>& ShaderContents, FString& PreprocessedSource, FString& EntryPoint, FShaderCompilerOutput& Output)
{
	/** This is the bytecode as well as the semantic info loaded from the .sb file */
	Shader::Binary::Program Program;
	Program.loadFromMemory(ShaderContents.GetData(), ShaderContents.Num());

	bool bFailedToRemoveInputs = false;
	TArray<FString> UsedAttributes;
	if (CompilerInfo.Input.Target.Frequency == SF_Pixel && CompilerInfo.Input.bCompilingForShaderPipeline)
	{
		// Retrieve the list of semantics the Pixel shader needs
		for (int32 i = 0; i < Program.m_numInputAttributes; ++i)
		{
			Shader::Binary::Attribute& Attribute = Program.m_inputAttributes[i];
			switch (Attribute.m_psslSemantic)
			{
			case sce::Shader::Binary::kSemanticSClipDistance:
			case sce::Shader::Binary::kSemanticSCullDistance:
			case sce::Shader::Binary::kSemanticSCoverage:
			case sce::Shader::Binary::kSemanticSDepthOutput:
			case sce::Shader::Binary::kSemanticSDepthGEOutput:
			case sce::Shader::Binary::kSemanticSDepthLEOutput:
			case sce::Shader::Binary::kSemanticSDispatchthreadId:
			case sce::Shader::Binary::kSemanticSDomainLocation:
			case sce::Shader::Binary::kSemanticSGroupId:
			case sce::Shader::Binary::kSemanticSGroupIndex:
			case sce::Shader::Binary::kSemanticSGroupThreadId:
			case sce::Shader::Binary::kSemanticSPosition:
			case sce::Shader::Binary::kSemanticSVertexId:
			case sce::Shader::Binary::kSemanticSInstanceId:
			case sce::Shader::Binary::kSemanticSSampleIndex:
			case sce::Shader::Binary::kSemanticSPrimitiveId:
			case sce::Shader::Binary::kSemanticSGsinstanceId:
			case sce::Shader::Binary::kSemanticSOutputControlPointId:
			case sce::Shader::Binary::kSemanticSFrontFace:
			case sce::Shader::Binary::kSemanticSRenderTargetIndex:
			case sce::Shader::Binary::kSemanticSViewportIndex:
			case sce::Shader::Binary::kSemanticSTargetOutput:
			case sce::Shader::Binary::kSemanticSEdgeTessFactor:
			case sce::Shader::Binary::kSemanticSInsideTessFactor:
			case sce::Shader::Binary::kSemanticSpriteCoord:
			{
				FString SemanticName = GetSystemSemanticName(sce::Shader::Binary::getPsslSemanticString((sce::Shader::Binary::PsslSemantic)Attribute.m_psslSemantic));
				if (SemanticName.Len() > 0)
				{
					if (SemanticName.Contains(TEXT("CLIP")))
					{
						FString String = FString::Printf(TEXT("%s%d"), *SemanticName, Attribute.m_semanticIndex);
						UsedAttributes.Add(String);
					}
					UsedAttributes.AddUnique(SemanticName);
				}
				else
				{
					Output.Errors.Add(FShaderCompilerError(*FString::Printf(TEXT("Unable to convert PSSL semantic %d"), Attribute.m_psslSemantic)));
					bFailedToRemoveInputs = true;
				}
			}
			break;

			case sce::Shader::Binary::kSemanticUserDefined:
			{
				FString SemanticName = ANSI_TO_TCHAR(Attribute.getSemanticName());
				const TCHAR* FoundCentroid = FCString::Strstr(*SemanticName, TEXT("_CENTROID"));
				if (FoundCentroid)
				{
					// strlen("_CENTROID") = 9
					FString String = SemanticName.LeftChop(9);
					// Add both TEXCOORD and TEXCOORD0 as we might get NORMAL
					UsedAttributes.Add(String);
				}
				else
				{
					FString String = FString::Printf(TEXT("%s%d"), *SemanticName, Attribute.m_semanticIndex);
					// Add both TEXCOORD and TEXCOORD0 as we might get NORMAL
					UsedAttributes.Add(String);
				}
				UsedAttributes.AddUnique(SemanticName);
			}
				break;

			default:
				Output.Errors.Add(FShaderCompilerError(*FString::Printf(TEXT("Unable to convert PSSL semantic %d"), Attribute.m_psslSemantic)));
				bFailedToRemoveInputs = true;
				break;
			}
		}

		if (bFailedToRemoveInputs)
		{
			Output.bFailedRemovingUnused = true;
		}
		else
		{
			if (bSecondPass)
			{
				Output.bSupportsQueryingUsedAttributes = true;
				Output.UsedAttributes = UsedAttributes;
			}
			else
			{
				TArray<FString> Errors;
				if (RemoveUnusedInputs(PreprocessedSource, UsedAttributes, EntryPoint, Errors))
				{
					CallCompileShader(true, CompilerInfo, PreprocessedSource, EntryPoint, Output);
					// Early out as the child call will process the info
					return;
				}
				else
				{
					UE_LOG(LogShaders, Warning, TEXT("Failed to Remove unused inputs [%s]!"), *CompilerInfo.Input.DumpDebugInfoPath);
					for (int32 Index = 0; Index < Errors.Num(); ++Index)
					{
						FShaderCompilerError NewError;
						NewError.StrippedErrorMessage = Errors[Index];
						Output.Errors.Add(NewError);
					}

					Output.bFailedRemovingUnused = true;
				}
			}
		}
	}

	Output.NumInstructions = 0;
	
	{
		Shader::Perf::ShaderParams ShaderAnalysisParams;
		Shader::Perf::initializeDefaultShaderParams(&ShaderAnalysisParams);
		Shader::Perf::ShaderHandle ShaderAnalysisHandle;
		int32 Ret = Shader::Perf::loadShader(ShaderContents.GetData(), ShaderContents.Num(), &ShaderAnalysisParams, &ShaderAnalysisHandle);
		if (Ret == SCE_OK)
		{
			Shader::Perf::ShaderInfo ShaderInfo;
			Ret = Shader::Perf::getShaderInfo(ShaderAnalysisHandle, &ShaderInfo);
			if (Ret == SCE_OK)
			{
				// we divide because the PS4 is a scalar GPU.  This means many ops cost up to 4x as much as as instructions on vector gpus / d3d.  
				// however assuming EVERY op is 4x is bad.  Settling on a value determined by current content for now. May change in the future.
				// This is only used for ShaderComplexityView so we want our view to look more or less like it does on PC.
				Output.NumInstructions = (int32)((float)ShaderInfo.instructionCount / 2.5f);
			}
		}
		Shader::Perf::terminateShader(ShaderAnalysisHandle);
	}

	// if the first uniform buffer is the default, then we want to output it's uniforms below,
	// but if it's not the default, then we want to just use the uniform buffer name ('View')
	bool bIsUniformBufferZeroTheDefault = false;

	TBitArray<> UsedUniformBufferSlots;
	UsedUniformBufferSlots.Init(false,32);

	// look through the buffers looking for textures and uniform buffers
	for (uint16 BufferIndex = 0; BufferIndex < Program.m_numBuffers; BufferIndex++)
	{
		const Shader::Binary::Buffer& Buffer = Program.m_buffers[BufferIndex];
		// is this a uniform buffer?
		if (Buffer.m_internalType == Shader::Binary::kInternalBufferTypeCbuffer)
		{
			// is the default buffer the zero buffer?
			if (Buffer.m_resourceIndex == 0 && FCStringAnsi::Strcmp((ANSICHAR*)Buffer.getName(), "__GLOBAL_CB__") == 0)
			{
				bIsUniformBufferZeroTheDefault = true;

				for( uint32 ElementIndex = Buffer.m_elementOffset; ElementIndex < Buffer.m_elementOffset + Buffer.m_numElements; ElementIndex++ )
				{
					check( ElementIndex < Program.m_numElements );
					sce::Shader::Binary::Element& CurrentElement = Program.m_elements[ElementIndex];
					Output.ParameterMap.AddParameterAllocation( ANSI_TO_TCHAR( CurrentElement.getName() ), Buffer.m_resourceIndex, ( uint16 )CurrentElement.m_byteOffset, CurrentElement.m_size );
				}
			}
			else
			{
				Output.ParameterMap.AddParameterAllocation( ANSI_TO_TCHAR( Buffer.getName() ), Buffer.m_resourceIndex, 0, Buffer.m_strideSize );
			}

			UsedUniformBufferSlots[Buffer.m_resourceIndex] = true;
		}
		// other buffers are just output normally
		else if (Buffer.m_internalType != Shader::Binary::kInternalBufferTypeInternal)
		{
			AddResource(Output, ANSI_TO_TCHAR(Buffer.getName()), 0, (uint16)Buffer.m_resourceIndex, 1);
		}
	}

	// look through the samplers
	for (uint16 SamplerIndex = 0; SamplerIndex < Program.m_numSamplerStates; SamplerIndex++)
	{
		const Shader::Binary::SamplerState& Sampler = Program.m_samplerStates[SamplerIndex];
		AddResource(Output, ANSI_TO_TCHAR(Sampler.getName()), 0, (uint16)Sampler.m_resourceIndex, 1);
	}

	// Build the SRT for this shader.
	FGnmShaderResourceTable SRT;
	{
		// Build the generic SRT for this shader.
		FShaderCompilerResourceTable GenericSRT;
		BuildResourceTableMapping(CompilerInfo.Input.Environment.ResourceTableMap, CompilerInfo.Input.Environment.ResourceTableLayoutHashes, UsedUniformBufferSlots, Output.ParameterMap, GenericSRT);

		// Copy over the bits indicating which resource tables are active.
		SRT.ResourceTableBits = GenericSRT.ResourceTableBits;

		SRT.ResourceTableLayoutHashes = GenericSRT.ResourceTableLayoutHashes;

		// Now build our token streams.
		BuildResourceTableTokenStream(GenericSRT.TextureMap, GenericSRT.MaxBoundResourceTable, SRT.TextureMap);
		BuildResourceTableTokenStream(GenericSRT.ShaderResourceViewMap, GenericSRT.MaxBoundResourceTable, SRT.ShaderResourceViewMap);
		BuildResourceTableTokenStream(GenericSRT.SamplerMap, GenericSRT.MaxBoundResourceTable, SRT.SamplerMap);
		BuildResourceTableTokenStream(GenericSRT.UnorderedAccessViewMap, GenericSRT.MaxBoundResourceTable, SRT.UnorderedAccessViewMap);
	}

	uint32 OriginalShaderSize = Program.calculateSize();

	//Buffers is all SRVs and UAVs.  That's the spirit of the NumTextureSamplers stat.
	Output.NumTextureSamplers = Program.m_numBuffers;	
	
	TArray<FGnmShaderAttributeInfo> DebugAttributeInputs;
	TArray<FGnmShaderAttributeInfo> DebugAttributeOutputs;

	if (GPS4StripExtraShaderBinaryData)
	{
		// Strip the shader of stuff we don't want to pay memory cost for at runtime.  e.g. element info + related strings.
		TArray<char> StrippedStringTable;
		StrippedStringTable.Reserve(Program.m_stringTableSize);		
		{
			//maps from an original string to the offset in the stripped table.
			//can't map on the nameoffsets because they are relative to the address of the Attribute's member.  So attributes that
			//share a string will have different offsets.
			TMap<void*, int32> RemappedStringIndices;

			int32 TotalTableSize = Program.m_stringTableSize;
			int32 CurrentStringOffset = 0;
			char* CurrentTableString = StrippedStringTable.GetData();

			//strip reflection data that we don't need at runtime.  If we end up needing any of this, we can add them back in and adjust the string table for them.
			Program.m_numBuffers = 0;
			Program.m_numConstants = 0;
			Program.m_numElements = 0;
			Program.m_numSamplerStates = 0;
			Program.m_numStreamOuts = 0;

			//we must update the string table for any data that we want to keep for the runtime. 
			for (int32 i = 0; i < Program.m_numInputAttributes; ++i)
			{
				Shader::Binary::Attribute& Attribute = Program.m_inputAttributes[i];
				RemapAttributeNames(Attribute, RemappedStringIndices, CurrentStringOffset, CurrentTableString, Program, TotalTableSize);
			}
			for (int32 i = 0; i < Program.m_numOutputAttributes; ++i)
			{
				Shader::Binary::Attribute& Attribute = Program.m_outputAttributes[i];
				RemapAttributeNames(Attribute, RemappedStringIndices, CurrentStringOffset, CurrentTableString, Program, TotalTableSize);
			}

			//sanity check
			check((uint32_t)CurrentStringOffset <= Program.m_stringTableSize);

			//copy over the original string table because the Program expects offsets relative to address of each element.
			FMemory::Memcpy(Program.m_stringTable, StrippedStringTable.GetData(), CurrentStringOffset);
			Program.m_stringTableSize = CurrentStringOffset;			
		}
	}
	else
	{
		auto CreateAttributeList = [&](TArray<FGnmShaderAttributeInfo>& List, uint8 NumAttributes, const sce::Shader::Binary::Attribute* Attributes)
			{
				List.AddUninitialized(NumAttributes);
				for (int32 i = 0; i < NumAttributes; ++i)
				{
					const sce::Shader::Binary::Attribute& Attribute = Attributes[i];
					FGnmShaderAttributeInfo& NewEntry = List[i];
					NewEntry.DataType = (sce::Shader::Binary::PsslType)Attribute.m_type;
					NewEntry.PSSLSemantic = (sce::Shader::Binary::PsslSemantic)Attribute.m_psslSemantic;
					//NewEntry.ResourceIndex = Attribute.m_resourceIndex;
					NewEntry.AttrName = ANSI_TO_TCHAR(Attribute.getName());
					if (!FCStringAnsi::Strcmp(Attribute.getSemanticName(), "(no_name)"))
					{
						NewEntry.SemanticName = NAME_None;
						//NewEntry.SemanticName = FName(*GetSystemSemanticName(sce::Shader::Binary::getPsslSemanticString((sce::Shader::Binary::PsslSemantic)Attribute.m_psslSemantic)));
					}
					else
					{
						NewEntry.SemanticName = ANSI_TO_TCHAR(Attribute.getSemanticName());
					}
				}
			};

		CreateAttributeList(DebugAttributeInputs, Program.m_numInputAttributes, Program.m_inputAttributes);
		CreateAttributeList(DebugAttributeOutputs, Program.m_numOutputAttributes, Program.m_outputAttributes);
	}

	uint32 StrippedShaderSize = Program.calculateSize();
	if (GPS4StripExtraShaderBinaryData)
	{
		UE_LOG(LogShaders, Warning, TEXT("Saved %i bytes by stripping"), OriginalShaderSize - StrippedShaderSize);
	}
	
	TArray<uint8> NewShaderContents;

	//reserve some extra space to guard against any potential bugs in Sony's size calculation.
	NewShaderContents.Reserve(StrippedShaderSize + 4);
	uint32* NewShaderGuardArea = (uint32*)(NewShaderContents.GetData() + StrippedShaderSize);
	*NewShaderGuardArea = 0xCDCDCDCD;

	Program.saveToMemory(NewShaderContents.GetData(), StrippedShaderSize);

	// make sure the library didn't overrun the space we gave it.
	check(*NewShaderGuardArea == 0xCDCDCDCD);

	FMemoryWriter Ar(Output.ShaderCode.GetWriteAccess(), true);
	Ar << SRT;
	Ar.Serialize((void*)NewShaderContents.GetData(), StrippedShaderSize);

	// append data that is generate from the shader code and assist the usage, mostly needed for DX12 
	{
		FShaderCodePackedResourceCounts PackedResourceCounts = { bIsUniformBufferZeroTheDefault, 0, 0, 0, 0 };
		Output.ShaderCode.AddOptionalData(PackedResourceCounts);

		if (DebugAttributeInputs.Num() > 0)
		{
			TArray<uint8> WriterBytes;
			FMemoryWriter Writer(WriterBytes);
			Writer << DebugAttributeInputs;
			if (WriterBytes.Num() < 255)
			{
				Output.ShaderCode.AddOptionalData('i', WriterBytes.GetData(), WriterBytes.Num());
			}
		}

		if (DebugAttributeOutputs.Num() > 0)
		{
			TArray<uint8> WriterBytes;
			FMemoryWriter Writer(WriterBytes);
			Writer << DebugAttributeOutputs;
			if (WriterBytes.Num() < 255)
			{
				Output.ShaderCode.AddOptionalData('o', WriterBytes.GetData(), WriterBytes.Num());
			}
		}
	}

	// store data we can pickup later with ShaderCode.FindOptionalData('n'), could be removed for shipping
	// Daniel L: This GenerateShaderName does not generate a deterministic output among shaders as the shader code can be shared. 
	//			uncommenting this will cause the project to have non deterministic materials and will hurt patch sizes
	//Output.ShaderCode.AddOptionalData('n', TCHAR_TO_UTF8(*CompilerInfo.Input.GenerateShaderName()));

	// Pass the target through to the output.
	Output.Target = CompilerInfo.Input.Target;
}

/**
 * This is a callback function that will return the contents of a given file to the compiler.
 * Since we only have one preprocessed file, we can just have a single object
 */
static ANSICHAR* GSourceFileContents = nullptr;

static Wave::Psslc::SourceFile* CallbackOpenFileWave(const char *fileName, const Wave::Psslc::SourceLocation *includedFrom, const Wave::Psslc::Options *compileOptions, void *userData, const char **errorString)
{
	// this is the only one we need to worry about at a given time
	static Wave::Psslc::SourceFile SingleFile;

	// return the only file!
	SingleFile.fileName = fileName;
	SingleFile.size = strlen(GSourceFileContents);
	SingleFile.text = GSourceFileContents;

	return &SingleFile;
}

static FString GetCommandLineExtraParameters(const FCompilerInfo& CompilerInfo)
{
	FString ExtraSwitches;
	if (GPS4PackParameters != 0)
	{
		ExtraSwitches += TEXT(" -packing-parameter");
	}

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PS4UseTTrace"));
	if (CVar)
	{
		int32 PS4TTrace = CVar->GetValueOnAnyThread();
		ExtraSwitches += FString::Printf(TEXT(" -ttrace %d"), PS4TTrace);
	}

	return ExtraSwitches;
}

static FString CreateShaderCompileCommandLine(const FCompilerInfo& CompilerInfo, const FString& SourceFile, const FString& OutputFile, const FString& EntryPointName, const FString& Profile) 
{
	const FString OutputFileNoExt = FPaths::GetBaseFilename(OutputFile);
	FString CmdLine;

	FString ExtraSwitches = GetCommandLineExtraParameters(CompilerInfo);
	CmdLine = FString::Printf(TEXT("\"%%SCE_ORBIS_SDK_DIR%%\\host_tools\\bin\\orbis-wave-psslc.exe\" -enable-dx10-clamp %s -entry %s -profile %s -o %s %s"), *ExtraSwitches, *EntryPointName, *Profile, *OutputFile, *SourceFile);
	CmdLine += FString::Printf(TEXT("\n\"%%SCE_ORBIS_SDK_DIR%%\\host_tools\\bin\\orbis-sb-dump.exe\" -disassemble \"%s\" > %s.asm"), *OutputFile, *OutputFileNoExt);
	CmdLine += FString::Printf(TEXT("\n\"%%SCE_ORBIS_SDK_DIR%%\\host_tools\\bin\\orbis-cu-as.exe\" -X \"%s\" > %sExInfo.asm"), *OutputFile, *OutputFileNoExt);
	CmdLine += TEXT("\npause");
	return CmdLine;
}

static inline FString GetTargetOutputModeString( EPixelFormat PixelFormat )
{
	FString OutputModeString;
	#define TARGETPIXELFORMATCASE(x,y) case x: OutputModeString = TEXT(#y); break;
	switch( PixelFormat )
			{
		TARGETPIXELFORMATCASE(PF_A32B32G32R32F, FMT_32_ABGR)
		TARGETPIXELFORMATCASE(PF_R32_FLOAT, FMT_32_R)
		TARGETPIXELFORMATCASE(PF_G16R16, FMT_UNORM16_ABGR)
		TARGETPIXELFORMATCASE(PF_G32R32F, FMT_32_ABGR)
		TARGETPIXELFORMATCASE(PF_A16B16G16R16, FMT_UNORM16_ABGR)
		TARGETPIXELFORMATCASE(PF_R16_UINT, FMT_UINT16_ABGR)
	default:
		// PF_B8G8R8A8
		// PF_FloatRGB
		// PF_FloatRGBA
		// PF_G16R16F
		// PF_R5G6B5_UNORM
		// PF_R8G8B8A8
		OutputModeString = TEXT("FMT_FP16_ABGR");
			break;
	}

	return OutputModeString;
}

/**
 * Compile a shader using the external psslc.exe shader compiler
 */
static void CompileUsingExternal(bool bSecondPass, FCompilerInfo& CompilerInfo, FString& PreprocessedShaderSource, FString& EntryPointName, FShaderCompilerOutput& Output)
{
	// generate a unique shader name for this thread
	FString InputName = FString::Printf(TEXT("%s/%s.pssl"), *CompilerInfo.WorkingDirectory, *CompilerInfo.Input.VirtualSourceFilePath);
	FString OutputName = FString::Printf(TEXT("%s/%s.sb"), *CompilerInfo.WorkingDirectory, *CompilerInfo.Input.VirtualSourceFilePath);
	FString ErrorOutputName = FString::Printf(TEXT("%s/%s_Errors.pssl"), *CompilerInfo.WorkingDirectory, *CompilerInfo.Input.VirtualSourceFilePath);

	// save preprocessed source to disk
	FFileHelper::SaveStringToFile(PreprocessedShaderSource, *InputName);

	// generate the commandline
	TCHAR SDKPath[4096];
	FPlatformMisc::GetEnvironmentVariable(TEXT("SCE_ORBIS_SDK_DIR"), SDKPath, ARRAY_COUNT(SDKPath));

	FString CommandLine;
	FString ExtraSwitches = GetCommandLineExtraParameters(CompilerInfo);

	CommandLine = FString::Printf(TEXT("/c \"%s/host_tools/bin/orbis-wave-psslc.exe\" -enable-dx10-clamp %s -entry %s -profile %s -o %s %s 2> %s"),
		SDKPath, *ExtraSwitches, *EntryPointName, *CompilerInfo.Profile, *OutputName, *InputName, *ErrorOutputName);

	UE_LOG(LogShaders, Log, TEXT("CommandLine for compiling '%s': \ncmd.exe %s"), *CompilerInfo.Input.VirtualSourceFilePath, *CommandLine);

	// kick off the compile
	FProcHandle Handle = FPlatformProcess::CreateProc(TEXT("cmd.exe"), *CommandLine, false, true, true, NULL, 0, NULL, NULL);
	
	// did it start?
	if (Handle.IsValid())
	{
		// wait for it to finish
		FPlatformProcess::WaitForProc(Handle);
		int32 RetCode;
		FPlatformProcess::GetProcReturnCode(Handle, &RetCode);
		FPlatformProcess::CloseProc(Handle);

		// if it failed, load the errors
		if (RetCode != 0)
		{
			// read in the error file
			FString ErrorFileContents;
			FFileHelper::LoadFileToString(ErrorFileContents, *ErrorOutputName);

			static int32 ErrorIndex = 0;
			FString ErrorSource = FPaths::EngineIntermediateDir() / FString::Printf(TEXT("PS4ShaderErrors/Error_%d.txt"), ErrorIndex++);
			FFileHelper::SaveStringToFile(PreprocessedShaderSource, *ErrorSource);

			// send back the commandline and any errors
			Output.Errors.Add(FShaderCompilerError(*CommandLine));
			Output.Errors.Add(FShaderCompilerError(*FString::Printf(TEXT("Saved source to %s"), *ErrorSource)));
			Output.Errors.Add(FShaderCompilerError(*ErrorFileContents));
		}
		else
		{
			// load the output byte code
			TArray<uint8> CompiledCode;
			if (FFileHelper::LoadFileToArray(CompiledCode, *OutputName))
			{
				// parse apart the info to get resource names/locations/etc
				ProcessShaderInformation(CompilerInfo, bSecondPass, CompiledCode, PreprocessedShaderSource, EntryPointName, Output);

				if (CompilerInfo.Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*CompilerInfo.Input.DumpDebugInfoPath))
				{
					FFileHelper::SaveArrayToFile(CompiledCode, *(CompilerInfo.Input.DumpDebugInfoPath / TEXT("Shader.sb")));
				}

				// success!
				Output.Target = CompilerInfo.Input.Target;
				Output.bSucceeded = true;

				// save the .sdb file if we are caching it
				// @todo ps4: One shader gives back corrupted SDB data. May need SOny's help here, but this will unblock at least
				// cache the .sdb files to the .SDB directory
/*
				TArray<FString> SDBFiles;
				do
				{
					FFilename Search = FFilename(OutputName.GetPath() + TEXT("*.sdb")).MakeStandardFilename();
					SDBFiles.Empty();
					IFileManager::Get().FindFiles(SDBFiles, *Search, true, false);
				}
				while (SDBFiles.Num() > 1);

				FFilename SDBPath = FPaths::EngineIntermediateDir() + TEXT("Shaders/PS4/");

				// move files from working directory to single PDB directory
				for (int32 Index = 0; Index < SDBFiles.Num(); Index++)
				{
					bool bSucceeded = IFileManager::Get().Move(*(SDBPath + SDBFiles(Index)), *(OutputName.GetPath() * SDBFiles(Index)), true);
				}
*/
			}
		}
	}

	// delete temp files
	IFileManager::Get().Delete(*OutputName);
	IFileManager::Get().Delete(*ErrorOutputName);
}

// Wrap this call to avoid memory leaks in the case of early returns
struct FRunPsslc
{
	Wave::Psslc::CallbackList Callbacks;
	const Wave::Psslc::Output* Results;

	FRunPsslc(FCompilerInfo& CompilerInfo)
	{
		// set up callback functions
		Wave::Psslc::initializeCallbackList(&Callbacks, Wave::Psslc::kCallbackDefaultsTrivial);
		Callbacks.openFile = CallbackOpenFileWave;

		// compile it!
		Results = Wave::Psslc::run(&CompilerInfo.WaveOptions, &Callbacks);
	}

	~FRunPsslc()
	{
		Wave::Psslc::destroyOutput(Results);
	}
};

/**
 * Compile a shader using the internal shader compiling library
 */
static void CompileUsingInternal(bool bSecondPass, FCompilerInfo& CompilerInfo, FString& PreprocessedShaderSource, FString& EntryPointName, FShaderCompilerOutput& Output)
{
	// set main function name
	TArray<ANSICHAR> EntryNameBuffer;
	EntryNameBuffer.AddUninitialized(EntryPointName.Len() + 1);
	FMemory::Memcpy(EntryNameBuffer.GetData(), TCHAR_TO_ANSI(*EntryPointName), EntryPointName.Len() + 1);
	CompilerInfo.WaveOptions.entryFunctionName = EntryNameBuffer.GetData();
	CompilerInfo.WaveOptions.packingParameter = (GPS4PackParameters != 0) ? 1 : 0;

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PS4UseTTrace"));
	if (CVar)
	{
		int32 PS4TTrace = CVar->GetValueOnAnyThread();
		CompilerInfo.WaveOptions.ttrace = (PS4TTrace > 0) ? PS4TTrace : 0;
	}

	// TODO: Is this still needed?
	CompilerInfo.WaveOptions.optimizeForDebug = bMixedModeShaderDebugInfo;
	
	// save out the contents to a string that can be returned to the compiler from the callback
	GSourceFileContents = (ANSICHAR*)FMemory::Malloc(PreprocessedShaderSource.Len() + 1);
	FMemory::Memcpy(GSourceFileContents, TCHAR_TO_ANSI(*PreprocessedShaderSource), PreprocessedShaderSource.Len() + 1);

	// compile it!
	FRunPsslc RunPsslc(CompilerInfo);

	// toss temp memory
	FMemory::Free(GSourceFileContents);

	if (RunPsslc.Results)
	{
		Output.bSucceeded = true;

		for (int32 MessageIndex = 0; MessageIndex < RunPsslc.Results->diagnosticCount; MessageIndex++)
		{
			const Wave::Psslc::DiagnosticMessage& Message = RunPsslc.Results->diagnostics[MessageIndex];
			if (Message.level == Wave::Psslc::kDiagnosticLevelWarning || Message.level == Wave::Psslc::kDiagnosticLevelError)
			{
				if (Message.level == Wave::Psslc::kDiagnosticLevelError)
				{
					Output.bSucceeded = false;
				}

				// make a double-clickable error
				FShaderCompilerError NewError;
				const TCHAR* PrefixString = Message.level == Wave::Psslc::kDiagnosticLevelError ? TEXT("error") : (Message.level == Wave::Psslc::kDiagnosticLevelWarning ? TEXT("warning") : TEXT("info"));
				if (Message.location)
				{
					if (CompilerInfo.Input.bSkipPreprocessedCache)
					{
						NewError.ErrorVirtualFilePath = ANSI_TO_TCHAR(Message.location->file->fileName);
					}
					else
					{
						FString ErrorFileAndPath = ANSI_TO_TCHAR(Message.location->file->fileName);
						NewError.ErrorVirtualFilePath = ErrorFileAndPath;
					}

					NewError.ErrorLineString = FString::Printf(TEXT("%d"), Message.location->lineNumber);
				}
				NewError.StrippedErrorMessage = FString::Printf(TEXT("[%s] %s\n"),
					PrefixString,
					ANSI_TO_TCHAR(Message.message));

				Output.Errors.Add(NewError);
			}
		}
	}
	else
	{
		Output.bSucceeded = false;
		FString FormattedMessage = TEXT("Shader compilation returned no results (this implies an Internal Compiler Error!");

		FString ErrorFileContents = FString::Printf(TEXT("// entry: %s shadername: %s\r\n"), *EntryPointName, *CompilerInfo.Input.VirtualSourceFilePath);
		ErrorFileContents += PreprocessedShaderSource;
		FFileHelper::SaveStringToFile(ErrorFileContents, *FString::Printf(TEXT("%sShaders/PS4/Errors/ICE_%s.txt"), *FPaths::EngineIntermediateDir(), *CompilerInfo.Input.VirtualSourceFilePath));

		Output.Errors.Add(FShaderCompilerError(*FormattedMessage));
	}

	if (Output.bSucceeded)
	{
		// return the actual compiled code
		TArray<uint8> CompiledCode;
		CompiledCode.AddUninitialized(RunPsslc.Results->programSize);
		FMemory::Memcpy(CompiledCode.GetData(), RunPsslc.Results->programData, RunPsslc.Results->programSize);

		// process the program for uniforms, etc
		ProcessShaderInformation(CompilerInfo, bSecondPass, CompiledCode, PreprocessedShaderSource, EntryPointName, Output);
		Output.Target = CompilerInfo.Input.Target;

		// save the .sdb file if we are caching it
		if (bDumpShaderSDB || bMixedModeShaderDebugInfo)
		{
			TArray<uint8> SDBContents;
			SDBContents.AddUninitialized(RunPsslc.Results->sdbDataSize);
			FMemory::Memcpy(SDBContents.GetData(), RunPsslc.Results->sdbData, RunPsslc.Results->sdbDataSize);
			FFileHelper::SaveArrayToFile(SDBContents, *FString::Printf(TEXT("%s/sdb/%s%s"), *CompilerInfo.Input.DumpDebugInfoRootPath, *CompilerInfo.Input.VirtualSourceFilePath, ANSI_TO_TCHAR(RunPsslc.Results->sdbExt)));

			if (CompilerInfo.WaveOptions.targetProfile == Wave::Psslc::kTargetProfileVsEs)
			{
				TArray<uint8> ShaderCode;
				Output.ShaderCode.GetShaderCodeLegacy(ShaderCode);
				FFileHelper::SaveArrayToFile(ShaderCode, *FString::Printf(TEXT("%s/sdb/Dumps/Export_%s_%s.sb"), *CompilerInfo.Input.DumpDebugInfoRootPath, *CompilerInfo.Input.VirtualSourceFilePath, *EntryPointName));
			}
			else if (CompilerInfo.WaveOptions.targetProfile == Wave::Psslc::kTargetProfileGs)
			{
				TArray<uint8> ShaderCode;
				Output.ShaderCode.GetShaderCodeLegacy(ShaderCode);
				FFileHelper::SaveArrayToFile(ShaderCode, *FString::Printf(TEXT("%s/sdb/Dumps/Geometry_%s_%s.sb"), *CompilerInfo.Input.DumpDebugInfoRootPath, *CompilerInfo.Input.VirtualSourceFilePath, *EntryPointName));
			}
		}
	}
}

bool PreprocessShaderPS4( FString& PreprocessedShaderSource, const FShaderCompilerInput& Input, FShaderCompilerOutput& Output, const FShaderCompilerDefinitions& AdditionalDefines, FString& OutFinalEntryPoint)
{
	if (Input.bSkipPreprocessedCache)
	{
		if (!FFileHelper::LoadFileToString(PreprocessedShaderSource, *Input.VirtualSourceFilePath))
		{
			return false;
		}

		// Remove const as we are on debug-only mode
		CrossCompiler::CreateEnvironmentFromResourceTable(PreprocessedShaderSource, (FShaderCompilerEnvironment&)Input.Environment);
	}
	else
	{
		if (!PreprocessShader(PreprocessedShaderSource, Output, Input, AdditionalDefines))
		{
			// The preprocessing stage will add any relevant errors.
			return false;
		}
	}

	if( Input.Target.Frequency == SF_Pixel )
	{
		// Inject shader output target format #pragmas at the start of the preprocessed shader source
		// These #pragmas are required if the render target format differs from the default of kPsTargetOutputModeA16B16G16R16Float
		for( uint32 TargetIndex = 0; TargetIndex < MaxSimultaneousRenderTargets; TargetIndex++ )
		{
			// Use the requested format for this index if it has been set
			const uint8* TargetPixelFormat = Input.Environment.RenderTargetOutputFormatsMap.Find( TargetIndex );
			if( TargetPixelFormat )
			{
				FString TargetOutputString = GetTargetOutputModeString( (EPixelFormat)*TargetPixelFormat );
				FString PragmaString = FString::Printf(TEXT("#pragma PSSL_target_output_format(target %d %s)\n"), TargetIndex, *TargetOutputString);
				PreprocessedShaderSource = PragmaString + PreprocessedShaderSource;
			}
		}
	}

	//can be modified by RemoveUnusedOutputs
	OutFinalEntryPoint = Input.EntryPointName;
	if (Input.Target.Frequency == SF_Vertex && Input.bCompilingForShaderPipeline)
	{
		// Always add S_POSITION
		TArray<FString> UsedOutputs = Input.UsedOutputs;
		UsedOutputs.AddUnique(TEXT("S_POSITION"));

		// We can't remove any of the output-only system semantics
		//@todo - there are a bunch of tessellation ones as well
		TArray<FString> Exceptions;
		Exceptions.AddUnique(TEXT("S_CLIP_DISTANCE"));
		Exceptions.AddUnique(TEXT("S_CLIP_DISTANCE0"));
		Exceptions.AddUnique(TEXT("S_CLIP_DISTANCE1"));
		Exceptions.AddUnique(TEXT("S_CLIP_DISTANCE2"));
		Exceptions.AddUnique(TEXT("S_CLIP_DISTANCE3"));
		Exceptions.AddUnique(TEXT("S_CULL_DISTANCE"));
		Exceptions.AddUnique(TEXT("S_CULL_DISTANCE0"));
		Exceptions.AddUnique(TEXT("S_CULL_DISTANCE1"));
		Exceptions.AddUnique(TEXT("S_CULL_DISTANCE2"));
		Exceptions.AddUnique(TEXT("S_CULL_DISTANCE3"));
		Exceptions.AddUnique(TEXT("S_COVERAGE"));
		Exceptions.AddUnique(TEXT("S_FRONT_FACE"));
		Exceptions.AddUnique(TEXT("S_RENDER_TARGET_INDEX"));
		Exceptions.AddUnique(TEXT("S_VIEWPORT_INDEX"));
		Exceptions.AddUnique(TEXT("S_OBJPRIM_ID"));

		TArray<FString> Errors;
		if (!RemoveUnusedOutputs(PreprocessedShaderSource, UsedOutputs, Exceptions, OutFinalEntryPoint, Errors))
		{
			UE_LOG(LogShaders, Warning, TEXT("Failed to Remove unused outputs [%s]!"), *Input.DumpDebugInfoPath);
			for (int32 Index = 0; Index < Errors.Num(); ++Index)
			{
				FShaderCompilerError NewError;
				NewError.StrippedErrorMessage = Errors[Index];
				Output.Errors.Add(NewError);
			}
			Output.bFailedRemovingUnused = true;
		}
	}

	if (!RemoveUniformBuffersFromSource(PreprocessedShaderSource))
	{
		return false;
	}

	return true;
}

void CompilePSSLShader(const FShaderCompilerInput& Input, FShaderCompilerOutput& Output, const FString& WorkingDirectory)
{
	check(Input.Target.Platform == SP_PS4);

	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PS4MixedModeShaderDebugInfo"));
		if( CVar && CVar->GetInt() != 0 )
		{
			bMixedModeShaderDebugInfo = true;
		}
	}
	{
		static const auto CVarSDB = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PS4DumpShaderSDB"));		
		if ((CVarSDB && CVarSDB->GetInt() != 0))
		{
			bDumpShaderSDB = true;
		}
	}

	/*
	We're using S_VIEWPORT_INDEX for multi-view ISR which isn't supported by the common hardware target.
	We need to compile both base and neo vertex shaders if multi-view is enabled. The resulting shader
	code is packed together here and then unpacked in FGnmDynamicRHI::RHICreateVertexShader. The correct
	version is then loaded based on the hardware being used.
	*/

	const TMap<FString, FString>& DefinitionMap = Input.Environment.GetDefinitions();
	const FString* const InstancedStereoDef = DefinitionMap.Find(TEXT("INSTANCED_STEREO"));
	const FString* const MultiViewDef = DefinitionMap.Find(TEXT("MULTI_VIEW"));
	const bool bIsInstancedStereoEnabled = InstancedStereoDef != nullptr && InstancedStereoDef->Equals(TEXT("1"));
	const bool bIsMultiViewEnabled = MultiViewDef != nullptr && MultiViewDef->Equals(TEXT("1"));
	bool bIsPlatformSpecific = Input.Target.Frequency == SF_Vertex && bIsInstancedStereoEnabled && bIsMultiViewEnabled;

	// set up #defines for the preprocess
	FShaderCompilerDefinitions AdditionalDefines;
	AdditionalDefines.SetDefine(TEXT("PS4_PROFILE"), 1);	
	AdditionalDefines.SetDefine(TEXT("PLATFORM_SUPPORTS_RENDERTARGET_WRITE_MASK"), 1);

	// Preprocess the shader.
	FString EntryPointName = Input.EntryPointName;
	FString PreprocessedShaderSource;
	FString PreprocessedShaderSourceNeo;
	
	if ( PreprocessShaderPS4( PreprocessedShaderSource, Input, Output, AdditionalDefines, EntryPointName) == false )
	{
		return;
	}

	int32 NumberOfDirectives = Output.PragmaDirectives.Num();
	for (int32 i = 0; i < NumberOfDirectives; i++)
	{
		FString CurrentDirective = Output.PragmaDirectives[i];
		if (CurrentDirective.Equals("PS4_NEO_PROFILE"))
		{
			bIsPlatformSpecific = true;
		}
	}

	if( bIsPlatformSpecific )
	{
		FString NeoEntryPointName = Input.EntryPointName;
		// Setup a Neo version of the shader source
		AdditionalDefines.SetDefine(TEXT("PS4_NEO_PROFILE"), 1);	
		if ( PreprocessShaderPS4( PreprocessedShaderSourceNeo, Input, Output, AdditionalDefines, NeoEntryPointName) == false )
		{
			return;
		}

		if (NeoEntryPointName != EntryPointName)
		{			
			Output.Errors.Add(FShaderCompilerError(*FString::Printf(TEXT("Platform specific entrypoint: %s does not match base entrypoint: %s"), *NeoEntryPointName, *EntryPointName)));
		}
	}

	// set up the compile options
	FCompilerInfo CompilerInfo(Input, WorkingDirectory);
	Wave::Psslc::initializeOptions(&CompilerInfo.WaveOptions, SCE_WAVE_API_VERSION);
	CompilerInfo.WaveOptions.useFastmath = bUseFastMath ? 1 : 0;
	CompilerInfo.WaveOptions.dxClamp = true;
	CompilerInfo.WaveOptions.mainSourceFile = TCHAR_TO_ANSI(*Input.VirtualSourceFilePath);
	CompilerInfo.WaveOptions.sdbCache = 1;
	CompilerInfo.WaveOptions.packingParameter = false;
	CompilerInfo.WaveOptions.cacheGenSourceHash = true;
	CompilerInfo.WaveOptions.hardwareTarget = Wave::Psslc::kHardwareTargetCommon;
	CompilerInfo.WaveOptions.sourceLanguage = Wave::Psslc::kSourceLanguagePssl;

	switch (Input.Target.Frequency)
	{
		case SF_Pixel:
			CompilerInfo.WaveOptions.targetProfile = Wave::Psslc::kTargetProfilePs;
			CompilerInfo.Profile = TEXT("sce_ps_orbis");
			break;
		case SF_Vertex:
			if( Input.Environment.CompilerFlags.Contains( CFLAG_VertexToGeometryShader ) )
			{
#if !HAS_MORPHEUS
				if( Input.Environment.CompilerFlags.Contains( CFLAG_OnChip ) )
				{
					CompilerInfo.WaveOptions.targetProfile = Wave::Psslc::kTargetProfileVsEsOnChip;
					CompilerInfo.Profile = TEXT("sce_vs_es_on_chip_orbis");
				}
				else
#endif
				{
					CompilerInfo.WaveOptions.targetProfile = Wave::Psslc::kTargetProfileVsEs;
					CompilerInfo.Profile = TEXT( "sce_vs_es_orbis" );
				}
			}
			else
			{
				CompilerInfo.WaveOptions.targetProfile = Wave::Psslc::kTargetProfileVsVs;
				CompilerInfo.Profile = TEXT("sce_vs_vs_orbis");
			}
			break;
		case SF_Geometry:
			if( Input.Environment.CompilerFlags.Contains( CFLAG_OnChip ) )
			{
				CompilerInfo.WaveOptions.targetProfile = Wave::Psslc::kTargetProfileGsOnChip;
				CompilerInfo.Profile = TEXT( "sce_gs_on_chip_orbis" );
			}
			else
			{
				CompilerInfo.WaveOptions.targetProfile = Wave::Psslc::kTargetProfileGs;
				CompilerInfo.Profile = TEXT( "sce_gs_orbis" );
			}
			break;
		case SF_Compute:
			CompilerInfo.WaveOptions.targetProfile = Wave::Psslc::kTargetProfileCs;
			CompilerInfo.Profile = TEXT("sce_cs_orbis");
			break;
		default:
			Output.Errors.Add(FShaderCompilerError(TEXT("Non-pixel/vertex/compute/geometry shader, not handled in shader compiler yet!")));
			return;
	}
	
	if (bIsPlatformSpecific)
	{
		// Do a full compile of both base and neo hardware targets
		FShaderCompilerOutput OutputNeo = Output;

		CompilerInfo.WaveOptions.hardwareTarget = Wave::Psslc::kHardwareTargetBase;
		CallCompileShader(false, CompilerInfo, PreprocessedShaderSource, EntryPointName, Output);

		CompilerInfo.WaveOptions.hardwareTarget = Wave::Psslc::kHardwareTargetNeo;
		CallCompileShader(false, CompilerInfo, PreprocessedShaderSourceNeo, EntryPointName, OutputNeo);

		Output.bSucceeded = Output.bSucceeded && OutputNeo.bSucceeded;

		// Pack shader code results together
		// [int32 key][uint32 base length][uint32 neo length][full base shader code][full neo shader code]
		TArray<uint8> CombinedSource;

		CombinedSource.Append(reinterpret_cast<const uint8*>(&PackedShaderKey), sizeof(PackedShaderKey));

		const uint32 BaseLength = Output.ShaderCode.GetReadAccess().Num();
		CombinedSource.Append(reinterpret_cast<const uint8*>(&BaseLength), sizeof(BaseLength));

		const uint32 NeoLength = OutputNeo.ShaderCode.GetReadAccess().Num();
		CombinedSource.Append(reinterpret_cast<const uint8*>(&NeoLength), sizeof(NeoLength));

		CombinedSource.Append(Output.ShaderCode.GetReadAccess());
		CombinedSource.Append(OutputNeo.ShaderCode.GetReadAccess());

		// Replace Output shader code with the combined result
		TArray<uint8>& FinalShaderCode = Output.ShaderCode.GetWriteAccess();
		FinalShaderCode.Empty();
		FinalShaderCode.Append(CombinedSource);
	}
	else
	{
		CallCompileShader(false, CompilerInfo, PreprocessedShaderSource, EntryPointName, Output);
	}
}

// Dumps the usf file if requested, then calls the appropriate compiler
static void CallCompileShader(bool bSecondPass, FCompilerInfo& CompilerInfo, FString& PreprocessedShaderSource, FString& EntryPointName, FShaderCompilerOutput& Output)
{
	const TCHAR* FileNamePrefix;
	//Turn to switch.
	switch (CompilerInfo.WaveOptions.hardwareTarget)
	{
	case Wave::Psslc::kHardwareTargetNeo:
		FileNamePrefix = TEXT("Neo_");
		break;
	case Wave::Psslc::kHardwareTargetBase:
		FileNamePrefix = TEXT("Base_");
		break;
	default:
		FileNamePrefix = TEXT("");
		break;
	}

	// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
	if (CompilerInfo.Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*CompilerInfo.Input.DumpDebugInfoPath))
	{
		const FString Filename = FString::Printf(TEXT("%s%s"), FileNamePrefix, *CompilerInfo.Input.GetSourceFilename());
		// can't check GDumpShaderDebugInfo directly, so set SDB output if we're dumping other debug info for any reason.
		bDumpShaderSDB = true;
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(CompilerInfo.Input.DumpDebugInfoPath / *Filename));
		if (FileWriter)
		{
			auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShaderSource);
			FileWriter->Serialize((ANSICHAR*)AnsiSourceFile.Get(), AnsiSourceFile.Length());
			{
				FString Line = CrossCompiler::CreateResourceTableFromEnvironment(CompilerInfo.Input.Environment);
				FileWriter->Serialize(TCHAR_TO_ANSI(*Line), Line.Len());
			}
			FileWriter->Close();
			delete FileWriter;
		}

		const FString OutputFile = CompilerInfo.Input.DumpDebugInfoPath / TEXT("Output.sb");
		const FString BatchFileContents = CreateShaderCompileCommandLine(CompilerInfo, Filename, OutputFile, *EntryPointName, CompilerInfo.Profile);
		FFileHelper::SaveStringToFile(BatchFileContents, *(CompilerInfo.Input.DumpDebugInfoPath / FString::Printf(TEXT("%sCompilePSSLC.bat"), FileNamePrefix)));

		if (CompilerInfo.Input.bGenerateDirectCompileFile)
		{
			FShaderCompilerInput InputCopy = CompilerInfo.Input;
			InputCopy.VirtualSourceFilePath = FPaths::GetPath(InputCopy.VirtualSourceFilePath);
			InputCopy.VirtualSourceFilePath.PathAppend(*Filename, Filename.Len());

			FFileHelper::SaveStringToFile(CreateShaderCompilerWorkerDirectCommandLine(InputCopy), *(CompilerInfo.Input.DumpDebugInfoPath / FString::Printf(TEXT("%sDirectCompile.txt"), FileNamePrefix)));
		}
	}

	if (GUseExternalShaderCompiler)
	{
		CompileUsingExternal(bSecondPass, CompilerInfo, PreprocessedShaderSource, EntryPointName, Output);
	}
	else
	{
		CompileUsingInternal(bSecondPass, CompilerInfo, PreprocessedShaderSource, EntryPointName, Output);
	}
}
