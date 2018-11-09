// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemPS4ServerModule.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "OnlineSubsystemModule.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemPS4Server.h"

IMPLEMENT_MODULE(FOnlineSubsystemPS4ServerModule, OnlineSubsystemPS4Server);

/**
 * Class responsible for creating instance(s) of the subsystem
 */
class FOnlineFactoryPS4Server : public IOnlineFactory
{
public:

	virtual IOnlineSubsystemPtr CreateSubsystem(FName InstanceName)
	{
		FOnlineSubsystemPS4ServerPtr OnlineSub = MakeShared<FOnlineSubsystemPS4Server, ESPMode::ThreadSafe>(InstanceName);
		if (OnlineSub->IsEnabled())
		{
			if(!OnlineSub->Init())
			{
				UE_LOG_ONLINE(Warning, TEXT("PS4Server API failed to initialize!"));
				OnlineSub->Shutdown();
				OnlineSub = nullptr;
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("PS4Server API disabled!"));
			OnlineSub->Shutdown();
			OnlineSub = nullptr;
		}

		return OnlineSub;
	}
};

void FOnlineSubsystemPS4ServerModule::StartupModule()
{
	PS4ServerFactory.Reset(new FOnlineFactoryPS4Server());

	// Create and register our singleton factory with the main online subsystem for easy access
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.RegisterPlatformService(PS4SERVER_SUBSYSTEM, PS4ServerFactory.Get());
}

void FOnlineSubsystemPS4ServerModule::ShutdownModule()
{
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.UnregisterPlatformService(PS4SERVER_SUBSYSTEM);
	
	PS4ServerFactory.Reset();
}
