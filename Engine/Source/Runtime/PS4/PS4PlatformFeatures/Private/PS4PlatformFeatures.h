// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PlatformFeatures.h"

#ifndef PS4_FEATURE_GAMELIVESTREAMING
#define PS4_FEATURE_GAMELIVESTREAMING 1
#endif

#ifndef PS4_FEATURE_SHAREVIDEO
#define PS4_FEATURE_SHAREVIDEO 1
#endif

#ifndef PS4_FEATURE_SHARESCREENSHOT
#define PS4_FEATURE_SHARESCREENSHOT 1
#endif

#ifndef PS4_FEATURE_SHAREPLAY
#define PS4_FEATURE_SHAREPLAY 1
#endif


class FPS4PlatformFeaturesModule : public IPlatformFeaturesModule
{
public:

	/** Creates a new instance of the audio device implemented by the module. */
	FPS4PlatformFeaturesModule();

	virtual ISaveGameSystem* GetSaveGameSystem() override;
	virtual IDVRStreamingSystem* GetStreamingSystem() override;
	virtual TSharedPtr<const class FJsonObject> GetTitleSettings() override;
	virtual FString GetUniqueAppId() override;
	virtual IVideoRecordingSystem* GetVideoRecordingSystem() override;

private:
	/**
	 * Load global/generic modules, and perform any initialization
	 */
	bool StartupModules();

	/** cached title settings json */
	TSharedPtr<class FJsonObject> TitleSettings;
};


