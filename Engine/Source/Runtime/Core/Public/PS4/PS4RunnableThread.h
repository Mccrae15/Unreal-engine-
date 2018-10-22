// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Core/Private/HAL/PThreadRunnableThread.h"
#include <pthread_np.h>


/**
 * PS4 implementation of the Pthreads
 */
class FPS4RunnableThread
	: public FRunnableThreadPThread
{
protected:

	virtual ~FPS4RunnableThread()
	{
		// Call the parent destructor body before the parent does it - see comment on that function for explanation why.
		FRunnableThreadPThread_DestructorBody();
	}

	// FRunnableThreadPThread interface

	virtual int CreateThreadWithName(pthread_t* HandlePtr, pthread_attr_t* AttrPtr, PthreadEntryPoint Proc, void* Arg, const ANSICHAR* Name) override
	{
		// chop the size down to something usable
		char ThreadName[32];
		FCStringAnsi::Strncpy(ThreadName, Name, 31);
		return pthread_create_name_np(HandlePtr, AttrPtr, Proc, Arg, ThreadName);
	}

	virtual int GetDefaultStackSize() override
	{
		// default is 64k as of SDK 0.930. Not enough for some threads (i.e. rendering)
		return 256 * 1024;
	}

	virtual uint32 AdjustStackSize(uint32 InStackSize) override
	{
		// NOTE: switched on PS4 as the thread pool threads are allocating a stack size of 32k by default, but tasks
		// can definitely be larger than that.  Also note, this code means that the minimum stack size is the default
		// stack size of PS4
		// allow the platform to override default stack size
		if (InStackSize < GetDefaultStackSize())
		{
			InStackSize = GetDefaultStackSize();
		}

		return InStackSize;
	}

	virtual int32 TranslateThreadPriority(EThreadPriority Priority) override
	{
		// these are some default priorities
		switch (Priority)
		{
			// 767 is the lowest, 256 is the highest possible priority for pthread on PS4
			case TPri_AboveNormal: return SCE_KERNEL_PRIO_FIFO_HIGHEST + 1;
			case TPri_Normal: return SCE_KERNEL_PRIO_FIFO_DEFAULT;
			case TPri_BelowNormal: return SCE_KERNEL_PRIO_FIFO_LOWEST - 1;
			case TPri_Highest: return SCE_KERNEL_PRIO_FIFO_HIGHEST;
			case TPri_Lowest: return SCE_KERNEL_PRIO_FIFO_LOWEST;
			case TPri_SlightlyBelowNormal: return SCE_KERNEL_PRIO_FIFO_DEFAULT + 1;
			default: UE_LOG(LogHAL, Fatal, TEXT("Unknown Priority passed to FRunnableThreadPThread::TranslateThreadPriority()")); return TPri_Normal;
		}
		
	}

	virtual void SetThreadPriority(pthread_t Thread, EThreadPriority NewPriority)
	{
		// initialize the structure
		struct sched_param Sched;
		FMemory::Memzero(&Sched, sizeof(struct sched_param));

		// set the priority appropriately
		Sched.sched_priority = TranslateThreadPriority(NewPriority);
		pthread_setschedparam(Thread, SCHED_FIFO, &Sched);
	}
};
