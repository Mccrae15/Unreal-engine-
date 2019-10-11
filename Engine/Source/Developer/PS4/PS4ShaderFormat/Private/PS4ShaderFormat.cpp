// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PS4ShaderFormat.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "ShaderCore.h"

// Needed for the version info
#include <binary.h>

FName NAME_SF_PS4(TEXT("SF_PS4"));

class FPS4ShaderFormat : public IShaderFormat
{
	enum
	{
		/** Version for UE portion of the shader format, this becomes part of the DDC key. */
		UE_SHADER_PS4_VER = 17,
	};

#if WITH_ENGINE
	mutable FPS4ShaderSDBExport SDBExport;
#endif

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
			FString SDKPath = FPlatformMisc::GetEnvironmentVariable(TEXT("SCE_ORBIS_SDK_DIR"));

			FString ExePath;
			ExePath = FString::Printf(TEXT("%s/host_tools/bin/orbis-wave-psslc.exe"), *SDKPath);

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
			UE_LOG(LogShaders, Display, TEXT("Building PS4 shader with Version: 0x%x orbis-wave-psslc version: %s"), Version, *StdOut);

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
	virtual const TCHAR* GetPlatformIncludeDirectory() const
	{
		return TEXT("PS4");
	}

	virtual void ModifyShaderCompilerInput(FShaderCompilerInput& Input) const override
	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PS4ShaderSDBMode"));
		if (CVar)
		{
			Input.Environment.ShaderFormatCVars.Add(TEXT("r.PS4ShaderSDBMode"), CVar->GetString());
		}
	}

#if WITH_ENGINE
	virtual void NotifyShaderCooked(const TArray<uint8>& PlatformDebugData) const override
	{
		SDBExport.NotifyShaderCooked(PlatformDebugData);
	}
#endif
};

/**
 * Module for PS4 shaders
 */

class FPS4ShaderFormatModule : public IShaderFormatModule
{
	FPS4ShaderFormat* Singleton;
	TArray<void*> Handles;

public:
	FPS4ShaderFormatModule()
		: Singleton(nullptr)
	{}

	virtual ~FPS4ShaderFormatModule()
	{
		delete Singleton;
		Singleton = NULL;
	}

	virtual void StartupModule() override
	{
		// find where we expect the DLLs to be
		FString SDKDir = FPlatformMisc::GetEnvironmentVariable(TEXT("SCE_ORBIS_SDK_DIR"));

		// if loading the DLLs fails, then we can't use this ShaderFormat, so return NULL
		const TCHAR* Libs[] =
		{
			TEXT("host_tools\\bin\\libSceShaderBinary.dll"),
			TEXT("host_tools\\bin\\libSceShaderWavePsslc.dll"),
			TEXT("host_tools\\bin\\libSceShaderPerf.dll"),
		};

		Handles.Reset(ARRAY_COUNT(Libs));
		for (int32 Index = 0; Index < ARRAY_COUNT(Libs); ++Index)
		{
			void* Handle = FPlatformProcess::GetDllHandle(*FString::Printf(TEXT("%s\\%s"), *SDKDir, Libs[Index]));
			if (Handle == nullptr)
			{
				// Abort if any dll failed to load.
				return;
			}

			Handles.Add(Handle);
		}

		Singleton = new FPS4ShaderFormat();
	}

	virtual void ShutdownModule() override
	{
		delete Singleton;
		Singleton = nullptr;

		// Release DLL handles in reverse order.
		for (int32 Index = Handles.Num() - 1; Index >= 0; --Index)
		{
			FPlatformProcess::FreeDllHandle(Handles[Index]);
		}
		Handles.Reset();
	}

	virtual IShaderFormat* GetShaderFormat() { return Singleton; }
};

IMPLEMENT_MODULE(FPS4ShaderFormatModule, PS4ShaderFormat);
