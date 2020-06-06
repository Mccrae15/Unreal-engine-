// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SonyPlatformProcess.h"

/**
 * PS4 platform implementation of the Process OS functions
 */
struct CORE_API FPS4PlatformProcess : public FSonyPlatformProcess
{
	static void SetThreadAffinityMask(uint64 AffinityMask);

	static bool CanLaunchURL(const TCHAR* URL);
	static void LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error);

	static class FEvent* CreateSynchEvent(bool bIsManualReset = false);

	static ENamedThreads::Type GetDesiredThreadForUObjectReferenceCollector();
	static void ModifyThreadAssignmentForUObjectReferenceCollector( int32& NumThreads, int32& NumBackgroundThreads, ENamedThreads::Type& NormalThreadName, ENamedThreads::Type& BackgroundThreadName );
};

typedef FPS4PlatformProcess FPlatformProcess;
