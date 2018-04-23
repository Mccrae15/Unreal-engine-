// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	PS4Properties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "GenericPlatformProperties.h"


/**
 * Implements PS4 platform properties.
 */
struct FPS4PlatformProperties
	: public FGenericPlatformProperties
{
	static FORCEINLINE const char* GetPhysicsFormat( )
	{
		return "PhysXPC";
	}

	static FORCEINLINE bool HasEditorOnlyData( )
	{
		return false;
	}

	static FORCEINLINE const char* PlatformName()
	{
		return "PS4";
	}

	static FORCEINLINE const char* IniPlatformName()
	{
		return "PS4";
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

	static FORCEINLINE bool SupportsLowQualityLightmaps()
	{
		return false;
	}

	static FORCEINLINE bool SupportsBuildTarget( EBuildTargets::Type BuildTarget )
	{
		return (BuildTarget == EBuildTargets::Game);
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
