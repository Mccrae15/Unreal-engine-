// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/**
*/

#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "AudioMixerPlatformAudioOut.h"


class FAudioMixerModuleAudioOut : public IAudioDeviceModule
{
public:
	virtual bool IsAudioMixerModule() const override { return true; }

	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new Audio::FMixerDevice(new Audio::FMixerPlatformAudioOut());
	}
};

IMPLEMENT_MODULE(FAudioMixerModuleAudioOut, AudioMixerAudioOut);


