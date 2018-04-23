// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4ShaderFormat.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "PS4ShaderFormat.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "ShaderCore.h"

// Needed for the version info
#include <binary.h>

static FName NAME_SF_PS4(TEXT("SF_PS4"));

class FPS4ShaderFormat : public IShaderFormat
{
	enum
	{
		/** Version for UE portion of the shader format, this becomes part of the DDC key. */
		UE_SHADER_PS4_VER = 12,
	};

public:
	virtual uint32 GetVersion(FName Format) const override
	{
		check(Format == NAME_SF_PS4);
		static uint32 Version = 1;

		// To protect ourselves from rogue machines cooking data with old orbis-psslc versions, we need to query the compiler that's actually running
		// on the cooking machine.  Root SDK versions and binary versions in some of the headers aren't enough as sometimes SDK patches come in without
		// changing these items.
		static bool bParsedVersion = false;
		if (!bParsedVersion)
		{
			int32 ReturnCode = -1;
			FString StdOut;

			// base path to the exe.  It's conceivable that the PATH env var isn't set appropriately.
			TCHAR SDKPath[4096];
			FPlatformMisc::GetEnvironmentVariable(TEXT("SCE_ORBIS_SDK_DIR"), SDKPath, ARRAY_COUNT(SDKPath));

			FString ExePath;
			ExePath = FString::Printf(TEXT("%s/host_tools/bin/orbis-wave-psslc.exe"), SDKPath);

			// Try to read the file version
			IFileManager& FileManager = IFileManager::Get();
			FString CachedVersionDir = FPaths::EngineIntermediateDir() / TEXT("PS4");
			FString CachedVersionPath = CachedVersionDir / ExePath.Replace(TEXT(":"), TEXT("+")).Replace(TEXT("/"), TEXT("+")).Replace(TEXT("\\"), TEXT("+")) + TEXT(".ver");
			if(!FileManager.FileExists(*CachedVersionPath) || !FileManager.FileExists(*ExePath) || FileManager.GetTimeStamp(*CachedVersionPath) < FileManager.GetTimeStamp(*ExePath) || !FFileHelper::LoadFileToString(StdOut, *CachedVersionPath))
			{
				FString ErrorOut;
				FPlatformProcess::ExecProcess(*FString::Printf(TEXT("\"%s\""), *ExePath), TEXT("--version"), &ReturnCode, &StdOut, &ErrorOut);

				// if you can't call psslc you can't compile ps4 shaders.
				if (ReturnCode != 0)
				{
					UE_LOG(LogShaders, Fatal, TEXT("Failed calling orbis-wave-psslc.exe (Path: \"%s\") to get proper shader format version.  Return code: 0x%x"), *ExePath, ReturnCode);
				}

				FileManager.MakeDirectory(*CachedVersionDir);
				FFileHelper::SaveStringToFile(StdOut, *CachedVersionPath);
			}
			
			// rather than try to parse whatever the compiler team decided to printout today, just mush the whole thing together.
			uint32 LargeVersion = FCrc::StrCrc32(StdOut.GetCharArray().GetData(), UE_SHADER_PS4_VER);
			Version = (LargeVersion & 0xFF) ^ (LargeVersion >> 16);

			// for posterity
			UE_LOG(LogShaders, Warning, TEXT("Building PS4 shader with Version: 0x%x orbis-wave-psslc version: %s"), Version, *StdOut);

			bParsedVersion = true;
		}
		check (Version != 0);
		return Version;
	}
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const
	{
		OutFormats.Add(NAME_SF_PS4);
	}
	virtual void CompileShader(FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory) const
	{
		check(Format == NAME_SF_PS4);
		CompilePSSLShader(Input, Output, WorkingDirectory);
	}
};

/**
 * Module for PS4 shaders
 */

static IShaderFormat* Singleton = NULL;

class FPS4ShaderFormatModule : public IShaderFormatModule
{
public:

	virtual ~FPS4ShaderFormatModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual IShaderFormat* GetShaderFormat()
	{
		if (!Singleton)
		{
			// find where we expect the DLLs to be
			TCHAR SDKDir[PLATFORM_MAX_FILEPATH_LENGTH];
			FPlatformMisc::GetEnvironmentVariable(TEXT("SCE_ORBIS_SDK_DIR"), SDKDir, PLATFORM_MAX_FILEPATH_LENGTH);

			// if loading the DLLs fails, then we can't use this ShaderFormat, so reutrn NULL
			void* Handle = FPlatformProcess::GetDllHandle(*FString::Printf(TEXT("%s\\host_tools\\bin\\libSceShaderBinary.dll"), SDKDir));
			if (Handle == NULL)
			{
				return NULL;
			}

			
			{
				Handle = FPlatformProcess::GetDllHandle(*FString::Printf(TEXT("%s\\host_tools\\bin\\libSceShaderWavePsslc.dll"), SDKDir));
				if (Handle == NULL)
				{
					return NULL;
				}
			}

			{
				Handle = FPlatformProcess::GetDllHandle(*FString::Printf(TEXT("%s\\host_tools\\bin\\libSceShaderPerf.dll"), SDKDir));
				if (Handle == NULL)
				{
					return NULL;
				}
			}
			
			Singleton = new FPS4ShaderFormat();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FPS4ShaderFormatModule, PS4ShaderFormat);
