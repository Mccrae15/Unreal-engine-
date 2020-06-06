// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformTime.h"
#include "SonySystemIncludes.h"

/**
 * Sony implementation of the Time OS functions
 */
struct CORE_API FSonyPlatformTime : public FGenericPlatformTime
{
	enum
	{
		CycleShift = 10,
	};
	static double InitTiming();

	static FORCEINLINE double Seconds()
	{
		// @todo Sony: should we add some arbitrary large number like windows does? (look at values from this)
		//Cycles() is normalized to microseconds, so don't use that for 'Seconds' since it will roll over in an hour.
		uint64 Time = sceKernelGetProcessTimeCounter();
		return (double)Time * InvFrequency;
	}

	static FORCEINLINE uint32 Cycles()
	{
		uint64 Time = sceKernelGetProcessTimeCounter() >> CycleShift;
		return (uint32)Time;
	}
	static FORCEINLINE uint64 Cycles64()
	{
		return sceKernelGetProcessTimeCounter();
	}

	static double InvFrequency;

	static void SystemTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec );
	static void UtcTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec );
};


typedef FSonyPlatformTime FPlatformTime;
