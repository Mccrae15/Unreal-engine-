// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "PS4MediaSettings.generated.h"


/**
 * Settings for the PS4Media plug-in.
 */
UCLASS(config=Engine)
class PS4MEDIAFACTORY_API UPS4MediaSettings
	: public UObject
{
	GENERATED_BODY()

public:
	 
	/** Default constructor. */
	UPS4MediaSettings();

public:

	/** The video buffer size for local file media sources (in MB, default = 1). */
	UPROPERTY(config, EditAnywhere, Category=Memory)
	int32 FileVideoBufferSizeMB;

	/** The video buffer size for HLS media sources (in MB, default = 8). */
	UPROPERTY(config, EditAnywhere, Category=Memory)
	int32 HlsVideoBufferSizeMB;

	/** The number of output video frame buffers (default = 6). */
	UPROPERTY(config, EditAnywhere, Category=Memory)
	int32 OutputVideoFrameBuffers;
};
