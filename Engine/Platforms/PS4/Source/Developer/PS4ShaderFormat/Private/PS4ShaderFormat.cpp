// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4ShaderFormat.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "ShaderCore.h"

// Needed for the version info
#include <shader/binary.h>

FName NAME_SF_PS4(TEXT("SF_PS4"));

class FPS4ShaderFormat : public IShaderFormat
{
	enum
	{
		/** Version for UE portion of the shader format, this becomes part of the DDC key. */
		UE_SHADER_PS4_VER = 18,
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
			// base path to the SDK.  It's conceivable that the PATH env var isn't set appropriately.
			FString SDKPath = FPlatformMisc::GetEnvironmentVariable(TEXT("SCE_ORBIS_SDK_DIR"));

			FString DllPath;
			DllPath = FString::Printf(TEXT("%s/host_tools/bin/libSceShaderWavePsslc.dll"), *SDKPath);

			uint64 DllVersion = FPlatformMisc::GetFileVersion(DllPath);
			Version = uint32(((DllVersion >> 32) ^ DllVersion) & 0xffffffff) ^ uint32(UE_SHADER_PS4_VER);

			// for posterity
			UE_LOG(LogShaders, Display, TEXT("Building PS4 shader with Version: 0x%x libSceShaderWavePsslc.dll version: 0x%llx"), Version, DllVersion);

			bParsedVersion = true;
		}
		check (Version != 0);
		return Version;
	}

	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override
	{
		OutFormats.Add(NAME_SF_PS4);
	}

	virtual void CompileShader(FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory) const override
	{
		check(Format == NAME_SF_PS4);
		CompilePSSLShader(Input, Output, WorkingDirectory);
	}
	virtual const TCHAR* GetPlatformIncludeDirectory() const override
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
	virtual void NotifyShaderCooked(const TConstArrayView<uint8>& PlatformDebugData, FName Format) const override
	{
		check(Format == NAME_SF_PS4);
		SDBExport.NotifyShaderCooked(PlatformDebugData);
	}
#endif

	virtual void AppendToKeyString(FString& KeyString) const override
	{
		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PS4MixedModeShaderDebugInfo"));
			if (CVar && CVar->GetValueOnAnyThread() != 0)
			{
				KeyString += TEXT("_MMDBG");
			}
		}

		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PS4ShaderSDBMode"));
			switch (CVar ? CVar->GetValueOnAnyThread() : 0)
			{
			case 1: KeyString += TEXT("_SDB1"); break;
			case 2: KeyString += TEXT("_SDB2"); break;
			default: break;
			}
		}

		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PS4UseTTrace"));
			if (CVar && CVar->GetValueOnAnyThread() > 0)
			{
				KeyString += FString::Printf(TEXT("TT%d"), CVar->GetValueOnAnyThread());
			}
		}
	}
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

		Handles.Reset(UE_ARRAY_COUNT(Libs));
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Libs); ++Index)
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
