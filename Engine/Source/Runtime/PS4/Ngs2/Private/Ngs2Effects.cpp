// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Ngs2Effects.h"
#include "Ngs2Support.h"

FNgs2EffectsManager::FNgs2EffectsManager(FNgs2Device* InDevice, int32 NumChannels)
	: FAudioEffectsManager(InDevice)
	, Ngs2Device(InDevice)
{
	//////////////////////
	// reverb setup
	//////////////////////
	ReverbRack = 0;

	// create a rack for performing reverb
	SceNgs2ReverbRackOption ReverbOption;
	sceNgs2ReverbRackResetOption(&ReverbOption);
	ReverbOption.rackOption.maxVoices = 1;
	AudioDevice->ValidateAPICall(TEXT("sceNgs2RackCreateWithAllocator_SCE_NGS2_RACK_ID_REVERB"),
		sceNgs2RackCreateWithAllocator(Ngs2Device->GetPathway(ENgs2Pathway::TV, 0)->Ngs2, SCE_NGS2_RACK_ID_REVERB, &ReverbOption.rackOption, &Ngs2Device->Ngs2Allocator, &ReverbRack));

	// pull the voice out
	AudioDevice->ValidateAPICall(TEXT("sceNgsRackGetVoiceHandle_ReverbRack"),
		sceNgs2RackGetVoiceHandle(ReverbRack, 0, &ReverbVoice));
	Ngs2Device->ReverbVoice = ReverbVoice;

	// set the number of output channels
	// @todo: How many input channels do we want? Most sounds are 1 or 2 channels, is it slower to specify 8 input channels?
	// @todo PS4: Use AJM?
	AudioDevice->ValidateAPICall(TEXT("sceNgs2ReverbVoiceSetup"),
		sceNgs2ReverbVoiceSetup(ReverbVoice, SCE_NGS2_CHANNELS_7_1CH, NumChannels, 0/*SCE_NGS2_SAMPLER_VOICE_SETUP_FLAG_ON_AJM*/));
	
	// set "nop" reverb by default
	SceNgs2ReverbI3DL2Param DefaultReverb = SCE_NGS2_I3DL2_PRESET_DEFAULT;
	AudioDevice->ValidateAPICall(TEXT("sceNgs2ReverbVoiceSetI3DL2"),
		sceNgs2ReverbVoiceSetI3DL2(ReverbVoice, &DefaultReverb));

	// route the reverb into mastering voice
	AudioDevice->ValidateAPICall(TEXT("sceNgs2VoicePatch"),
		sceNgs2VoicePatch(ReverbVoice, 0, Ngs2Device->GetPathway(ENgs2Pathway::TV, 0)->MasteringVoice, 0));

	AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceKickEvent_ReverbPlay"),
		sceNgs2VoiceKickEvent(ReverbVoice, SCE_NGS2_VOICE_EVENT_PLAY));

	//////////////////////
	// EQ (submixer) setup
	//////////////////////
	EQRack = 0;
return;
	// create a rack for performing filtering
	SceNgs2ReverbRackOption SubmixerOption;
	sceNgs2ReverbRackResetOption(&SubmixerOption);
	SubmixerOption.rackOption.maxVoices = 1;

	AudioDevice->ValidateAPICall(TEXT("sceNgs2RackCreateWithAllocator_SCE_NGS2_RACK_ID_SUBMIXER"),
		sceNgs2RackCreateWithAllocator(Ngs2Device->GetPathway(ENgs2Pathway::TV, 0)->Ngs2, SCE_NGS2_RACK_ID_SUBMIXER, &SubmixerOption.rackOption, &Ngs2Device->Ngs2Allocator, &EQRack));

	// pull the voice out
	AudioDevice->ValidateAPICall(TEXT("sceNgsRackGetVoiceHandle_EQRack"),
		sceNgs2RackGetVoiceHandle(EQRack, 0, &EQVoice));
	Ngs2Device->EQVoice = EQVoice;

	// set the number of output channels
	// @todo: How many input channels do we want? Most sounds are 1 or 2 channels, is it slower to specify 8 input channels?
	// @todo PS4: Use AJM?
	AudioDevice->ValidateAPICall(TEXT("sceNgs2ReverbVoiceSetup"),
		sceNgs2SubmixerVoiceSetup(EQVoice, NumChannels, 0/*SCE_NGS2_SAMPLER_VOICE_SETUP_FLAG_ON_AJM*/)); // SCE_NGS2_CHANNELS_7_1CH

	// route the reverb into mastering voice
	AudioDevice->ValidateAPICall(TEXT("sceNgs2VoicePatch"),
		sceNgs2VoicePatch(EQVoice, 0, Ngs2Device->GetPathway(ENgs2Pathway::TV, 0)->MasteringVoice, 0));

	AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceKickEvent_EQPlay"),
		sceNgs2VoiceKickEvent(EQVoice, SCE_NGS2_VOICE_EVENT_PLAY));
}

FNgs2EffectsManager::~FNgs2EffectsManager()
{
	if (ReverbRack)
	{
		sceNgs2RackDestroy(ReverbRack, NULL);
	}
	if (EQRack)
	{
		sceNgs2RackDestroy(EQRack, NULL);
	}
}

void FNgs2EffectsManager::SetReverbEffectParameters(const FAudioReverbEffect& ReverbEffectParameters)
{
	// setup standard I3DL2 struct
	SceNgs2ReverbI3DL2Param I3DL2;
	// all wet (other platforms use 100 for WetDryMix)
	I3DL2.wet = 1.0f;
	I3DL2.dry = 0.0f;
	I3DL2.room = VolumeToMilliBels(ReverbEffectParameters.Volume * ReverbEffectParameters.Gain, 0);
	I3DL2.roomHF = VolumeToMilliBels(ReverbEffectParameters.GainHF, -45);
	// @todo ngs2: It's not clear how reflectionPattern and RoomRolloffFactor equate, but they are in the same "slot"
	I3DL2.reflectionPattern = FMath::Clamp<uint32>(ReverbEffectParameters.RoomRolloffFactor, 0, 2);
	I3DL2.decayTime = ReverbEffectParameters.DecayTime;
	I3DL2.decayHFRatio = ReverbEffectParameters.DecayHFRatio;
	I3DL2.reflections = VolumeToMilliBels(ReverbEffectParameters.ReflectionsGain, 1000);
	I3DL2.reflectionsDelay = ReverbEffectParameters.ReflectionsDelay;
	I3DL2.reverb = VolumeToMilliBels(ReverbEffectParameters.LateGain, 2000);
	I3DL2.reverbDelay = ReverbEffectParameters.LateDelay;
	I3DL2.diffusion = ReverbEffectParameters.Diffusion * 100.0f;
	I3DL2.density = ReverbEffectParameters.Density * 100.0f;
	I3DL2.HFReference = DEFAULT_HIGH_FREQUENCY;

   	AudioDevice->ValidateAPICall(TEXT("sceNgs2ReverbVoiceSetI3DL2"),
   		sceNgs2ReverbVoiceSetI3DL2(ReverbVoice, &I3DL2));
}

void FNgs2EffectsManager::SetEQEffectParameters( const FAudioEQEffect& EQEffectParameters )
{
#if 0
	// TODO: the following code is not working, fix
	check(EQEffectParameters.Bandwidth0 > 0.0f);
	check(EQEffectParameters.Bandwidth1 > 0.0f);
	check(EQEffectParameters.Bandwidth2 > 0.0f);
	check(EQEffectParameters.Bandwidth3 > 0.0f);

	float Q0 = EQEffectParameters.FrequencyCenter0 / EQEffectParameters.Bandwidth0;
	float Q1 = EQEffectParameters.FrequencyCenter1 / EQEffectParameters.Bandwidth1;
	float Q2 = EQEffectParameters.FrequencyCenter2 / EQEffectParameters.Bandwidth2;
	float Q3 = EQEffectParameters.FrequencyCenter3 / EQEffectParameters.Bandwidth3;

	AudioDevice->ValidateAPICall(TEXT("sceNgs2SubmixerVoiceSetFilterFcq"),
								 sceNgs2SubmixerVoiceSetFilterByFcq(EQVoice, 0, SCE_NGS2_SUBMIXER_FILTER_LOCATION_PRE_SEND_FIRST + 1, 
								 SCE_NGS2_FILTER_TYPE_BAND_PASS_A, 0x0, EQEffectParameters.FrequencyCenter0, Q0, EQEffectParameters.Gain0));

	AudioDevice->ValidateAPICall(TEXT("sceNgs2SubmixerVoiceSetFilterFcq"),
								 sceNgs2SubmixerVoiceSetFilterByFcq(EQVoice, 1, SCE_NGS2_SUBMIXER_FILTER_LOCATION_PRE_SEND_FIRST + 2, 
								 SCE_NGS2_FILTER_TYPE_BAND_PASS_A, 0x0, EQEffectParameters.FrequencyCenter1, Q1, EQEffectParameters.Gain1));

	AudioDevice->ValidateAPICall(TEXT("sceNgs2SubmixerVoiceSetFilterFcq"),
								 sceNgs2SubmixerVoiceSetFilterByFcq(EQVoice, 2, SCE_NGS2_SUBMIXER_FILTER_LOCATION_PRE_SEND_FIRST + 3,
								 SCE_NGS2_FILTER_TYPE_BAND_PASS_A, 0x0, EQEffectParameters.FrequencyCenter2, Q2, EQEffectParameters.Gain2));

	AudioDevice->ValidateAPICall(TEXT("sceNgs2SubmixerVoiceSetFilterFcq"),
								 sceNgs2SubmixerVoiceSetFilterByFcq(EQVoice, 3, SCE_NGS2_SUBMIXER_FILTER_LOCATION_PRE_SEND_FIRST + 4,
								 SCE_NGS2_FILTER_TYPE_BAND_PASS_A, 0x0, EQEffectParameters.FrequencyCenter3, Q3, EQEffectParameters.Gain3));

#endif
}

void FNgs2EffectsManager::SetRadioEffectParameters( const FAudioRadioEffect& RadioEffectParameters ) 
{
}
