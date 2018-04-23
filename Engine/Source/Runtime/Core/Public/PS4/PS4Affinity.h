// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
PS4Affinity.h: PS4 affinity profile masks definitions.
==============================================================================================*/

#pragma once

#include "GenericPlatformAffinity.h"

class FPS4Affinity : public FGenericPlatformAffinity
{
public:
	// by default let all threads run whenever they can be scheduled. Games can override as necessary based on 
	// game specific measurements.
	static const CORE_API uint64 GetMainGameMask()
	{
		// Keep the game thread on module 0 to avoid cache penalties
		return MAKEAFFINITYMASK4(0,1,2,3);
	}

	static const CORE_API uint64 GetRenderingThreadMask()
	{
		// Keep the render thread on module 1 to avoid cache penalties
		return MAKEAFFINITYMASK2(4,5);
	}

	static const CORE_API uint64 GetRTHeartBeatMask()
	{
		return SCE_KERNEL_CPUMASK_6CPU_ALL;
		//return MAKEAFFINITYMASK1(5);
	}

	static const CORE_API uint64 GetPoolThreadMask()
	{
		return SCE_KERNEL_CPUMASK_6CPU_ALL;
		//return MAKEAFFINITYMASK1(5);
	}

	static const CORE_API uint64 GetTaskGraphThreadMask()
	{
		return SCE_KERNEL_CPUMASK_6CPU_ALL;
		//return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetStatsThreadMask()
	{
		// Stats thread on the 7th core when available
		// Allow at least one other thread because when PS button is pressed the 7th core becomes unavailable.
		// This would cause gamethread to stall out waiting on the stats tick.
#if USE_7TH_CORE
		return MAKEAFFINITYMASK2(5,6);
#else
		return SCE_KERNEL_CPUMASK_6CPU_ALL;
#endif
		//return MAKEAFFINITYMASK1(5);
	}

	static const CORE_API uint64 GetRHIThreadMask()
	{
		return SCE_KERNEL_CPUMASK_6CPU_ALL;
	}

	static const CORE_API uint64 GetAudioThreadMask()
	{
#if USE_7TH_CORE && 0
		// Allow audio thread on the 7th core when available
		return SCE_KERNEL_CPUMASK_7CPU_ALL;
#else
		return SCE_KERNEL_CPUMASK_6CPU_ALL;
#endif
	}

	static const CORE_API uint64 GetNoAffinityMask()
	{
		return SCE_KERNEL_CPUMASK_6CPU_ALL;
	}

	static const CORE_API uint64 GetTaskGraphBackgroundTaskMask()
	{
		// Allow background thread on the 7th core when available
#if USE_7TH_CORE
		return SCE_KERNEL_CPUMASK_7CPU_ALL;
#else
		return SCE_KERNEL_CPUMASK_6CPU_ALL;
#endif
	}
};

typedef FPS4Affinity FPlatformAffinity;