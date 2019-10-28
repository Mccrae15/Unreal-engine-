// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "PS4/PS4Window.h"
#include "PS4/PS4InputInterface.h"
#include "PS4/PS4ConsoleInput.h"
#include "Misc/Optional.h"
#include <np/np_common.h>
#include <np/np_play_together.h>
#include <np/np_event.h>

//disable warnings from overriding the deprecated forcefeedback.  
//calls to the deprecated function will still generate warnings.
PRAGMA_DISABLE_DEPRECATION_WARNINGS

/** Delegate for when we receive a play together system event */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayTogetherSystemServiceEventDelegate, const SceNpPlayTogetherHostEventParamA&);

/** Delegates for when we receieve Join Match tournament system events */
DECLARE_EVENT_OneParam(FPS4Application, FOnPS4JoinEventDelegate, const SceNpEventJoinEventParam& /*Param*/);
DECLARE_EVENT_OneParam(FPS4Application, FOnPS4JoinMatchEventDelegate, const SceNpEventJoinMatchEventParam& /*Param*/);
DECLARE_EVENT_OneParam(FPS4Application, FOnPS4JoinTeamMatchEventDelegate, const SceNpEventJoinTeamOnTeamMatchEventParam& /*Param*/);

DECLARE_MULTICAST_DELEGATE(FOnVRHeadsetRecenterDefaultDelegate);

/**
 * PS4-specific application implementation.
 */
class FPS4Application
	: public GenericApplication
{
public:
	/** Explicit allocation of the single possible FPS4Application. */
	static FPS4Application* CreatePS4Application();

	/** Gets the FPS4Application.  Must be explicitly initialized already. */
	static FPS4Application* GetPS4Application();

	static bool IsInitialized();

public:

	virtual ~FPS4Application();

	void SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);
	virtual void PollGameDeviceState(const float TimeDelta) override;
	virtual void Tick(const float TimeDelta) override;
	virtual bool IsMouseAttached() const override;
	virtual bool IsGamepadAttached() const override;
	virtual FPlatformRect GetWorkArea(const FPlatformRect& CurrentWindow) const override;

	virtual IInputInterface* GetInputInterface() override;

	virtual TSharedRef<FGenericWindow> MakeWindow() override;

	virtual FModifierKeysState GetModifierKeys() const override;

	const TSharedPtr<FGenericWindow>& GetMainWindow()
	{
		return MainWindow;
	}

	// returns the internal platform index of the given SceUserServiceUserId.
	// Generally unsafe off the main thread as the returned value could be immediately invalidated on the main thread by handling of signout/signin events.
	int32 GetUserIndex(SceUserServiceUserId UserID) const;

	// returns the SceUserServiceUserId of the given index (thread safe).
	SceUserServiceUserId GetUserID(int32 UserIndex) const;

	// update the UserID array
	int32 InsertUserID(SceUserServiceUserId NewUserId);
	void RemoveUserID(SceUserServiceUserId UserId);

	void SetIsInBackground(bool InIsInBackground);

	// Connects the user controller state to the specified user ID
	int32 ConnectControllerStateToUser(int32 UserID, int32 UserIndex);

	// Disconnects the user controller state from the specified user ID
	void DisconnectControllerStateFromUser(int32 UserID, int32 UserIndex);

	// calls scePadResetOrientation on the given user's controller
	void ResetControllerOrientation(int32 UserIndex);

	virtual void RegisterConsoleCommandListener( const FOnConsoleCommandListener& InListener ) override;
	virtual void AddPendingConsoleCommand( const FString& InCommand ) override;

	void CheckForSafeAreaChanges();
	FOnPlayTogetherSystemServiceEventDelegate OnPlayTogetherSystemServiceEventDelegate;
	TOptional<SceNpPlayTogetherHostEventParamA> CachedPlayTogetherHostEventParam;

	FOnVRHeadsetRecenterDefaultDelegate OnVRHeadsetRecenterDefaultDelegate;

	FOnPS4JoinEventDelegate OnTournamentJoinEventDelegate;
	FOnPS4JoinMatchEventDelegate OnTournamentJoinMatchEventDelegate;
	FOnPS4JoinTeamMatchEventDelegate OnTournamentJoinTeamMatchEventDelegate;

private:
	friend class SystemEventGatherRunnable;
	TOptional<SceNpEventJoinEventParam> CachedTournamentJoinEventParam;
	TOptional<SceNpEventJoinMatchEventParam> CachedTournamentJoinMatchEventParam;
	TOptional<SceNpEventJoinTeamOnTeamMatchEventParam> CachedTournamentJoinTeamOnTeamMatchEventParam;

private:

	FPS4Application();

private:

	/**
	 * Country code with age restriction
	 */
	struct FCountryAgeRestriction
	{
		FCountryAgeRestriction(const FString& InCountry = FString(), int32 InAge = 0)
			: Country(InCountry)
			, Age(InAge)
		{}

		/** parse from ini string */
		bool ParseConfig(const FString& ConfigStr)
		{
			return (FParse::Value(*ConfigStr, TEXT("Country="), Country) &&
					FParse::Value(*ConfigStr, TEXT("Age="), Age));
		}

		/** Country code */
		FString Country;
		/** Min age required */
		int32 Age;
	};
	/** List of countries with age restrictions */
	TArray<FCountryAgeRestriction> CountryAgeRestrictions;
	
	TSharedPtr< class FPS4InputInterface > InputInterface;
	TSharedPtr< FGenericWindow > MainWindow;

	TSharedPtr<class SystemEventGatherRunnable, ESPMode::ThreadSafe> SystemEventGatherThreadRunnable;

	int32 UserIDs[SCE_USER_SERVICE_MAX_LOGIN_USERS];

	float SafeAreaPercentage;
	FDisplayMetrics  LastKnownMetrics;
	mutable FCriticalSection UserCriticalSection;

	bool bIsInBackground;

#if !UE_BUILD_SHIPPING
	FPS4ConsoleInputManager ConsoleInput;
	FOnConsoleCommandAdded CommandListeners;
#endif

};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
