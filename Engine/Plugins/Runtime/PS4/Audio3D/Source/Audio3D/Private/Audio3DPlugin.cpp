// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "Audio3DPlugin.h"
#include "Features/IModularFeatures.h"

#if USING_A3D
#include <libsysmodule.h>
#include <errno.h>
#include <user_service.h>

static const float DefaultObjectGain = 1.0f;
static const float UnrealUnitsToMeters = 1.0f;

static FORCEINLINE SceAudio3dPosition UnrealToA3dCoordinates(const FVector& Input)
{
	return { UnrealUnitsToMeters * Input.Y, UnrealUnitsToMeters * Input.X, -UnrealUnitsToMeters * Input.Z };
}

#endif // #if USING_A3D



FA3DSpatialization::FA3DSpatialization()
				  : bIsInitialized(false)
				  , SourceIndex(0)
				  , NumActiveSources(0)
{
}

FA3DSpatialization::~FA3DSpatialization()
{
	Shutdown();
}

void FA3DSpatialization::Initialize(const FAudioPluginInitializationParams InitializationParams)
{

#if USING_A3D
	check(InitializationParams.BufferLength % 256 == 0);
	NumActiveSources = 0;
	SourceIndex = 0;

	//If we haven't already opened a port here, open a new one.
	if (!bIsInitialized)
	{
		SceAudio3dOpenParameters Audio3dParams;
		sceAudio3dGetDefaultOpenParameters(&Audio3dParams);
		Audio3dParams.uiGranularity = InitializationParams.BufferLength;
		Audio3dParams.uiMaxObjects = InitializationParams.NumSources;
		Audio3dParams.uiQueueDepth = 4;

		int32 Result = sceAudio3dPortOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, &Audio3dParams, &Audio3dPortID);
		UE_LOG(LogInit, Log, TEXT("Audio3d Port Open: %d with buffer length %u"), Result, InitializationParams.BufferLength);
		check(Result == SCE_OK);

		sceAudio3dPortFlush(Audio3dPortID);
		sceAudio3dPortAdvance(Audio3dPortID);
		sceAudio3dPortPush(Audio3dPortID, SCE_AUDIO3D_BLOCKING_ASYNC);
	}
	 
	Audio3dObjectIdArray.Init(SCE_AUDIO3D_OBJECT_INVALID, InitializationParams.NumSources);
	Audio3dObjectIDsPtr = Audio3dObjectIdArray.GetData();
#endif // #if USING_A3D

	bIsInitialized = true;
}

void FA3DSpatialization::Shutdown()
{
#if USING_A3D
	if (bIsInitialized == true)
	{
		sceAudio3dPortClose(Audio3dPortID);
	}
#endif // #if USING_A3D
}


void FA3DSpatialization::OnInitSource(const uint32 SourceId, const FName& AudioComponentUserId, USpatializationPluginSourceSettingsBase* InSettings)
{
#if USING_A3D
	int32 Result = sceAudio3dObjectReserve(Audio3dPortID, &(Audio3dObjectIDsPtr[SourceId]));
	check(Result == SCE_OK);
	NumActiveSources++;
#endif // #if USING_A3D
}

void FA3DSpatialization::OnReleaseSource(const uint32 SourceId)
{
#if USING_A3D
	int32 Result = sceAudio3dObjectUnreserve(Audio3dPortID, Audio3dObjectIDsPtr[SourceId]);
	check(Result == SCE_OK);
	Audio3dObjectIDsPtr[SourceId] = SCE_AUDIO3D_OBJECT_INVALID;
	NumActiveSources--;
#endif // #if USING_A3D
}

void FA3DSpatialization::ProcessAudio(const FAudioPluginSourceInputData& InputData, FAudioPluginSourceOutputData& OutputData)
{
#if USING_A3D
	SceAudio3dAttribute Audio3dObjectAttributes[3];
	SceAudio3dPcm Audio3dPcmHandle;
	

	// Attach the audio buffer to an Audio3D attribute
	Audio3dPcmHandle.eFormat = SCE_AUDIO3D_FORMAT_FLOAT;
	Audio3dPcmHandle.pSampleBuffer = InputData.AudioBuffer->GetData();
	Audio3dPcmHandle.uiNumSamples = InputData.AudioBuffer->Num();

	// Attach the source position to an Audio3D attribute
	SceAudio3dPosition Audio3dObjectPosition = UnrealToA3dCoordinates(InputData.SpatializationParams->EmitterPosition);

	// Set up our array of Audio3d attributes:
	Audio3dObjectAttributes[0].uiAttributeId = SCE_AUDIO3D_OBJECT_ATTRIBUTE_PCM;
	Audio3dObjectAttributes[0].pValue = &Audio3dPcmHandle;
	Audio3dObjectAttributes[0].szValue = sizeof(Audio3dPcmHandle);
	Audio3dObjectAttributes[1].uiAttributeId = SCE_AUDIO3D_OBJECT_ATTRIBUTE_POSITION;
	Audio3dObjectAttributes[1].pValue = &Audio3dObjectPosition;
	Audio3dObjectAttributes[1].szValue = sizeof(Audio3dObjectPosition);
	Audio3dObjectAttributes[2].uiAttributeId = SCE_AUDIO3D_OBJECT_ATTRIBUTE_GAIN;
	Audio3dObjectAttributes[2].pValue = &DefaultObjectGain;
	Audio3dObjectAttributes[2].szValue = sizeof(DefaultObjectGain);

	// Send the attribute array to the Audio3D renderer with the associated Audio3dObjectId
	sceAudio3dObjectSetAttributes(Audio3dPortID, Audio3dObjectIDsPtr[InputData.SourceId], 3, Audio3dObjectAttributes);
#endif // #if USING_A3D
}

void FA3DSpatialization::OnAllSourcesProcessed()
{
#if USING_A3D
	sceAudio3dPortAdvance(Audio3dPortID);
	sceAudio3dPortPush(Audio3dPortID, SCE_AUDIO3D_BLOCKING_ASYNC);
#endif //USING_A3D
}

void FA3DModule::StartupModule()
{

	IModularFeatures::Get().RegisterModularFeature(FA3DPluginFactory::GetModularFeatureName(), &PluginFactory);

#if USING_A3D
	int32 Result;

#if PLATFORM_PS4
	// Load the neccesary dynamic libraries
	Result = sceSysmoduleLoadModule(SCE_SYSMODULE_AUDIO_3D);
	check(Result == SCE_OK);
#endif // #if PLATFORM_PS4

	// Initialize Audio3D
	Result = sceAudio3dInitialize(0);
	check(Result == SCE_OK);
#endif // #if USING_A3D
}

void FA3DModule::ShutdownModule()
{
#if USING_A3D
	//Shut down Audio3D
	int32 Result = sceAudio3dTerminate();
	check(Result == SCE_OK);

#if PLATFORM_PS4
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_AUDIO_3D) == true)
	{
		//Unload the associated library.
		Result = sceSysmoduleUnloadModule(SCE_SYSMODULE_AUDIO_3D);
		check(Result == SCE_OK);
	}
#endif

#endif // USING_A3D
}

IMPLEMENT_MODULE(FA3DModule, Audio3D)