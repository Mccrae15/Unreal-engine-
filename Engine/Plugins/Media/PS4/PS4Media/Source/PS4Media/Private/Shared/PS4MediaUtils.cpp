// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4MediaUtils.h"

#include "Containers/StringConv.h"

#include <audioout.h>
#include <sceavplayer.h>


namespace PS4Media
{
	/**
	 * Convert an event code to a human readable string.
	 *
	 * @param Event The event code to convert.
	 * @return The event string.
	 */
	FString EventToString(int32 Event)
	{
		switch (Event)
		{
		case SCE_AVPLAYER_STATE_STOP:
			return TEXT("Stop");

		case SCE_AVPLAYER_STATE_READY:
			return TEXT("Ready");

		case SCE_AVPLAYER_STATE_PLAY:
			return TEXT("Play");

		case SCE_AVPLAYER_STATE_PAUSE:
			return TEXT("Pause");

		case SCE_AVPLAYER_STATE_BUFFERING:
			return TEXT("Buffering");

		case SCE_AVPLAYER_ENCRYPTION:
			return TEXT("Encryption");

		case SCE_AVPLAYER_DRM_ERROR:
			return TEXT("DRM Error");

		default:
			return FString::Printf(TEXT("Unknown event 0x%08x (%i)"), Event, Event);
		}
	}


	/**
	 * Convert an language code to a string.
	 *
	 * @param Language The language code to convert.
	 * @return The language string.
	 */
	FString LanguageToString(const uint8* Language)
	{
		return (Language != nullptr) ? FString(4, StringCast<TCHAR>((char*)Language).Get()) : FString();
	}


	/**
	 * Convert a result code to a human readable string.
	 *
	 * @param Result The result code to convert.
	 * @return The result string.
	 */
	FString ResultToString(int32 Result)
	{
		switch (Result)
		{
		// AudioOut codes
		case SCE_AUDIO_OUT_ERROR_NOT_OPENED:
			return TEXT("Audio port not opened");

		case SCE_AUDIO_OUT_ERROR_BUSY:
			return TEXT("Tried to output while outputting audio");

		case SCE_AUDIO_OUT_ERROR_INVALID_PORT:
			return TEXT("Invalid audio port number");
			
		case SCE_AUDIO_OUT_ERROR_INVALID_POINTER:
			return TEXT("Invalid pointer");

		case SCE_AUDIO_OUT_ERROR_PORT_FULL:
			return TEXT("All audio ports are in use");

		case SCE_AUDIO_OUT_ERROR_INVALID_SIZE:
			return TEXT("Invalid sample length");

		case SCE_AUDIO_OUT_ERROR_INVALID_FORMAT:
			return TEXT("Invalid sample format");

		case SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ:
			return TEXT("Invalid sample frequency");

		case SCE_AUDIO_OUT_ERROR_INVALID_VOLUME:
			return TEXT("Invalid volume");

		case SCE_AUDIO_OUT_ERROR_INVALID_PORT_TYPE:
			return TEXT("Invalid audio port type");

		case SCE_AUDIO_OUT_ERROR_INVALID_CONF_TYPE:
			return TEXT("Invalid config type");

		case SCE_AUDIO_OUT_ERROR_OUT_OF_MEMORY:
			return TEXT("Out of memory");

		case SCE_AUDIO_OUT_ERROR_ALREADY_INIT:
			return TEXT("Audio already initialized");

		case SCE_AUDIO_OUT_ERROR_NOT_INIT:
			return TEXT("Audio not initialized");

		case SCE_AUDIO_OUT_ERROR_MEMORY:
			return TEXT("Memory handling error");

		case SCE_AUDIO_OUT_ERROR_SYSTEM_RESOURCE:
			return TEXT("System resource error");

		case SCE_AUDIO_OUT_ERROR_TRANS_EVENT:
			return TEXT("Transfer event send error");

		case SCE_AUDIO_OUT_ERROR_INVALID_FLAG:
			return TEXT("Invalid volume flag value");

		case SCE_AUDIO_OUT_ERROR_INVALID_MIXLEVEL:
			return TEXT("Invalid mix level value");

		case SCE_AUDIO_OUT_ERROR_INVALID_ARG:
			return TEXT("Invalid argument");

		case SCE_AUDIO_OUT_ERROR_INVALID_PARAM:
			return TEXT("Invalid parameter");

		// AvPlayer codes
		case SCE_AVPLAYER_NO_ERR:
			return TEXT("No error");

		case SCE_AVPLAYER_ERROR_INVALID_PARAMS:
			return TEXT("Invalid parameter specified");

		case SCE_AVPLAYER_ERROR_OPERATION_FAILED:
			return TEXT("The requested operation has failed");

		case SCE_AVPLAYER_ERROR_NO_MEMORY:
			return TEXT("Memory allocation has failed");

		case SCE_AVPLAYER_ERROR_NOT_SUPPORTED:
			return TEXT("The requested operation is not supported");

		case SCE_AVPLAYER_ERROR_WAR_FILE_NONINTERLEAVED:
			return TEXT("File is non-interleaved");

		case SCE_AVPLAYER_ERROR_WAR_LOOPING_BACK:
			return TEXT("Stream is looping back");

		case SCE_AVPLAYER_ERROR_WAR_JUMP_COMPLETE:
			return TEXT("Jump was completed");

		case SCE_AVPLAYER_ERROR_INFO_MARLIN_ENCRY:
			return TEXT("Stream encrypted using Marlin");

		case SCE_AVPLAYER_ERROR_INFO_PLAYREADY_ENCRY:
			return TEXT("Stream encrypted using PlayReady");

		case SCE_AVPLAYER_ERROR_INFO_AES_ENCRY:
			return TEXT("Stream encrypted using AES");

		case SCE_AVPLAYER_ERROR_INFO_OTHER_ENCRY:
			return TEXT("Stream encrypted using other encryption technology");

		default:
			return FString::Printf(TEXT("Unknown result 0x%08x (%i)"), Result, Result);
		}
	}
}
