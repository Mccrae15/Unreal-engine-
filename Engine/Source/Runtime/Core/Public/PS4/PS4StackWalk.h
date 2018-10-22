// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "CoreTypes.h"

#undef PLATFORM_SUPPORTS_STACK_SYMBOLS
#define PLATFORM_SUPPORTS_STACK_SYMBOLS 1

struct CORE_API FPS4PlatformStackWalk
	: public FGenericPlatformStackWalk
{
	/**
	 * Converts the passed in program counter address to a human readable string and appends it to the passed in one.
	 * @warning: The code assumes that HumanReadableString is large enough to contain the information.
	 *
	 * @param CurrentCallDepth Depth of the call, if known (-1 if not - note that some platforms may not return meaningful information in the latter case).
	 * @param ProgramCounter Address to look symbol information up for.
	 * @param HumanReadableString String to concatenate information with.
	 * @param HumanReadableStringSize Size of the string in characters.
	 * @param Context Pointer to crash context, if any.
	 * @return true if the symbol was found, otherwise false.
	 */
	static void ProgramCounterToSymbolInfo( uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo );

	/**
	 * Capture a stack backtrace and optionally use the passed in exception pointers.
	 *
	 * @param BackTrace [out] Pointer to array to take backtrace.
	 * @param MaxDepth Entries in BackTrace array.
	 * @param Context Optional thread context information.
	 */
	static uint32 CaptureStackBackTrace( uint64* BackTrace, uint32 MaxDepth, void* Context = nullptr );

	/**
	 * Walks the stack and appends the human readable string to the passed in one.
	 * @warning: The code assumes that HumanReadableString is large enough to contain the information.
	 *
	 * @param HumanReadableString String to concatenate information with.
	 * @param HumanReadableStringSize Size of the string in characters.
	 * @param IgnoreCount Number of stack entries to ignore (some are guaranteed to be in the stack walking code).
	 * @param Context Optional thread context information.
	 */ 
	static void StackWalkAndDump( ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, int32 IgnoreCount, void* Context = nullptr );

	/**
	 * Get the index into the symbol info table for the passed in address
	 *
	 * @param ProgramCounter Address to find the Symbol info for.
	 * @return The index into the symbol info table if the symbol was found, otherwise INDEX_NONE.
	 */
	static bool ProgramCounterToHumanReadableString( int32 CurrentCallDepth, uint64 ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext* Context = nullptr );

	/**
	 * Gets the meta-data associated with all symbols of this target.
	 * This may include things that are needed to perform further offline processing of symbol information (eg, the source binary).
	 *
	 * @return	A map containing the meta-data (if any).
	 */
	static TMap<FName, FString> GetSymbolMetaData();

	/** Load the correct symbol info file for the current build configuration. */
	static void LoadSymbolInfo();

	/** Info about each symbol (Address, Size, NameOffset) */
	struct FPS4SymbolInfo
	{
		uint32	Address;
		uint32	Size;
		uint32	NameOffset;
	};

	/** Info about all the symbols */
	static TArray<FPS4SymbolInfo> SymbolInfo;

	/** The filename to use for looking up symbol names */
	static FString SymbolNamesFileName;

	/** Cached symbol name file */
	static TArray<uint8> SymbolNamesFileData;

	/** Cached symbol meta-data */
	static TMap<FName, FString> SymbolMetaData;
};


typedef FPS4PlatformStackWalk FPlatformStackWalk;
