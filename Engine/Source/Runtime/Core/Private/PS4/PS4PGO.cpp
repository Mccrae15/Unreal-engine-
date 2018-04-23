// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Logging/LogMacros.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if ENABLE_PGO_PROFILE

#include <clang/clang_runtime_profile.h>

DEFINE_LOG_CATEGORY_STATIC(LogPGO, Log, Log)

static uint64 FileCounter = 0;

extern "C" void __llvm_profile_reset_counters(void);

static FString PGO_GetOutputDirectory()
{
	FString PGOOutputDirectory;
	if (FParse::Value(FCommandLine::Get(), TEXT("-pgoprofile="), PGOOutputDirectory))
	{
		UE_LOG(LogPGO, Log, TEXT("PGO output directory: %s"), *PGOOutputDirectory);
		return PGOOutputDirectory;
	}

	// Fallback to the data drive is a directory isn't provided on the command line.
	return TEXT("/data");
}

extern void PGO_ResetCounters()
{
	// Reset all counters, essentially restarting the profile run.

	UE_LOG(LogPGO, Log, TEXT("Resetting PGO counters."));
	__llvm_profile_reset_counters();
}

extern void PGO_WriteFile()
{
	static const FString OutputDirectory = PGO_GetOutputDirectory();
	FString OutputFileName = OutputDirectory + FString::Printf(TEXT("\\%d.profraw"), ++FileCounter);

	UE_LOG(LogPGO, Log, TEXT("Writing out PGO results file: \"%s\"."), *OutputFileName);
	__llvm_profile_set_filename(TCHAR_TO_ANSI(*OutputFileName));

	if (__llvm_profile_write_file() != 0)
	{
		UE_LOG(LogPGO, Error, TEXT("Failed to write output file."));
	}
	else
	{
		UE_LOG(LogPGO, Log, TEXT("PGO results file written successfully."));
	}
	
	// Reset counters after writing a file so we don't count the
	// profiling data twice if another file is written out.
	PGO_ResetCounters();
}

#endif // ENABLE_PGO_PROFILE
