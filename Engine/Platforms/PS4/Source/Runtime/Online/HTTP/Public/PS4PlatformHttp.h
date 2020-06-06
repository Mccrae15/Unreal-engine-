// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SonyPlatformHttp.h"

/**
 * Platform specific Http implementations
 */
class FPS4PlatformHttp
	: public FSonyPlatformHttp
{
public:

	/**
	 * Platform initialization step.
	 *
	 * @see Shutdown
	 */
	static void Init();

	/**
	 * Platform shutdown step.
	 *
	 * @see Init
	 */
	static void Shutdown();
};


typedef FPS4PlatformHttp FPlatformHttp;
