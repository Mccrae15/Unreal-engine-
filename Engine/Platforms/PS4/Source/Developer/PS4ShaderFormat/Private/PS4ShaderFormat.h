// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/FileManagerGeneric.h"

struct FSDB
{
	TArray<uint8> Contents;
	uint64 Hash;

	inline friend FArchive& operator<<(FArchive& Ar, FSDB& SDB)
	{
		Ar << SDB.Hash;
		Ar << SDB.Contents;
		return Ar;
	}

	static inline uint64 ExtensionToHash(const char* Ext)
	{
		uint64 Value = 0;
		for (int32 Index = 1; Index < 17; ++Index)
		{
			char C = Ext[Index];

			if (C >= 'A' && C <= 'F') { Value |= (C - 'A') + 10; }
			else if (C >= 'a' && C <= 'f') { Value |= (C - 'a') + 10; }
			else
			{
				check(C >= '0' && C <= '9');
				Value |= C - '0';
			}

			if (Index < 16)
			{
				Value <<= 4;
			}
		}

		return Value;
	}

	static inline FString HashToExtension(uint64 Hash)
	{
		TCHAR Buffer[22] = {};
		Buffer[0] = '_';

		for (int32_t Index = 1; Index < 17; ++Index, Hash <<= 4)
		{
			uint8_t C = (uint8_t)(Hash >> 60);
			Buffer[Index] = C + ((C < 10) ? '0' : 'a' - 10);
		}

		Buffer[17] = '.';
		Buffer[18] = 's';
		Buffer[19] = 'd';
		Buffer[20] = 'b';

		return Buffer;
	}
};

/**
 * The platform specific debug data structure which is serialized with each shader resource.
 * We use this to store shader SDB data in the DDC, and retrieve it when shaders are cooked.
 */
struct FPS4ShaderDebugData
{
	// Since neo specific shaders are packed with base shaders, we have
	// to allow storing of multiple SDBs per shader compiler output.
	TArray<FSDB, TInlineAllocator<2>> SDBs;

	inline friend FArchive& operator<<(FArchive& Ar, FPS4ShaderDebugData& Data)
	{
		Ar << Data.SDBs;
		return Ar;
	}
};



#if WITH_ENGINE

class FZipArchiveWriter;

class FPS4ShaderSDBExport
{
	bool bExportSDBs;
	FString SDBExportPath;
	uint64 TotalSDBBytes;
	uint64 TotalSDBs;
	TSet<uint64> ExportedShaderHashes;
	FZipArchiveWriter* ZipWriter;

public:
	FPS4ShaderSDBExport();
	~FPS4ShaderSDBExport();

	void NotifyShaderCooked(const TConstArrayView<uint8>& PlatformDebugData);

private:
	void Initialize();
};

#endif

extern FName NAME_SF_PS4;

extern void CompilePSSLShader(const struct FShaderCompilerInput& Input,struct FShaderCompilerOutput& Output,const class FString& WorkingDirectory);
