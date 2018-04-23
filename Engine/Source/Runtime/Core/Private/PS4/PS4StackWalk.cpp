// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4StackWalk.h"
#include "Misc/App.h"

// Use cached symbol name resolution when using the malloc profiler, as the standard symbol resolution on PS4 is *very* slow right now
#define USE_CACHED_SYMBOL_NAME_RESOLUTION (USE_MALLOC_PROFILER)

TArray<FPS4PlatformStackWalk::FPS4SymbolInfo> FPS4PlatformStackWalk::SymbolInfo;
FString FPS4PlatformStackWalk::SymbolNamesFileName;
TArray<uint8> FPS4PlatformStackWalk::SymbolNamesFileData;
TMap<FName, FString> FPS4PlatformStackWalk::SymbolMetaData;

void FPS4PlatformStackWalk::LoadSymbolInfo()
{
	extern FString GFileRootDirectory;
	extern FString GSandboxName;
		
	const EBuildConfigurations::Type BuildConfig = FApp::GetBuildConfiguration();
	
	// There are three files to load, each of which has a prefix of our build config
	FString SymbolFileName = FString::Printf(TEXT("%s-Symbols.bin"), EBuildConfigurations::ToString(BuildConfig));
	SymbolNamesFileName = FString::Printf(TEXT("%s-SymbolNames.bin"), EBuildConfigurations::ToString(BuildConfig));
	FString SymbolMetaDataFileName = FString::Printf(TEXT("%s-SymbolMetaData.txt"),EBuildConfigurations::ToString(BuildConfig));
	
	// three possibilities of places to look.
	// 1) We're running from DevStudio and the working dir is <path>/<game>/Build/PS4 for prx loading
	// 2) We've been booted via neighborhood / PS4DevkitUtil / orbis-ctrl and the working dir is probably <path>/<game>, maybe #1
	// 3) We're a packaged/deployed build and should look in the root of our sandbox (e.g. /data/<game> or /app0/game)
	//
	// Note: When reading from the working dir we don't need to worry about filecase, but for #3 we expect all deployed files to
	// be in lower case.

	const FString PathOptions[] = { 
		TEXT("/app0/symbols/"),
		TEXT("/app0/build/ps4/symbols/"),
		GFileRootDirectory / GSandboxName / TEXT("symbols"),
	};

	FString SymbolPath;
	
	for (const FString& Path : PathOptions)
	{
		FString TestFile = (Path / SymbolFileName).ToLower();

		if (FILE* TestHandle = fopen(TCHAR_TO_ANSI(*TestFile), "r"))
		{
			SymbolPath = Path;
			fclose(TestHandle);
			break;
		}
	}

	if (SymbolPath.Len() == 0)
	{
		printf("Failed to locate symbol files on /app0 or %s s!\n", TCHAR_TO_ANSI(*PathOptions[2]));
	}

	// Create full paths to the files we want, staging of non-UFS files should result in them being lower case.
	SymbolFileName = (SymbolPath / SymbolFileName).ToLower();
	SymbolNamesFileName = (SymbolPath / SymbolNamesFileName).ToLower();
	SymbolMetaDataFileName = (SymbolPath / SymbolMetaDataFileName).ToLower();	

	{
		FILE* FileHandle = fopen(TCHAR_TO_ANSI(*SymbolFileName), "r");

		if (FileHandle != nullptr)
		{
			fseek(FileHandle, 0, SEEK_END);
			const int FileSize = ftell(FileHandle);

			const int NumElements = FileSize / sizeof(FPS4SymbolInfo);

			SymbolInfo.AddUninitialized(NumElements);

			fseek(FileHandle, 0, SEEK_SET);
			fread((void*)SymbolInfo.GetData(), 1, FileSize, FileHandle);
			fclose(FileHandle);
			
			printf("Loaded symbols from %s!\n", TCHAR_TO_ANSI(*SymbolFileName));
		}
		else
		{
			printf("Failed to load symbols from %s!\n", TCHAR_TO_ANSI(*SymbolFileName));
		}
	}

	if (USE_CACHED_SYMBOL_NAME_RESOLUTION)
	{
		FILE* FileHandle = fopen(TCHAR_TO_ANSI(*SymbolNamesFileName), "r");

		if (FileHandle != nullptr)
		{
			fseek(FileHandle, 0, SEEK_END);
			const int FileSize = ftell(FileHandle);

			SymbolNamesFileData.AddUninitialized(FileSize);

			fseek(FileHandle, 0, SEEK_SET);
			fread((void*)SymbolNamesFileData.GetData(), 1, FileSize, FileHandle);
			fclose(FileHandle);
		}
	}

	{
		FILE* FileHandle = fopen(TCHAR_TO_ANSI(*SymbolMetaDataFileName), "r");

		if (FileHandle != nullptr)
		{
			fseek(FileHandle, 0, SEEK_END);
			const int FileSize = ftell(FileHandle);

			TArray<uint8> RawMetaData;
			RawMetaData.AddUninitialized(FileSize + 1);
			fseek(FileHandle, 0, SEEK_SET);
			fread((void*)RawMetaData.GetData(), 1, FileSize, FileHandle);
			RawMetaData[FileSize] = 0;

			fclose(FileHandle);
			FileHandle = nullptr;

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
		}
	}
}


uint32 FPS4PlatformStackWalk::CaptureStackBackTrace( uint64* BackTrace, uint32 MaxDepth, void* Context )
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


void FPS4PlatformStackWalk::StackWalkAndDump( ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, int32 IgnoreCount, void* Context )
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
		const TCHAR* ExeName = FPS4PlatformProcess::ExecutableName( false );
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


static uint32 ProgramCounterToSymbolNameOffset( uint64 ProgramCounter )
{
	for( uint32 SymbolIndex = 0; SymbolIndex < FPS4PlatformStackWalk::SymbolInfo.Num(); SymbolIndex++ )
	{
		FPS4PlatformStackWalk::FPS4SymbolInfo& SymbolInfo = FPS4PlatformStackWalk::SymbolInfo[ SymbolIndex ];
		if( ProgramCounter >= SymbolInfo.Address && ProgramCounter < (SymbolInfo.Address + SymbolInfo.Size ) )
		{
			return SymbolInfo.NameOffset;
		}
	}

	return INDEX_NONE;
}


static void ReadSymbolNameFromFile(uint32 SymbolNameFileOffset, char* DestStringBuffer, int32 DestStringBufferLength)
{
	FILE* FileHandle = fopen( TCHAR_TO_ANSI(*FPS4PlatformStackWalk::SymbolNamesFileName), "r" );
	if( FileHandle == nullptr )
	{
		FCStringAnsi::Strncpy( DestStringBuffer, "*** UNKNOWN ***", DestStringBufferLength );
		return;
	}

	fseek( FileHandle, SymbolNameFileOffset, SEEK_SET );

	// Length of string is stored as LEB128
	int32 SymbolNameLength = 0;
	int32 Shift = 0;
	uint8 Byte;

	while( true )
	{
		fread( ( void* )&Byte, 1, 1, FileHandle );
		SymbolNameLength |= ( Byte & 0x7f ) << Shift;
		if( ( Byte & 0x80 ) == 0 )
		{
			break;
		}
		Shift += 7;
	}

	SymbolNameLength = FMath::Min( SymbolNameLength, DestStringBufferLength - 1 );

	fread( ( void* )DestStringBuffer, 1, SymbolNameLength, FileHandle );
	DestStringBuffer[ SymbolNameLength ] = 0;

	fclose( FileHandle );
}


static void ReadSymbolNameFromCache(uint32 SymbolNameFileOffset, char* DestStringBuffer, int32 DestStringBufferLength)
{
	if (FPS4PlatformStackWalk::SymbolNamesFileData.Num() == 0)
	{
		return ReadSymbolNameFromFile(SymbolNameFileOffset, DestStringBuffer, DestStringBufferLength);
	}

	const uint8* ReadPos = FPS4PlatformStackWalk::SymbolNamesFileData.GetData() + SymbolNameFileOffset;

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


void FPS4PlatformStackWalk::ProgramCounterToSymbolInfo( uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo )
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


bool FPS4PlatformStackWalk::ProgramCounterToHumanReadableString( int32 CurrentCallDepth, uint64 ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext* Context )
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

			if( NameOffset != INDEX_NONE )
			{
				char SymbolName[256];
				ReadSymbolName( NameOffset, SymbolName, sizeof( SymbolName ) );

				FCStringAnsi::Strncat(HumanReadableString, SymbolName, HumanReadableStringSize);
			}
			else
			{
				FCStringAnsi::Strncat(HumanReadableString, "UnknownFunction", HumanReadableStringSize);
			}

			// No file info, but include this anyway to confirm to requirements
			FCStringAnsi::Strncat(HumanReadableString, " []", HumanReadableStringSize);
		}
	}

	return true;
}

TMap<FName, FString> FPS4PlatformStackWalk::GetSymbolMetaData()
{
	return SymbolMetaData;
}

#undef USE_CACHED_SYMBOL_NAME_RESOLUTION
