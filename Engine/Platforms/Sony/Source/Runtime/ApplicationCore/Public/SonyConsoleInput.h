// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "HAL/Runnable.h"

#if !UE_BUILD_SHIPPING

class FSonyConsoleInputReader : public FRunnable
{
public:
	FSonyConsoleInputReader( int32 StdInHandle );

	virtual uint32 Run() override;

	virtual void Stop() override;

	virtual void Exit() override;

private:

	int32 NewFileHandle;
	FString CommandBuffer;
	int32 EventCount;
	SceKernelEqueue Queue;
};


class FSonyConsoleInputReporter
{
public:
	FSonyConsoleInputReporter( FString&& InCommand );

	void DoTask( ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent );

	static FORCEINLINE TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( SonyConsoleInputReporter, STATGROUP_TaskGraphTasks );
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


class FSonyConsoleInputManager
{
public:
	FSonyConsoleInputManager();

	void Initialize();
	void Finalize();

private:

	FSonyConsoleInputReader* InputReader;
	FRunnableThread* Thread;
};

#endif 