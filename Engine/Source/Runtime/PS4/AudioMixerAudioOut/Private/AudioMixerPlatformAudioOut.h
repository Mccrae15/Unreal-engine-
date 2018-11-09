// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"

// Any platform defines
namespace Audio
{
	class FMixerPlatformAudioOut : public IAudioMixerPlatformInterface
	{
	public:
		FMixerPlatformAudioOut();
		virtual ~FMixerPlatformAudioOut();

		//~ Begin IAudioMixerPlatformInterface
		virtual EAudioMixerPlatformApi::Type GetPlatformApi() const override { return EAudioMixerPlatformApi::AudioOut; }
		virtual bool InitializeHardware() override;
		virtual bool TeardownHardware() override;
		virtual bool IsInitialized() const override;
		virtual bool GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo) override;
		virtual bool GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const override;
		virtual bool OpenAudioStream(const FAudioMixerOpenStreamParams& Params) override;
		virtual bool CloseAudioStream() override;
		virtual bool StartAudioStream() override;
		virtual bool StopAudioStream() override;
		virtual FAudioPlatformDeviceInfo GetPlatformDeviceInfo() const override;
		virtual void SubmitBuffer(const uint8* Buffer) override;
		virtual FName GetRuntimeFormat(USoundWave* InSoundWave) override;
		virtual int32 GetNumFrames(const int32 InNumReqestedFrames) override;
		virtual bool HasCompressedAudioInfoClass(USoundWave* InSoundWave) override { return true; }
		virtual bool SupportsHardwareDecompression() const override { return true; }
		virtual bool SupportsRealtimeDecompression() const override { return true; }
		virtual ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* InSoundWave) override;
		virtual FString GetDefaultDeviceName() override;
		virtual FAudioPlatformSettings GetPlatformSettings() const override;
		virtual uint32 RunInternal() override;
		//~ End IAudioMixerPlatformInterface

	private:
		const TCHAR* GetErrorString(int32 Result);

		int32 AudioOutPortHandle;

		FThreadSafeBool bIsShuttingDown;
		FThreadSafeBool bIsRendering;

		uint32 bIsInitialized : 1;
		uint32 bIsDeviceOpen : 1;
	};

}

