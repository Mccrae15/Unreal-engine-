// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Atrac9Decoder.h"
#include "AudioDecompress.h"
#include "AudioWaveFormatParser.h"

#include <ajm.h>

DECLARE_LOG_CATEGORY_EXTERN(LogAtrac9Decoder, Display, All);

#if AT9_BENCHMARK
struct FBenchmarkResult
{
	uint32 InstanceNum = 0;
	uint32 MaxProcessTime = 0;
	uint32 AvgProcessTime = 0;
	float MaxActivationRate = 0;
	float AvgActivationRate = 0;
};
#endif


class FAtrac9AudioInfo : public IStreamedCompressedInfo
{
public:
	ENGINE_API FAtrac9AudioInfo();
	ENGINE_API virtual ~FAtrac9AudioInfo();

	//~ Begin IStreamedCompressedInfo Interface
	virtual bool ParseHeader(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, FSoundQualityInfo* QualityInfo) override;
	virtual int32 GetFrameSize() override;
	virtual uint32 GetMaxFrameSizeSamples() const override;
	virtual bool CreateDecoder() override;
	virtual FDecodeResult Decode(const uint8* CompressedData, const int32 CompressedDataSize, uint8* OutPCMData, const int32 OutputPCMDataSize) override;
	virtual void PrepareToLoop() override;
	//~ End IStreamedCompressedInfo Interface

protected:

	/** The AT9 Header struct. */
	FWaveFormatInfo Header;

	/** How many normal frames are in a block (aka a "super frame"). 1 if no blocks are used. 4 if blocks are used. */
	uint32 FramesPerBlock;

	/** How many samples are encoded per block. */
	uint32 SamplesPerBlock;

	/** Total number of AT9 encoded frames in the AT9 file. */
	uint32 TotalAt9Frames;

	/** The number of samples in the last frame. */
	uint32 SamplesLastFrame;

	/** Id of the Ajm instance used to decode this voice. */
	SceAjmInstanceId InstanceId;

	/** Buffer used to store batch decode information. */
	TArray<uint8> AjmBatchBuffer;

	/** Critical section for batch decoding. Prevents destroying AjmInstance while decoding. */
	FCriticalSection DecodeCriticalSection;

#if AT9_BENCHMARK
	/** Total time for all decode batch processing */
	uint64 TotalProcessTime;

	/** Duration of the slowest decode batch */
	uint32 MaxProcessTime;

	/** Number of decode batches run */
	uint32 BatchCount;
#endif
};
