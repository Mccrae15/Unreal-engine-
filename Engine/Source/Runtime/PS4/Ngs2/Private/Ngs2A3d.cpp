// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Ngs2A3D.h"
#include "Ngs2Device.h"

#if A3D
void Audio3dService::Init()
{
	// Load audio3d Module
	int32 Ret = sceSysmoduleLoadModule(SCE_SYSMODULE_AUDIO_3D);
	checkf(Ret == SCE_OK, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_AUDIO_3D) failed: 0x%x\n"), Ret);
	// Initialize Audio 3D
	Ret = sceAudio3dInitialize(0);
	checkf(Ret == SCE_OK, TEXT("sceAudio3dInitialize failed: 0x%x\n"), Ret);

	bIsInitialized = true;
}

void Audio3dService::Terminate()
{	
	if (bIsInitialized == false)
	{
		return;
	}

	int32 Ret;

	// Terminate Audio 3D
	Ret = sceAudio3dTerminate();
	checkf(Ret == SCE_OK, TEXT("sceAudio3dTerminate failed; 0x%x"), Ret);
	
	// Unload Audio 3D module
	Ret = sceSysmoduleUnloadModule(SCE_SYSMODULE_AUDIO_3D);
	checkf(Ret == SCE_OK, TEXT("sceSysmoduleUnloadModule failed: 0x%x"), Ret);
	
	bIsInitialized = false;	
}


// Audio 3D module setup
int32_t Audio3dUtils::A3dModuleSetup(SceNgs2UserFx2SetupContext* Context)
{
	A3dModuleWork *work = (A3dModuleWork*)Context->work;
	work->objectId = SCE_AUDIO3D_OBJECT_INVALID;
	return SCE_OK;
}

// Audio 3D module process
int32_t Audio3dUtils::A3dModuleProcess(SceNgs2UserFx2ProcessContext* Context)
{
	uint32_t numAttributes = 0;
	SceAudio3dAttribute aAttribute[5];
	SceAudio3dPortId portId = ((SceAudio3dPortId)(Context->userData));
	int32_t result;
	SceAudio3dPosition pos;
	SceAudio3dPcm pcm;

	const A3dModuleParam *param = (A3dModuleParam*)Context->param;
	A3dModuleWork *work = (A3dModuleWork*)Context->work;

	// Reserve
	if (Context->flags & SCE_NGS2_USER_FX_FLAG_RESET)
	{
		if (work->objectId == SCE_AUDIO3D_OBJECT_INVALID)
		{
			result = sceAudio3dObjectReserve(portId, &work->objectId);
			if (result < SCE_OK)
			{
				UE_LOG(LogAudio, Log, TEXT("Error : sceAudio3dObjectReserve (%d)\n"), result);
			}
		}
	}

	// Update attributes
	if (Context->flags & SCE_NGS2_USER_FX_FLAG_DIRTY)
	{
		pos.fX = param->X;
		pos.fY = param->Y;
		pos.fZ = param->Z;

		aAttribute[numAttributes].uiAttributeId = SCE_AUDIO3D_ATTRIBUTE_POSITION;
		aAttribute[numAttributes].pValue = &pos;
		aAttribute[numAttributes].szValue = sizeof (SceAudio3dPosition);
		numAttributes++;

		aAttribute[numAttributes].uiAttributeId = SCE_AUDIO3D_ATTRIBUTE_GAIN;
		aAttribute[numAttributes].pValue = &param->Gain;
		aAttribute[numAttributes].szValue = sizeof (float);
		numAttributes++;

		aAttribute[numAttributes].uiAttributeId = SCE_AUDIO3D_ATTRIBUTE_SPREAD;
		aAttribute[numAttributes].pValue = &param->Spread;
		aAttribute[numAttributes].szValue = sizeof (float);
		numAttributes++;

		aAttribute[numAttributes].uiAttributeId = SCE_AUDIO3D_ATTRIBUTE_PRIORITY;
		aAttribute[numAttributes].pValue = &param->Priority;
		aAttribute[numAttributes].szValue = sizeof (int);
		numAttributes++;
	}

	// Write
	pcm.eFormat = SCE_AUDIO3D_FORMAT_FLOAT;
	pcm.pSampleBuffer = Context->aChannelData[0];
	pcm.uiNumSamples = Context->numGrainSamples;
	aAttribute[numAttributes].uiAttributeId = SCE_AUDIO3D_ATTRIBUTE_PCM;
	aAttribute[numAttributes].pValue = &pcm;
	aAttribute[numAttributes].szValue = sizeof(pcm);

	numAttributes++;

	// Set Audio 3D object attributes
	result = sceAudio3dObjectSetAttributes(portId, work->objectId, numAttributes, aAttribute);
	if (result < SCE_OK)
	{
		UE_LOG(LogAudio, Log, TEXT("Error : sceAudio3dObjectSetAttributes (%d)\n"), result);
	}

	// Cleanup
	if (Context->flags & SCE_NGS2_USER_FX_FLAG_KILLED)
	{
		if (work->objectId != SCE_AUDIO3D_OBJECT_INVALID)
		{
			result = sceAudio3dObjectUnreserve(portId, work->objectId);
			if (result < SCE_OK)
			{
				UE_LOG(LogAudio, Log, TEXT("Error : sceAudio3dObjectUnreserve (%d)\n"), result);
			}
			work->objectId = SCE_AUDIO3D_OBJECT_INVALID;
		}
	}
	return SCE_OK;
}
#endif