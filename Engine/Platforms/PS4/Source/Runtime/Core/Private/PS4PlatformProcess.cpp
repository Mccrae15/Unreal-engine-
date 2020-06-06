// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4PlatformProcess.h"
#include "PS4PlatformEvent.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/SingleThreadEvent.h"
#include "Misc/Parse.h"
#include "HAL/IConsoleManager.h"
#include "Async/TaskGraphInterfaces.h"

#include <system_service.h>

void FPS4PlatformProcess::SetThreadAffinityMask( uint64 InAffinityMask )
{
	uint64 AffinityMask = InAffinityMask & SCE_KERNEL_CPUMASK_7CPU_ALL;
	checkf( AffinityMask != 0, TEXT( "Invalid affinity mask" ) );

	int Ret = scePthreadSetaffinity( scePthreadSelf(), AffinityMask );
	if( Ret == SCE_KERNEL_ERROR_EPERM )
	{
		checkf( false, TEXT("ERROR: A CPU not permitted for usage is specified by the affinity mask. Check that CPU 7 is not trying to be used when 6 CPU Mode is set in the param.sfo\n"));
	}
	check( Ret == SCE_OK );
}

bool FPS4PlatformProcess::CanLaunchURL(const TCHAR* URL)
{
	return false;
}

void FPS4PlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	if (URL)
	{
		if (FCoreDelegates::ShouldLaunchUrl.IsBound() && !FCoreDelegates::ShouldLaunchUrl.Execute(URL))
		{
			if (Error)
			{
				*Error = TEXT("LaunchURL cancelled by delegate");
			}
			return;
		}

		int32 Ret = sceSystemServiceLaunchWebBrowser(TCHAR_TO_ANSI(URL), nullptr);
		if (Ret != SCE_OK)
		{
			UE_LOG(LogSony, Warning, TEXT("Could not open URL: 0x%x, %s"), Ret, URL);
}
	}
}

#if !(UE_BUILD_SHIPPING)

static int32 GMaxNumPThreadEvents = 10000;
static FAutoConsoleVariableRef CVarMaxNumPThreadEvents(
	TEXT("ps4.FileOpenLog.MaxNumPThreadEvents"),
	GMaxNumPThreadEvents,
	TEXT("The maximum allowed number of pthread events when doing file open order log."),
	ECVF_ReadOnly);

static TAtomic<int32> GNumPThreadEvents(0);

FEvent* PS4CreateEventHelper()
{
	static const bool bFileOpenLogActive = FParse::Param(FCommandLine::Get(), TEXT("FileOpenLog"));

	static int32 EventType = -1;
	if (EventType == -1)
	{
		FParse::Value(FCommandLine::Get(), TEXT("PS4EventType="), EventType);
		if (EventType < 0 || EventType > 2)
		{
			EventType = 0;
		}
	}

	// FORT-184304 - We have too many events and eat up too much system memory during file open order log
	if (bFileOpenLogActive)
	{
		if (EventType == 0)
		{
			return new FPS4TlsPThreadEvent;
		}
		else if (EventType == 1)
		{
			if (GNumPThreadEvents.Load(EMemoryOrder::Relaxed) >= GMaxNumPThreadEvents)
			{
				return new FPS4UserSpaceEvent;
			}
			else
			{
				// FEventPool never release events so there is no need to decrement
				++GNumPThreadEvents;
			}
		}
	}

	return new FPThreadEvent;
}

#else

FEvent* PS4CreateEventHelper()
{
	return new FPThreadEvent;
}

#endif

FEvent* FPS4PlatformProcess::CreateSynchEvent(bool bIsManualReset)
{
	FEvent* Event = NULL;
	if (SupportsMultithreading())
	{
		// Allocate the new object
		Event = PS4CreateEventHelper();
	}
	else
	{
		// Fake event.
		Event = new FSingleThreadEvent();
	}
	// If the internal create fails, delete the instance and return NULL
	if (!Event->Create(bIsManualReset))
	{
		delete Event;
		Event = NULL;
	}
	return Event;
}

ENamedThreads::Type FPS4PlatformProcess::GetDesiredThreadForUObjectReferenceCollector()
{
	if (ENamedThreads::bHasHighPriorityThreads)
	{
		if (ENamedThreads::bHasBackgroundThreads) // on the PS4, background threads can use the 7th core, so lets put it to work.
		{
			int32 CoreRand = FMath::RandRange(0, 6);
			if (CoreRand < 2)
			{
				return ENamedThreads::AnyBackgroundThreadNormalTask;
			}
			else if (CoreRand < 4)
			{
				return ENamedThreads::AnyHiPriThreadNormalTask;
			}
		}
		else
		{
			if (FMath::RandRange(0, 1))
			{
				return ENamedThreads::AnyHiPriThreadNormalTask;
			}
		}
	}
	return ENamedThreads::AnyThread;
}

void FPS4PlatformProcess::ModifyThreadAssignmentForUObjectReferenceCollector( int32& NumThreads, int32& NumBackgroundThreads, ENamedThreads::Type& NormalThreadName, ENamedThreads::Type& BackgroundThreadName )
{
#if (USE_7TH_CORE)
	if (NumBackgroundThreads)
	{
		NumBackgroundThreads = 7 - NumThreads;
	}
#else
	if (NumBackgroundThreads)
	{
		NumBackgroundThreads = 6 - NumThreads;
	}
#endif
}


