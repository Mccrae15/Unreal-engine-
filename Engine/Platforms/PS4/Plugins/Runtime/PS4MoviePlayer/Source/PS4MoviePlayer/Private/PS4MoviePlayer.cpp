// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PS4MovieStreamer.h"

#include "Misc/CoreDelegates.h"

TSharedPtr<FAVPlayerMovieStreamer> PS4MovieStreamer;

class FPS4MoviePlayerModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		FAVPlayerMovieStreamer *Streamer = new FAVPlayerMovieStreamer;
		PS4MovieStreamer = MakeShareable(Streamer);

        FCoreDelegates::RegisterMovieStreamerDelegate.Broadcast(PS4MovieStreamer);
	}

	virtual void ShutdownModule() override
	{
        if (PS4MovieStreamer.IsValid())
        {
            FCoreDelegates::UnRegisterMovieStreamerDelegate.Broadcast(PS4MovieStreamer);
        }

		PS4MovieStreamer.Reset();
	}
};

IMPLEMENT_MODULE( FPS4MoviePlayerModule, PS4MoviePlayer )
