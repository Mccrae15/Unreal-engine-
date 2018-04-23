// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Atrac9AudioInfo.h"
#include "Atrac9Decoder.h"

#include "IAudioFormat.h"
#include "Sound/SoundWave.h"
#include "AudioDecompress.h"
#include "ContentStreaming.h"

#include <ajm.h>
#include <ajm/at9_decoder.h>
#include <audioout.h>
#include <libsysmodule.h>
#include <stdint.h>


/** The AT9 Subformat codec ID. Used to identify this file as an actual At9 file. */
static const uint8_t At9CodecId[16] = {
	// 0x47E142D2, 0x36BA, 0x4D8D, 0x88FC61654F8C836C
	0xD2, 0x42, 0xE1, 0x47, 0xBA, 0x36, 0x8D, 0x4D, 0x88, 0xFC, 0x61, 0x65, 0x4F, 0x8C, 0x83, 0x6C
};


#define AT9_LOG_HEADER_PARSE_RESULTS 0
#define AT9_TEST_SINE_TONE 0
#define AT9_USE_DUMP_COMMAND 0

// Wrapper around the ajm error dump function to allow cleaner splicing out.
static void AjmDumpError(const SceAjmBatchInfo * const pInfo, SceAjmBatchError * const pBatchError)
{
#if AT9_USE_DUMP_COMMAND
	sceAjmBatchErrorDump(pInfo, pBatchError);
#endif
}

#if AT9_TEST_SINE_TONE
struct FSineTest
{
	FSineTest(float Frequency = 440.0f)
		: CurrPhase(0.0f)
	{
		static int32 Id = 1;
		PhaseDelta = (2.0f * PI * Frequency * Id++) / 48000;
	}

	float GetValue()
	{
		float Result = FMath::Sin(CurrPhase);
		CurrPhase += PhaseDelta;
		CurrPhase = FMath::Fmod(CurrPhase, 2.0f * PI);
		return Result;
	}

	float CurrPhase;
	float PhaseDelta;
};

static FSineTest SineTest[2];
#endif

FAtrac9AudioInfo::FAtrac9AudioInfo()
	: FramesPerBlock(0)
	, SamplesPerBlock(0)
	, TotalAt9Frames(0)
	, SamplesLastFrame(0)
#if AT9_BENCHMARK
	, TotalProcessTime(0)
	, MaxProcessTime(0)
	, BatchCount(0)
#endif
{
}

FAtrac9AudioInfo::~FAtrac9AudioInfo()
{
#if AT9_BENCHMARK
	if (BatchCount > 0)
	{
		uint32 AvgProcessTime = TotalProcessTime / BatchCount;
		UE_LOG(LogAtrac9Decoder, Warning,
			TEXT("At9Benchmark: %d, %d, %d, %d, %d, %llu"),
			FramesPerBlock,
			SamplesPerBlock,
			TotalAt9Frames,
			AvgProcessTime,
			MaxProcessTime,
			TotalProcessTime);
	}
#endif

	// Make sure we're done using the AjmInstance before destroying
	FScopeLock Lock(&DecodeCriticalSection);

	const SceAjmContextId AudioCodecSystemContextId = FAtrac9DecoderModule::GetCodecSystemContextId();
	
	const int32 Result = sceAjmInstanceDestroy(AudioCodecSystemContextId, InstanceId);
	if (Result < 0)
	{
		printf("sceAjmInstanceDestroy() failed: 0x%08X (%s).", Result, sceAjmStrError(Result));
	}
}

bool FAtrac9AudioInfo::ParseHeader(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, FSoundQualityInfo* QualityInfo)
{
	SrcBufferData = InSrcBufferData;
	SrcBufferDataSize = InSrcBufferDataSize;
	SrcBufferOffset = 0;
	CurrentSampleCount = 0;

	if (!ParseWaveFormatHeader(InSrcBufferData, InSrcBufferDataSize, Header))
	{
		return false;
	}

	CONSTEXPR uint16 FORMAT_EXTENSIBLE = 0xFFFE;

	if (Header.FmtChunk.FormatTag != FORMAT_EXTENSIBLE)
	{
		UE_LOG(LogAtrac9Decoder, Error, TEXT("Unknown At9 format tag ID (%X)."), Header.FmtChunk.FormatTag);
		return false;
	}

	// Make sure we got an AT9 codec
	if (FMemory::Memcmp(At9CodecId, Header.FmtChunk.SubFormat, sizeof(At9CodecId)))
	{
		// Get the codec ID that we found for the error log
		FString CodecId;
		for (int32 i = 0; i < sizeof(Header.FmtChunk.SubFormat); ++i)
		{
			CodecId += FString::Printf(TEXT(" %02x"), Header.FmtChunk.SubFormat[i]);
		}

		UE_LOG(LogAtrac9Decoder, Error, TEXT("Unknown At9 codec ID (%s)."), *CodecId);
		return false;
	}

	// Set the current src buffer offset. This increments whenever more data is decoded.
	SrcBufferOffset = Header.DataStartOffset;

	// Setting this allows us to restart the audio stream for looped audio
	AudioDataOffset = SrcBufferOffset;

	// Write out the the header info into the QualityInfo struct
	if (QualityInfo)
	{
		check(Header.FmtChunk.SamplesPerSec > 0);
		QualityInfo->SampleRate = Header.FmtChunk.SamplesPerSec;
		QualityInfo->NumChannels = Header.FmtChunk.NumChannels;
		QualityInfo->SampleDataSize = Header.FactChunk.TotalSamples * Header.FmtChunk.NumChannels * sizeof(int16);
		QualityInfo->Duration = (float)Header.FactChunk.TotalSamples / Header.FmtChunk.SamplesPerSec;
	}

	// Copy to the decoder since QualityInfo is optionally passed in
	SampleRate = Header.FmtChunk.SamplesPerSec;
	NumChannels = Header.FmtChunk.NumChannels;
	TrueSampleCount = Header.FactChunk.TotalSamples;

	int32 FrameSamples = 0;
	switch (SampleRate)
	{
		case 48000:
			FrameSamples = SCE_AJM_DEC_AT9_MAX_FRAME_SAMPLES;
			break;
		case 24000:
			FrameSamples = SCE_AJM_DEC_AT9_MAX_FRAME_SAMPLES / 2;
			break;
		case 12000:
			FrameSamples = SCE_AJM_DEC_AT9_MAX_FRAME_SAMPLES / 4;
			break;
		default:
			checkf(false, TEXT("Unsupported sample rate in AT9 file."));
			return false;
	}
	check(FrameSamples > 0);

	// If super frames are being used, this will be 4, 1 otherwise.
	SamplesPerBlock = Header.FmtChunk.SamplesPerBlock;
	FramesPerBlock = SamplesPerBlock / FrameSamples;

	// Compute the total number of At9 frames in the At9 file. We will use this to determine if we're done
	// decoding the entire AT9 file.
	uint32 SamplesPerFrame = SamplesPerBlock / FramesPerBlock;

	// This is taken from sample code, results in 2 more samples than I'd expect but the first
	// frame produces no output due to codec delay.
	TotalAt9Frames = (TrueSampleCount + (SamplesPerFrame - 1)) / SamplesPerFrame + 1;

	// This will be used to return the true number of samples in the last frame (it zero-pads output otherwise)
	SamplesLastFrame = TrueSampleCount % SamplesPerFrame;

	return true;
}

int32 FAtrac9AudioInfo::GetFrameSize()
{
	// AT9 blocksize is constant and read in when parsing header.
	// This block may contain 4 actual frames if FramesPerBlock is 4
	return Header.FmtChunk.BlockAlign;
}

uint32 FAtrac9AudioInfo::GetMaxFrameSizeSamples() const
{
	return SCE_AJM_DEC_AT9_MAX_FRAME_SAMPLES * FramesPerBlock * NumChannels;
}

bool FAtrac9AudioInfo::CreateDecoder()
{
	FScopeLock Lock(&DecodeCriticalSection);

	// Get the singleton codec system context ID
	const SceAjmContextId AudioCodecSystemContextId = FAtrac9DecoderModule::GetCodecSystemContextId();

	// Now create the decode instance
	const uint64_t InstanceFlag = SCE_AJM_INSTANCE_FLAG_MAX_CHANNEL(NumChannels) | SCE_AJM_INSTANCE_FLAG_FORMAT(SCE_AJM_FORMAT_ENCODING_S16);
	int32 Result = sceAjmInstanceCreate(AudioCodecSystemContextId, SCE_AJM_CODEC_AT9_DEC, InstanceFlag, &InstanceId);
	if (Result < 0)
	{
		printf("sceAjmInstanceCreate() failed: 0x%08X (%s).", Result, sceAjmStrError(Result));
		return false;
	}

	SceAjmBatchError BatchError;
	SceAjmDecAt9InitializeParameters InitParams;
	SceAjmInitializeResult InitResults;

	// Copy the AT9 configuration data to the init params
	FMemory::Memcpy(InitParams.uiConfigData, Header.FmtChunk.ConfigData, sizeof(Header.FmtChunk.ConfigData));

	AjmBatchBuffer.Reset();
	AjmBatchBuffer.AddZeroed(SCE_AJM_JOB_INITIALIZE_SIZE);

	// Queue a batch initialize job
	SceAjmBatchInfo BatchInfo;
	sceAjmBatchInitialize(AjmBatchBuffer.GetData(), AjmBatchBuffer.Num(), &BatchInfo);

	// Queue up a bunch of initialize jobs for each instance id
	Result = sceAjmBatchJobInitialize(&BatchInfo, InstanceId, &InitParams, sizeof(SceAjmDecAt9InitializeParameters), &InitResults);
	if (Result < 0)
	{
		printf("sceAjmBatchJobInitialize() failed: 0x%08X (%s).", Result, sceAjmStrError(Result));
		return false;
	}

	// Start the batch job
	SceAjmBatchId BatchId;
	Result = sceAjmBatchStart(AudioCodecSystemContextId, &BatchInfo, SCE_AJM_PRIORITY_GAME_DEFAULT, &BatchError, &BatchId);
	if (Result < 0)
	{
		printf("sceAjmBatchStart() failed: 0x%08X (%s).", Result, sceAjmStrError(Result));
		if (Result == SCE_AJM_ERROR_MALFORMED_BATCH)
		{
			UE_LOG(LogAtrac9Decoder, Error, TEXT("      detailed error code: %d"), BatchError.iErrorCode);
			AjmDumpError(&BatchInfo, &BatchError);
		}
		return false;
	}

	Result = sceAjmBatchWait(AudioCodecSystemContextId, BatchId, SCE_AJM_WAIT_INFINITE, &BatchError);
	if (Result < 0)
	{
		printf("sceAjmBatchWait() failed: 0x%08X (%s).", Result, sceAjmStrError(Result));
		if (Result == SCE_AJM_ERROR_MALFORMED_BATCH)
		{
			UE_LOG(LogAtrac9Decoder, Error, TEXT("      detailed error code: %d"), BatchError.iErrorCode);
			AjmDumpError(&BatchInfo, &BatchError);
		}
		return false;
	}

	// Check the result code of the "sideband result". If non-zero then something went wrong, but not necessarily fatal.
	if (InitResults.sResult.iResult != 0)
	{
		FString LogString = FString::Printf(TEXT("AjmJob: iResult (0%08X) \"%s\", iInternalResult (0x%08X) \"%s\""),
			InitResults.sResult.iResult,
			sceAjmStrError(InitResults.sResult.iResult),
			InitResults.sResult.iInternalResult,
			sceAjmStrError(InitResults.sResult.iInternalResult));

		// if fatal error, log as error then return false
		if (InitResults.sResult.iResult < 0)
		{
			UE_LOG(LogAtrac9Decoder, Error, TEXT("%s"), *LogString);
			return false;
		}

		// Continue on, but log warning
		UE_LOG(LogAtrac9Decoder, Warning, TEXT("%s"), *LogString);
	}

	return true;
}

FDecodeResult FAtrac9AudioInfo::Decode(const uint8* CompressedData, const int32 CompressedDataSize, uint8* OutPCMData, const int32 OutputPCMDataSize)
{
	FScopeLock Lock(&DecodeCriticalSection);

	const SceAjmContextId AudioCodecSystemContextId = FAtrac9DecoderModule::GetCodecSystemContextId();

	// Prepare the batch buffer
	AjmBatchBuffer.Reset();
	AjmBatchBuffer.AddZeroed(SCE_AJM_JOB_DECODE_SIZE);

	// Initialize the batch info struct
	SceAjmBatchInfo BatchInfo;
	sceAjmBatchInitialize(AjmBatchBuffer.GetData(), AjmBatchBuffer.Num(), &BatchInfo);

	SceAjmDecodeResult AjmDecodeResult;

	int32 JobInputSize = FMath::Min(SCE_AJM_DEC_AT9_MAX_INPUT_BUFFER_SIZE, CompressedDataSize);
	int32 JobOutputSize = FMath::Min(SCE_AJM_DEC_AT9_MAX_OUTPUT_BUFFER_SIZE, OutputPCMDataSize);

	bool bError = false;

	int32 Result = sceAjmBatchJobDecode(
		&BatchInfo,
		InstanceId,
		CompressedData,
		JobInputSize,
		OutPCMData,
		JobOutputSize,
		&AjmDecodeResult);

	if (Result < 0)
	{
		bError = true;
		printf("error: sceAjmBatchJobDecode() failed: 0x%08X (%s)\n", Result, sceAjmStrError(Result));
	}

	// Now start the batch
	SceAjmBatchId BatchId;
	SceAjmBatchError BatchError;

#if AT9_BENCHMARK
	uint32 StartTime = sceKernelGetProcessTime();
#endif

	Result = sceAjmBatchStart(AudioCodecSystemContextId, &BatchInfo, SCE_AJM_PRIORITY_GAME_DEFAULT, &BatchError, &BatchId);
	if (Result < 0)
	{
		bError = true;
		printf("error: sceAjmBatchStart() failed: 0x%08X (%s)\n", Result, sceAjmStrError(Result));

		if (Result == SCE_AJM_ERROR_MALFORMED_BATCH)
		{
			sceAjmBatchErrorDump(&BatchInfo, &BatchError);
		}
	}

	Result = sceAjmBatchWait(AudioCodecSystemContextId, BatchId, SCE_AJM_WAIT_INFINITE, &BatchError);
	if (Result < 0)
	{
		bError = true;
		printf("error: sceAjmBatchWait() failed: 0x%08X (%s)\n", Result, sceAjmStrError(Result));

		if (Result == SCE_AJM_ERROR_MALFORMED_BATCH)
		{
			UE_LOG(LogAtrac9Decoder, Error, TEXT("      detailed error code: %d"), BatchError.iErrorCode);
			sceAjmBatchErrorDump(&BatchInfo, &BatchError);
		}
	}

#if AT9_BENCHMARK
	if (!bError)
	{
		uint32 EndTime = sceKernelGetProcessTime();
		uint32 ProcessTime = EndTime - StartTime;
		TotalProcessTime += ProcessTime;
		if (ProcessTime > MaxProcessTime)
		{
			MaxProcessTime = ProcessTime;
		}
		BatchCount++;
	}
#endif

	FDecodeResult DecodeResult;
	DecodeResult.NumCompressedBytesConsumed = AjmDecodeResult.sStream.iSizeConsumed;
	DecodeResult.NumPcmBytesProduced = AjmDecodeResult.sStream.iSizeProduced;
	DecodeResult.NumAudioFramesProduced = DecodeResult.NumPcmBytesProduced / (NumChannels * sizeof(int16));

	return DecodeResult;
}

void FAtrac9AudioInfo::PrepareToLoop()
{
	FScopeLock Lock(&DecodeCriticalSection);

	AjmBatchBuffer.Reset();
	AjmBatchBuffer.AddZeroed(SCE_AJM_JOB_DECODE_SIZE);

	SceAjmBatchInfo BatchInfo;
	sceAjmBatchInitialize(AjmBatchBuffer.GetData(), AjmBatchBuffer.Num(), &BatchInfo);

	// Queue up clear context jobs for each instance id
	SceAjmClearContextResult ClearContextResult;
	int32 Result = sceAjmBatchJobClearContext(&BatchInfo, InstanceId, &ClearContextResult);
	if (Result < 0)
	{
		UE_LOG(LogAtrac9Decoder, Error, TEXT("sceAjmBatchJobClearContext() failed: 0x%08X (%s)\n"), Result, sceAjmStrError(Result));
		return;
	}

	const SceAjmContextId AudioCodecSystemContextId = FAtrac9DecoderModule::GetCodecSystemContextId();

	SceAjmBatchError BatchError;
	SceAjmBatchId BatchId;
	Result = sceAjmBatchStart(AudioCodecSystemContextId, &BatchInfo, SCE_AJM_PRIORITY_GAME_DEFAULT, &BatchError, &BatchId);

	if (Result < 0)
	{
		UE_LOG(LogAtrac9Decoder, Error, TEXT("sceAjmBatchStart() failed: 0x%08X (%s)\n"), Result, sceAjmStrError(Result));

		if (Result == SCE_AJM_ERROR_MALFORMED_BATCH)
		{
			sceAjmBatchErrorDump(&BatchInfo, &BatchError);
		}
		return;
	}

	Result = sceAjmBatchWait(AudioCodecSystemContextId, BatchId, SCE_AJM_WAIT_INFINITE, &BatchError);
	if (Result < 0)
	{
		UE_LOG(LogAtrac9Decoder, Error, TEXT("sceAjmBatchWait() failed: 0x%08X (%s)\n"), Result, sceAjmStrError(Result));

		if (Result == SCE_AJM_ERROR_MALFORMED_BATCH)
		{
			sceAjmBatchErrorDump(&BatchInfo, &BatchError);
		}
		return;
	}
}
