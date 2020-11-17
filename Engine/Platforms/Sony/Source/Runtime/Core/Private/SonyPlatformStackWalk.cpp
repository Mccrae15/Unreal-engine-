// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyPlatformStackWalk.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#if SONY_ENABLE_ASLR_STACKWALK
#include <libdbg.h>
#endif

// Use cached symbol name resolution when using the malloc profiler, as the standard symbol resolution on PS4 is *very* slow right now
#define USE_CACHED_SYMBOL_NAME_RESOLUTION (USE_MALLOC_PROFILER)

TArray<FSonyPlatformStackWalk::FSonySymbolInfo> FSonyPlatformStackWalk::SymbolInfo;
FString FSonyPlatformStackWalk::SymbolNamesFileName;
TArray<uint8> FSonyPlatformStackWalk::SymbolNamesFileData;
TMap<FName, FString> FSonyPlatformStackWalk::SymbolMetaData;
#if SONY_ENABLE_ASLR_STACKWALK
TArray<FSonyPlatformStackWalk::FSonySegmentInfo> FSonyPlatformStackWalk::SelfSegments;
#endif

void FSonyPlatformStackWalk::LoadSymbolInfo(const FString& RootDirectory, const FString& SandboxName)
{		
	const EBuildConfiguration BuildConfig = FApp::GetBuildConfiguration();
	
	// There are three files to load, each of which has a prefix of our build config
	FString SymbolFileName = FString::Printf(TEXT("%s-Symbols.bin"), LexToString(BuildConfig));
	SymbolNamesFileName = FString::Printf(TEXT("%s-SymbolNames.bin"), LexToString(BuildConfig));
	FString SymbolMetaDataFileName = FString::Printf(TEXT("%s-SymbolMetaData.txt"),LexToString(BuildConfig));
	
	// three possibilities of places to look.
	// 1) We're running from DevStudio and the working dir is <path>/<game>/Build/PS4 for prx loading
	// 2) We've been booted via neighborhood / PS4DevkitUtil / orbis-ctrl and the working dir is probably <path>/<game>, maybe #1
	// 3) We're a packaged/deployed build and should look in the root of our sandbox (e.g. /data/<game> or /app0/game)
	//
	// Note: When reading from the working dir we don't need to worry about filecase, but for #3 we expect all deployed files to
	// be in lower case.

	FString DirectoryName = FString(FPlatformProperties::IniPlatformName()).ToLower();

	TArray<FString> PathOptions = { 
		TEXT("/app0/symbols/"),
		FString(TEXT("/app0")) / FApp::GetProjectName() / TEXT("build") / DirectoryName / TEXT("symbols/"),
		RootDirectory / SandboxName / TEXT("symbols/"),
		RootDirectory / SandboxName / FApp::GetProjectName() / TEXT("build") / DirectoryName / TEXT("symbols/"),
	};

#if !UE_BUILD_SHIPPING
	// QA and Automation run local .selfs against installed packages.
	// We don't currently store symbols in the .pkg so in non-Shipping builds look for symbols
	// on the host PC relative to the .self running
	char HostExecutablePath[SCE_KERNEL_PATH_MAX];

	int32_t PathLength = sceDbgGetExecutablePath(HostExecutablePath, SCE_KERNEL_PATH_MAX);
	if (PathLength)
	{
		FString HostPath = FPaths::GetPath(HostExecutablePath);
		FPaths::NormalizeDirectoryName(HostPath);
		HostPath += TEXT("/../../");
		FPaths::CollapseRelativeDirectories(HostPath);
		PathOptions.Add(HostPath / TEXT("build") / DirectoryName / TEXT("symbols/"));
	}
#endif

	FString SymbolPath;
	
	for (const FString& Path : PathOptions)
	{
		FString TestFile = (Path / SymbolFileName).ToLower();

		SceKernelStat FileStat;
		if (sceKernelStat(TCHAR_TO_UTF8(*TestFile), &FileStat) == SCE_OK)
		{
			SymbolPath = Path;
			break;
		}
	}

	if (SymbolPath.Len() == 0)
	{
		printf("Failed to locate symbol files on /app0");
		for (const FString& Path : PathOptions)
		{
			printf(" or %s", TCHAR_TO_ANSI(*Path));
		}
		printf("!\n");
	}

	// Create full paths to the files we want, staging of non-UFS files should result in them being lower case.
	SymbolFileName = (SymbolPath / SymbolFileName).ToLower();
	SymbolNamesFileName = (SymbolPath / SymbolNamesFileName).ToLower();
	SymbolMetaDataFileName = (SymbolPath / SymbolMetaDataFileName).ToLower();	

#if SONY_ENABLE_ASLR_STACKWALK
	SceKernelModule Modules[50];
	size_t Num = 0;
	size_t Res = sceDbgGetModuleList(Modules, 50, &Num);
	if (Res == SCE_OK && Num >= 1)
	{
		SceDbgModuleInfo Info;
		Info.size = sizeof(SceDbgModuleInfo);
		// Module 0 is this binary.
		Res = sceDbgGetModuleInfo(Modules[0], &Info);
		if (Res == SCE_OK)
		{
			SelfSegments.SetNum(Info.numSegments);
			for (int j = 0; j < Info.numSegments; ++j)
			{
				SelfSegments[j].Address = (uintptr_t)Info.segmentInfo->baseAddr;
				SelfSegments[j].Size = Info.segmentInfo->size;
			}
		}
	}
#endif

	{
		int RetVal;
		int FileHandle;
		for (;;)
		{
			FileHandle = RetVal = sceKernelOpen(TCHAR_TO_UTF8(*SymbolFileName), SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_INONE);
			if (RetVal < SCE_OK)
			{
				break;
			}

			SceKernelStat FileStat;
			RetVal = sceKernelFstat(FileHandle, &FileStat);
			if (RetVal < SCE_OK)
			{
				break;
			}

			const int NumElements = FileStat.st_size / sizeof(FSonySymbolInfo);

			SymbolInfo.AddUninitialized(NumElements);

			RetVal = sceKernelRead(FileHandle, (void*)SymbolInfo.GetData(), FileStat.st_size);
			if (RetVal < SCE_OK)
			{
				break;
			}

			printf("Loaded symbols from %s!\n", TCHAR_TO_ANSI(*SymbolFileName));
			break;
		}
		
		if (FileHandle >= SCE_OK)
		{
			sceKernelClose(FileHandle);
		}
		if (RetVal < SCE_OK)
		{
			printf("Failed to load symbols from %s!\n", TCHAR_TO_ANSI(*SymbolFileName));
		}
	}

	if (USE_CACHED_SYMBOL_NAME_RESOLUTION)
	{

		int RetVal;
		int FileHandle;
		for (;;)
		{
			FileHandle = RetVal = sceKernelOpen(TCHAR_TO_UTF8(*SymbolNamesFileName), SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_INONE);
			if (RetVal < SCE_OK)
			{
				break;
			}

			SceKernelStat FileStat;
			RetVal = sceKernelFstat(FileHandle, &FileStat);
			if (RetVal < SCE_OK)
			{
				break;
			}

			SymbolNamesFileData.AddUninitialized(FileStat.st_size);

			RetVal = sceKernelRead(FileHandle, (void*)SymbolNamesFileData.GetData(), FileStat.st_size);
			break;
		}

		if (FileHandle >= SCE_OK)
		{
			sceKernelClose(FileHandle);
		}
	}

	{
		int RetVal;
		int FileHandle;

		for (;;)
		{
			FileHandle = RetVal = sceKernelOpen(TCHAR_TO_UTF8(*SymbolMetaDataFileName), SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_INONE);
			if (RetVal < SCE_OK)
			{
				break;
			}

			SceKernelStat FileStat;
			RetVal = sceKernelFstat(FileHandle, &FileStat);
			if (RetVal < SCE_OK)
			{
				break;
			}

			TArray<uint8> RawMetaData;
			RawMetaData.AddUninitialized(FileStat.st_size + 1);
			RetVal = sceKernelRead(FileHandle, (void*)RawMetaData.GetData(), FileStat.st_size);
			if (RetVal < SCE_OK)
			{
				break;
			}
			RawMetaData[FileStat.st_size] = 0;

			TArray<FString> MetaDataLines;
			FString((char*)RawMetaData.GetData()).ParseIntoArrayLines(MetaDataLines);

			SymbolMetaData.Reserve(MetaDataLines.Num());
			for (const FString& MetaDataLine : MetaDataLines)
			{
				int32 KeyValueSeparatorIndex = INDEX_NONE;
				if (MetaDataLine.FindChar(':', KeyValueSeparatorIndex))
				{
					FString MetaDataKey = MetaDataLine.Mid(0, KeyValueSeparatorIndex).TrimStartAndEnd();
					FString MetaDataValue = MetaDataLine.Mid(KeyValueSeparatorIndex + 1).TrimStartAndEnd().TrimQuotes();
					SymbolMetaData.Add(*MetaDataKey, MoveTemp(MetaDataValue));
				}
			}
			break;
		}

		if (FileHandle >= SCE_OK)
		{
			sceKernelClose(FileHandle);
		}
	}
}


uint32 FSonyPlatformStackWalk::CaptureStackBackTrace( uint64* BackTrace, uint32 MaxDepth, void* Context )
{
	if( BackTrace == nullptr || MaxDepth == 0 )
	{
		return 0;
	}

	struct FStackFrame
	{
		uint64	NextFrame;
		uint64	ReturnAddr;
	};

	FMemory::Memzero(BackTrace, sizeof(*BackTrace) * MaxDepth);
	FStackFrame* StackFrame = ( FStackFrame* )__builtin_frame_address( 0 );
	uint32 TraceLevel = 0;

	while( StackFrame )
	{
		BackTrace[ TraceLevel ] = StackFrame->ReturnAddr;
		if( BackTrace[ TraceLevel ] != 0 )
			BackTrace[ TraceLevel ] -= 4; // Fixup return address

		TraceLevel++;

		if( TraceLevel >= MaxDepth )
			break;

		StackFrame = ( FStackFrame* )( StackFrame->NextFrame );
	}

	return TraceLevel;
}


void FSonyPlatformStackWalk::StackWalkAndDump( ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, int32 IgnoreCount, void* Context )
{
	// Temporary memory holding the stack trace.
	static const int MAX_DEPTH = 100;
	uint64 StackTrace[MAX_DEPTH];
	memset( StackTrace, 0, sizeof( StackTrace ) );

	// Capture stack backtrace.
	uint32 Depth = CaptureStackBackTrace( StackTrace, MAX_DEPTH, Context );

	{
		// Skip the first two entries as they are inside the stack walking code.
		int32 CurrentDepth = IgnoreCount;
		while( CurrentDepth < Depth )
		{
			FPlatformStackWalk::ProgramCounterToHumanReadableString( CurrentDepth, StackTrace[CurrentDepth], HumanReadableString, HumanReadableStringSize, reinterpret_cast< FGenericCrashContext* >( Context ) );
			FCStringAnsi::Strncat(HumanReadableString, LINE_TERMINATOR_ANSI, HumanReadableStringSize);
			CurrentDepth++;
		}
	}

	if( FCommandLine::IsInitialized() && FParse::Param( FCommandLine::Get(), TEXT( "CrashForUAT" ) ) && FParse::Param( FCommandLine::Get(), TEXT( "stdout" ) ) )
	{
		FPlatformMisc::LowLevelOutputDebugString( ANSI_TO_TCHAR( HumanReadableString ) );
		wprintf( TEXT( "\nbegin: stack for UAT" ) );
		wprintf( TEXT( "\n%s" ), ANSI_TO_TCHAR( HumanReadableString ) );
		wprintf( TEXT( "\nend: stack for UAT" ) );
		fflush( stdout );
	}

	{
		// Build command line for using orbis-addr2line to convert addresses to line number/filename
		ANSICHAR TempArray[MAX_SPRINTF];
		const TCHAR* ExeName = FPlatformProcess::ExecutableName( false );
		FCStringAnsi::Sprintf( TempArray, "orbis-addr2line -e %s ", TCHAR_TO_ANSI( ExeName ) );
		FCStringAnsi::Strncat(HumanReadableString, TempArray, HumanReadableStringSize);

		int32 CurrentDepth = IgnoreCount;
		while( StackTrace[CurrentDepth] || ( CurrentDepth == IgnoreCount ) )
		{
			FCStringAnsi::Sprintf( TempArray, "0x%p ", ( void* )StackTrace[CurrentDepth] );
			FCStringAnsi::Strncat(HumanReadableString, TempArray, HumanReadableStringSize);
			CurrentDepth++;
		}
		FCStringAnsi::Strncat(HumanReadableString, LINE_TERMINATOR_ANSI, HumanReadableStringSize);
	}
}

static uint64 GetOffsetInModuleFromProgramCounter(uint64 ProgramCounter)
{
#if SONY_ENABLE_ASLR_STACKWALK
	for (int Index = 0; Index < FSonyPlatformStackWalk::SelfSegments.Num(); ++Index)
	{
		FSonyPlatformStackWalk::FSonySegmentInfo& Segment = FSonyPlatformStackWalk::SelfSegments[Index];
		if (ProgramCounter >= Segment.Address && ProgramCounter < Segment.Address + Segment.Size)
		{
			return ProgramCounter - Segment.Address;
		}
	}
	return 0;
#else
	return ProgramCounter;
#endif
}

static uint32 ProgramCounterToSymbolNameOffset( uint64 ProgramCounter )
{
	uint64 OffsetInModule = GetOffsetInModuleFromProgramCounter(ProgramCounter);
	if (ProgramCounter != 0)
	{
		for( uint32 SymbolIndex = 0; SymbolIndex < FSonyPlatformStackWalk::SymbolInfo.Num(); SymbolIndex++ )
		{
			FSonyPlatformStackWalk::FSonySymbolInfo& SymbolInfo = FSonyPlatformStackWalk::SymbolInfo[ SymbolIndex ];
			if( OffsetInModule >= SymbolInfo.Address && OffsetInModule < (SymbolInfo.Address + SymbolInfo.Size ) )
			{
				return SymbolInfo.NameOffset;
			}
		}
	}

	return INDEX_NONE;
}


static void ReadSymbolNameFromFile(uint32 SymbolNameFileOffset, char* DestStringBuffer, int32 DestStringBufferLength)
{
	int FileHandle = sceKernelOpen(TCHAR_TO_UTF8(*FSonyPlatformStackWalk::SymbolNamesFileName), SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_INONE);
	if( FileHandle < SCE_OK)
	{
		FCStringAnsi::Strncpy( DestStringBuffer, "*** UNKNOWN ***", DestStringBufferLength );
		return;
	}

	int RetVal = sceKernelLseek(FileHandle, SymbolNameFileOffset, SCE_KERNEL_SEEK_SET);
	if (RetVal < SCE_OK)
	{
		FCStringAnsi::Strncpy(DestStringBuffer, "*** UNKNOWN ***", DestStringBufferLength);
		sceKernelClose(FileHandle);
		return;
	}

	// Length of string is stored as LEB128
	int32 SymbolNameLength = 0;
	int32 Shift = 0;
	uint8 Byte;

	while( true )
	{
		RetVal = sceKernelRead(FileHandle, (void*)&Byte, 1);
		if (RetVal < SCE_OK)
		{
			FCStringAnsi::Strncpy(DestStringBuffer, "*** UNKNOWN ***", DestStringBufferLength);
			sceKernelClose(FileHandle);
			return;
		}
		SymbolNameLength |= ( Byte & 0x7f ) << Shift;
		if( ( Byte & 0x80 ) == 0 )
		{
			break;
		}
		Shift += 7;
	}

	SymbolNameLength = FMath::Min( SymbolNameLength, DestStringBufferLength - 1 );

	RetVal = sceKernelRead(FileHandle, (void*)DestStringBuffer, SymbolNameLength);
	if (RetVal < SCE_OK)
	{
		FCStringAnsi::Strncpy(DestStringBuffer, "*** UNKNOWN ***", DestStringBufferLength);
		sceKernelClose(FileHandle);
		return;
	}
	DestStringBuffer[ SymbolNameLength ] = 0;

	sceKernelClose(FileHandle);
}


static void ReadSymbolNameFromCache(uint32 SymbolNameFileOffset, char* DestStringBuffer, int32 DestStringBufferLength)
{
	if (FSonyPlatformStackWalk::SymbolNamesFileData.Num() == 0)
	{
		return ReadSymbolNameFromFile(SymbolNameFileOffset, DestStringBuffer, DestStringBufferLength);
	}

	const uint8* ReadPos = FSonyPlatformStackWalk::SymbolNamesFileData.GetData() + SymbolNameFileOffset;

	// Length of string is stored as LEB128
	int32 SymbolNameLength = 0;
	int32 Shift = 0;

	while (true)
	{
		const uint8 Byte = *(ReadPos++);
		SymbolNameLength |= (Byte & 0x7f) << Shift;
		if ((Byte & 0x80) == 0)
		{
			break;
		}
		Shift += 7;
	}

	SymbolNameLength = FMath::Min(SymbolNameLength, DestStringBufferLength - 1);

	FMemory::Memcpy(DestStringBuffer, ReadPos, SymbolNameLength);
	DestStringBuffer[SymbolNameLength] = 0;
}


static void ReadSymbolName(uint32 SymbolNameFileOffset, char* DestStringBuffer, int32 DestStringBufferLength)
{
#if USE_CACHED_SYMBOL_NAME_RESOLUTION
	return ReadSymbolNameFromCache(SymbolNameFileOffset, DestStringBuffer, DestStringBufferLength);
#else
	return ReadSymbolNameFromFile(SymbolNameFileOffset, DestStringBuffer, DestStringBufferLength);
#endif
}


void FSonyPlatformStackWalk::ProgramCounterToSymbolInfo( uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo )
{
	// Set the program counter.
	out_SymbolInfo.ProgramCounter = ProgramCounter;

	// Append function name if the symbol info is available
	if( SymbolInfo.Num() != 0 )
	{
		const uint32 NameOffset = ProgramCounterToSymbolNameOffset( ProgramCounter );

		if( NameOffset != INDEX_NONE )
		{
			ReadSymbolName( NameOffset, out_SymbolInfo.FunctionName, FProgramCounterSymbolInfo::MAX_NAME_LENGTH );
		}
	}
}


bool FSonyPlatformStackWalk::ProgramCounterToHumanReadableString( int32 CurrentCallDepth, uint64 ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext* Context )
{
	if( HumanReadableString && HumanReadableStringSize > 0 )
	{
		//
		// Callstack lines should be written in this standard format
		//
		//	0xaddress module!func [file]
		// 
		// E.g. 0x045C8D01 OrionClient.self!UEngine::PerformError() [D:\Epic\Orion\Engine\Source\Runtime\Engine\Private\UnrealEngine.cpp:6481]
		//
		// Module may be omitted, everything else should be present, or substituted with a string that conforms to the expected type
		//
		// E.g 0x00000000 UnknownFunction []
		//
		// 

		ANSICHAR TempArray[MAX_SPRINTF];
		FCStringAnsi::Sprintf( TempArray, "0x%08p ", ( void* )ProgramCounter );
		FCStringAnsi::Strncat(HumanReadableString, TempArray, HumanReadableStringSize);

		// Append function name if the symbol info is available
		if( SymbolInfo.Num() != 0 )
		{
			uint32 NameOffset = ProgramCounterToSymbolNameOffset( ProgramCounter );

			ANSICHAR FileNameLine[MAX_SPRINTF] = { 0 };

			if (NameOffset != INDEX_NONE)
			{
				char SymbolName[256];

				ReadSymbolName(NameOffset, SymbolName, sizeof(SymbolName));

				if (FCStringAnsi::Strlen(SymbolName))
				{
					// try to add source file and line number, too
					FCStringAnsi::Sprintf(FileNameLine, " [%s:0] ", SymbolName);
				}
			}

			if (FCStringAnsi::Strlen(FileNameLine))
			{
				FCStringAnsi::Strncat(HumanReadableString, FileNameLine, HumanReadableStringSize);
			}
			else
			{
				// No file info, but include this anyway to confirm to requirements
				FCStringAnsi::Strncat(HumanReadableString, "[UnknownFunction]", HumanReadableStringSize);
			}
		}
	}

	return true;
}

TMap<FName, FString> FSonyPlatformStackWalk::GetSymbolMetaData()
{
	return SymbolMetaData;
}

#undef USE_CACHED_SYMBOL_NAME_RESOLUTION
