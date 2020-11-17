// Copyright Epic Games, Inc. All Rights Reserved.

/**
*/

#include "AudioMixer.h"
#include "AudioMixerPlatformAudioOut.h"
#include "Modules/ModuleManager.h"

class FAudioMixerModuleAudioOut : public IAudioDeviceModule
{
public:
	
	virtual void StartupModule() override
	{
		IAudioDeviceModule::StartupModule();

		FModuleManager::Get().LoadModuleChecked(TEXT("AudioMixerCore"));
	}

	virtual bool IsAudioMixerModule() const override { return true; }

	virtual Audio::IAudioMixerPlatformInterface* CreateAudioMixerPlatformInterface() override
	{
		return new Audio::FMixerPlatformAudioOut();
	}
};

IMPLEMENT_MODULE(FAudioMixerModuleAudioOut, AudioMixerAudioOut);
