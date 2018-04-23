// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineUserInterfacePS4.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineIdentityInterfacePS4.h"
#include "SlateApplication.h"


/**
 *	Task object for PS4 async query of users
 */
class FOnlineAsyncTaskPS4QueryUserInfo : public FOnlineAsyncTaskPS4
{
	static const int32 MaxUsersPerRequest = NpToolkit::UserProfile::Request::GetNpProfiles::SIZE_ACCOUNT_IDS;

public:

	/** Constructor */
	FOnlineAsyncTaskPS4QueryUserInfo(FOnlineSubsystemPS4* InSubsystem, const TArray<TSharedRef<const FUniqueNetId>>& InRequestedUserIds, TSharedRef<const FUniqueNetIdPS4> InLocalUserId, int32 InLocalUserIndex, FOnlineUserPS4Map& InUsersMap)
		: FOnlineAsyncTaskPS4(InSubsystem)
		, RequestedUserIds(InRequestedUserIds)
		, LocalUserId(InLocalUserId)
		, LocalUserIndex(InLocalUserIndex)
		, UsersMap(InUsersMap)
		, NumPlayersSuccessfullyLookedUp(0)
	{
		const int32 NumRequests = FMath::DivideAndRoundUp(RequestedUserIds.Num(), MaxUsersPerRequest);
		Responses.AddDefaulted(NumRequests);

		for (int32 RequestIndex = 0, AbsUserIndex = 0; RequestIndex < NumRequests; ++RequestIndex)
		{
			// Make a request for the user's profile data
			NpToolkit::UserProfile::Request::GetNpProfiles Request;
			Request.userId = LocalUserId->GetUserId();
			Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
			Request.async = true;
			Request.numValidAccountIds = FMath::Min(RequestedUserIds.Num() - AbsUserIndex, MaxUsersPerRequest);

			// Fill the account Id array for the current request
			for (int32 Index = 0; Index < Request.numValidAccountIds; ++Index)
			{
				Request.accountIds[Index] = FUniqueNetIdPS4::Cast(RequestedUserIds[AbsUserIndex++])->GetAccountId();
			}

			// Make an async request for the user's profile
			const int32 Ret = NpToolkit::UserProfile::getNpProfiles(Request, &Responses[RequestIndex]);
			if (Ret < SCE_TOOLKIT_NP_V2_SUCCESS)
			{
				if (!ErrorStr.IsEmpty())
				{
					ErrorStr += TEXT("; ");
				}
				ErrorStr += FString::Printf(TEXT("getNpProfiles failed: 0x%x"), Ret);
				UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo: %d/%d: getNpProfiles failed: 0x%x"), RequestIndex, NumRequests, Ret);

				if (Ret == SCE_TOOLKIT_NP_V2_ERROR_INCORRECT_ARGUMENTS)
				{
					// Unexpected, print the values we set
					UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo: Incorrect arguments: begin"));
					UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo:  userId=%d"), Request.userId);
					UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo:  serviceLabel=%d"), Request.serviceLabel);
					UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo:  async=%d"), Request.async);
					UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo:  numValidAccountIds=%d"), Request.numValidAccountIds);
					for (int32 Index = 0; Index < Request.numValidAccountIds; ++Index)
					{
						UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo:  accountIds[%d]=%s"), Index, *PS4AccountIdToString(Request.accountIds[Index]));
					}
					UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo: Incorrect arguments: end"));
				}
			}
		}
	}

	~FOnlineAsyncTaskPS4QueryUserInfo()
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4QueryUserInfo bWasSuccessful: %d RequestedUserIds.Num: %d NumPlayersSuccessfullyLookedUp: %d ErrorStr: %s"), bWasSuccessful, RequestedUserIds.Num(), NumPlayersSuccessfullyLookedUp, *ErrorStr);
	}

	virtual void Tick() override
	{
		// As long as at least one request is busy we have not completed this async task
		bool bAllDone = true;
		for (int32 ResponseIndex = 0; ResponseIndex < Responses.Num(); ++ResponseIndex)
		{
			if (Responses[ResponseIndex].isLocked())
			{
				bAllDone = false;
				break;
			}
		}

		if (bAllDone)
		{
			// None of our requests are busy anymore
			// Mark our task as successful if we have any users in any of our requests
			bWasSuccessful = false;
			for (NpToolkit::Core::Response<NpToolkit::UserProfile::NpProfiles>& Response : Responses)
			{
				const NpToolkit::UserProfile::NpProfiles* ProfilesList = Response.get();
				check(ProfilesList);

				if (ProfilesList->numNpProfiles > 0)
				{
					NumPlayersSuccessfullyLookedUp += ProfilesList->numNpProfiles;
				}
			}

			bWasSuccessful = NumPlayersSuccessfullyLookedUp == RequestedUserIds.Num();
			bIsComplete = true;
		}
	}

	virtual void Finalize() override
	{
		FOnlineAsyncTaskPS4::Finalize();

		// Grab the list we're writing user profile data to
		TMap<FString, FOnlineUserInfoPS4Ref>& UsersList = UsersMap.FindOrAdd(LocalUserId->ToString());

		for (int32 ResponseIndex = 0, AbsUserIndex = 0; ResponseIndex < Responses.Num(); ++ResponseIndex, AbsUserIndex += MaxUsersPerRequest)
		{
			const int32 UsersThisResponse = FMath::Min(RequestedUserIds.Num() - AbsUserIndex, MaxUsersPerRequest);
			NpToolkit::Core::Response<NpToolkit::UserProfile::NpProfiles>& Response = Responses[ResponseIndex];

			// Print out the ServerError if any exists
			const NpToolkit::Core::ServerError* ServerError = Response.getServerError();
			if (ServerError)
			{
				// Print out more info
				UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo: %d/%d: ServerError: httpStatusCode: %d"), ResponseIndex, Responses.Num(), ServerError->httpStatusCode);
				UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo: %d/%d: ServerError: webApiNextAvailableTime: %lld"), ResponseIndex, Responses.Num(), ServerError->webApiNextAvailableTime);

				// Ensure that the jsonData string is null terminated (documentation does not indicate it will be)
				char ServerErrorJsonData[NpToolkit::Core::ServerError::JSON_DATA_MAX_LEN + 1];
				FCStringAnsi::Strncpy(ServerErrorJsonData, ServerError->jsonData, NpToolkit::Core::ServerError::JSON_DATA_MAX_LEN + 1);
				int32 ServerErrorJsonDataLen = FCStringAnsi::Strlen(ServerErrorJsonData);
				if (ServerErrorJsonDataLen > 0)
				{
					auto ConvertedString = StringCast<TCHAR>(ServerErrorJsonData, ServerErrorJsonDataLen);
					UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo: %d/%d: ServerError: jsonData: %s"), ResponseIndex, Responses.Num(), ConvertedString.Get());
				}
				else
				{
					UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo: : %d/%d: ServerError: jsonData: <missing>"), ResponseIndex, Responses.Num());
				}
			}

			const NpToolkit::UserProfile::NpProfiles* ProfilesList = Response.get();
			check(ProfilesList);

			if (ProfilesList->numNpProfiles != UsersThisResponse)
			{
				if (!ErrorStr.IsEmpty())
				{
					ErrorStr += TEXT("; ");
				}
				ErrorStr += FString::Printf(TEXT("Requested %i UserIds, got %i"), UsersThisResponse, ProfilesList->numNpProfiles);
				UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4QueryUserInfo: Request %d/%d: Requested %i UserIds, got %i"), UsersThisResponse, ProfilesList->numNpProfiles);
			}

			for (int32 ProfileIndex = 0; ProfileIndex < ProfilesList->numNpProfiles; ++ProfileIndex)
			{
				const NpToolkit::UserProfile::NpProfile& CurrentNpUser = ProfilesList->npProfiles[ProfileIndex];

				// Write out our new user (updating if it already existed)
				const TSharedRef<FOnlineUserInfoPS4> NewUser = MakeShared<FOnlineUserInfoPS4>(CurrentNpUser);
				UsersList.Add(NewUser->GetUserId()->ToString(), NewUser);
			}
		}
	}

	virtual void TriggerDelegates() override
	{
		IOnlineUserPtr OnlineUserPtr = Subsystem->GetUserInterface();
		OnlineUserPtr->TriggerOnQueryUserInfoCompleteDelegates(LocalUserIndex, bWasSuccessful, RequestedUserIds, ErrorStr);
	}

private:

	/** The list of unique userIds we queried profile data for*/
	TArray< TSharedRef<const FUniqueNetId> > RequestedUserIds;

	/** The local ID/index requesting profile data */
	TSharedRef<const FUniqueNetIdPS4> LocalUserId;
	int32 LocalUserIndex;

	/** The map of online users we are writing too once the requests have finished */
	FOnlineUserPS4Map& UsersMap;

	/** The array of user profile data that is filled out asynchronously */
	TArray<NpToolkit::Core::Response<NpToolkit::UserProfile::NpProfiles>> Responses;

	/** The error string */
	FString ErrorStr;

	/** Number of players successfully looked up */
	int32 NumPlayersSuccessfullyLookedUp;
};

bool FOnlineUserInfoPS4::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	if (AttrName.Equals(TEXT("id"), ESearchCase::IgnoreCase))
	{
		OutAttrValue = PS4AccountIdToString(UniqueNetIdPS4->GetAccountId());
		return true;
	}
	return false;
}


// FOnlineUserPS4

FOnlineUserPS4::FOnlineUserPS4(class FOnlineSubsystemPS4* InSubsystem)
	: PS4Subsystem(InSubsystem)
	, UsersMap()
{
}


FOnlineUserPS4::~FOnlineUserPS4()
{
}


bool FOnlineUserPS4::QueryUserInfo(int32 LocalUserNum, const TArray<TSharedRef<const FUniqueNetId>>& UserIds)
{
	FString ErrorStr;
	bool bStarted = false;
	TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum));

	if (UserIds.Num() == 0)
	{
		ErrorStr = TEXT("No users specified");
	}
	else if (LocalUserId.IsValid() == false)
	{
		ErrorStr = TEXT("Invalid local user");
	}
	else
	{
		FOnlineAsyncTaskPS4QueryUserInfo* NewFOnlineAsyncTaskPS4QueryUserInfo = new FOnlineAsyncTaskPS4QueryUserInfo(PS4Subsystem, UserIds, LocalUserId.ToSharedRef(), LocalUserNum, UsersMap);
		PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(NewFOnlineAsyncTaskPS4QueryUserInfo);
		bStarted = true;
	}

	if (!bStarted)
	{
		UE_LOG_ONLINE(Warning, TEXT("QueryUserInfo request failed. %s"), *ErrorStr);
		TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, false, UserIds, ErrorStr);
	}

	return bStarted;
}

bool FOnlineUserPS4::GetAllUserInfo(int32 LocalUserNum, TArray<TSharedRef<FOnlineUser> >& OutUsers)
{
	OutUsers.Empty();
	TSharedPtr<const FUniqueNetId> LocalUserId = PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum);
	if (LocalUserId.IsValid())
	{
		TMap<FString, FOnlineUserInfoPS4Ref>* UserList = UsersMap.Find(LocalUserId->ToString());
		if (UserList != nullptr)
		{
			for (const TPair<FString, FOnlineUserInfoPS4Ref>& Pair : *UserList)
			{
				OutUsers.Add(Pair.Value);
			}
			return true;
		}
	}
	return false;
}

TSharedPtr<FOnlineUser> FOnlineUserPS4::GetUserInfo(int32 LocalUserNum, const FUniqueNetId& UserId)
{
	TSharedPtr<FOnlineUser> Result;
	TSharedPtr<const FUniqueNetId> LocalUserId = PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum);
	if (LocalUserId.IsValid())
	{
		TMap<FString, FOnlineUserInfoPS4Ref>* UserList = UsersMap.Find(LocalUserId->ToString());
		if (UserList != nullptr)
		{
			for (const TPair<FString, FOnlineUserInfoPS4Ref>& Pair : *UserList)
			{
				const FUniqueNetId& CurrentUserId = *Pair.Value->GetUserId();
				if (CurrentUserId == UserId)
				{
					Result = Pair.Value;
					break;
				}
			}
		}
	}
	return Result;
}

bool FOnlineUserPS4::QueryUserIdMapping(const FUniqueNetId& UserId, const FString& DisplayName, const FOnQueryUserMappingComplete& CompletionDelegate)
{
	if (DisplayName.IsEmpty())
	{
		FString ErrorStr = TEXT("No display name specified");

		UE_LOG_ONLINE(Warning, TEXT("QueryUserIdMapping request failed. %s"), *ErrorStr);
		CompletionDelegate.ExecuteIfBound(false, UserId, DisplayName, FUniqueNetIdPS4::GetInvalidUser(), ErrorStr);

		return false;
	}
	else
	{
		// Convert the display name string to an online ID.
		SceNpOnlineId OnlineId = PS4StringToOnlineId(DisplayName);

		// Resolve the online id into an account id. This may complete synchronously if the online id is already in the cache.
		FOnIdResolveComplete ResolveDelegate;
		TSharedRef<FUniqueNetId const> UserIdRef = UserId.AsShared();
		ResolveDelegate.AddLambda([=](FOnlineIdMap const& InResolvedIds, bool bInWasSuccessful, FString const& InErrorString)
		{
			FString ErrorStr;
			SceNpAccountId AccountId;
			TSharedRef<FUniqueNetIdPS4 const> FoundUserId = FUniqueNetIdPS4::GetInvalidUser().AsShared();

			bool bSuccess = true;
			if (!bInWasSuccessful)
			{
				ErrorStr = InErrorString;
				bSuccess = false;
			}
			else if (!InResolvedIds.GetAccountId(OnlineId, AccountId))
			{
				ErrorStr = FString::Printf(TEXT("Did not find any PSN profile for user '%s'."), *DisplayName);
				bSuccess = false;
			}
			else
			{
				// We've got a valid account id.
				FoundUserId = FUniqueNetIdPS4::FindOrCreate(AccountId);
			}

			if (!bSuccess)
			{
				UE_LOG_ONLINE(Warning, TEXT("QueryUserIdMapping request failed. %s"), *ErrorStr);
			}

			CompletionDelegate.ExecuteIfBound(bSuccess, *UserIdRef, DisplayName, *FoundUserId, ErrorStr);
		});

		PS4Subsystem->ResolveAccountIdsAsync(FUniqueNetIdPS4::Cast(UserId), { OnlineId }, ResolveDelegate);
	}

	return true;
}

bool FOnlineUserPS4::QueryExternalIdMappings(const FUniqueNetId& LocalUserId, const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, const FOnQueryExternalIdMappingsComplete& Delegate)
{
	// Not implemented for PS4 - Call delegate
	Delegate.ExecuteIfBound(false, LocalUserId, QueryOptions, ExternalIds, TEXT("not implemented"));
	return false;
}

void FOnlineUserPS4::GetExternalIdMappings(const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, TArray<TSharedPtr<const FUniqueNetId>>& OutIds)
{
	// Not implemented for PS4 - return an array full of empty values
	OutIds.SetNum(ExternalIds.Num());
}

TSharedPtr<const FUniqueNetId> FOnlineUserPS4::GetExternalIdMapping(const FExternalIdQueryOptions& QueryOptions, const FString& ExternalId)
{
	// Not implemented for PS4 - return an empty value
	return TSharedPtr<FUniqueNetId>();
}
