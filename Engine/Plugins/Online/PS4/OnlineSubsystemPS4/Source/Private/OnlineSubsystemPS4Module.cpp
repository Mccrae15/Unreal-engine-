// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemPS4Module.h"
#include "OnlineSubsystemPS4Private.h"
#include "OnlineSubsystemPS4.h"
#include "ModuleManager.h"


IMPLEMENT_MODULE( FOnlineSubsystemPS4Module, OnlineSubsystemPS4 );

/**
 * Class responsible for creating instance(s) of the subsystem
 */
class FOnlineFactoryPS4 : public IOnlineFactory
{

private:

	/** Single instantiation of the PS4 interface */
	static FOnlineSubsystemPS4Ptr PS4Singleton;

	virtual void DestroySubsystem()
	{
		if (PS4Singleton.IsValid())
		{
			PS4Singleton->Shutdown();
			PS4Singleton = NULL;
		}
	}

public:

	FOnlineFactoryPS4() {}
	virtual ~FOnlineFactoryPS4() 
	{
		DestroySubsystem();
	}

	virtual IOnlineSubsystemPtr CreateSubsystem(FName InstanceName)
	{
		// If we ever failed to init, we are not configured correctly so suppress future requests to create.
		// Also IsEnabled does not change at runtime so if we were disabled there is no value in checking every time.
		static bool bCreatedFailed = false;
		if (!bCreatedFailed)
		{
			if (!PS4Singleton.IsValid())
			{
				PS4Singleton = MakeShared<FOnlineSubsystemPS4, ESPMode::ThreadSafe>(InstanceName);
				if (PS4Singleton->IsEnabled())
				{
					if(!PS4Singleton->Init())
					{
						UE_LOG_ONLINE(Warning, TEXT("PS4 API failed to initialize!"));
						DestroySubsystem();
						bCreatedFailed = true;
					}
				}
				else
				{
					UE_LOG_ONLINE(Log, TEXT("PS4 API disabled!"));
					DestroySubsystem();
					bCreatedFailed = true;	
				}

				return PS4Singleton;
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("Can't create more than one instance of PS4 online subsystem!"));
			}
		}

		return NULL;
	}
};

FOnlineSubsystemPS4Ptr FOnlineFactoryPS4::PS4Singleton = NULL;

void FOnlineSubsystemPS4Module::StartupModule()
{
	UE_LOG_ONLINE(Log, TEXT("PS4 Module Startup!"));

	PS4Factory = new FOnlineFactoryPS4();

	// Create and register our singleton factory with the main online subsystem for easy access
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.RegisterPlatformService(PS4_SUBSYSTEM, PS4Factory);
}

void FOnlineSubsystemPS4Module::ShutdownModule()
{
	UE_LOG_ONLINE(Log, TEXT("PS4 Module Shutdown!"));

	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.UnregisterPlatformService(PS4_SUBSYSTEM);

	delete PS4Factory;
	PS4Factory = NULL;
}