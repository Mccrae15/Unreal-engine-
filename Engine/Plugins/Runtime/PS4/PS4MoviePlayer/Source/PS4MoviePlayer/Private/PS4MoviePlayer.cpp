// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PS4MovieStreamer.h"

TSharedPtr<FAVPlayerMovieStreamer> PS4MovieStreamer;

class FPS4MoviePlayerModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		FAVPlayerMovieStreamer *Streamer = new FAVPlayerMovieStreamer;
		PS4MovieStreamer = MakeShareable(Streamer);
		GetMoviePlayer()->RegisterMovieStreamer(PS4MovieStreamer);
	}

	virtual void ShutdownModule() override
	{
		PS4MovieStreamer.Reset();
	}
};

IMPLEMENT_MODULE( FPS4MoviePlayerModule, PS4MoviePlayer )
