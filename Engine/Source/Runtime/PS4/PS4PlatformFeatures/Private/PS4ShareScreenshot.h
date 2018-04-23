// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FPS4ShareScreenshot
{
public:
	FPS4ShareScreenshot();
	virtual ~FPS4ShareScreenshot();
	virtual void EnableScreenshots(bool Enable);

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
