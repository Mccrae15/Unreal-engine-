// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformProperties.h"


/**
 * Implements Sony platform properties.
 */
struct FSonyPlatformProperties : public FGenericPlatformProperties
{
	static FORCEINLINE const char* GetPhysicsFormat( )
	{
		return "PhysXPC";
	}

	static FORCEINLINE bool HasEditorOnlyData( )
	{
		return false;
	}

	static FORCEINLINE bool IsGameOnly()
	{
		return true;
	}

	static FORCEINLINE bool IsClientOnly()
	{
		return !WITH_SERVER_CODE;
	}

	static FORCEINLINE bool RequiresCookedData()
	{
		return true;
	}

	static FORCEINLINE bool HasSecurePackageFormat()
	{
		return true;
	}

	static FORCEINLINE bool SupportsLowQualityLightmaps()
	{
		return false;
	}

	static FORCEINLINE bool SupportsBuildTarget( EBuildTargetType BuildTarget )
	{
		return (BuildTarget == EBuildTargetType::Game);
	}

	static FORCEINLINE bool SupportsAutoSDK()
	{
		return true;
	}

	static FORCEINLINE bool SupportsGrayscaleSRGB()
	{
		return false; // Requires expand from G8 to RGBA
	}

	static FORCEINLINE bool HasFixedResolution()
	{
		return true;
	}

	static FORCEINLINE bool AllowsFramerateSmoothing()
	{
		return true;
	}

	static FORCEINLINE bool AllowsCallStackDumpDuringAssert()
	{
		return true;
	}

	static FORCEINLINE bool SupportsAudioStreaming()
	{
		return !IsServerOnly();
	}
};
