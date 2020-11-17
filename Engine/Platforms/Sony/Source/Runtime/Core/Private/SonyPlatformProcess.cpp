// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyPlatformProcess.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HAL/PlatformAffinity.h"
#include "Misc/CoreStats.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeLock.h"
#include "CoreGlobals.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include <libdbg.h>
#include <kernel.h>
#include <user_service.h>
#include <system_service.h>

void FSonyPlatformProcess::SetupRenderThread()
{
	SetThreadAffinityMask(FPlatformAffinity::GetRenderingThreadMask());
}

uint32 FSonyPlatformProcess::GetCurrentCoreNumber()
{
	return sceKernelGetCurrentCpu();
}

const TCHAR* FSonyPlatformProcess::ComputerName()
{
	static FString ComputerName;
	if (ComputerName.Len() == 0)
	{
		ComputerName = FString::Printf(TEXT("%s %s"), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()), 
			FPlatformMisc::IsRunningOnDevKit() ? TEXT("Devkit") : TEXT("Kit"));
	}
	return *ComputerName;
}

const TCHAR* FSonyPlatformProcess::UserName(bool bOnlyAlphaNumeric)
{
	static const int32 MaxNameLength = 512;
	static FString Name;
	static ANSICHAR NameAscii[MaxNameLength];
	static bool bHaveResult = false;

	if (!bHaveResult)
	{
		//just in case this is called earlier as part of some kind of caching.
		//double init is ok.
		sceUserServiceInitialize(nullptr);

		SceUserServiceUserId InitialUser;
		int32 Ret = sceUserServiceGetInitialUser(&InitialUser);
		if (Ret == SCE_OK)
		{
			sceUserServiceGetUserName(InitialUser, NameAscii, MaxNameLength);
			Name = FString(ANSI_TO_TCHAR(NameAscii));
		}
		else
		{
			UE_LOG(LogSony, Warning, TEXT("sceUserServiceGetInitialUser failed: 0x%x"), Ret);
			Name = FString(TEXT("UnknownUser"));
		}
		bHaveResult = true;
	}
	return (*Name);
}


const TCHAR* FSonyPlatformProcess::BaseDir()
{
	return TEXT("");
}

const TCHAR* FSonyPlatformProcess::UserSettingsDir()
{
	static const FString UserSettingsDir = FString::Printf(TEXT("/download0/%s/saved/"), FApp::GetProjectName());
	static const FString OtherValue = FPaths::CloudDir();
	return *UserSettingsDir;
}

bool FSonyPlatformProcess::ShouldSaveToUserDir()
{
	/** 
	 * Disabled since /download0/ is unknown and saving various ini is dangerous
	 * (logs / screenshots / profiling saved here also)
	 */
	return false;
}

const TCHAR* FSonyPlatformProcess::ExecutableName(bool bRemoveExtension)
{
	static TCHAR Result[512] = TEXT( "" );
	static TCHAR ResultWithExt[512] = TEXT( "" );

#if !UE_BUILD_SHIPPING 
	if( !Result[0] )
	{
		char ExecutablePath[512];
		int32 PathSize = 0;

		if(FPlatformMisc::IsRunningOnDevKit())
		{ 
			PathSize = sceDbgGetExecutablePath( ExecutablePath, 512 );
		}

		if( PathSize )
		{
			FString FileName = ExecutablePath;
			FString FileNameWithExt = ExecutablePath;
			FCString::Strncpy( Result, *( FPaths::GetBaseFilename( FileName ) ), UE_ARRAY_COUNT( Result ) );
			FCString::Strncpy( ResultWithExt, *( FPaths::GetCleanFilename( FileNameWithExt ) ), UE_ARRAY_COUNT( ResultWithExt ) );
		}
		else
		{
			// If the call failed, zero out the memory to be safe
			FMemory::Memzero( Result, sizeof( Result ) );
			FMemory::Memzero( ResultWithExt, sizeof( ResultWithExt ) );
		}
	}
#endif

	return ( bRemoveExtension ? Result : ResultWithExt );
}

const TCHAR* FSonyPlatformProcess::ApplicationSettingsDir()
{
	static const FString DownloadDir( "/download0/" );
	return *DownloadDir;
}

void FSonyPlatformProcess::Sleep( float Seconds )
{
	SCOPE_CYCLE_COUNTER(STAT_Sleep);
	// sleep is in microseconds
	FThreadIdleStats::FScopeIdle Scope;
	sceKernelUsleep(static_cast<SceKernelUseconds>(Seconds * 1000000.0f));
}

void FSonyPlatformProcess::SleepNoStats( float Seconds )
{
	// sleep is in microseconds	
	sceKernelUsleep(static_cast<SceKernelUseconds>(Seconds * 1000000.0f));
}


void FSonyPlatformProcess::SleepInfinite()
{
	while (1)
	{
		sceKernelUsleep((SceKernelUseconds)-1);
	}
}

void* FSonyPlatformProcess::GetDllHandle(const TCHAR* Filename)
{
	SceKernelModule Module;
	
	// Try loading the module using the passed filename
	Module = sceKernelLoadStartModule(TCHAR_TO_ANSI(Filename), 0, 0, 0, NULL, NULL);
	if( Module > 0 )
	{
		// Module loaded ok
		return reinterpret_cast<void*>(Module);
	}

	// Try loading the module from app0/prx
	FString App0Filename = "/app0/prx/" + FString(Filename);
	// prx filenames are converted to lowercase when copied to /app0/prx/, see CopyPrxDependecies and CopyFile in UEDeploy*.cs for Sony platforms
	App0Filename.ToLowerInline();
	Module = sceKernelLoadStartModule( TCHAR_TO_ANSI(*App0Filename), 0, 0, 0, NULL, NULL );

	if( Module > 0 )
	{
		return reinterpret_cast<void*>(Module);
	}

	return nullptr;

}

void FSonyPlatformProcess::FreeDllHandle( void* DllHandle )
{
	SceKernelModule Module = (SceKernelModule)reinterpret_cast<uint64_t>(DllHandle);

	if( Module > 0 )
	{
		sceKernelStopUnloadModule( Module, 0, NULL, 0, NULL, NULL );
	}
}


#include "SonyPlatformRunnableThread.h"


FRunnableThread* FSonyPlatformProcess::CreateRunnableThread()
{
	return new FSonyRunnableThread();
}
