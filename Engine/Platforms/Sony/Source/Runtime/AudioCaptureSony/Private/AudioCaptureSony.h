// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioCaptureCore.h"
#include "AudioMixerNullDevice.h"
#include "SampleBuffer.h"
#include <user_service.h>

namespace Audio
{
	class FAudioCaptureSonyStream : public IAudioCaptureStream
	{
	public:
		FAudioCaptureSonyStream();

		virtual bool RegisterUser(const TCHAR* UserId) override;
		virtual bool UnregisterUser(const TCHAR* UserId) override;
		virtual bool GetCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo, int32 DeviceIndex) override;
		virtual bool OpenCaptureStream(const FAudioCaptureDeviceParams& InParams, FOnCaptureFunction InOnCapture, uint32 NumFramesDesired) override;
		virtual bool CloseStream() override;
		virtual bool StartStream() override;
		virtual bool StopStream() override;
		virtual bool AbortStream() override;
		virtual bool GetStreamTime(double& OutStreamTime) override;
		virtual int32 GetSampleRate() const override { return SampleRate; }
		virtual bool IsStreamOpen() const override;
		virtual bool IsCapturing() const override;
		virtual void OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow) override;
		virtual bool GetInputDevicesAvailable(TArray<FCaptureDeviceInfo>& OutDevices) override;

	private:
		/** Call this to get the list of User Ids. */
		static TArray<SceUserServiceUserId>& GetUserIdList();

		/** Used to lock access to UserIdList. */
		static FCriticalSection UserIdListLock;

		FOnCaptureFunction OnCapture;
		int32 NumChannels;
		int32 SampleRate;

		TUniquePtr<Audio::FMixerNullCallback> InputCallback;

		int32 PortID;
		TArray<int16> VoicePortWorkspace;
		Audio::TSampleBuffer<float> ConversionBuffer;
		bool bStreamStarted;
	};
}