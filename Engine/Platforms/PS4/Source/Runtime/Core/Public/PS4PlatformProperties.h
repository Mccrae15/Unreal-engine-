// Copyright Epic Games, Inc. All Rights Reserved.

/*================================================================================
	PS4Properties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "SonyPlatformProperties.h"


/**
 * Implements PS4 platform properties.
 */
struct FPS4PlatformProperties : public FSonyPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "PS4";
	}

	static FORCEINLINE const char* IniPlatformName()
	{
		return "PS4";
	}

	static FORCEINLINE const TCHAR* GetRuntimeSettingsClassName()
	{
		return TEXT("/Script/PS4PlatformEditor.PS4TargetSettings");
	}

	static FORCEINLINE bool SupportsBuildTarget( EBuildTargetType TargetType )
	{
		return (TargetType == EBuildTargetType::Game);
	}

	static FORCEINLINE bool SupportsMeshLODStreaming()
	{
		return true;
	}

	static FORCEINLINE bool SupportsVirtualTextureStreaming()
	{
		return true;
	}
};

#ifdef PROPERTY_HEADER_SHOULD_DEFINE_TYPE
typedef FPS4PlatformProperties FPlatformProperties;
#endif
