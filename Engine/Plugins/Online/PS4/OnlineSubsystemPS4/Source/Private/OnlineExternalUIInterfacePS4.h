// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlineExternalUIInterface.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemPS4Package.h"

/** 
 * Implementation for the PS4 external UIs
 */
class FOnlineExternalUIPS4 : public IOnlineExternalUI
{

private:
	/** Reference to the main PS4 subsystem */
	class FOnlineSubsystemPS4* PS4Subsystem;

PACKAGE_SCOPE:

	explicit FOnlineExternalUIPS4(class FOnlineSubsystemPS4* InPS4Subsystem);

public:

	virtual ~FOnlineExternalUIPS4();

	// IOnlineExternalUI
	virtual bool ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate = FOnLoginUIClosedDelegate()) override;
	virtual bool ShowFriendsUI(int32 LocalUserNum) override;
	virtual bool ShowInviteUI(int32 LocalUserNum, FName SessionName = NAME_GameSession) override;
	virtual bool ShowAchievementsUI(int32 LocalUserNum) override;
	virtual bool ShowLeaderboardUI(const FString& LeaderboardName) override;
	virtual bool ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate = FOnShowWebUrlClosedDelegate()) override;
	virtual bool CloseWebURL() override;
	virtual bool ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate = FOnProfileUIClosedDelegate()) override;
	virtual bool ShowAccountUpgradeUI(const FUniqueNetId& UniqueId) override;
	virtual bool ShowPlatformMessageBox(const FUniqueNetId& UserId, EPlatformMessageType MessageType) override;
	virtual void ReportEnterInGameStoreUI() override;
	virtual void ReportExitInGameStoreUI() override;
	virtual bool ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate = FOnShowStoreUIClosedDelegate()) override;
	virtual bool ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate = FOnShowSendMessageUIClosedDelegate()) override;
	void Tick(float DeltaTime);

private:
	/** we must pump the PSPlus dialog if it's up. */
	bool bLaunchedPSPlusDialog;

	/** delegate that is stored from a call to ShowLoginUI, this will be called after the dialog box is closed */
	FOnLoginUIClosedDelegate OnLoginUIClosedDelegate;

	/** controller index that triggered the login dialog box */
	int32 LoginUIControllerIndex;

	/** The int value of the position to show the PlayStation Store Icon when ReportEnterInGameStoreUI() is called */
	int32 StoreIconPosition;
};

typedef TSharedPtr<FOnlineExternalUIPS4, ESPMode::ThreadSafe> FOnlineExternalUIPS4Ptr;

