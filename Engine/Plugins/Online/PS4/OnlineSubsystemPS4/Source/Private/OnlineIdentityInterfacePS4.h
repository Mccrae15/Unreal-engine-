// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineIdentityInterface.h"
#include "OnlineSubsystemPS4Package.h"
#include "OnlineSubsystemPS4.h"

struct FAccessToken
{
	FString Data;
	NpToolkit::Auth::IssuerIdType IssuerId;

	FAccessToken()
		: IssuerId(NpToolkit::Auth::IssuerIdType::invalid)
	{}
};

/**
 * Local user with an online PS4 account
 */
class FUserOnlineAccountPS4 : public FUserOnlineAccount
{
public:
	FCriticalSection* AccountCS;

	/** Pointers to the parent subsystems. */
	FOnlineIdentityPS4 const* const PS4Identity;

	/** Primary identifier for this user account in the wider engine and OSS. */
	TSharedRef<FUniqueNetIdPS4> const UserId;

	/** Returned by PSN when login completes. */
	FAccessToken AccessToken;

	/** Either the SceOnlineId or local account name of the user. */
	FString Username;

	/** Current login state of the user. Driven by system events. */
	ELoginStatus::Type LoginStatus;

	TMap<FString, FString> AccountData;
	TMap<EUserPrivileges::Type, uint32> CachedUserPrivileges;

	FUserOnlineAccountPS4(FOnlineIdentityPS4 const* InPS4Identity, FUniqueNetIdPS4& InUserId);

	virtual ~FUserOnlineAccountPS4()
	{
		delete AccountCS;
		AccountCS = nullptr;
	}

	void UpdateNames();

	// FOnlineUser
	virtual TSharedRef<const FUniqueNetId> GetUserId() const override { return UserId; }
	virtual inline FString GetRealName() const override { return FString(); }
	virtual inline FString GetDisplayName(const FString& Platform) const override { return Username; }
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;

	// FUserOnlineAccount
	virtual inline FString GetAccessToken() const override { return AccessToken.Data; }
	virtual inline bool GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const override { return GetUserAttribute(AttrName, OutAttrValue); }
	virtual bool SetUserAttribute(const FString& AttrName, const FString& AttrValue) override;
};

class FOnlineIdentityPS4 : public IOnlineIdentity
{
public:
	/** Reference to the main PS4 subsystem */
	class FOnlineSubsystemPS4* const PS4Subsystem;

private:
	enum class ELoginType
	{
		System,
		PSN
	};

	/** Accounts that have logged in */
	mutable TMap<SceUserServiceUserId, TSharedRef<FUserOnlineAccountPS4>> UserAccounts;

	void HandleLoginStatusChanged(ELoginType LoginType, bool bLoggingIn, SceUserServiceUserId UserId, int32 UserIndex);

PACKAGE_SCOPE:

	FOnlineIdentityPS4(class FOnlineSubsystemPS4* InSubsystem);

	TSharedPtr<FUserOnlineAccountPS4> GetOnlineAccount(SceUserServiceUserId UserId, bool bCreateIfNew) const;

	/** Translates the index of the local user to the corresponding SceUserService User Id from the PS4 system. */
	static SceUserServiceUserId GetSceUserId(int32 LocalUserIndex);

	/** Translates a SceUserService User Id to the local engine user index. */
	static int32 GetLocalUserIndex(SceUserServiceUserId UserId);

	/** Event handler for system login/logout */
	void UserSystemLoginEventHandler(bool bLoggingIn, SceUserServiceUserId UserId, int32 LocalUserNum);

	/** Event handler for PSN login/logout */
	void UserPlatformLoginEventHandler(bool bLoggingIn, SceUserServiceUserId UserId);

	bool GetCachedUserPrivilege(FUniqueNetIdPS4 const& UserId, EUserPrivileges::Type Privilege, uint32& OutPrivilegeFailures);

public:
	virtual ~FOnlineIdentityPS4() {};

	// IOnlineIdentity

	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;
	virtual bool AutoLogin(int32 LocalUserNum) override;
	virtual TSharedPtr<FUserOnlineAccount> GetUserAccount(const FUniqueNetId& UserId) const override;
	virtual TArray<TSharedPtr<FUserOnlineAccount> > GetAllUserAccounts() const override;
	virtual TSharedPtr<const FUniqueNetId> GetUniquePlayerId(int32 LocalUserNum) const override;
	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(uint8* Bytes, int32 Size) override;
	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(const FString& Str) override;
	virtual ELoginStatus::Type GetLoginStatus(int32 LocalUserNum) const override;
	virtual ELoginStatus::Type GetLoginStatus(const FUniqueNetId& UserId) const override;
	virtual FString GetPlayerNickname(int32 LocalUserNum) const override;
	virtual FString GetPlayerNickname(const FUniqueNetId& UserId) const override;
	virtual FString GetAuthToken(int32 LocalUserNum) const override;
	virtual void RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate) override;
	virtual void GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate) override;
	virtual FPlatformUserId GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const override;
	virtual FString GetAuthType() const override;
};

typedef TSharedPtr<FOnlineIdentityPS4, ESPMode::ThreadSafe> FOnlineIdentityPS4Ptr;
