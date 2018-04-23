// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Interfaces/IAudioFormat.h"
#include "Interfaces/IAudioFormatModule.h"
#if WITH_ENGINE
	#include "AudioDecompress.h"
	#include "AudioWaveFormatParser.h"
#endif	//WITH_ENGINE
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#include <Mmreg.h>

static FName NAME_AT9(TEXT("AT9"));
/**
 * IAudioFormat, audio compression abstraction
**/
class FAT9AudioFormat : public IAudioFormat
{
	enum
	{
		/** Version for AT9 format, this becomes part of the DDC key. */
		UE_AUDIO_AT9_VER = 7,
	};

public:
	virtual bool AllowParallelBuild() const
	{
		return false;
	}

	virtual uint16 GetVersion(FName Format) const override
	{
		check(Format == NAME_AT9);
		return UE_AUDIO_AT9_VER;
	}


	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const
	{
		OutFormats.Add(NAME_AT9);
	}

	/**
	 * Writes out a wav file from source data and format with no validation or error checking
	 *
	 * @param	WaveFile		File handle to write to
	 * @param	SrcBuffer		Pointer to source buffer
	 * @param	SrcBufferSize	Size in bytes of source buffer
	 * @param	WaveFormat		Pointer to platform specific wave format description
	 *
	 * @return	TRUE if succeeded, FALSE otherwise
	 */
	bool WriteWaveFile(FArchive& WaveFile, const uint8* SrcBuffer, uint32 SrcBufferSize, void* WaveFormat) const
	{
		int32	ID;
		int32	ChunkSize;
		int32	HeaderSize;

		WAVEFORMATEXTENSIBLE* ExtFormat = (WAVEFORMATEXTENSIBLE*)WaveFormat;
		WAVEFORMATEX* Format = (WAVEFORMATEX*)&ExtFormat->Format;

		HeaderSize = sizeof(WAVEFORMATEX);
		ChunkSize = sizeof(ID) + sizeof(ChunkSize) + sizeof(ID) + sizeof(ID) + sizeof(HeaderSize) + SrcBufferSize + HeaderSize;

		ID = 'FFIR';
		WaveFile << ID << ChunkSize;

		ID = 'EVAW';
		WaveFile << ID;

		ID = ' tmf';
		WaveFile << ID;

		// now serialize the raw format header
		WaveFile << HeaderSize;
		WaveFile.Serialize(Format, HeaderSize);

		ID = 'atad';
		WaveFile << ID;

		// now serialize the data
		WaveFile << SrcBufferSize;
		WaveFile.Serialize((void*)SrcBuffer, SrcBufferSize);

		return true;
	}

	static int32 GetBitRate(int32 QualityLevel, int32 NumChannels)
	{
		if (NumChannels <= 2) // mono or stereo
		{
			int32 BitRate = 0;
			if (QualityLevel < 25)
			{
				BitRate = 36;
			}
			else if (QualityLevel < 35)
			{
				BitRate = 48;
			}
			else if (QualityLevel < 45)  
			{
				BitRate = 60;
			}
			else if (QualityLevel < 55) // Quality 40 will be about half bit rate at quality 80
			{
				BitRate = 72;
			}
			else if (QualityLevel < 65)
			{
				BitRate = 84;
			}
			else if (QualityLevel < 75)
			{
				BitRate = 96;
			}
			else if (QualityLevel < 85)
			{
				BitRate = 120;
			}
			else if (QualityLevel < 95)
			{
				BitRate = 120;
			}
			else
			{
				BitRate = 144;
			}

			check(BitRate > 0);

			// For 1 and 2 channels bitrate is just NumChannels * BitRate.
			return BitRate * NumChannels;
		}
		else if (NumChannels == 4) // Quad source file
		{
			if (QualityLevel < 30)
			{
				return 192;
			}
			else if (QualityLevel < 60)
			{
				return 240;
			}
			else if (QualityLevel < 90)
			{
				return 288;
			}
			else
			{
				return 384;
			}
		}
		else if (NumChannels == 6) // 5.1 source file
		{
			if (QualityLevel < 30)
			{
				return 240;
			}
			else if (QualityLevel < 60)
			{
				return 300;
			}
			else if (QualityLevel < 90)
			{
				return 360;
			}
			else
			{
				return 480;
			}
		}
		else if (NumChannels == 8) // 7.1 source file
		{
			if (QualityLevel < 30)
			{
				return 336;
			}
			else if (QualityLevel < 60)
			{
				return 420;
			}
			else if (QualityLevel < 90)
			{
				return 504;
			}
			else
			{
				return 672;
			}
		}
		else
		{
			// Unsupported channel count!
			check(false);
			return 0;
		}
	}

	virtual bool Cook(FName AudioFormat, const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo, TArray<uint8>& CompressedDataStore) const
	{
		check(AudioFormat == NAME_AT9);

		// default to success
		bool bSucceeded = true;

		// save out the audio data to a .wave file the tool can read
		WAVEFORMATEXTENSIBLE Format = { 0 };

		// Set up the waveformat
		Format.Format.nChannels = (WORD)QualityInfo.NumChannels;
		Format.Format.nSamplesPerSec = QualityInfo.SampleRate;
		Format.Format.nBlockAlign = (WORD)(QualityInfo.NumChannels * sizeof(uint16));
		Format.Format.nAvgBytesPerSec = Format.Format.nBlockAlign * QualityInfo.SampleRate;
		Format.Format.wBitsPerSample = 16;
		Format.Format.wFormatTag = WAVE_FORMAT_PCM;

		FString SourceFilename = FPaths::ProjectIntermediateDir() + TEXT("AT9/AT9Input.wav");
		FString SourceFilenameExternal = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*SourceFilename);
		FString DestFilename = SourceFilename + TEXT(".at9");
		FString DestFilenameExternal = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*DestFilename);

		// open up the file to save the wave to
		FArchive* SourceFileWriter = IFileManager::Get().CreateFileWriter(*SourceFilename);
		// save it out
		if (SourceFileWriter)
		{
			TArray<uint8> PaddedBuffer = SrcBuffer;
			// the at9tool needs at least 3072 samples, so let's pad it out!
			uint32 MinBytes = 3072 * 2 * QualityInfo.NumChannels;
			if ((uint32)PaddedBuffer.Num() < MinBytes)
			{
				check(QualityInfo.SampleDataSize <= (uint32)PaddedBuffer.Num());

				// @todo: is 0 what we want to add?
				PaddedBuffer.AddZeroed(MinBytes - PaddedBuffer.Num());

				UE_LOG(LogHAL, Log, TEXT("Padded a sound out to 3072 samples..."));
			}

			if (WriteWaveFile(*SourceFileWriter, PaddedBuffer.GetData(), FMath::Max(QualityInfo.SampleDataSize, MinBytes), &Format))
			{
				// close the input file
				delete SourceFileWriter;

				// get path to executable
				TCHAR SDKDir[MAX_PATH];
				FPlatformMisc::GetEnvironmentVariable(TEXT("SCE_ORBIS_SDK_DIR"), SDKDir, MAX_PATH);
				FString ExecutablePath = FString(SDKDir) + "/host_tools/bin/at9tool.exe";

				// calculate some settings

				int32 BitRate = GetBitRate(QualityInfo.Quality, QualityInfo.NumChannels);

				// set the output SR to 48000 because, interestingly, this has no effect on the size of at9 file!
				int32 OutputSampleRate = 48000;
				FString Looping = TEXT("");//QualityInfo.bLoopingSound ? TEXT("-wholeloop") : TEXT("");

				// make commandline
				FString Commandline = FString::Printf(TEXT("-e -fs %d -br %d %s \"%s\" \"%s\""), OutputSampleRate, BitRate, *Looping, *SourceFilenameExternal, *DestFilenameExternal);

				// run the commandline tool
				int32 ReturnCode;
				FString StdOut;
				FString StdErr;
				FPlatformProcess::ExecProcess(*ExecutablePath, *Commandline, &ReturnCode, &StdOut, &StdErr);

				if (ReturnCode != 0)
				{
					UE_LOG(LogHAL, Warning, TEXT("Failed to encode AT9 audio, err code = %d:\nCommand: %s %s\nStdOut: %s\nStdErr: %s"), ReturnCode, *ExecutablePath, *Commandline, *StdOut, *StdErr);
					bSucceeded = false;
				}
				else
				{
					// load the output data
					bSucceeded = FFileHelper::LoadFileToArray(CompressedDataStore, *DestFilename);
				}
			}
		}

		// no matter what happened, try to cleanup the temp files
		IFileManager::Get().Delete(*SourceFilename);
		IFileManager::Get().Delete(*DestFilename);

		return bSucceeded;
	}

	virtual bool CookSurround(FName Format, const TArray<TArray<uint8> >& SrcBuffers, FSoundQualityInfo& QualityInfo, TArray<uint8>& CompressedDataStore) const
	{
		check(Format == NAME_AT9);

		// default to success
		bool bSucceeded = true;

		// save out the audio data to a .wave file the tool can read
		WAVEFORMATEXTENSIBLE WaveFormat = { 0 };

		// Set up the waveformat
		WaveFormat.Format.nChannels = (WORD)QualityInfo.NumChannels;
		WaveFormat.Format.nSamplesPerSec = QualityInfo.SampleRate;
		WaveFormat.Format.nBlockAlign = (WORD)(QualityInfo.NumChannels * sizeof(uint16));
		WaveFormat.Format.nAvgBytesPerSec = WaveFormat.Format.nBlockAlign * QualityInfo.SampleRate;
		WaveFormat.Format.wBitsPerSample = 16;
		WaveFormat.Format.wFormatTag = WAVE_FORMAT_PCM;

		FString SourceFilename = FPaths::ProjectIntermediateDir() + TEXT("AT9/AT9Input.wav");
		FString SourceFilenameExternal = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*SourceFilename);
		FString DestFilename = SourceFilename + TEXT(".at9");
		FString DestFilenameExternal = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*DestFilename);

		// open up the file to save the wave to
		FArchive* SourceFileWriter = IFileManager::Get().CreateFileWriter(*SourceFilename);

		// save it out
		if (SourceFileWriter)
		{
			// Find the size of the largest buffer
			int32 LargestBufferSize = 0;
			for( int32 SourceBufferIndex = 0; SourceBufferIndex< SrcBuffers.Num(); SourceBufferIndex++ )
			{
				LargestBufferSize = FMath::Max<int32>(LargestBufferSize, SrcBuffers[SourceBufferIndex].Num() );
			}

			// Interleave the source buffers into the destination
			int32 InterleavedBufferSize = LargestBufferSize * QualityInfo.NumChannels;
			TArray<uint8> InterleavedBuffer;
			InterleavedBuffer.AddZeroed( InterleavedBufferSize );

			int32 Stride = WaveFormat.Format.nBlockAlign / sizeof( int16 );
			for( uint32 ChannelIndex = 0; ChannelIndex < QualityInfo.NumChannels; ChannelIndex++ )
			{
				int16* WriteBuffer = (int16*)InterleavedBuffer.GetData();
				WriteBuffer += ChannelIndex;
				int16* ReadBuffer = ( int16* )( SrcBuffers[ChannelIndex].GetData() );
				for( int32 SrcIndex = 0; SrcIndex < SrcBuffers[ChannelIndex].Num() / sizeof( int16 ); SrcIndex++ )
				{
					*WriteBuffer = ReadBuffer[SrcIndex];
					WriteBuffer += Stride;
				}
			}

			// the at9tool needs at least 3072 samples, so let's pad it out!
			uint32 MinBytes = 3072 * 2 * QualityInfo.NumChannels;
			if ((uint32)InterleavedBuffer.Num() < MinBytes)
			{
				// @todo: is 0 what we want to add?
				InterleavedBuffer.AddZeroed(MinBytes - InterleavedBuffer.Num());

				UE_LOG(LogHAL, Log, TEXT("Padded a sound out to 3072 samples..."));
			}

			if (WriteWaveFile(*SourceFileWriter, InterleavedBuffer.GetData(), InterleavedBuffer.Num(), &WaveFormat))
			{
				// close the input file
				delete SourceFileWriter;

				// get path to executable
				TCHAR SDKDir[MAX_PATH];
				FPlatformMisc::GetEnvironmentVariable(TEXT("SCE_ORBIS_SDK_DIR"), SDKDir, MAX_PATH);
				FString ExecutablePath = FString(SDKDir) + "/host_tools/bin/at9tool.exe";

				// calculate some settings
				int32 BitRate = GetBitRate(QualityInfo.Quality, QualityInfo.NumChannels);

				// set the output SR to 48000 because, interestingly, this has no effect on the size of at9 file!
				int32 OutputSampleRate = 48000;
				FString Looping = TEXT("");//QualityInfo.bLoopingSound ? TEXT("-wholeloop") : TEXT("");

				// make commandline
				FString Commandline = FString::Printf(TEXT("-e -fs %d -br %d %s %s %s"), OutputSampleRate, BitRate, *Looping, *SourceFilenameExternal, *DestFilenameExternal);

				// run the commandline tool
				int32 ReturnCode;
				FString StdOut;
				FString StdErr;
				FPlatformProcess::ExecProcess(*ExecutablePath, *Commandline, &ReturnCode, &StdOut, &StdErr);

				if (ReturnCode != 0)
				{
					UE_LOG(LogHAL, Warning, TEXT("Failed to encode AT9 audio, err code = %d:\nCommand: %s %s\nStdOut: %s\nStdErr: %s"), ReturnCode, *ExecutablePath, *Commandline, *StdOut, *StdErr);
					bSucceeded = false;
				}
				else
				{
					// load the output data
					bSucceeded = FFileHelper::LoadFileToArray(CompressedDataStore, *DestFilename);
				}
			}
		}

		// no matter what happened, try to cleanup the temp files
		IFileManager::Get().Delete(*SourceFilename);
		IFileManager::Get().Delete(*DestFilename);

		return bSucceeded;
	}

	virtual int32 Recompress(FName Format, const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo, TArray<uint8>& OutBuffer) const
	{
		check(Format == NAME_AT9);

		return false;
/*
		FVorbisAudioInfo	OggInfo;
		int32					CompressedSize = -1;

		// Cannot quality preview multichannel sounds
		if( QualityInfo.NumChannels > 2 )
		{
			return 0;
		}
		TArray<uint8> CompressedDataStore;
		if( !Cook( Format, SrcBuffer, QualityInfo, CompressedDataStore ) )
		{
			return 0;
		}

		// Parse the ogg vorbis header for the relevant information
		if( !OggInfo.ReadCompressedInfo( CompressedDataStore.GetTypedData(), CompressedDataStore.Num(), &QualityInfo ) )
		{
			return 0;
		}

		// Decompress all the sample data
		OutBuffer.Empty(QualityInfo.SampleDataSize);
		OutBuffer.AddZeroed(QualityInfo.SampleDataSize);
		OggInfo.ExpandFile( OutBuffer.GetTypedData(), &QualityInfo );

		return CompressedDataStore.Num();
*/
	}

#if WITH_ENGINE

	virtual bool SplitDataForStreaming(const TArray<uint8>& SrcBuffer, TArray<TArray<uint8>>& OutBuffers, const int32 MaxChunkSize) const override
	{
		const uint32 NumSrcBufferBytes = SrcBuffer.Num();
		if (!NumSrcBufferBytes)
		{
			return false;
		}

		// The size in bytes of each block... we can't split these up in the chunks
		FWaveFormatInfo Header;
		if (!ParseWaveFormatHeader(SrcBuffer.GetData(), SrcBuffer.Num(), Header))
		{
			return false;
		}

		/** The AT9 Subformat codec ID. Used to identify this file as an actual At9 file. */
		CONSTEXPR uint8_t At9CodecId[16] = {
			// 0x47E142D2, 0x36BA, 0x4D8D, 0x88FC61654F8C836C
			0xD2, 0x42, 0xE1, 0x47, 0xBA, 0x36, 0x8D, 0x4D, 0x88, 0xFC, 0x61, 0x65, 0x4F, 0x8C, 0x83, 0x6C
		};

		CONSTEXPR uint16 FORMAT_EXTENSIBLE = 0xFFFE;

		if (Header.FmtChunk.FormatTag != FORMAT_EXTENSIBLE)
		{
			UE_LOG(LogHAL, Error, TEXT("Unknown At9 format tag ID (%X)."), Header.FmtChunk.FormatTag);
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

			UE_LOG(LogHAL, Error, TEXT("Unknown At9 codec ID (%s)."), *CodecId);
			return false;
		}

		// The size of an encoded frame, for AT9, that's constant
		const uint32 FrameSize = Header.FmtChunk.BlockAlign;
		const int32 NumFrames = MaxChunkSize / FrameSize;

		// Do (arbitrarily) 1000 encoded frames per chunk
		const uint32 BytesPerChunk = NumFrames * FrameSize;

		const uint8* SrcBufferPtr = SrcBuffer.GetData();

		uint32 CurrentWriteByteIndex = 0;

		// The first block will be the bytes per chunk plus the header information
		uint32 ChunkByteSize = BytesPerChunk + Header.DataStartOffset;

		// Loop until we've processed the entire file
		for (;;)
		{
			// Check for the edge of the file end condition (last stream chunk won't be a full 100 frames)
			if (CurrentWriteByteIndex + ChunkByteSize >= NumSrcBufferBytes)
			{
				ChunkByteSize = NumSrcBufferBytes - CurrentWriteByteIndex;
			}

			CurrentWriteByteIndex += AddDataChunk(OutBuffers, SrcBufferPtr + CurrentWriteByteIndex, ChunkByteSize);
			check(CurrentWriteByteIndex <= NumSrcBufferBytes);

			// Check the terminating condition
			if (CurrentWriteByteIndex == NumSrcBufferBytes)
			{
				break;
			}

			// Assume our next chunk byte size will be the bytes per chunk param determined above (100 frames per chunk)
			ChunkByteSize = BytesPerChunk;
		}

		return true;
	}

	/**
	* Adds a new chunk of data to the array
	*
	* @param	OutBuffers	Array of buffers to add to
	* @param	ChunkData	Pointer to chunk data
	* @param	ChunkSize	How much data to write
	* @return	How many bytes were written
	*/
	int32 AddDataChunk(TArray<TArray<uint8>>& OutBuffers, const uint8* ChunkData, const int32 ChunkSize) const
	{
		TArray<uint8>& NewBuffer = *new (OutBuffers) TArray<uint8>;
		NewBuffer.Empty(ChunkSize);
		NewBuffer.AddUninitialized(ChunkSize);
		FMemory::Memcpy(NewBuffer.GetData(), ChunkData, ChunkSize);
		return ChunkSize;
	}

#endif
};


/**
 * Module for AT9 audio compression
 */

static IAudioFormat* Singleton = NULL;

class FAT9AudioPlatformModule : public IAudioFormatModule
{
public:
	virtual ~FAT9AudioPlatformModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual IAudioFormat* GetAudioFormat()
	{
		if (!Singleton)
		{
			Singleton = new FAT9AudioFormat();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FAT9AudioPlatformModule, AT9AudioFormat);
