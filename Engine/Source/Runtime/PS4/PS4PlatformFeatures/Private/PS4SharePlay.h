// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FPS4SharePlay
{
public:
	FPS4SharePlay();
	virtual ~FPS4SharePlay();
	virtual void EnableSharing(bool Enable);

private:
	/**
	 * Loads the library and initializes
	 */
	bool Initialize();

	/**
	 * Unloads the associated library
	 */
	void Shutdown();

private:
	bool bIsInitialized;
	bool bIsEnabled;
};
