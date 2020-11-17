// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyPlatformTime.h"
#include "CoreGlobals.h"
#include "Misc/Timespan.h"
#include "Misc/DateTime.h"
#include <kernel.h>
#include <rtc.h>

double FSonyPlatformTime::InvFrequency = 0.0000000006274579;

namespace /*unnamed*/{
	inline void GetTimeInline(const SceRtcDateTime& DateTime, int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec)
	{
		Year = DateTime.year;
		Month = DateTime.month;
		Day = DateTime.day;
		Hour = DateTime.hour;
		Min = DateTime.minute;
		Sec = DateTime.second;
		MSec = DateTime.microsecond / 1000;
		// don't use our input vars, they may alias the same memory Year=Secs.
		FTimespan hms(DateTime.hour, DateTime.minute, DateTime.second);
		FDateTime ymd(DateTime.year, DateTime.month, DateTime.day);
		FDateTime Date = ymd + hms;
		DayOfWeek = static_cast<int32>(Date.GetDayOfWeek());
	}
}


double FSonyPlatformTime::InitTiming()
{	
	uint64 Frequency = sceKernelGetProcessTimeCounterFrequency();
	InvFrequency = 1.0 / (double)Frequency;
	SecondsPerCycle = InvFrequency * (1 << CycleShift);
	SecondsPerCycle64 = InvFrequency;
	return FPlatformTime::Seconds();
}


void FSonyPlatformTime::SystemTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec )
{
	//https://ps4.scedev.net/resources/documents/SDK/3.500/Rtc-Reference/0005.html
	SceRtcDateTime LocalTime = { 0 };

	int Err = SCE_OK;
	if ((Err = sceRtcGetCurrentClockLocalTime(&LocalTime)) != SCE_OK)
	{
		UE_LOG(LogCore, Warning, TEXT("sceRtcGetCurrentClockLocalTime returned 0x08x"), Err);
	}

	GetTimeInline(LocalTime, Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
}


void FSonyPlatformTime::UtcTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec )
{
	//https://ps4.scedev.net/resources/documents/SDK/3.500/Rtc-Reference/0003.html
	//current time (UTC) in Tick format
	SceRtcTick Ticks;
	sceRtcGetCurrentTick(&Ticks);

	//https://ps4.scedev.net/resources/documents/SDK/3.500/Rtc-Reference/0015.html
	//Convert SceRtcTick to SceRtcDateTime
	SceRtcDateTime UniversalTime;
	sceRtcSetTick(&UniversalTime, &Ticks);

	GetTimeInline(UniversalTime, Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
}
