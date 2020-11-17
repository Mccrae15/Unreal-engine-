// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerPlatformAudioOut.h"
#include "AudioMixer.h"
#include "HAL/PlatformAffinity.h"
#include "HAL/RunnableThread.h"
#include "CoreGlobals.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_ENGINE
#include "Atrac9Decoder.h"
#include "ADPCMAudioInfo.h"
#include "AudioPluginUtilities.h"
#endif //WITH_ENGINE

// AudioOut PS4 library
#include <audioout.h>
#include <user_service.h>
#include <libsysmodule.h>

// Macro to check result code for AudioOut failure, get the string version, log, and goto a cleanup
#define AUDIOOUT_CLEANUP_ON_FAIL(Result)					\
	if (Result != 0)										\
	{														\
		const TCHAR* ErrorString = GetErrorString(Result);	\
		AUDIO_PLATFORM_ERROR(ErrorString);					\
		goto Cleanup;										\
	}

// Macro to check result for AudiOut failure, get string version, log, and return false
#define AUDIOOUT_RETURN_ON_FAIL(Result)						\
	if (Result != 0)										\
	{														\
		const TCHAR* ErrorString = GetErrorString(Result);	\
		AUDIO_PLATFORM_ERROR(ErrorString);					\
		return false;										\
	}

// Macro to simply log an error result
#define AUDIOOUT_LOG_ON_FAIL(Result)						\
	if (Result != 0)										\
	{														\
		const TCHAR* ErrorString = GetErrorString(Result);	\
		AUDIO_PLATFORM_ERROR(ErrorString);					\
	}


namespace Audio
{
	FMixerPlatformAudioOut::FMixerPlatformAudioOut()
		: AudioOutPortHandle(0)
		, bIsInitialized(false)
		, bIsDeviceOpen(false)
	{
	}

	FMixerPlatformAudioOut::~FMixerPlatformAudioOut()
	{
	}

	const TCHAR* FMixerPlatformAudioOut::GetErrorString(int32 Result)
	{
		switch (Result)
		{
			case SCE_AUDIO_OUT_ERROR_NOT_INIT:			return TEXT("SCE_AUDIO_OUT_ERROR_NOT_INIT");
			case SCE_AUDIO_OUT_ERROR_NOT_OPENED:		return TEXT("SCE_AUDIO_OUT_ERROR_NOT_OPENED");
			case SCE_AUDIO_OUT_ERROR_ALREADY_INIT:		return TEXT("SCE_AUDIO_OUT_ERROR_ALREADY_INIT");
			case SCE_AUDIO_OUT_ERROR_BUSY:				return TEXT("SCE_AUDIO_OUT_ERROR_BUSY");
			case SCE_AUDIO_OUT_ERROR_MEMORY:			return TEXT("SCE_AUDIO_OUT_ERROR_MEMORY");
			case SCE_AUDIO_OUT_ERROR_SYSTEM_RESOURCE:	return TEXT("SCE_AUDIO_OUT_ERROR_SYSTEM_RESOURCE");
			case SCE_AUDIO_OUT_ERROR_TRANS_EVENT:		return TEXT("SCE_AUDIO_OUT_ERROR_TRANS_EVENT");
			case SCE_AUDIO_OUT_ERROR_PORT_FULL:			return TEXT("SCE_AUDIO_OUT_ERROR_PORT_FULL");
			case SCE_AUDIO_OUT_ERROR_INVALID_PORT:		return TEXT("SCE_AUDIO_OUT_ERROR_INVALID_PORT");
			case SCE_AUDIO_OUT_ERROR_INVALID_PORT_TYPE: return TEXT("SCE_AUDIO_OUT_ERROR_INVALID_PORT_TYPE");
			case SCE_AUDIO_OUT_ERROR_INVALID_POINTER:	return TEXT("SCE_AUDIO_OUT_ERROR_INVALID_POINTER");
			case SCE_AUDIO_OUT_ERROR_INVALID_SIZE:		return TEXT("SCE_AUDIO_OUT_ERROR_INVALID_SIZE");
			case SCE_AUDIO_OUT_ERROR_INVALID_FORMAT:	return TEXT("SCE_AUDIO_OUT_ERROR_INVALID_FORMAT");
			case SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ: return TEXT("SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ");
			case SCE_AUDIO_OUT_ERROR_INVALID_MIXLEVEL:	return TEXT("SCE_AUDIO_OUT_ERROR_INVALID_MIXLEVEL");
			case SCE_AUDIO_OUT_ERROR_INVALID_FLAG:		return TEXT("SCE_AUDIO_OUT_ERROR_INVALID_FLAG");
			case SCE_AUDIO_OUT_ERROR_INVALID_VOLUME:	return TEXT("SCE_AUDIO_OUT_ERROR_INVALID_VOLUME");

			default:
				return TEXT("UNKNOWN");
		}
	}

	bool FMixerPlatformAudioOut::InitializeHardware()
	{
		if (bIsInitialized)
		{
			AUDIO_PLATFORM_ERROR(TEXT("AudioOut already initialized."));
			return false;
		}

#if WITH_ENGINE
		// Initialize the Ajm library
		FAtrac9DecoderModule::InitializeAjm();
#endif // WITH_ENGINE

		// Initialize the AudioOut library
		int32 Result = sceAudioOutInit();

		// Gotcha: If we are using the Audio3D plugin, AudioOut is initialized before this point.
		if (Result != SCE_AUDIO_OUT_ERROR_ALREADY_INIT)
		{
			AUDIOOUT_RETURN_ON_FAIL(Result);
		}

		bIsInitialized = true;

		return true;
	}

	bool FMixerPlatformAudioOut::TeardownHardware()
	{
		check(bIsRendering == false);

#if WITH_ENGINE
		// Shutdown the at9 decoder module
		FAtrac9DecoderModule::ShutdownAjm();
#endif

		return true;
	}

	bool FMixerPlatformAudioOut::IsInitialized() const
	{
		return bIsInitialized;
	}

	bool FMixerPlatformAudioOut::GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo)
	{
		OutInfo.Name = TEXT("PS4 Audio Device");
		OutInfo.DeviceId = 0;
		OutInfo.NumChannels = 8;
		OutInfo.SampleRate = 48000;
		OutInfo.Format = EAudioMixerStreamDataFormat::Float;

		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::FrontLeft);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::FrontRight);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::FrontCenter);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::LowFrequency);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::SideLeft);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::SideRight);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::BackLeft);
		OutInfo.OutputChannelArray.Add(EAudioMixerChannel::BackRight);

		OutInfo.bIsSystemDefault = true;

		return true;
	}

	bool FMixerPlatformAudioOut::GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const
	{
		OutDefaultDeviceIndex = 0;
		return true;
	}

	// Find closest smallest number of frames supported. Not all frame counts are supported on Ps4.
	static const int32 sSupportedNumFrames[] = { 256, 512, 768, 1024, 1280, 1536, 1792, 2048 };

	int32 FMixerPlatformAudioOut::GetNumFrames(const int32 InNumReqestedFrames)
	{
		return 256;
	}

	bool FMixerPlatformAudioOut::OpenAudioStream(const FAudioMixerOpenStreamParams& Params)
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_ERROR(TEXT("AudioOut was not initialized."));
			return false;
		}

		if (bIsDeviceOpen)
		{
			AUDIO_PLATFORM_ERROR(TEXT("AudioOut audio stream already opened."));
			return false;
		}

		OpenStreamParams = Params;

		AudioStreamInfo.Reset();

		AudioStreamInfo.OutputDeviceIndex = OpenStreamParams.OutputDeviceIndex;
		AudioStreamInfo.NumOutputFrames = OpenStreamParams.NumFrames;
		AudioStreamInfo.NumBuffers = OpenStreamParams.NumBuffers;
		AudioStreamInfo.AudioMixer = OpenStreamParams.AudioMixer;

		if (!GetOutputDeviceInfo(AudioStreamInfo.OutputDeviceIndex, AudioStreamInfo.DeviceInfo))
		{
			return false;
		}

		return true;
	}

	bool FMixerPlatformAudioOut::CloseAudioStream()
	{
		AudioStreamInfo.StreamState = EAudioOutputStreamState::Closed;
		return true;
	}

	bool FMixerPlatformAudioOut::StartAudioStream()
	{
		// Start generating audio with our output source voice
		BeginGeneratingAudio();

		return true;
	}

	bool FMixerPlatformAudioOut::StopAudioStream()
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_ERROR(TEXT("AudioOut was not initialized."));
			return false;
		}

		if (AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped)
		{
			check(AudioStreamInfo.StreamState == EAudioOutputStreamState::Running);
			StopGeneratingAudio();
			check(AudioStreamInfo.StreamState == EAudioOutputStreamState::Stopped);
		}

		return true;
	}

	FAudioPlatformDeviceInfo FMixerPlatformAudioOut::GetPlatformDeviceInfo() const
	{
		return AudioStreamInfo.DeviceInfo;
	}

	void FMixerPlatformAudioOut::SubmitBuffer(const uint8* Buffer)
	{
		int32 NumSamplesRendered = sceAudioOutOutput(AudioOutPortHandle, (const void*)Buffer);
		if (NumSamplesRendered < 0)
		{
			AUDIOOUT_LOG_ON_FAIL(NumSamplesRendered);
		}
	}

	FName FMixerPlatformAudioOut::GetRuntimeFormat(USoundWave* InSoundWave)
	{
#if WITH_ENGINE
		static FName NAME_AT9(TEXT("AT9"));
		static FName NAME_ADPCM(TEXT("ADPCM"));

		if (InSoundWave->IsStreaming(nullptr) && InSoundWave->IsSeekableStreaming())
		{
			return NAME_ADPCM;
		}
		return NAME_AT9;
#else
		checkNoEntry();
		return FName(TEXT("NONE"));
#endif //WITH_ENGINE
	}

	ICompressedAudioInfo* FMixerPlatformAudioOut::CreateCompressedAudioInfo(USoundWave* InSoundWave)
	{
#if WITH_ENGINE
		// this is a runtime check, so pass null for current platform
		if (InSoundWave->IsStreaming(nullptr) && InSoundWave->IsSeekableStreaming())
		{		
			return new FADPCMAudioInfo();
		}
		return FAtrac9DecoderModule::CreateCompressedAudioInfo();
#else
		return nullptr;
#endif // WITH_ENGINE
	}

	FString FMixerPlatformAudioOut::GetDefaultDeviceName()
	{
		return TEXT("Sony Audio Device");
	}

	FAudioPlatformSettings FMixerPlatformAudioOut::GetPlatformSettings() const
	{
#if WITH_ENGINE
		return FAudioPlatformSettings::GetPlatformSettings(FPlatformProperties::GetRuntimeSettingsClassName());
#else
		return FAudioPlatformSettings();
#endif //WITH_ENGINE
	}

	uint32 FMixerPlatformAudioOut::RunInternal()
	{
		// Variable used for results of API calls
		int32 Result = 0;

		// Setup the rendering port
		AudioOutPortHandle = sceAudioOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_AUDIO_OUT_PORT_TYPE_MAIN, 0, AudioStreamInfo.NumOutputFrames, 48000, SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH);

		const int32 ChannelFlag = (SCE_AUDIO_VOLUME_FLAG_FL_CH | SCE_AUDIO_VOLUME_FLAG_FR_CH | SCE_AUDIO_VOLUME_FLAG_CNT_CH | SCE_AUDIO_VOLUME_FLAG_LFE_CH | SCE_AUDIO_VOLUME_FLAG_RL_CH | SCE_AUDIO_VOLUME_FLAG_RR_CH | SCE_AUDIO_VOLUME_FLAG_BL_CH | SCE_AUDIO_VOLUME_FLAG_BR_CH);

		int32 Volume[8];
		for (int32 i = 0; i < 8; ++i)
		{
			Volume[i] = SCE_AUDIO_VOLUME_0dB;
		}

		Result = sceAudioOutSetVolume(AudioOutPortHandle, ChannelFlag, Volume);
		AUDIOOUT_LOG_ON_FAIL(Result);

		OutputBuffers[0].MixNextBuffer();

		// Start immediately processing the next buffer
		check(CurrentBufferReadIndex == INDEX_NONE);
		check(CurrentBufferWriteIndex == INDEX_NONE);

		CurrentBufferReadIndex = 0;
		CurrentBufferWriteIndex = 1;

		while (AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopping)
		{
			RenderTimeAnalysis.Start();

			// Render mixed buffers till our queued buffers are filled up
			while (CurrentBufferReadIndex != CurrentBufferWriteIndex)
			{
				OutputBuffers[CurrentBufferWriteIndex].MixNextBuffer();
				ReadNextBuffer();

				CurrentBufferWriteIndex = (CurrentBufferWriteIndex + 1) % NumOutputBuffers;
			}

			RenderTimeAnalysis.End();
		}

		CurrentBufferReadIndex = INDEX_NONE;
		CurrentBufferWriteIndex = INDEX_NONE;

		// Close the AudioOut port
		Result = sceAudioOutClose(AudioOutPortHandle);
		AUDIOOUT_LOG_ON_FAIL(Result);

		// No longer rendering
		bIsRendering = false;
		bIsShuttingDown = false;

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Stopped;

		return 0;
	}
}