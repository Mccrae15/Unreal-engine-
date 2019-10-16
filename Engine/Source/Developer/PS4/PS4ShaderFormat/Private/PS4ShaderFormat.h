// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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

/** Helper class for generating an uncompressed zip archive file. */
class FZipArchiveWriter
{
	struct FFileEntry
	{
		FString Filename;
		uint32 Crc32;
		uint64 Length;
		uint64 Offset;
		uint32 Time;

		FFileEntry(const FString& InFilename, uint32 InCrc32, uint64 InLength, uint64 InOffset, uint32 InTime)
			: Filename(InFilename)
			, Crc32(InCrc32)
			, Length(InLength)
			, Offset(InOffset)
			, Time(InTime)
		{}
	};

	TArray<FFileEntry> Files;

	TArray<uint8> Buffer;
	IFileHandle* File;

	inline void Write(uint16 V) { Write((void*)&V, sizeof(V)); }
	inline void Write(uint32 V) { Write((void*)&V, sizeof(V)); }
	inline void Write(uint64 V) { Write((void*)&V, sizeof(V)); }
	inline void Write(void* Src, uint64 Size)
	{
		void* Dst = &Buffer[Buffer.AddUninitialized(Size)];
		FMemory::Memcpy(Dst, Src, Size);
	}
	inline uint64 Tell() { return (File ? File->Tell() : 0) + Buffer.Num(); }
	void Flush();

public:
	FZipArchiveWriter(IFileHandle* InFile);
	~FZipArchiveWriter();

	void AddFile(const FString& Filename, const TArray<uint8>& Data, const FDateTime& Timestamp);
};

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

	void NotifyShaderCooked(const TArray<uint8>& PlatformDebugData);

private:
	void Initialize();
};
#endif

extern FName NAME_SF_PS4;

extern void CompilePSSLShader(const struct FShaderCompilerInput& Input,struct FShaderCompilerOutput& Output,const class FString& WorkingDirectory);
