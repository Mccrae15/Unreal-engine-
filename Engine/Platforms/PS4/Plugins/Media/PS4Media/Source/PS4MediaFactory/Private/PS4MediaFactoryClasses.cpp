// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4MediaSettings.h"


UPS4MediaSettings::UPS4MediaSettings()
	: FileVideoBufferSizeMB(8)
	, HlsVideoBufferSizeMB(8)
	, OutputVideoFrameBuffers(6)
	, AudioDecoderStackSizeKB(0)
	, VideoDecoderStackSizeKB(0)
	, DemuxerStackSizeKB(0)
	, ControllerStackSizeKB(0)
	, HttpStreamingStackSizeKB(0)
	, FileStreamingStackSizeKB(0)
{ }
