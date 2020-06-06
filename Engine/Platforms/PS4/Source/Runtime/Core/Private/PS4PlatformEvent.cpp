// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4PlatformEvent.h"
#include "Misc/CoreStats.h"
#include "ProfilingDebugging/CsvProfiler.h"

bool FPS4UserSpaceEvent::Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats)
{
	WaitForStats();

	SCOPE_CYCLE_COUNTER(STAT_EventWait);
	CSV_SCOPED_WAIT_CONDITIONAL(WaitTime > 0 && IsInGameThread());
	FThreadIdleStats::FScopeIdle Scope(bIgnoreThreadIdleStats);

	++NumWaitingThreads;

	check(bInitialized);

	double WaitTimeSecs = WaitTime / 1000.;
	double StartTime;

	const bool bNotPollOrInifiniteWait = WaitTime > 0 && WaitTime != (uint32)-1;
	if (bNotPollOrInifiniteWait)
	{
		StartTime = FPlatformTime::Seconds();
	}

	bool bRet = false;

	for (;;)
	{
		if (bPendingDestroy)
		{
			bRet = true;
			break;
		}

		if (bManualReset)
		{
			if (bTriggered.Load(EMemoryOrder::Relaxed))
			{
				bRet = true;
				break;
			}
		}
		else
		{
			int32 Expected = 1;
			if (bTriggered.Load(EMemoryOrder::Relaxed) && bTriggered.CompareExchange(Expected, 0))
			{
				bRet = true;
				break;
			}
		}

		if (bNotPollOrInifiniteWait)
		{
			const double Now = FPlatformTime::Seconds();
			WaitTimeSecs -= Now - StartTime;
			StartTime = Now;
		}

		if (WaitTimeSecs > 0.)
		{
			FPlatformProcess::Sleep(SleepDuration);
		}
		else
		{
			break;
		}
	}

	--NumWaitingThreads;

	return bRet || bPendingDestroy;
}

FCriticalSection FPS4TlsPThreadEvent::ThreadEventsCS;
TMap<FPS4TlsPThreadEvent::ThreadIdType, FPThreadEvent*> FPS4TlsPThreadEvent::ThreadEvents;

bool FPS4TlsPThreadEvent::Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats)
{
	WaitForStats();

	SCOPE_CYCLE_COUNTER(STAT_EventWait);
	CSV_SCOPED_WAIT_CONDITIONAL(WaitTime > 0 && IsInGameThread());
	FThreadIdleStats::FScopeIdle Scope(bIgnoreThreadIdleStats);

	check(bInitialized);

	bool bRet = false;

	FScopedSpinLock Lock1(CS);

	if (Triggered == TRIGGERED_ALL)
	{
		bRet = true;
	}
	else if (Triggered == TRIGGERED_ONE)
	{
		Triggered = TRIGGERED_NONE;
		bRet = true;
	}
	else if (WaitTime != 0)
	{
		const ThreadIdType CurThreadId = FPlatformTLS::GetCurrentThreadId();
		FPThreadEvent* ThreadEvent;
		{
			FScopeLock Lock2(&ThreadEventsCS);
			FPThreadEvent** EventPtr = ThreadEvents.Find(CurThreadId);
			if (!EventPtr)
			{
				ThreadEvent = new FPThreadEvent;
				verify(ThreadEvent->Create(false));
				EventPtr = &ThreadEvents.Add(CurThreadId, ThreadEvent);
			}
			ThreadEvent = *EventPtr;
			check(ThreadEvent);
		}

		++NumWaitingThreads;
		ThreadsWaitingOnMe.Add(CurThreadId);

		CS.Unlock();
		bRet = ThreadEvent->Wait(WaitTime, bIgnoreThreadIdleStats);
		CS.Lock();

		// If this thread is waked after timeout, treat as successful wait
		if (!bRet && !ThreadsWaitingOnMe.Remove(CurThreadId))
		{
			ThreadEvent->Reset();
			bRet = true;
		}

		--NumWaitingThreads;
		check(NumWaitingThreads >= 0);
	}

	return bRet;
}
