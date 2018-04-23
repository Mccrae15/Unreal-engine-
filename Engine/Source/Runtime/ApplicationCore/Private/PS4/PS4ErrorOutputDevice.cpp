// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4ErrorOutputDevice.h"
#include "CoreGlobals.h"
#include "HAL/PlatformMisc.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/OutputDeviceRedirector.h"

FPS4ErrorOutputDevice::FPS4ErrorOutputDevice()
{
	LogOutputDevice = TEXT("LogOutputDevice");
}

void FPS4ErrorOutputDevice::Serialize(const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	FPlatformMisc::LowLevelOutputDebugString(*FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Msg, GPrintLogTimes));

	if (GIsGuarded)
	{
		UE_DEBUG_BREAK();
	}
	else
	{
		HandleError();
		FPlatformMisc::RequestExit(true);
	}
}

void FPS4ErrorOutputDevice::HandleError()
{
	// make sure we don't report errors twice
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("HandleError re-entered."));
		return;
	}

	GIsGuarded = 0;
	GIsRunning = 0;
	GIsCriticalError = 1;
	GLogConsole = NULL;
	GErrorHist[ARRAY_COUNT(GErrorHist) - 1] = 0;

	// Dump the error and flush the log.
#if !NO_LOGGING
	FDebug::LogFormattedMessageWithCallstack(LogOutputDevice, __FILE__, __LINE__, TEXT("=== Critical error: ==="), GErrorHist, ELogVerbosity::Error);
#endif

	GLog->PanicFlushThreadedLogs();
}
