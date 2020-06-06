// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Event.h"
#include "HAL/PThreadEvent.h"
#include "HAL/PlatformProcess.h"
#include "HAL/CriticalSection.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Misc/ScopeLock.h"


/**
 * An implementation of FEvent that just busy wait.
 * The purpose is to avoid over subscribing system resources.
 */
class FPS4UserSpaceEvent : public FEvent
{
	static constexpr float SleepDuration = 0.000001f;

	bool bInitialized;
	bool bPendingDestroy;
	bool bManualReset;
	TAtomic<int32> bTriggered;
	TAtomic<int32> NumWaitingThreads;

	void FlushWaitingThreads() const
	{
		while (NumWaitingThreads.Load(EMemoryOrder::Relaxed) > 0)
		{
			FPlatformProcess::Sleep(SleepDuration);
		}
	}

public:
	FPS4UserSpaceEvent()
		: bInitialized(false)
		, bPendingDestroy(false)
		, bManualReset(false)
		, bTriggered(0)
		, NumWaitingThreads(0)
	{}

	virtual ~FPS4UserSpaceEvent()
	{
		// Any wait after this point will assert
		bInitialized = false;

		bPendingDestroy = true;

		FlushWaitingThreads();
	}

	virtual bool Create(bool bInManualReset = false) override
	{
		check(!bInitialized && !NumWaitingThreads);

		bPendingDestroy = false;
		bManualReset = bInManualReset;
		bTriggered = 0;
		NumWaitingThreads = 0;
		bInitialized = true;

		return true;
	}

	virtual bool IsManualReset() override
	{
		return bManualReset;
	}

	virtual void Trigger() override
	{
		TriggerForStats();

		check(bInitialized);

		bTriggered.Store(1);
	}

	virtual void Reset() override
	{
		ResetForStats();

		check(bInitialized);

		bTriggered.Store(0);
	}

	virtual bool Wait(uint32 WaitTime = (uint32)-1, const bool bIgnoreThreadIdleStats = false) override;
};

/**
 * An implementation of FEvent that only allocates a single FPThreadEvent per thread.
 * The purpose is to avoid over subscribing system resources.
 */
class FPS4TlsPThreadEvent : public FEvent
{
public:
	FPS4TlsPThreadEvent()
		: bInitialized(false)
		, bManualReset(false)
		, Triggered(TRIGGERED_NONE)
		, NumWaitingThreads(0)
	{}

	virtual ~FPS4TlsPThreadEvent()
	{
		if (bInitialized)
		{
			CS.Lock();
			bInitialized = false;
			bManualReset = true;
			CS.Unlock();

			Trigger();

			while (NumWaitingThreads > 0)
			{
				FPlatformProcess::Sleep(SleepDuration);
			}
		}
	}

	virtual bool Create(bool bInManualReset = false) override
	{
		check(!bInitialized && !ThreadsWaitingOnMe.Num() && !NumWaitingThreads);

		bManualReset = bInManualReset;
		Triggered = TRIGGERED_NONE;
		bInitialized = true;

		return true;
	}

	virtual bool IsManualReset() override
	{
		return bManualReset;
	}

	virtual void Trigger() override
	{
		TriggerForStats();

		check(bInitialized);

		FScopedSpinLock Lock2(CS);

		if (bManualReset)
		{
			Triggered = TRIGGERED_ALL;

			{
				FScopeLock Lock1(&ThreadEventsCS);
				
				// Wake all waiting threads
				for (int32 Idx = 0; Idx < ThreadsWaitingOnMe.Num(); ++Idx)
				{
					const ThreadIdType ThreadId = ThreadsWaitingOnMe[Idx];
					FPThreadEvent* ThreadEvent = ThreadEvents.FindChecked(ThreadId);
					ThreadEvent->Trigger();
				}
			}

			ThreadsWaitingOnMe.Reset();
		}
		else
		{
			// Wake one thread
			if (ThreadsWaitingOnMe.Num() > 0)
			{
				const ThreadIdType ThreadId = ThreadsWaitingOnMe[0];
				ThreadsWaitingOnMe.RemoveAt(0, 1, false);

				FPThreadEvent* ThreadEvent;
				{
					FScopeLock Lock1(&ThreadEventsCS);
					ThreadEvent = ThreadEvents.FindChecked(ThreadId);
				}

				ThreadEvent->Trigger();
			}
			else
			{
				Triggered = TRIGGERED_ONE;
			}
		}
	}

	virtual void Reset() override
	{
		ResetForStats();

		check(bInitialized);

		FScopedSpinLock Lock(CS);

		Triggered = TRIGGERED_NONE;
	}

	virtual bool Wait(uint32 WaitTime = (uint32)-1, const bool bIgnoreThreadIdleStats = false) override;

	typedef uint32 ThreadIdType;

private:
	static constexpr float SleepDuration = 0.000001f;

	class FSpinLock
	{
		TAtomic<int32> bLocked;

	public:
		FSpinLock()
			: bLocked(0)
		{}

		void Lock()
		{
			for (;;)
			{
				if (!bLocked.Load(EMemoryOrder::Relaxed))
				{
					int32 Expected = 0;
					if (bLocked.CompareExchange(Expected, 1))
					{
						break;
					}
				}
				FPlatformProcess::Sleep(SleepDuration);
			}
		}

		void Unlock()
		{
			bLocked = 0;
		}
	};

	class FScopedSpinLock
	{
		FSpinLock& Inner;

	public:
		FScopedSpinLock(FSpinLock& InLock)
			: Inner(InLock)
		{
			Inner.Lock();
		}

		~FScopedSpinLock()
		{
			Inner.Unlock();
		}
	};

	enum TriggerType
	{
		TRIGGERED_NONE,
		TRIGGERED_ONE,
		TRIGGERED_ALL
	};

	static FCriticalSection ThreadEventsCS;
	static TMap<ThreadIdType, FPThreadEvent*> ThreadEvents;

	bool bInitialized;
	bool bManualReset;

	volatile TriggerType Triggered;
	volatile int32 NumWaitingThreads;

	FSpinLock CS;
	TArray<ThreadIdType> ThreadsWaitingOnMe;
};
