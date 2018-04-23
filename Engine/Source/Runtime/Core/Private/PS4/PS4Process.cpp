// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4Process.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HAL/PlatformAffinity.h"
#include "Misc/CoreStats.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeLock.h"
#include "CoreGlobals.h"
#include <libdbg.h>
#include <user_service.h>
#include <system_service.h>

void FPS4PlatformProcess::SetThreadAffinityMask( uint64 InAffinityMask )
{
	uint64 AffinityMask = InAffinityMask & SCE_KERNEL_CPUMASK_7CPU_ALL;

	checkf( AffinityMask != 0, TEXT( "Invalid affinity mask" ) );

	int Ret = scePthreadSetaffinity( scePthreadSelf(), AffinityMask );
	if( Ret == SCE_KERNEL_ERROR_EPERM )
	{
		checkf( false, TEXT("ERROR: A CPU not permitted for usage is specificed by the affinity mask. Check that CPU 7 is not trying to be used when 6 CPU Mode is set in the param.sfo\n"));
	}
	check( Ret == SCE_OK );
}

void FPS4PlatformProcess::SetupRenderThread()
{
	SetThreadAffinityMask(FPlatformAffinity::GetRenderingThreadMask());
}

const TCHAR* FPS4PlatformProcess::ComputerName()
{
	if (FPS4Misc::IsRunningOnDevKit())
	{
		return TEXT("PS4 Devkit");
	}
	else
	{
		return TEXT("PS4 kit");
	}	
}

const TCHAR* FPS4PlatformProcess::UserName(bool bOnlyAlphaNumeric)
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
			UE_LOG(LogPS4, Warning, TEXT("sceUserServiceGetInitialUser failed: 0x%x"), Ret);
			Name = FString(TEXT("UnknownUser"));
		}
		bHaveResult = true;
	}
	return (*Name);
}


const TCHAR* FPS4PlatformProcess::BaseDir()
{
	return TEXT("");
}


const TCHAR* FPS4PlatformProcess::ExecutableName(bool bRemoveExtension)
{
	static TCHAR Result[512] = TEXT( "" );
	static TCHAR ResultWithExt[512] = TEXT( "" );

#if !UE_BUILD_SHIPPING 
	if( !Result[0] )
	{
		char ExecutablePath[512];
		int32 PathSize = 0;

		if( FPS4Misc::IsRunningOnDevKit() )
		{ 
			PathSize = sceDbgGetExecutablePath( ExecutablePath, 512 );
		}

		if( PathSize )
		{
			FString FileName = ExecutablePath;
			FString FileNameWithExt = ExecutablePath;
			FCString::Strncpy( Result, *( FPaths::GetBaseFilename( FileName ) ), ARRAY_COUNT( Result ) );
			FCString::Strncpy( ResultWithExt, *( FPaths::GetCleanFilename( FileNameWithExt ) ), ARRAY_COUNT( ResultWithExt ) );
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

const TCHAR* FPS4PlatformProcess::ApplicationSettingsDir()
{
	static const FString DownloadDir( "/download0/" );
	return *DownloadDir;
}

void FPS4PlatformProcess::Sleep( float Seconds )
{
	SCOPE_CYCLE_COUNTER(STAT_Sleep);
	// sleep is in microseconds
	FThreadIdleStats::FScopeIdle Scope;
	sceKernelUsleep(Seconds * 1000000.0);
}

bool FPS4PlatformProcess::CanLaunchURL(const TCHAR* URL)
{
	return true;
}

void FPS4PlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	if (URL)
	{
		int32 Ret = sceSystemServiceLaunchWebBrowser(TCHAR_TO_ANSI(URL), nullptr);
		if (Ret != SCE_OK)
		{
			UE_LOG(LogPS4, Warning, TEXT("Could not open URL: 0x%x, %s"), Ret, URL);
		}
	}	
}

void FPS4PlatformProcess::SleepNoStats( float Seconds )
{
	// sleep is in microseconds	
	sceKernelUsleep(Seconds * 1000000.0);
}


void FPS4PlatformProcess::SleepInfinite()
{
	while (1)
	{
		sceKernelUsleep((SceKernelUseconds)-1);
	}
}

void* FPS4PlatformProcess::GetDllHandle(const TCHAR* Filename)
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
	Module = sceKernelLoadStartModule( TCHAR_TO_ANSI(*App0Filename), 0, 0, 0, NULL, NULL );

	if( Module > 0 )
	{
		return reinterpret_cast<void*>(Module);
	}

	return nullptr;

}

void FPS4PlatformProcess::FreeDllHandle( void* DllHandle )
{
	SceKernelModule Module = (SceKernelModule)reinterpret_cast<uint64_t>(DllHandle);

	if( Module > 0 )
	{
		sceKernelStopUnloadModule( Module, 0, NULL, 0, NULL, NULL );
	}
}


#include "PS4RunnableThread.h"


FRunnableThread* FPS4PlatformProcess::CreateRunnableThread()
{
	return new FPS4RunnableThread();
}
