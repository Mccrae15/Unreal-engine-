// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if USING_A3D
#include <audio3d.h>
#endif // #if USING_A3D

#include "CoreMinimal.h"
#include "ModuleManager.h"
#include "IAudioExtensionPlugin.h"

class FA3DSpatialization : public IAudioSpatialization
{
public:
	FA3DSpatialization();
	~FA3DSpatialization();

	/** IAudioSpatialization implementation */
	virtual void Initialize(const FAudioPluginInitializationParams InitializationParams) override;
	virtual void Shutdown() override;
	virtual bool IsSpatializationEffectInitialized() const override { return bIsInitialized; };
	virtual void OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, USpatializationPluginSourceSettingsBase* InSettings) override;
	virtual void OnReleaseSource(const uint32 SourceId) override;
	virtual void ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData);
	virtual void OnAllSourcesProcessed() override;
	/** End of IAudioSpatialization implementation */

private:
#if USING_A3D
	//Audio3D port belonging to this instance of FA3DSpatialization.
	SceAudio3dPortId Audio3dPortID;
	//Array used to map Audio3dObjectIds to UE4's SourceID.
	TArray<SceAudio3dPortId> Audio3dObjectIdArray;
	//Cached pointer to speed up object ID dereferencing:
	SceAudio3dPortId* Audio3dObjectIDsPtr;
#endif // #if USING_A3D
	bool bIsInitialized;
	uint32 SourceIndex;
	uint32 NumActiveSources;
};

class FA3DPluginFactory : public IAudioSpatializationFactory
{
public:
	virtual FString GetDisplayName() override
	{
		static FString DisplayName = FString(TEXT("Sony Audio3D"));
		return DisplayName;
	}

	virtual bool SupportsPlatform(EAudioPlatform Platform) override
	{
		return (Platform == EAudioPlatform::Playstation4);
	}

	/* Audio3D sends object audio to external hardware to be processed. */
	virtual bool IsExternalSend() override { return true; };

	virtual TAudioSpatializationPtr CreateNewSpatializationPlugin(FAudioDevice* OwningDevice) override
	{ 
		return TAudioSpatializationPtr(new FA3DSpatialization());
	};
};

class FA3DModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

private:
	FA3DPluginFactory PluginFactory;
};