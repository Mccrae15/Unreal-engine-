// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4MediaPrivate.h"

#include "CoreTypes.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

#include "IPS4MediaModule.h"
#include "PS4MediaPlayer.h"

#include <libsysmodule.h>


DEFINE_LOG_CATEGORY(LogPS4Media);


/**
 * Implements the PS4Media module.
 */
class FPS4MediaModule
	: public IPS4MediaModule
{
public:

	/** Default constructor. */
	FPS4MediaModule()
		: Initialized(false)
	{ }

public:

	//~ IPS4MediaModule interface

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		if (!Initialized)
		{
			return nullptr;
		}

		return MakeShared<FPS4MediaPlayer, ESPMode::ThreadSafe>(EventSink);
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		int32 Result = sceSysmoduleLoadModule(SCE_SYSMODULE_AV_PLAYER);

		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4Media, Error, TEXT("Failed to load AvPlayer module (%i)"), Result);

			return;
		}

		Initialized = true;
	}

	virtual void ShutdownModule() override
	{
		if (!Initialized)
		{
			return;
		}

		int32 Result = sceSysmoduleUnloadModule(SCE_SYSMODULE_AV_PLAYER);

		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4Media, Error, TEXT("Failed to unload AvPlayer module (%i)"), Result);
		}

		Initialized = false;
	}

private:

	/** Whether this module is initialized. */
	bool Initialized;
};


IMPLEMENT_MODULE(FPS4MediaModule, PS4Media)
