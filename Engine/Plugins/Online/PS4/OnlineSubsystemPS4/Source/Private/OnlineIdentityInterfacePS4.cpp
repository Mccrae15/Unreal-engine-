// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineIdentityInterfacePS4.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "Misc/ScopeLock.h"
#include "Async/AsyncWork.h"
#include "OnlineError.h"

/** Async task wrapper for NpToolkit::Auth::getAuthCode */
class FOnlineAsyncGetAuthCode : public FOnlineAsyncTaskPS4
{
public:
	FOnlineAsyncGetAuthCode(FOnlineSubsystemPS4* InSubsystem, FUniqueNetIdPS4 const& InUserId, SceNpClientId const& InClientId)
		: FOnlineAsyncTaskPS4(InSubsystem)
		, UserId(InUserId.AsShared())
	{
		// Setup request for obtaining an auth code which can be used for s2s calls
		NpToolkit::Auth::Request::GetAuthCode Request;
		Request.userId = UserId->GetUserId();
		Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
		Request.async = true;
		Request.clientId = InClientId;
		FCStringAnsi::Strcpy(Request.scope, "psn:s2s");

		// kick off async call to query access code
		int32 RequestID = NpToolkit::Auth::getAuthCode(Request, &Response);
		if (RequestID < SCE_TOOLKIT_NP_V2_SUCCESS)
		{
			ErrorStr = FString::Printf(TEXT("getAuthCode failed (sync) : 0x%x"), RequestID);
			bWasSuccessful = false;
			bIsComplete = true;
		}
	}

	virtual ~FOnlineAsyncGetAuthCode() {}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncGetAuthCode bWasSuccessful: %d"), bWasSuccessful);
	}

	virtual void Tick() override 
	{
		if (!Response.isLocked())
		{
			bWasSuccessful = Response.getReturnCode() == SCE_TOOLKIT_NP_V2_SUCCESS;
			bIsComplete = true;
		}
	}

	virtual void Finalize() override
	{
		// Validate the response code
		if (!bWasSuccessful)
		{
			ErrorStr = FString::Printf(TEXT("getAuthCode failed (async) : 0x%08x"), Response.getReturnCode());
			return;
		}

		// Get the auth code and issuer id.
		NpToolkit::Auth::AuthCode const* AuthCode = Response.get();
		if (AuthCode == nullptr)
		{
			bWasSuccessful = false;
			ErrorStr = TEXT("getAuthCode succeeded but no auth code was returned.");
			return;
		}

		Token.Data = ANSI_TO_TCHAR(AuthCode->authCode.code);
		Token.IssuerId = AuthCode->issuerId;

		// Validate the auth token we just received
		if (Token.Data.Len() != 6)
		{
			UE_LOG_ONLINE(Warning, TEXT("PS4 Auth Token expected to be 6 characters, found %d in string %s"), Token.Data.Len(), *Token.Data);
		}
		else
		{
			for (int32 Index = 0; Index < Token.Data.Len(); ++Index)
			{
				if (!FChar::IsAlnum(Token.Data[Index]))
				{
					UE_LOG_ONLINE(Warning, TEXT("Encountered non alpha-numeric character in string %s."), *Token.Data);
					break;
				}
			}
		}

		bWasSuccessful = true;
	}

	virtual void TriggerDelegates() override
	{
		FOnlineIdentityPS4Ptr Identity = StaticCastSharedPtr<FOnlineIdentityPS4>(Subsystem->GetIdentityInterface());
		if (Identity.IsValid())
		{
			// Cache the access token in the user online account.
			TSharedPtr<FUserOnlineAccountPS4> Account = Identity->GetOnlineAccount(UserId->GetUserId(), false);
			if (Account.IsValid())
			{
				FScopeLock Lock(Account->AccountCS);
				Account->AccessToken = bWasSuccessful ? Token : FAccessToken();
			}

			if (bWasSuccessful)
			{
				// Update issuer ID from new token. This might update the reported PSN environment.
				Subsystem->SetIssuerId(Token.IssuerId);
			}

			Identity->TriggerOnLoginCompleteDelegates(
				FOnlineIdentityPS4::GetLocalUserIndex(UserId->GetUserId()), 
				bWasSuccessful,
				UserId.Get(),
				ErrorStr);
		}
	}

private:
	/** Ticket result from async call */
	NpToolkit::Core::Response<NpToolkit::Auth::AuthCode> Response;

	/** The local user account for the request */
	TSharedRef<FUniqueNetIdPS4 const> UserId;

	/** The retrieved access token from PSN */
	FAccessToken Token;

	/** Contains an error message if an error occurred, otherwise empty string. */
	FString ErrorStr;
};

class FAsyncEventGetUserPrivilegeComplete : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
private:
	FOnlineSubsystemPS4* PS4Subsystem;
	TSharedRef<FUserOnlineAccountPS4> Account;
	EUserPrivileges::Type Privilege;
	IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate Delegate;
	uint32 PrivilegeFailures;
	bool bHasPSPlus;

public:
	/**
	* Constructor.
	*
	* @param InPS4Subsystem The owner of the external UI interface that triggered this event.
	* @param InSessionName Name of the session we tried to destroy
	* @param bInWasSuccessful Whether or not the destroy was successful
	*/
	FAsyncEventGetUserPrivilegeComplete(FOnlineSubsystemPS4* InPS4Subsystem, TSharedRef<FUserOnlineAccountPS4> InAccount, EUserPrivileges::Type InPrivilege, const IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate& InDelegate, uint32 InPriviligeFailures, bool InbHasPSPlus)
		: FOnlineAsyncEvent(InPS4Subsystem)
		, PS4Subsystem(InPS4Subsystem)
		, Account(InAccount)
		, Privilege(InPrivilege)
		, Delegate(InDelegate)
		, PrivilegeFailures(InPriviligeFailures)
		, bHasPSPlus(InbHasPSPlus)
	{
	}

	virtual FString ToString() const override
	{
		return TEXT("Get user privilige task complete");
	}

	virtual void TriggerDelegates() override
	{
		Delegate.ExecuteIfBound(Account->UserId.Get(), Privilege, PrivilegeFailures);
	}

	virtual void Finalize() override
	{
		FOnlineIdentityPS4Ptr Identity = StaticCastSharedPtr<FOnlineIdentityPS4>(PS4Subsystem->GetIdentityInterface());
		if (Identity.IsValid())
		{
			FScopeLock Lock(Account->AccountCS);

			// Cache the result in the account
			Account->CachedUserPrivileges.FindOrAdd(Privilege) = PrivilegeFailures;

			// Cache PlayStation Plus status
			Account->SetUserAttribute(TEXT("psplus"), bHasPSPlus ? TEXT("1") : TEXT("0"));
		}
	}
};

class FOnlineAsyncTaskPS4GetUserPrivilege : public FNonAbandonableTask
{
public:

	FOnlineAsyncTaskPS4GetUserPrivilege(FOnlineSubsystemPS4* InPS4Subsystem, TSharedRef<FUserOnlineAccountPS4> InAccount, EUserPrivileges::Type InPrivilege, const IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate& InDelegate)
		: PS4Subsystem(InPS4Subsystem)
		, Account(InAccount)
		, Privilege(InPrivilege)
		, Delegate(InDelegate)
	{}

	void DoWork()
	{
		uint32 PrivilegeFailures = 0;
		int32 RequestId = 0;
		bool bHasPSPlus = false;

		SceUserServiceUserId SceUserId = Account->UserId->GetUserId();

		// these checks can take a long time, don't do them unless necessary.
		if (Privilege == EUserPrivileges::CanPlayOnline)
		{
			RequestId = sceNpCreateRequest();
			int32 CheckNpResult = sceNpCheckNpAvailabilityA(RequestId, SceUserId);
			sceNpDeleteRequest(RequestId);

			if (CheckNpResult == SCE_NP_ERROR_LATEST_SYSTEM_SOFTWARE_EXIST || CheckNpResult == SCE_NP_ERROR_LATEST_SYSTEM_SOFTWARE_EXIST_FOR_TITLE)
			{
				PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::RequiredSystemUpdate;
			}
			if (CheckNpResult == SCE_NP_ERROR_LATEST_PATCH_PKG_EXIST || CheckNpResult == SCE_NP_ERROR_LATEST_PATCH_PKG_DOWNLOADED)
			{
				PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::RequiredPatchAvailable;
			}
			if (CheckNpResult == SCE_NP_ERROR_AGE_RESTRICTION)
			{
				PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::AgeRestrictionFailure;
			}
			if (CheckNpResult == SCE_NP_ERROR_USER_NOT_FOUND)
			{
				PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::UserNotFound;
			}
			if (CheckNpResult == SCE_NP_ERROR_SIGNED_OUT || CheckNpResult == SCE_NP_ERROR_LOGOUT)
			{
				PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::UserNotLoggedIn;
			}
			// if sceNpCheckNpAvailability fails the results are invalid.
			if (CheckNpResult < 0)
			{
				PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::GenericFailure;
			}

			if (PS4Subsystem->IsPSNPlusRequired())
			{
				// check PS+
				RequestId = sceNpCreateRequest();
				SceNpCheckPlusParameter CheckPSPlusParam = {};
				SceNpCheckPlusResult CheckPlusResult = {};

				CheckPSPlusParam.size = sizeof(CheckPSPlusParam);
				CheckPSPlusParam.userId = SceUserId;
				CheckPSPlusParam.features = SCE_NP_PLUS_FEATURE_REALTIME_MULTIPLAY;

				int32 CheckPSPlusResult = sceNpCheckPlus(RequestId, &CheckPSPlusParam, &CheckPlusResult);

				sceNpDeleteRequest(RequestId);
				bHasPSPlus = CheckPSPlusResult == SCE_OK && CheckPlusResult.authorized;

				if (CheckPSPlusResult == SCE_OK && !CheckPlusResult.authorized)
				{
					PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::AccountTypeFailure;
				}
				// if sceNpCheckPlus fails the results are invalid.
				if (CheckPSPlusResult < 0)
				{
					PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::GenericFailure;
				}
			}
		}

		// we must only check parental content for UGC and Online communication.  Control over starting the game is presumably handled by the system software.
		// only check it if necessary as sceNpGetParentalControlInfosceNpGetParentalControlInfo is a network call with potentially long timeout.
		if (Privilege == EUserPrivileges::CanCommunicateOnline || Privilege == EUserPrivileges::CanUseUserGeneratedContent)
		{
			RequestId = sceNpCreateRequest();
			int8_t UserAge = 0;
			SceNpParentalControlInfo ParentalControlInfo;
			int CheckParentalRet = sceNpGetParentalControlInfoA(RequestId, SceUserId, &UserAge, &ParentalControlInfo);
			sceNpDeleteRequest(RequestId);

			if (CheckParentalRet == SCE_NP_ERROR_AGE_RESTRICTION)
			{
				PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::AgeRestrictionFailure;
			}
			else if (CheckParentalRet == SCE_NP_ERROR_SIGNED_OUT || CheckParentalRet == SCE_NP_ERROR_LOGOUT)
			{
				PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::UserNotLoggedIn;
			}
			else if (CheckParentalRet == SCE_NP_ERROR_USER_NOT_FOUND)
			{
				PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::UserNotFound;
			}
			// if sceNpGetParentalControlInfo fails the results are invalid.
			else if (CheckParentalRet < 0)
			{
				PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::GenericFailure;
			}
			else
			{
				// We should only check these flags when we don't get an error from sceNpGetParentalControlInfo,
				// according to Sony docs
				if (ParentalControlInfo.chatRestriction)
				{
					PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::ChatRestriction;
				}
				if (ParentalControlInfo.ugcRestriction)
				{
					PrivilegeFailures |= (uint32)IOnlineIdentity::EPrivilegeResults::UGCRestriction;
				}
			}
		}

		// mask out privilege failures by what each request cares about.
		uint32 PrivilegeMask = (uint32)IOnlineIdentity::EPrivilegeResults::AgeRestrictionFailure;
		const uint32 OnlineBasePrivilegeMask = (uint32)IOnlineIdentity::EPrivilegeResults::UserNotFound | (uint32)IOnlineIdentity::EPrivilegeResults::UserNotLoggedIn | (uint32)IOnlineIdentity::EPrivilegeResults::GenericFailure;

		switch (Privilege)
		{
		case EUserPrivileges::CanPlay:
			PrivilegeFailures &= PrivilegeMask;
			break;
		case EUserPrivileges::CanPlayOnline:
			PrivilegeMask |= OnlineBasePrivilegeMask | (uint32)IOnlineIdentity::EPrivilegeResults::RequiredSystemUpdate | (uint32)IOnlineIdentity::EPrivilegeResults::RequiredPatchAvailable | (uint32)IOnlineIdentity::EPrivilegeResults::AccountTypeFailure;
			PrivilegeFailures &= PrivilegeMask;
			break;
		case EUserPrivileges::CanCommunicateOnline:
			PrivilegeMask |= OnlineBasePrivilegeMask | (uint32)IOnlineIdentity::EPrivilegeResults::ChatRestriction;
			PrivilegeFailures &= PrivilegeMask;
			break;
		case EUserPrivileges::CanUseUserGeneratedContent:
			PrivilegeMask |= OnlineBasePrivilegeMask | (uint32)IOnlineIdentity::EPrivilegeResults::UGCRestriction;
			PrivilegeFailures &= PrivilegeMask;
			break;
		default:
			//Assume privilege if we don't handle it
			PrivilegeFailures = (uint32)IOnlineIdentity::EPrivilegeResults::NoFailures;
			break;
		}

		auto NewTask = new FAsyncEventGetUserPrivilegeComplete(PS4Subsystem, Account, Privilege, Delegate, PrivilegeFailures, bHasPSPlus);
		PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue(NewTask);
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FOnlineAsyncTaskPS4GetUserPrivilege, STATGROUP_ThreadPoolAsyncTasks );
	}

private:

	FOnlineSubsystemPS4* PS4Subsystem;
	TSharedRef<FUserOnlineAccountPS4> Account;
	EUserPrivileges::Type Privilege;
	IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate Delegate;

};

typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4GetUserPrivilege> FAutoDeleteGetUserPrivilegeTask;


FUserOnlineAccountPS4::FUserOnlineAccountPS4(FOnlineIdentityPS4 const* InPS4Identity, FUniqueNetIdPS4& InUserId)
	: AccountCS(new FCriticalSection)
	, PS4Identity(InPS4Identity)
	, UserId(InUserId.AsShared())
	, LoginStatus(ELoginStatus::NotLoggedIn)
{
}

void FUserOnlineAccountPS4::UpdateNames()
{
	int32 Result;
	SceNpOnlineId TempOnlineId;

	// Attempt to get the user's online ID. This will fail if the user is not signed into PSN.
	if ((Result = sceNpGetOnlineId(UserId->GetUserId(), &TempOnlineId)) == SCE_OK)
	{
		Username = PS4OnlineIdToString(TempOnlineId);
	}
	else
	{
		// Fall back to the local user name for players not signed into PSN.
		SceUserServiceUserId SceUserId = UserId->GetUserId();
		char UsernameBuffer[SCE_USER_SERVICE_MAX_USER_NAME_LENGTH + 1];

		if ((Result = sceUserServiceGetUserName(SceUserId, UsernameBuffer, sizeof(UsernameBuffer))) != SCE_OK)
		{
			UE_LOG_ONLINE(Error, TEXT("sceUserServiceGetUserName Failed : 0x%x, UserID: 0x%x"), Result, SceUserId);
			Username = TEXT("");
		}
		else
		{
			Username = UsernameBuffer;
		}
	}
}

bool FUserOnlineAccountPS4::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	if (AttrName.Equals(TEXT("id"), ESearchCase::IgnoreCase))
	{
		OutAttrValue = PS4AccountIdToString(UserId->GetAccountId());
		return true;
	}
	const FString* FoundVal = AccountData.Find(AttrName);
	if (FoundVal != NULL)
	{
		OutAttrValue = *FoundVal;
		return true;
	}
	return false;
}

bool FUserOnlineAccountPS4::SetUserAttribute(const FString& AttrName, const FString& AttrValue)
{
	FString& FoundVal = AccountData.FindOrAdd(AttrName);
	FoundVal = AttrValue;
	return true;
}


/** Translates the index of the local user to the corresponding SceUserService User Id from the PS4 system. */
SceUserServiceUserId FOnlineIdentityPS4::GetSceUserId(int32 LocalUserIndex)
{
	verify(LocalUserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS);
	return FPS4Application::GetPS4Application()->GetUserID(LocalUserIndex);
}

/** Translates a SceUserService User Id to the local engine user index. */
int32 FOnlineIdentityPS4::GetLocalUserIndex(SceUserServiceUserId UserId)
{
	return FPS4Application::GetPS4Application()->GetUserIndex(UserId);
}

FOnlineIdentityPS4::FOnlineIdentityPS4(class FOnlineSubsystemPS4* InSubsystem)
	: PS4Subsystem(InSubsystem)
{
	FCoreDelegates::OnUserLoginChangedEvent.AddRaw(this, &FOnlineIdentityPS4::UserSystemLoginEventHandler);
}

void FOnlineIdentityPS4::HandleLoginStatusChanged(ELoginType LoginType, bool bLoggingIn, SceUserServiceUserId UserId, int32 UserIndex)
{
	UE_LOG_ONLINE(Log, TEXT("PS4 Online User Service Event - %s %s, UserIndex: %d, UserID: %d"), 
		bLoggingIn ? TEXT("LogIn") : TEXT("LogOut"),
		LoginType == ELoginType::PSN ? TEXT("PSN") : TEXT("SYSTEM"),
		UserIndex,
		UserId);

	// Find the online account for this user (only create the account if we're logging in).
	TSharedPtr<FUserOnlineAccountPS4> Account = GetOnlineAccount(UserId, bLoggingIn);
	if (!Account.IsValid())
		return;

	FScopeLock Lock(Account->AccountCS);

	// Always clear the cached user privileges
	Account->CachedUserPrivileges.Empty();

	ELoginStatus::Type PreviousLoginStatus = Account->LoginStatus;
	if (bLoggingIn)
	{
		// Only upgrade login status. I.e. don't downgrade to UsingLocalProfile
		// if we receive a PSN login first, and a system login second.
		if (Account->LoginStatus != ELoginStatus::LoggedIn)
		{
			Account->LoginStatus = LoginType == ELoginType::PSN
				? ELoginStatus::LoggedIn
				: ELoginStatus::UsingLocalProfile;
		}
	}
	else
	{
		// Only downgrade login status. I.e. don't upgrade to UsingLocalProfile
		// if we receive a system logout first, and a PSN log out second.
		if (Account->LoginStatus != ELoginStatus::NotLoggedIn)
		{
			Account->LoginStatus = LoginType == ELoginType::PSN
				? ELoginStatus::UsingLocalProfile
				: ELoginStatus::NotLoggedIn;
		}
	}

	if (Account->LoginStatus == ELoginStatus::LoggedIn)
	{
		// We're logged in to PSN. Upgrade user ID to include the SceNpAccountId.
		if (!Account->UserId->IsOnlineId())
			Account->UserId->UpgradeLocal();
	}
	else
	{
		// Not logged in to PSN. No need to clear the SceNpAccountId here, as
		// it is permanently associated with the user's local account.
		Account->AccessToken = FAccessToken();
	}

	Account->UpdateNames();

	if (Account->LoginStatus != PreviousLoginStatus)
		TriggerOnLoginStatusChangedDelegates(UserIndex, PreviousLoginStatus, Account->LoginStatus, Account->UserId.Get());

	// Completely remove the user account instance if the user signed out of the system.
	if (Account->LoginStatus == ELoginStatus::NotLoggedIn && LoginType == ELoginType::System)
	{
		UserAccounts.Remove(UserId);
	}
}

void FOnlineIdentityPS4::UserSystemLoginEventHandler(bool bLoggingIn, SceUserServiceUserId UserId, int32 LocalUserNum)
{
	// We receive notifications of player login/logout of the PS4 system here.
	// e.g. we get a login notification when a player chooses their user account from the PS4 login screen.
	HandleLoginStatusChanged(ELoginType::System, bLoggingIn, UserId, LocalUserNum);
}

void FOnlineIdentityPS4::UserPlatformLoginEventHandler(bool bLoggingIn, SceUserServiceUserId UserId)
{
	// We receive notifications of PSN account login/logout here.
	// e.g. we get a log out notification when the player chooses "Sign Out" on the PlayStation Network settings page.
	HandleLoginStatusChanged(ELoginType::PSN, bLoggingIn, UserId, GetLocalUserIndex(UserId));
}

bool FOnlineIdentityPS4::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	// Find the existing user account.
	SceUserServiceUserId UserId = GetSceUserId(LocalUserNum);
	TSharedPtr<FUserOnlineAccountPS4> Account = GetOnlineAccount(UserId, false);

	if (Account.IsValid())
	{
		FScopeLock Lock(Account->AccountCS);

		SceNpClientId const& ClientId = PS4Subsystem->GetNpClientId();
		if (ClientId.id[0] != 0)
		{
			// The login task should generate a new auth token for the user.
			PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(new FOnlineAsyncGetAuthCode(PS4Subsystem, *Account->UserId, ClientId));
		}
		else
		{
			// No client ID set, so cannot generate an auth token. This game may not be using s2s calls.
			// Just complete the login immediately.
			UE_LOG_ONLINE(Warning, TEXT("No NpClientId specified. User will not receive an auth token."));
			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *Account->UserId, FString());
		}

		return true;
	}

	// No valid user account. Call failed.
	TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdPS4::GetInvalidUser(), FString());
	return false;
}

bool FOnlineIdentityPS4::AutoLogin(int32 LocalUserNum)
{
	return Login(LocalUserNum, FOnlineAccountCredentials());
}

bool FOnlineIdentityPS4::Logout(int32 LocalUserNum)
{
	SceUserServiceUserId UserId = GetSceUserId(LocalUserNum);
	TSharedPtr<FUserOnlineAccountPS4> Account = GetOnlineAccount(UserId, false);

	if (Account.IsValid())
	{
		FScopeLock Lock(Account->AccountCS);

		// If we get a log out call from the engine, restore the
		// user account to the state before Login() was called.
		Account->AccessToken = FAccessToken();
		Account->CachedUserPrivileges.Empty();

		TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
		return true;
	}

	return false;
}

TSharedPtr<FUserOnlineAccountPS4> FOnlineIdentityPS4::GetOnlineAccount(SceUserServiceUserId UserId, bool bCreateIfNew) const
{
	if (UserId == SCE_USER_SERVICE_USER_ID_INVALID)
		return nullptr;

	// Find the existing account...
	TSharedRef<FUserOnlineAccountPS4>* AccountPtr = UserAccounts.Find(UserId);
	if (AccountPtr)
	{
		return *AccountPtr;
	}

	if (bCreateIfNew)
	{
		// We haven't seen this user yet. Create one...
		TSharedPtr<FUserOnlineAccountPS4> NewAccount = MakeShared<FUserOnlineAccountPS4>(this, FUniqueNetIdPS4::FindOrCreate(UserId).Get());
		UserAccounts.Add(UserId, NewAccount.ToSharedRef());

		return NewAccount;
	}
	else
	{
		return nullptr;
	}
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityPS4::GetUserAccount(const FUniqueNetId& UserId) const
{
	const FUniqueNetIdPS4& PS4UserId = FUniqueNetIdPS4::Cast(UserId);

	TSharedPtr<FUserOnlineAccount> Result;

	const TSharedRef<FUserOnlineAccountPS4>* FoundUserAccount = UserAccounts.Find(PS4UserId.GetUserId());
	if (FoundUserAccount != NULL)
	{
		Result = *FoundUserAccount;
	}

	return Result;
}

TArray<TSharedPtr<FUserOnlineAccount>> FOnlineIdentityPS4::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount>> Result;

	for (auto It = UserAccounts.CreateConstIterator(); It; ++It)
	{
		Result.Add(It.Value());
	}

	return Result;
}

ELoginStatus::Type FOnlineIdentityPS4::GetLoginStatus(const FUniqueNetId& UserId) const
{
	const FUniqueNetIdPS4& PS4UserId = FUniqueNetIdPS4::Cast(UserId);
	TSharedPtr<FUserOnlineAccountPS4> Account = GetOnlineAccount(PS4UserId.GetUserId(), false);

	if (Account.IsValid())
	{
		FScopeLock Lock(Account->AccountCS);
		return Account->LoginStatus;
	}

	return ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FOnlineIdentityPS4::GetLoginStatus(int32 LocalUserNum) const 
{
	auto UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		return GetLoginStatus(*UserId);
	}

	return ELoginStatus::NotLoggedIn;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityPS4::GetUniquePlayerId(int32 LocalUserNum) const
{
	SceUserServiceUserId UserId = GetSceUserId(LocalUserNum);
	TSharedPtr<FUserOnlineAccountPS4> Account = GetOnlineAccount(UserId, false);

	if (Account.IsValid())
	{
		FScopeLock Lock(Account->AccountCS);
		return Account->UserId;
	}

	return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityPS4::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityPS4::CreateUniquePlayerId(uint8*, int32) is not implemented."));
	return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityPS4::CreateUniquePlayerId(const FString& Str)
{
	return FUniqueNetIdPS4::FromString(Str);
}

FString FOnlineIdentityPS4::GetPlayerNickname(int32 LocalUserNum) const
{
	SceUserServiceUserId UserId = GetSceUserId(LocalUserNum);
	TSharedPtr<FUserOnlineAccountPS4> Account = GetOnlineAccount(UserId, false);

	if (Account.IsValid())
	{
		FScopeLock Lock(Account->AccountCS);
		return Account->GetDisplayName(FString());
	}

	return FString();
}

FString FOnlineIdentityPS4::GetPlayerNickname(const FUniqueNetId& UserId) const 
{
	FUniqueNetIdPS4 const& PS4UserId = FUniqueNetIdPS4::Cast(UserId);
	TSharedPtr<FUserOnlineAccountPS4> Account = GetOnlineAccount(PS4UserId.GetUserId(), false);

	if (Account.IsValid())
	{
		FScopeLock Lock(Account->AccountCS);
		return Account->GetDisplayName(FString());
	}

	return FString();
}

FString FOnlineIdentityPS4::GetAuthType() const
{
	return TEXT("psn");
}

FString FOnlineIdentityPS4::GetAuthToken(int32 LocalUserNum) const
{
	SceUserServiceUserId UserId = GetSceUserId(LocalUserNum);
	TSharedPtr<FUserOnlineAccountPS4> Account = GetOnlineAccount(UserId, false);

	if (Account.IsValid())
	{
		FScopeLock Lock(Account->AccountCS);
		return Account->GetAccessToken();
	}

	return FString();
}

void FOnlineIdentityPS4::RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineIdentityPS4::RevokeAuthToken not implemented"));
	TSharedRef<const FUniqueNetId> UserIdRef(UserId.AsShared());
	PS4Subsystem->ExecuteNextTick([UserIdRef, Delegate]()
	{
		Delegate.ExecuteIfBound(*UserIdRef, FOnlineError(FString(TEXT("RevokeAuthToken not implemented"))));
	});
}

bool FOnlineIdentityPS4::GetCachedUserPrivilege(FUniqueNetIdPS4 const& UserId, EUserPrivileges::Type Privilege, uint32& OutPrivilegeFailures)
{
	TSharedPtr<FUserOnlineAccountPS4> Account = GetOnlineAccount(UserId.GetUserId(), false);
	check(Account.IsValid());

	FScopeLock Lock(Account->AccountCS);
	uint32* Result = Account->CachedUserPrivileges.Find(Privilege);
	if (Result)
	{
		OutPrivilegeFailures = *Result;
		return true;
	}
	else
	{
		return false;
	}
}

void FOnlineIdentityPS4::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	// Find the user account from the given net ID.
	FUniqueNetIdPS4 const& PS4UserId = FUniqueNetIdPS4::Cast(UserId);
	TSharedPtr<FUserOnlineAccountPS4> Account = GetOnlineAccount(PS4UserId.GetUserId(), false);
	check(Account.IsValid());

	// Do not use cached results to catch dynamic changing permissions (patches, etc)
	// Use GetCachedUserPrivilege directly, if desired
	{
		FScopeLock Lock(Account->AccountCS);
		(new FAutoDeleteGetUserPrivilegeTask(PS4Subsystem, Account.ToSharedRef(), Privilege, Delegate))->StartBackgroundTask();
	}
}

FPlatformUserId FOnlineIdentityPS4::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
	FUniqueNetIdPS4 const& NetIdPS4 = FUniqueNetIdPS4::Cast(UniqueNetId);
	return GetLocalUserIndex(NetIdPS4.GetUserId());
}
