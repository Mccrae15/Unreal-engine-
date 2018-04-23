// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Atrac9Decoder.h"
#include "Atrac9Audioinfo.h"

#include <ajm.h>
#include <ajm/at9_decoder.h>
#include <audioout.h>
#include <libsysmodule.h>
#include <stdint.h>

DEFINE_LOG_CATEGORY(LogAtrac9Decoder);

IMPLEMENT_MODULE(FAtrac9DecoderModule, Atrac9Decoder);

static SceAjmContextId sAudioCodecSystemContextId = SCE_AJM_CONTEXT_INVALID;

/**
* Implementation of FAtrac9DecoderModule
*/

FAtrac9DecoderModule::FAtrac9DecoderModule()
{
}

FAtrac9DecoderModule::~FAtrac9DecoderModule()
{
}

ICompressedAudioInfo* FAtrac9DecoderModule::CreateCompressedAudioInfo()
{
	return new FAtrac9AudioInfo();
}

bool FAtrac9DecoderModule::InitializeAjm()
{
	bool bSuccess = true;

	if (sAudioCodecSystemContextId == SCE_AJM_CONTEXT_INVALID)
	{
		// Initialize libajm
		int32 Result = sceAjmInitialize(0, &sAudioCodecSystemContextId);
		if (Result < 0)
		{
			UE_LOG(LogAtrac9Decoder, Error, TEXT("sceAjmInitialize() failed: 0x%08X (%s)"), Result, sceAjmStrError(Result));
			bSuccess = false;
			goto Term;
		}

		// Register the AT9 codec with libajm
		Result = sceAjmModuleRegister(sAudioCodecSystemContextId, SCE_AJM_CODEC_AT9_DEC, 0);
		if (Result < 0)
		{
			UE_LOG(LogAtrac9Decoder, Error, TEXT("sceAjmModuleRegister() failed: 0x%08X (%s)"), Result, sceAjmStrError(Result));
			bSuccess = false;
			goto Term;
		}

		/**
		 * If this check fails our theoretical max memory footprint is too
		 * large for the ACP to handle.
		 *
		 * This module will have to be extended to determine how many
		 * simultaneous batches can be run and make sure that the different
		 * decoder instances (possibly running on different threads) negotiate
		 * to not run too many batches at once.
		 */
		check(SanityCheckMemoryUsage() && "Atrac9 decoder out of memory");
	}

Term:

	if (!bSuccess && sAudioCodecSystemContextId != SCE_AJM_CONTEXT_INVALID)
	{
		ShutdownAjm();
	}

	return bSuccess;
}

bool FAtrac9DecoderModule::ShutdownAjm()
{
	if (sAudioCodecSystemContextId != SCE_AJM_CONTEXT_INVALID)
	{
		// Loop through any instances created and destroy them
		int32 Result = 0;

		// unregister all the codecs from libajm
		Result = sceAjmModuleUnregister(sAudioCodecSystemContextId, SCE_AJM_CODEC_AT9_DEC);
		if (Result < 0)
		{
			UE_LOG(LogAtrac9Decoder, Error, TEXT("sceAjmModuleRegister() failed: 0x%08X (%s)"), Result, sceAjmStrError(Result));
			return false;
		}

		// finalize libajm
		Result = sceAjmFinalize(sAudioCodecSystemContextId);
		if (Result < 0)
		{
			UE_LOG(LogAtrac9Decoder, Error, TEXT("sceAjmFinalize() failed: 0x%08X (%s)"), Result, sceAjmStrError(Result));
			return false;
		}

		sAudioCodecSystemContextId = SCE_AJM_CONTEXT_INVALID;
	}

	return true;
}

uint32 FAtrac9DecoderModule::GetCodecSystemContextId()
{
	return sAudioCodecSystemContextId;
}

bool FAtrac9DecoderModule::SanityCheckMemoryUsage()
{
	// At most all threads will be running 1 decoder each
	int32 NumInstances = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
	int32 MaxInputBuffer = SCE_AJM_DEC_AT9_MAX_INPUT_BUFFER_SIZE;
	int32 MaxOutputBuffer = SCE_AJM_DEC_AT9_MAX_OUTPUT_BUFFER_SIZE;
	int32 NumChannels = SCE_AJM_DEC_AT9_MAX_CHANNELS;

	TArray<SceAjmInstanceId> Instances;
	Instances.AddDefaulted(NumInstances);

	const uint64_t InstanceFlag = SCE_AJM_INSTANCE_FLAG_MAX_CHANNEL(NumChannels) | SCE_AJM_INSTANCE_FLAG_FORMAT(SCE_AJM_FORMAT_ENCODING_S16);

	for (int32 i = 0; i < NumInstances; ++i)
	{
		int32 Result = sceAjmInstanceCreate(sAudioCodecSystemContextId, SCE_AJM_CODEC_AT9_DEC, InstanceFlag, &Instances[i]);
		if (Result < 0)
		{
			printf("sceAjmInstanceCreate() failed: 0x%08X (%s).", Result, sceAjmStrError(Result));
			return false;
		}
	}

	// Create a batch with all instances running a job to see if the ACP has enough space
	TArray<uint8> BatchBuffer;
	BatchBuffer.AddZeroed(SCE_AJM_JOB_DECODE_SIZE * NumInstances);
	SceAjmBatchInfo BatchInfo;
	sceAjmBatchInitialize(BatchBuffer.GetData(), BatchBuffer.Num(), &BatchInfo);

	uint8 InputBuffer[SCE_AJM_DEC_AT9_MAX_INPUT_BUFFER_SIZE];
	uint8 OutputBuffer[SCE_AJM_DEC_AT9_MAX_OUTPUT_BUFFER_SIZE];
	SceAjmDecodeResult AjmDecodeResult;

	for (SceAjmInstanceId& InstanceId : Instances)
	{
		int32 Result = sceAjmBatchJobDecode(
			&BatchInfo,
			InstanceId,
			&InputBuffer[0],
			MaxInputBuffer,
			&OutputBuffer[0],
			MaxOutputBuffer,
			&AjmDecodeResult);

		if (Result < 0)
		{
			printf("error: sceAjmBatchJobDecode() failed: 0x%08X (%s)\n", Result, sceAjmStrError(Result));
			return false;
		}
	}

	// Now start the batch
	SceAjmBatchId BatchId;
	SceAjmBatchError BatchError;

	int32 Result = sceAjmBatchStart(sAudioCodecSystemContextId, &BatchInfo, SCE_AJM_PRIORITY_GAME_DEFAULT, &BatchError, &BatchId);
	if (Result < 0)
	{
		printf("error: sceAjmBatchStart() failed: 0x%08X (%s)\n", Result, sceAjmStrError(Result));

		if (Result == SCE_AJM_ERROR_MALFORMED_BATCH)
		{
			sceAjmBatchErrorDump(&BatchInfo, &BatchError);
		}

		return false;
	}

	Result = sceAjmBatchWait(sAudioCodecSystemContextId, BatchId, SCE_AJM_WAIT_INFINITE, &BatchError);
	if (Result < 0)
	{
		printf("error: sceAjmBatchWait() failed: 0x%08X (%s)\n", Result, sceAjmStrError(Result));

		if (Result == SCE_AJM_ERROR_MALFORMED_BATCH)
		{
			UE_LOG(LogAtrac9Decoder, Error, TEXT("      detailed error code: %d"), BatchError.iErrorCode);
			sceAjmBatchErrorDump(&BatchInfo, &BatchError);
		}

		return false;
	}

	return true;
}
