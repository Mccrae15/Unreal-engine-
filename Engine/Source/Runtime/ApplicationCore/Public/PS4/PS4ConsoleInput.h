// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "HAL/Runnable.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST 

class FPS4ConsoleInputReader : public FRunnable
{
public:
	FPS4ConsoleInputReader( int32 StdInHandle );

	virtual uint32 Run() override;

	virtual void Stop() override;

	virtual void Exit() override;

private:

	int32 NewFileHandle;
	FString CommandBuffer;
	int32 EventCount;
	SceKernelEqueue Queue;
};


class FPS4ConsoleInputReporter
{
public:
	FPS4ConsoleInputReporter( FString&& InCommand );

	void DoTask( ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent );

	static FORCEINLINE TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( PS4ConsoleInputReporter, STATGROUP_TaskGraphTasks );
	}

	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::GameThread;
	}

	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::FireAndForget;
	}

private:
	FString Command;
};


class FPS4ConsoleInputManager
{
public:
	FPS4ConsoleInputManager();

	void Initialize();
	void Finalize();

private:

	FPS4ConsoleInputReader* InputReader;
	FRunnableThread* Thread;
};

#endif 