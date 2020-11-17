// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SonyPlatformMisc.h"

#include <playgo.h>

struct FCustomChunkMapping;
/**
 * PS4 implementation of the misc OS functions
 */
struct CORE_API FPS4PlatformMisc : public FSonyPlatformMisc
{
	static void PlatformInit();
	static bool IsRunningOnDevKit();

	static bool RestartApplication();

	FORCEINLINE static int32 NumberOfCores()
	{
		//machine has 8 cores, but the OS steals 2 for its exclusive use.
		return 6;
	}

	FORCEINLINE static const TCHAR* GetNullRHIShaderFormat()  
	{
		return TEXT("SF_PS4"); 
	}

	FORCEINLINE static const TCHAR* GetPlatformFeaturesModuleName()
	{
		return TEXT("PS4PlatformFeatures");
	}

	static class IPlatformChunkInstall* GetPlatformChunkInstall();
	static bool GetCustomChunkMappings(TArray<FCustomChunkMapping>& Mapping);
	static TArray<FCustomChunk> GetCustomChunksByType(ECustomChunkType DesiredChunkType);
	static TArray<FCustomChunk> GetAllOnDemandChunks();
	static TArray<FCustomChunk> GetAllLanguageChunks();

	static void PopulateChunkPakchunkMapping();
 	static void UpdateChunkPakchunkMapping(uint32 ChunkID);
	static uint32 GetChunkIDFromPakchunkIndex(int32 PakchunkIndex);
	static TArray<FString> GetPakchunkFilesFromChunkID(uint32 ChunkID);

	static FString GetCPUBrand();
	static FString GetPrimaryGPUBrand();
	static void GetOSVersions(FString& out_OSVersionLabel, FString& out_OSSubVersionLabel);
	static bool GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes);
	static bool GetDownloadAreaSizeAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes);

	// Returns a non-zero handle if successfull
	static void PlayGoInit(ScePlayGoHandle& OutHandle);
	static void PlayGoShutdown(ScePlayGoHandle& InOutHandle);

	static ScePlayGoLanguageMask GetPlayGoLanguageMask(const FString& LanguageName);
};

typedef FPS4PlatformMisc FPlatformMisc;
