// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AudioCaptureDeviceInterface.h"
#include "AudioCaptureSony.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

namespace Audio
{
	class FAudioCaptureSonyFactory : public IAudioCaptureFactory
	{
	public:
		virtual TUniquePtr<IAudioCaptureStream> CreateNewAudioCaptureStream() override
		{
			return TUniquePtr<IAudioCaptureStream>(new FAudioCaptureSonyStream());
		}
	};

	class FAudioCaptureSonyModule : public IModuleInterface
	{
	private:
		FAudioCaptureSonyFactory AudioCaptureFactory;

	public:
		virtual void StartupModule() override
		{
			IModularFeatures::Get().RegisterModularFeature(IAudioCaptureFactory::GetModularFeatureName(), &AudioCaptureFactory);
		}

		virtual void ShutdownModule() override
		{
			IModularFeatures::Get().UnregisterModularFeature(IAudioCaptureFactory::GetModularFeatureName(), &AudioCaptureFactory);
		}
	};
}

IMPLEMENT_MODULE(Audio::FAudioCaptureSonyModule, AudioCaptureSony)
