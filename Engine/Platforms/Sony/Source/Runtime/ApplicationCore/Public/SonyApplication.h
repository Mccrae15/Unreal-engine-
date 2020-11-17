// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "SonyWindow.h"
#include "SonyInputInterface.h"
#include "SonyConsoleInput.h"
#include "Misc/Optional.h"
#include <np/np_common.h>
#include <system_service.h>

/**
 * Sony-specific application implementation.
 */
class FSonyApplication : public GenericApplication
{
protected:
	static FSonyApplication* Singleton;

public:
	static bool IsInitialized() { return Singleton != nullptr; }
	static inline FSonyApplication* GetSonyApplication() { return Singleton; }

	virtual ~FSonyApplication();

	void SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);
	virtual void PollGameDeviceState(const float TimeDelta) override;
	virtual void Tick(const float TimeDelta) override;
	virtual bool IsMouseAttached() const override;
	virtual bool IsGamepadAttached() const override;

	virtual IInputInterface* GetInputInterface() override;

	virtual TSharedRef<FGenericWindow> MakeWindow() override;

	virtual FModifierKeysState GetModifierKeys() const override;

	virtual bool HandleSystemServiceEvent(SceSystemServiceEvent& Event) { return false; }

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

	static constexpr float DefaultSafeAreaPercentage = 0.9f;

protected:

	FSonyApplication();

private:
	
	TSharedPtr< class FSonyInputInterface > InputInterface;

	TSharedPtr<class SystemEventGatherRunnable, ESPMode::ThreadSafe> SystemEventGatherThreadRunnable;

	int32 UserIDs[SCE_USER_SERVICE_MAX_LOGIN_USERS];

	float SafeAreaPercentage;
	FDisplayMetrics  LastKnownMetrics;
	mutable FCriticalSection UserCriticalSection;

	bool bIsInBackground;

#if !UE_BUILD_SHIPPING
	FSonyConsoleInputManager ConsoleInput;
	FOnConsoleCommandAdded CommandListeners;
#endif

};
