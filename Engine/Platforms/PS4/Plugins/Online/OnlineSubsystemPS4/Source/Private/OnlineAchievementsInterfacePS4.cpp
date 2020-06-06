// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAchievementsInterfacePS4.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineError.h"

#define LOCTEXT_NAMESPACE "OnlineSubsystemPS4"
#define ONLINE_ERROR_NAMESPACE "errors.com.epicgames.oss.achievements"

inline static const TCHAR* LexToCString(const ETrophyServiceRegistrationStatus InStatus)
{
	switch (InStatus)
	{
	case ETrophyServiceRegistrationStatus::NotRegistered:
		return TEXT("NotRegistered");
	case ETrophyServiceRegistrationStatus::Registering:
		return TEXT("Registering");
	case ETrophyServiceRegistrationStatus::Registered:
		return TEXT("Registered");
	case ETrophyServiceRegistrationStatus::Failed:
		return TEXT("Failed");
	}
	checkf(false, TEXT("Unexpected registration status %d"), static_cast<int32>(InStatus));
	return TEXT("Invalid");
}

/**
 * Async task that registers with the trophy service
 * Other trophy tasks require that we are registered with the trophy service before they can work
 */
class FOnlineAsyncTaskPS4RegisterTrophyPack : public FOnlineAsyncTaskPS4
{
public:
	/** Constructor */
	FOnlineAsyncTaskPS4RegisterTrophyPack(FOnlineSubsystemPS4* const InSubsystem, const FUniqueNetIdPS4& InLocalUserId)
		: FOnlineAsyncTaskPS4(InSubsystem)
		, LocalUserId(InLocalUserId.AsShared())
		, Result(false)
	{
		FOnlineAchievementsPS4Ptr AchievementsPtr = StaticCastSharedPtr<FOnlineAchievementsPS4>(Subsystem->GetAchievementsInterface());
		check(AchievementsPtr.IsValid()); // Must have achievements to create this task
		check(AchievementsPtr->GetUserTrophyRegistrationStatus(*LocalUserId) == ETrophyServiceRegistrationStatus::NotRegistered);
		AchievementsPtr->SetUserTrophyRegistrationStatus(*LocalUserId, ETrophyServiceRegistrationStatus::Registering);

		// Register with the trophy service
		NpToolkit::Trophy::Request::RegisterTrophyPack Request;
		Request.userId = LocalUserId->GetUserId();
		Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
		Request.async = true;

		const int32 RequestId = NpToolkit::Trophy::registerTrophyPack(Request, &Response);
		if (RequestId < SCE_TOOLKIT_NP_V2_SUCCESS)
		{
			Result = OnlineAchievementsPS4::Errors::RequestFailure(RequestId);
		}
	}

	//~ Begin FOnlineAsyncTaskPS4 interface
	virtual void Tick() override
	{
		if (!Response.isLocked())
		{
			const int32 ReturnCode = Response.getReturnCode();
			if (ReturnCode == SCE_TOOLKIT_NP_V2_SUCCESS)
			{
				Result.bSucceeded = true;
			}
			else
			{
				const NpToolkit::Core::ServerError* const ServerError = Response.getServerError();
				if (ServerError)
				{
					PrintNPToolkitServerError(TEXT("FOnlineAsyncTaskPS4RegisterTrophyPack"), *ServerError);
				}
				Result = OnlineAchievementsPS4::Errors::ResultsError(ReturnCode);
			}
			UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("FOnlineAsyncTaskPS4RegisterTrophyPack: User=[%s] registering with the trophy service completed with result=[%s]"), *LocalUserId->ToDebugString(), *Result.ToLogString());

			FOnlineAchievementsPS4Ptr AchievementsPtr = StaticCastSharedPtr<FOnlineAchievementsPS4>(Subsystem->GetAchievementsInterface());
			check(AchievementsPtr.IsValid()); // Must have achievements to create this task
			ETrophyServiceRegistrationStatus NewStatus = Result.WasSuccessful() ? ETrophyServiceRegistrationStatus::Registered : ETrophyServiceRegistrationStatus::Failed;
			AchievementsPtr->SetUserTrophyRegistrationStatus(*LocalUserId, NewStatus);

			bWasSuccessful = Result.WasSuccessful();
			bIsComplete = true;
		}
	}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4RegisterTrophyPack bWasSuccessful=%d Result=[%s]"), WasSuccessful(), *Result.ToLogString());
	}
	//~ End FOnlineAsyncTaskPS4 Interface
protected:
	/** Local user that we are registering for */
	const TSharedRef<const FUniqueNetIdPS4> LocalUserId;
	/** Response that will be filled out asynchronously */
	NpToolkit::Core::Response<NpToolkit::Core::Empty> Response;
	/** Result */
	FOnlineError Result;
};

/**
 * Base class for trophy tasks that require registration with the trophy service
 * @see FOnlineAsyncTaskPS4RegisterTrophyPack
 */
class FOnlineAsyncTaskPS4TrophyBase : public FOnlineAsyncTaskPS4
{
public:
	/** Constructor */
	FOnlineAsyncTaskPS4TrophyBase(FOnlineSubsystemPS4* const InSubsystem, const FUniqueNetIdPS4& InLocalUserId)
		: FOnlineAsyncTaskPS4(InSubsystem)
		, LocalUserId(InLocalUserId.AsShared())
		, Result(false)
	{
	}

	virtual void Tick() override
	{
		if (bRequestStarted)
		{
			TickTrophyRequest();
		}
		else
		{
			// Check if we are registered, if not, delay until registration completes
			FOnlineAchievementsPS4Ptr AchievementsPtr = StaticCastSharedPtr<FOnlineAchievementsPS4>(Subsystem->GetAchievementsInterface());
			check(AchievementsPtr.IsValid()); // Must have achievements to create this task
			ETrophyServiceRegistrationStatus RegistrationStatus = AchievementsPtr->GetUserTrophyRegistrationStatus(*LocalUserId);
			if (RegistrationStatus == ETrophyServiceRegistrationStatus::NotRegistered)
			{
				// Start registering the user
				FOnlineAsyncTaskPS4RegisterTrophyPack* RegisterTask = new FOnlineAsyncTaskPS4RegisterTrophyPack(Subsystem, *LocalUserId);
				Subsystem->GetAsyncTaskManager()->AddToParallelTasks(RegisterTask);
				check(AchievementsPtr->GetUserTrophyRegistrationStatus(*LocalUserId) == ETrophyServiceRegistrationStatus::Registering);
			}
			else if (RegistrationStatus == ETrophyServiceRegistrationStatus::Failed)
			{
				// End the task as failed
				Result.SetFromErrorCode(ACHIEVEMENTS_FAILURE_NOT_REGISTERED);
				bWasSuccessful = false;
				bIsComplete = true;
			}
			else if (RegistrationStatus == ETrophyServiceRegistrationStatus::Registering)
			{
				// Do nothing - wait for the registration to complete
			}
			else
			{
				// Start the request
				bRequestStarted = true;
				StartTrophyRequest();
			}
		}
	}

	/** Start the trophy request, called after we are registered with the trophy service */
	virtual void StartTrophyRequest() = 0;
	/** Tick the trophy task, called each tick after StartTrophyRequest */
	virtual void TickTrophyRequest() = 0;

protected:
	/** Local user that we are registering for */
	const TSharedRef<const FUniqueNetIdPS4> LocalUserId;
	/** Result */
	FOnlineError Result;
private:
	/** Has the request been started? We might be waiting on registration */
	bool bRequestStarted = false;
};

class FOnlineAsyncTaskPS4QueryUnlockedTrophies : public FOnlineAsyncTaskPS4TrophyBase
{
public:
	/** Constructor */
	FOnlineAsyncTaskPS4QueryUnlockedTrophies(FOnlineSubsystemPS4* const InSubsystem, const FUniqueNetIdPS4& InLocalUserId, const FOnQueryAchievementsCompleteDelegate& InCompletionDelegate)
		: FOnlineAsyncTaskPS4TrophyBase(InSubsystem, InLocalUserId)
		, CompletionDelegate(InCompletionDelegate)
	{
	}

	//~ Begin FOnlineAsyncTaskPS4TrophyBase interface
	virtual void StartTrophyRequest() override
	{
		NpToolkit::Trophy::Request::GetUnlockedTrophies Request;
		Request.userId = LocalUserId->GetUserId();
		Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
		Request.async = true;

		const int32 RequestId = NpToolkit::Trophy::getUnlockedTrophies(Request, &UnlockedTrophiesResponse);
		if (RequestId < SCE_TOOLKIT_NP_V2_SUCCESS)
		{
			Result = OnlineAchievementsPS4::Errors::RequestFailure(RequestId);
		}
	}

	virtual void TickTrophyRequest() override
	{
		if (!UnlockedTrophiesResponse.isLocked())
		{
			const int32 ReturnCode = UnlockedTrophiesResponse.getReturnCode();
			if (ReturnCode == SCE_TOOLKIT_NP_V2_SUCCESS)
			{
				Result.bSucceeded = true;
			}
			else
			{
				const NpToolkit::Core::ServerError* const ServerError = UnlockedTrophiesResponse.getServerError();
				if (ServerError)
				{
					PrintNPToolkitServerError(TEXT("FOnlineAsyncTaskPS4QueryUnlockedTrophies"), *ServerError);
				}
				Result = OnlineAchievementsPS4::Errors::ResultsError(ReturnCode);
			}
			UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("FOnlineAsyncTaskPS4QueryUnlockedTrophies: User=[%s] query unlocked trophies completed with result=[%s]"), *LocalUserId->ToDebugString(), *Result.ToLogString());

			bWasSuccessful = Result.bSucceeded;
			bIsComplete = true;
		}
	}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4QueryUnlockedTrophies bWasSuccessful=%d Result=[%s]"), WasSuccessful(), *Result.ToLogString());
	}

	virtual void Finalize() override
	{
		FOnlineAsyncTaskPS4::Finalize();

		FOnlineAchievementsPS4Ptr AchievementsPtr = StaticCastSharedPtr<FOnlineAchievementsPS4>(Subsystem->GetAchievementsInterface());
		check(AchievementsPtr.IsValid()); // Must have achievements to create this task
		
		if (Result.WasSuccessful())
		{
			const NpToolkit::Trophy::UnlockedTrophies* const ResponseUnlockedTrophies = UnlockedTrophiesResponse.get();
			check(ResponseUnlockedTrophies);

			const int32 NumUnlockedTrophies = static_cast<int32>(ResponseUnlockedTrophies->numTrophiesIds);
			UE_LOG_ONLINE_ACHIEVEMENTS(VeryVerbose, TEXT("FOnlineAsyncTaskPS4QueryUnlockedTrophies: user=[%s] has %d unlocked trophies"), *LocalUserId->ToDebugString(), NumUnlockedTrophies);

			// Populate our achievements with no progress
			FOnlineUserAchievementsMapPS4 Achievements;
			AchievementsPtr->PopulateBlankAchievements(Achievements);
			
			// Update the progress of the achievements
			for (int32 UnlockedTrophyIndex = 0; UnlockedTrophyIndex < NumUnlockedTrophies; ++UnlockedTrophyIndex)
			{
				const SceNpTrophyId TrophyId = ResponseUnlockedTrophies->trophiesIds[UnlockedTrophyIndex];
				FOnlineAchievement* const Achievement = Achievements.Find(TrophyId);
				if (Achievement)
				{
					UE_LOG_ONLINE_ACHIEVEMENTS(VeryVerbose, TEXT("FOnlineAsyncTaskPS4QueryUnlockedTrophies: user=[%s] has trophy=%d unlocked"), *LocalUserId->ToDebugString(), static_cast<int32>(TrophyId));
					Achievement->Progress = 100.0;
				}
				else
				{
					UE_LOG_ONLINE_ACHIEVEMENTS(Verbose, TEXT("FOnlineAsyncTaskPS4QueryUnlockedTrophies: Unlocked trophyid=%d not found in the trophy map"), static_cast<int32>(TrophyId));
				}
			}
			AchievementsPtr->SetUserAchievements(*LocalUserId, MoveTemp(Achievements));
		}
	}

	virtual void TriggerDelegates() override
	{
		FOnlineAsyncTaskPS4TrophyBase::TriggerDelegates();
		CompletionDelegate.ExecuteIfBound(*LocalUserId, Result.WasSuccessful());
	}
	//~ End FOnlineAsyncTaskPS4TrophyBase Interface
protected:
	/** Response that will be filled out asynchronously */
	NpToolkit::Core::Response<NpToolkit::Trophy::UnlockedTrophies> UnlockedTrophiesResponse;
	/** Delegate to trigger when the query is completed */
	FOnQueryAchievementsCompleteDelegate CompletionDelegate;
};

class FOnlineAsyncTaskPS4QueryTrophyDescriptions : public FOnlineAsyncTaskPS4TrophyBase
{
public:
	/** Constructor */
	FOnlineAsyncTaskPS4QueryTrophyDescriptions(FOnlineSubsystemPS4* const InSubsystem, const FUniqueNetIdPS4& InLocalUserId, const FOnQueryAchievementsCompleteDelegate& InCompletionDelegate)
		: FOnlineAsyncTaskPS4TrophyBase(InSubsystem, InLocalUserId)
		, MaxSimultaneousTrophyQueries(10)
		, CompletionDelegate(InCompletionDelegate)
	{
		GConfig->GetInt(TEXT("OnlineSubsystemPS4"), TEXT("MaxSimultaneousTrophyQueries"), MaxSimultaneousTrophyQueries, GEngineIni);
	}

	//~ Begin FOnlineAsyncTaskPS4TrophyBase interface
	virtual void StartTrophyRequest() override
	{
		// Query the summary first, when that completes let the subtask perform their operations
		StartSummaryQuery();
	}

	virtual void TickTrophyRequest() override
	{
		if (!bTrophyPackSummaryProcessed)
		{
			TickSummaryQuery();
		}
		else
		{
			TickTrophiesQuery();
		}
	}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4QueryTrophyDescriptions bWasSuccessful=%d Result=[%s]"), WasSuccessful(), *Result.ToLogString());
	}

	virtual void Finalize() override
	{
		FOnlineAsyncTaskPS4::Finalize();

		FOnlineAchievementsPS4Ptr AchievementsPtr = StaticCastSharedPtr<FOnlineAchievementsPS4>(Subsystem->GetAchievementsInterface());
		check(AchievementsPtr.IsValid()); // Must have achievements to create this task
		
		if (Result.WasSuccessful())
		{
			AchievementsPtr->SetAchievementDescriptions(MoveTemp(AchievementDescriptions));
		}
	}

	virtual void TriggerDelegates() override
	{
		FOnlineAsyncTaskPS4TrophyBase::TriggerDelegates();
		CompletionDelegate.ExecuteIfBound(*LocalUserId, Result.WasSuccessful());
	}
	//~ End FOnlineAsyncTaskPS4TrophyBase Interface

	/** Start the summary query */
	void StartSummaryQuery()
	{
		// Get the trophy pack summary to identify the number of trophies, then query the data for each individual trophy
		sce::Toolkit::NP::V2::Trophy::Request::GetTrophyPackSummary Request;
		Request.userId = LocalUserId->GetUserId();
		Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
		Request.async = true;
		Request.retrieveTrophyPackSummaryIcon = false;

		const int32 RequestId = NpToolkit::Trophy::getTrophyPackSummary(Request, &TrophyPackSummaryResponse);
		if (RequestId < SCE_TOOLKIT_NP_V2_SUCCESS)
		{
			Result = OnlineAchievementsPS4::Errors::RequestFailure(RequestId);
		}
	}

	/** Tick the summary query */
	void TickSummaryQuery()
	{
		if (!TrophyPackSummaryResponse.isLocked())
		{
			bTrophyPackSummaryProcessed = true;
			const int32 ReturnCode = TrophyPackSummaryResponse.getReturnCode();
			if (ReturnCode == SCE_TOOLKIT_NP_V2_SUCCESS)
			{
				StartTrophiesQuery();
			}
			else
			{
				const NpToolkit::Core::ServerError* const ServerError = TrophyPackSummaryResponse.getServerError();
				if (ServerError)
				{
					PrintNPToolkitServerError(TEXT("FOnlineAsyncTaskPS4QueryTrophyDescriptions: TrophyPackSummary"), *ServerError);
				}
				Result = OnlineAchievementsPS4::Errors::ResultsError(ReturnCode);
				bWasSuccessful = Result.WasSuccessful();
				bIsComplete = true;
			}
		}
	}

	/** Start the trophy queries */
	void StartTrophiesQuery()
	{
		// Query the details of the individual trophies
		const NpToolkit::Trophy::TrophyPackSummary* const ResponseTrophyPackSummary = TrophyPackSummaryResponse.get();
		check(ResponseTrophyPackSummary);

		const int32 NumTrophies = ResponseTrophyPackSummary->staticConfiguration.numTrophies;
		UE_LOG_ONLINE_ACHIEVEMENTS(Verbose, TEXT("FOnlineAsyncTaskPS4QueryTrophyDescriptions: User=[%s] query trophy descriptions summary completed with %d trophies, beginning the individual trophy lookup"), *LocalUserId->ToDebugString(), NumTrophies);

		const int32 NumTrophiesToQuery = FMath::Min(NumTrophies, MaxSimultaneousTrophyQueries);
		TrophyPackTrophyQueries.SetNum(NumTrophiesToQuery);

		for (int32 TrophyIndex = 0; TrophyIndex < NumTrophiesToQuery; ++TrophyIndex)
		{
			FTrophyPackTrophyQuery& TrophyPackTrophyQuery = TrophyPackTrophyQueries[TrophyIndex];
			TrophyPackTrophyQuery.TrophyId = static_cast<SceNpTrophyId>(TrophyIndex);
			NpToolkit::Core::Response<NpToolkit::Trophy::TrophyPackTrophy>& TrophyPackTrophyResponse = TrophyPackTrophyQuery.Response;

			NpToolkit::Trophy::Request::GetTrophyPackTrophy Request;
			Request.userId = LocalUserId->GetUserId();
			Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
			Request.async = true;
			Request.trophyId = TrophyPackTrophyQuery.TrophyId;
			Request.retrieveTrophyPackTrophyIcon = false;

			const int32 RequestId = NpToolkit::Trophy::getTrophyPackTrophy(Request, &TrophyPackTrophyResponse);
			if (RequestId < SCE_TOOLKIT_NP_V2_SUCCESS)
			{
				UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("FOnlineAsyncTaskPS4QueryTrophyDescriptions: User=[%s] query trophy description for trophyid=%d failed to start with result=0x%08x"), *LocalUserId->ToDebugString(), TrophyIndex, RequestId);
				TrophyPackTrophyQuery.Reset();
			}
		}

		// If we have any trophies we haven't queried, add them to our pending list
		TrophyIdsAwaitingQuery.Reserve(NumTrophies - NumTrophiesToQuery);
		for (int32 TrophyIndex = NumTrophiesToQuery; TrophyIndex < NumTrophies; ++TrophyIndex)
		{
			TrophyIdsAwaitingQuery.Emplace(static_cast<SceNpTrophyId>(TrophyIndex));
		}
	}

	void TickTrophiesQuery()
	{
		int32 NumOutstandingRequests = 0;
		for (FTrophyPackTrophyQuery& TrophyPackTrophyQuery : TrophyPackTrophyQueries)
		{
			NpToolkit::Core::Response<NpToolkit::Trophy::TrophyPackTrophy>& TrophyPackTrophyResponse = TrophyPackTrophyQuery.Response;
			NpToolkit::Core::ResponseState ResponseState = TrophyPackTrophyResponse.getState();
			if (ResponseState == NpToolkit::Core::ResponseState::ready)
			{
				// Process the result then query up the next trophy
				const int32 ReturnCode = TrophyPackTrophyResponse.getReturnCode();
				if (ReturnCode == SCE_TOOLKIT_NP_V2_SUCCESS)
				{
					const NpToolkit::Trophy::TrophyPackTrophy* const ResponseTrophyPackTrophy = TrophyPackTrophyResponse.get();
					check(ResponseTrophyPackTrophy);

					const SceNpTrophyDetails& TrophyDetails(ResponseTrophyPackTrophy->staticConfiguration);

					FString TrophyName(UTF8_TO_TCHAR(TrophyDetails.name));
					FString TrophyDescription(UTF8_TO_TCHAR(TrophyDetails.description));

					UE_LOG_ONLINE_ACHIEVEMENTS(VeryVerbose, TEXT("FOnlineAsyncTaskPS4QueryTrophyDescriptions: Queried trophy details for trophyid=%d title=[%s] description=[%s] hidden=%d"),
						static_cast<int32>(TrophyPackTrophyQuery.TrophyId), *TrophyName, *TrophyDescription, static_cast<int32>(TrophyDetails.hidden));

					FOnlineAchievementDesc AchievementDescription;
					AchievementDescription.Title = FText::FromString(MoveTemp(TrophyName));
					AchievementDescription.LockedDesc = FText::FromString(MoveTemp(TrophyDescription));
					AchievementDescription.UnlockedDesc = AchievementDescription.LockedDesc;
					AchievementDescription.bIsHidden = TrophyDetails.hidden;

					AchievementDescriptions.Emplace(TrophyPackTrophyQuery.TrophyId, AchievementDescription);
				}
				else
				{
					UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("FOnlineAsyncTaskPS4QueryTrophyDescriptions: Queried trophy details for trophyid=%d failed with result=0x%08x"), static_cast<int32>(TrophyPackTrophyQuery.TrophyId), ReturnCode);
					const NpToolkit::Core::ServerError* const ServerError = TrophyPackTrophyResponse.getServerError();
					if (ServerError)
					{
						PrintNPToolkitServerError(FString::Printf(TEXT("FOnlineAsyncTaskPS4QueryTrophyDescriptions: TrophyPackTrophyResponse trophyid=%d"), static_cast<int32>(TrophyPackTrophyQuery.TrophyId)), *ServerError);
					}
				}

				TrophyPackTrophyQuery.Reset();
				ResponseState = TrophyPackTrophyResponse.getState();
			}
			else if (ResponseState == NpToolkit::Core::ResponseState::locked)
			{
				++NumOutstandingRequests;
			}
			
			if (ResponseState == NpToolkit::Core::ResponseState::reset && TrophyIdsAwaitingQuery.Num() > 0)
			{
				// Do we need to query any more?
				const SceNpTrophyId NextTrophyId = TrophyIdsAwaitingQuery.Pop();
				TrophyPackTrophyQuery.TrophyId = NextTrophyId;

				NpToolkit::Trophy::Request::GetTrophyPackTrophy Request;
				Request.userId = LocalUserId->GetUserId();
				Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
				Request.async = true;
				Request.trophyId = TrophyPackTrophyQuery.TrophyId;
				Request.retrieveTrophyPackTrophyIcon = false;

				const int32 RequestId = NpToolkit::Trophy::getTrophyPackTrophy(Request, &TrophyPackTrophyResponse);
				if (RequestId >= SCE_TOOLKIT_NP_V2_SUCCESS)
				{
					++NumOutstandingRequests;
				}
				else
				{
					UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("FOnlineAsyncTaskPS4QueryTrophyDescriptions: User=[%s] query trophy description for trophyid=%d failed to start with result=0x%08x"), *LocalUserId->ToDebugString(), NextTrophyId, RequestId);
					TrophyPackTrophyResponse.reset();
				}
			}
		}

		if (NumOutstandingRequests == 0 && TrophyIdsAwaitingQuery.Num() == 0)
		{
			// We are done
			Result.bSucceeded = true;
			bWasSuccessful = true;
			bIsComplete = true;
		}
	}

protected:
	struct FTrophyPackTrophyQuery
	{
		/** Constructor */
		FTrophyPackTrophyQuery()
		{
			Reset();
		}

		/** Reset for using in new queries */
		void Reset()
		{
			TrophyId = SCE_NP_TROPHY_INVALID_TROPHY_ID;
			Response.reset();
		}

		/** The id of the trophy we are querying */
		SceNpTrophyId TrophyId;
		/** Response containing details about the trophy that will be filled out asynchronously */
		NpToolkit::Core::Response<NpToolkit::Trophy::TrophyPackTrophy> Response;
	};

	/** Response containing number of trophies that will be filled out asynchronously */
	NpToolkit::Core::Response<NpToolkit::Trophy::TrophyPackSummary> TrophyPackSummaryResponse;
	/** Have we processed the trophy pack summary response? If so then we are processing the individual trophy lookups */
	bool bTrophyPackSummaryProcessed = false;
	/** The maximum number of trophies to query simultaneously */
	int32 MaxSimultaneousTrophyQueries;
	/** List of trophy ids that we have not started a query for yet */
	TArray<SceNpTrophyId> TrophyIdsAwaitingQuery;
	/** Responses containing details about individual trophies. Max size is MaxSimultaneousTrophyQueries. Elements are reused. */
	TArray<FTrophyPackTrophyQuery> TrophyPackTrophyQueries;
	/** Queried achievement descriptions, populated on the online thread then copied to the achievements manager on the game thread */
	FOnlineAchievementDescMapPS4 AchievementDescriptions;
	/** Delegate to trigger when the query is completed */
	FOnQueryAchievementsCompleteDelegate CompletionDelegate;
};

class FOnlineAsyncTaskPS4UnlockTrophies : public FOnlineAsyncTaskPS4TrophyBase
{
public:
	/** Constructor */
	FOnlineAsyncTaskPS4UnlockTrophies(FOnlineSubsystemPS4* const InSubsystem, const FUniqueNetIdPS4& InLocalUserId, TArray<SceNpTrophyId>&& InTrophyIds, const FOnAchievementsWrittenDelegate& InCompletionDelegate)
		: FOnlineAsyncTaskPS4TrophyBase(InSubsystem, InLocalUserId)
		, TrophyIds(MoveTemp(InTrophyIds))
		, CompletionDelegate(InCompletionDelegate)
	{
		check(TrophyIds.Num() > 0);
		if (UE_LOG_ACTIVE(LogOnlineAchievements, Verbose))
		{
			UE_LOG_ONLINE_ACHIEVEMENTS(Verbose, TEXT("FOnlineAsyncTaskPS4UnlockTrophies: Unlocking %d trophies"), TrophyIds.Num());
			int32 TrophyIdCounter = 0;
			for (const SceNpTrophyId TrophyId : TrophyIds)
			{
				UE_LOG_ONLINE_ACHIEVEMENTS(Verbose, TEXT("  %d=%d"), TrophyIdCounter, static_cast<int32>(TrophyId));
				++TrophyIdCounter;
			}
		}
	}

	//~ Begin FOnlineAsyncTaskPS4TrophyBase interface
	virtual void StartTrophyRequest() override
	{
		UnlockNextTrophy();
	}

	virtual void TickTrophyRequest() override
	{
		if (!Response.isLocked())
		{
			const SceNpTrophyId TrophyId = TrophyIds[UnlockingTrophyIndex];
			const int32 ReturnCode = Response.getReturnCode();
			if (ReturnCode != SCE_TOOLKIT_NP_V2_SUCCESS)
			{
				const NpToolkit::Core::ServerError* const ServerError = Response.getServerError();
				if (ServerError)
				{
					PrintNPToolkitServerError(TEXT("FOnlineAsyncTaskPS4UnlockTrophies"), *ServerError);
				}
				Result = OnlineAchievementsPS4::Errors::ResultsError(ReturnCode);
				FailedToUnlockTrophyIds.Add(TrophyId);
			}
			UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("FOnlineAsyncTaskPS4UnlockTrophies: User=[%s] unlock trophy=%d completed with returncode=0x%08x"), *LocalUserId->ToDebugString(), static_cast<int32>(TrophyId), ReturnCode);

			++UnlockingTrophyIndex;
			UnlockNextTrophy();
		}
	}

	virtual FString ToString() const override
	{
		FString TrophyIdsString;
		for (const SceNpTrophyId TrophyId : TrophyIds)
		{
			if (!TrophyIdsString.IsEmpty())
			{
				TrophyIdsString += TEXT(", ");
			}
			TrophyIdsString += FString::FromInt(static_cast<int32>(TrophyId));
		}
		FString FailedToUnlockTrophyIdsString;
		for (const SceNpTrophyId TrophyId : FailedToUnlockTrophyIds)
		{
			if (!FailedToUnlockTrophyIdsString.IsEmpty())
			{
				FailedToUnlockTrophyIdsString += TEXT(", ");
			}
			FailedToUnlockTrophyIdsString += FString::FromInt(static_cast<int32>(TrophyId));
		}
		if (FailedToUnlockTrophyIdsString.IsEmpty())
		{
			FailedToUnlockTrophyIdsString = TEXT("<none>");
		}
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4UnlockTrophies bWasSuccessful=%d Result=[%s] TrophyIds=[%s] FailedTrophyIds=[%s]"), WasSuccessful(), *Result.ToLogString(), *TrophyIdsString, *FailedToUnlockTrophyIdsString);
	}

	virtual void Finalize() override
	{
		FOnlineAsyncTaskPS4::Finalize();

		if (Result.WasSuccessful())
		{
			FOnlineAchievementsPS4Ptr AchievementsPtr = StaticCastSharedPtr<FOnlineAchievementsPS4>(Subsystem->GetAchievementsInterface());
			check(AchievementsPtr.IsValid()); // Must have achievements to create this task
			for (const SceNpTrophyId TrophyId : TrophyIds)
			{
				AchievementsPtr->UpdateAchievementProgress(*LocalUserId, TrophyId, 100.0);
			}
		}
	}

	virtual void TriggerDelegates() override
	{
		FOnlineAsyncTaskPS4TrophyBase::TriggerDelegates();
		CompletionDelegate.ExecuteIfBound(*LocalUserId, Result.WasSuccessful());
	}
	//~ End FOnlineAsyncTaskPS4TrophyBase Interface

protected:
	/**
	 * Unlock the next trophy
	 */
	void UnlockNextTrophy()
	{
		if (TrophyIds.IsValidIndex(UnlockingTrophyIndex))
		{
			const SceNpTrophyId TrophyId = TrophyIds[UnlockingTrophyIndex];

			check(Response.getState() != NpToolkit::Core::ResponseState::locked);
			Response.reset();

			UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("Starting unlock request for trophyid=%d"), static_cast<int32>(TrophyId));

			NpToolkit::Trophy::Request::Unlock Request;
			Request.userId = LocalUserId->GetUserId();
			Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
			Request.async = true;
			Request.trophyId = TrophyId;

			const int32 RequestId = NpToolkit::Trophy::unlock(Request, &Response);
			if (RequestId < SCE_TOOLKIT_NP_V2_SUCCESS)
			{
				// We will continue to try the other trophies (because Response will not be locked in the next tick)
				// If any requests fail, then we report the entire task failed
				UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("Failed to start unlock request for trophyid=%d with result=0x%08x"), static_cast<int32>(TrophyId), RequestId);
				Result = OnlineAchievementsPS4::Errors::RequestFailure(RequestId);
				FailedToUnlockTrophyIds.Add(TrophyId);
			}
		}
		else
		{
			Result.bSucceeded = Result.ErrorCode.IsEmpty();
			bWasSuccessful = Result.WasSuccessful();
			bIsComplete = true;
		}
	}

protected:
	/** Response that will be filled out asynchronously. This is re-used between all of the requests. */
	NpToolkit::Core::Response<NpToolkit::Core::Empty> Response;
	/** The trophy ids to unlock */
	TArray<SceNpTrophyId> TrophyIds;
	/** The current index into TrophyIds we are currently unlocking */
	int32 UnlockingTrophyIndex = 0;
	/** Trophy ids we failed to unlock */
	TArray<SceNpTrophyId, TInlineAllocator<8>> FailedToUnlockTrophyIds;
	/** Delegate to trigger when the unlock is completed */
	FOnAchievementsWrittenDelegate CompletionDelegate;
};

FOnlineAchievementsPS4::FOnlineAchievementsPS4(FOnlineSubsystemPS4* InSubsystem)
	: PS4Subsystem(InSubsystem)
{
	// Load trophy module
	const int32 Result = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TROPHY);
	bTrophyModuleLoaded = (Result == SCE_OK);
	if (!bTrophyModuleLoaded)
	{
		UE_LOG_ONLINE_ACHIEVEMENTS(Error, TEXT("FOnlineSubsystemPS4::FOnlineAchievementsPS4: Load module failed with result=0x%08x"), Result);
	}
}

FOnlineAchievementsPS4::~FOnlineAchievementsPS4()
{
	Shutdown();
}

void FOnlineAchievementsPS4::Shutdown()
{
	if (bTrophyModuleLoaded)
	{
		// Shutdown trophy module
		const int32 Result = sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TROPHY);
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE_ACHIEVEMENTS(Error, TEXT("FOnlineSubsystemPS4::Shutdown: Unload module failed with result=0x%08x"), Result);
		}
		bTrophyModuleLoaded = false;
	}
}

void FOnlineAchievementsPS4::LoadAchievementsFromJsonConfig()
{
	if (bJsonConfigLoadAttempted)
	{
		return;
	}
	bJsonConfigLoadAttempted = true;

	const TCHAR* const JsonConfigName = TEXT("Achievements.json");
	const FString BaseDir = FPaths::ProjectDir() + TEXT("Config/OSS/PS4/");
	const FString JsonConfigFilename = BaseDir + JsonConfigName;

	FString JsonText;

	if (FFileHelper::LoadFileToString(JsonText, *JsonConfigFilename))
	{
		if (!AchievementsConfig.FromJson(JsonText))
		{
			UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("FOnlineAchievementsPS4: Failed to parse json filename=[%s] jsontext=[%s]"), *JsonConfigFilename, *JsonText);
		}
	}
	else
	{
		UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("FOnlineAchievementsPS4: Failed to load config filename=[%s]"), *JsonConfigFilename);
	}
}

const SceNpTrophyId* FOnlineAchievementsPS4::LookupTrophyId(const FString& AchievementId)
{
	LoadAchievementsFromJsonConfig();
	return AchievementsConfig.AchievementMap.Find(AchievementId);
}

void FOnlineAchievementsPS4::QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate)
{
	LoadAchievementsFromJsonConfig(); // Ensure we have loaded from config for PopulateBlankAchievements
	FOnlineAsyncTaskPS4QueryUnlockedTrophies* NewTask = new FOnlineAsyncTaskPS4QueryUnlockedTrophies(PS4Subsystem, FUniqueNetIdPS4::Cast(PlayerId), Delegate);
	PS4Subsystem->GetAsyncTaskManager()->AddToParallelTasks(NewTask);
}

void FOnlineAchievementsPS4::QueryAchievementDescriptions(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate)
{
	FOnlineAsyncTaskPS4QueryTrophyDescriptions* NewTask = new FOnlineAsyncTaskPS4QueryTrophyDescriptions(PS4Subsystem, FUniqueNetIdPS4::Cast(PlayerId), Delegate);
	PS4Subsystem->GetAsyncTaskManager()->AddToParallelTasks(NewTask);
}

void FOnlineAchievementsPS4::WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate)
{
	const FUniqueNetIdPS4& PS4User = FUniqueNetIdPS4::Cast(PlayerId);

	TArray<SceNpTrophyId> UnlockTrophyIds;
	for (const TPair<FName, FVariantData> It : WriteObject->Properties)
	{
		float Percent = 0.0f;
		It.Value.GetValue(Percent);

		if (Percent < 100.0f)
		{
			continue;
		}

		// Convert the achievement name to the trophy index
		const FString AchievementId = It.Key.ToString();
		const SceNpTrophyId* const TrophyId = LookupTrophyId(AchievementId);

		if (TrophyId == nullptr)
		{
			UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("FOnlineAchievementsPS4::WriteAchievements: No mapping for achievement=[%s]"), *AchievementId);
			continue;
		}

		// Is it already unlocked?
		FOnlineAchievement ExistingAchievement;
		if (GetCachedAchievement(PlayerId, AchievementId, ExistingAchievement) == EOnlineCachedResult::Success)
		{
			const bool bAlreadyUnlocked = ExistingAchievement.Progress >= 100.0;
			if (bAlreadyUnlocked)
			{
				UE_LOG_ONLINE_ACHIEVEMENTS(VeryVerbose, TEXT("FOnlineAchievementsPS4::WriteAchievements: User=[%s] already has achievement=[%s] unlocked trophyid=%d, not unlocking again"), *PS4User.ToDebugString(), *AchievementId, static_cast<int32>(*TrophyId));
				continue;
			}
		}

		UnlockTrophyIds.Emplace(*TrophyId);
	}

	if (UnlockTrophyIds.Num() > 0)
	{
		// Create a task to unlock the trophies
		FOnlineAsyncTaskPS4UnlockTrophies* NewTask = new FOnlineAsyncTaskPS4UnlockTrophies(PS4Subsystem, PS4User, MoveTemp(UnlockTrophyIds), Delegate);
		PS4Subsystem->GetAsyncTaskManager()->AddToParallelTasks(NewTask);
	}
	else
	{
		// Nothing to unlock, so we are done (successfully)
		Delegate.ExecuteIfBound(PlayerId, true);
	}
}

EOnlineCachedResult::Type FOnlineAchievementsPS4::GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement)
{
	EOnlineCachedResult::Type Result = EOnlineCachedResult::NotFound;

	const FUniqueNetIdPS4& PS4User = FUniqueNetIdPS4::Cast(PlayerId);
	const FOnlineUserAchievementsMapPS4* const Achievements = UserAchievements.Find(PS4User.GetUserId());

	if (Achievements)
	{
		// Look up platform ID from achievement mapping
		const SceNpTrophyId* const TrophyId = LookupTrophyId(AchievementId);
		if (TrophyId)
		{
			const FOnlineAchievement* const Achievement = Achievements->Find(*TrophyId);
			if (Achievement)
			{
				OutAchievement = *Achievement;
				Result = EOnlineCachedResult::Success;
			}
			else
			{
				UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("FOnlineAchievementsPS4::GetCachedAchievement: No trophy entry found for achievement=[%s]"), *AchievementId);
			}
		}
		else
		{
			UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("FOnlineAchievementsPS4::GetCachedAchievement: No mapping for achievement=[%s]"), *AchievementId);
		}
	}
	else
	{
		UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("FOnlineAchievementsPS4::GetCachedAchievement: Achievements have not been read for player=[%s], call QueryAchievements first"), *PlayerId.ToDebugString());
	}
	return Result;
}

EOnlineCachedResult::Type FOnlineAchievementsPS4::GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement>& OutAchievements)
{
	EOnlineCachedResult::Type Result = EOnlineCachedResult::NotFound;

	const FUniqueNetIdPS4& PS4User = FUniqueNetIdPS4::Cast(PlayerId);
	const FOnlineUserAchievementsMapPS4* const Achievements = UserAchievements.Find(PS4User.GetUserId());

	if (Achievements)
	{
		OutAchievements.Empty(Achievements->Num());
		Achievements->GenerateValueArray(OutAchievements);
		Result = EOnlineCachedResult::Success;
	}
	else
	{
		UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("FOnlineAchievementsPS4::GetCachedAchievement: Achievements have not been read for player=[%s], call QueryAchievements first"), *PlayerId.ToDebugString());
	}
	return Result;
}

EOnlineCachedResult::Type FOnlineAchievementsPS4::GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc)
{
	EOnlineCachedResult::Type Result = EOnlineCachedResult::NotFound;

	// Look up platform ID from achievement mapping
	const SceNpTrophyId* const TrophyId = LookupTrophyId(AchievementId);
	if (TrophyId)
	{
		const FOnlineAchievementDesc* const AchievementDesc = AchievementDescriptions.Find(*TrophyId);

		if (AchievementDesc)
		{
			OutAchievementDesc = *AchievementDesc;
			Result = EOnlineCachedResult::Success;
		}
		else
		{
			UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("FOnlineAchievementsPS4::GetCachedAchievementDescription: Achievements have not been read for id=[%s] trophyid=%d"), *AchievementId, static_cast<int32>(*TrophyId));
		}
	}
	else
	{
		UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("FOnlineAchievementsPS4::GetCachedAchievementDescription: No mapping for achievement=[%s]"), *AchievementId);
	}
	return Result;
}

#if !UE_BUILD_SHIPPING
bool FOnlineAchievementsPS4::ResetAchievements(const FUniqueNetId& PlayerId)
{
	UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("ResetAchievements is not implemented for PS4"));
	return false;
};
#endif // !UE_BUILD_SHIPPING

void FOnlineAchievementsPS4::Dump() const
{
#if !UE_BUILD_SHIPPING
	UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("FOnlineAchievementsPS4: Begin"));
	UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("  bTrophyModuleLoaded=%d"), static_cast<int32>(bTrophyModuleLoaded));

	// RegistrationStatus should only be modified from the async task thread. This Dump command must be called explicitly by the user, so trust that it is not during registration!
	UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("  RegistrationStatus: Begin"));
	for (const TPair<SceUserServiceUserId, ETrophyServiceRegistrationStatus>& Status : RegistrationStatus)
	{
		UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("    %d=[%s]"), static_cast<int32>(Status.Key), LexToCString(Status.Value));
	}
	UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("  RegistrationStatus: End"));

	const_cast<FOnlineAchievementsPS4*>(this)->LoadAchievementsFromJsonConfig();
	UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("  AchievementsConfig (JSON config file): Begin"));
	for (const TPair<FString, int32>& ConfigAchievement : AchievementsConfig.AchievementMap)
	{
		UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("    [%s]=%d"), *ConfigAchievement.Key, ConfigAchievement.Value);
	}
	UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("  AchievementsConfig: End"));

	UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("  UserAchievements: Begin"));
	for (const TPair<SceUserServiceUserId, FOnlineUserAchievementsMapPS4>& User : UserAchievements)
	{
		UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("    User=%d: Begin"), static_cast<int32>(User.Key));
		for (const TPair<SceNpTrophyId, FOnlineAchievement>& Achievement : User.Value)
		{
			UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("      TrophyId=%d"), static_cast<int32>(Achievement.Key));
			UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("      Value=[%s]"), *Achievement.Value.ToDebugString());
		}
		UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("    User=%d: End"), static_cast<int32>(User.Key));
	}
	UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("  UserAchievements: End"));

	UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("  AchievementDescriptions: Begin"));
	for (const TPair<SceNpTrophyId, FOnlineAchievementDesc>& Description : AchievementDescriptions)
	{
		UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("    TrophyId=%d"), static_cast<int32>(Description.Key));
		UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("    Value=[%s]"), *Description.Value.ToDebugString());
	}
	UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("  AchievementDescriptions: End"));

	UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("FOnlineAchievementsPS4: End"));
#endif // !UE_BUILD_SHIPPING
}

ETrophyServiceRegistrationStatus FOnlineAchievementsPS4::GetUserTrophyRegistrationStatus(const FUniqueNetIdPS4& UserId) const
{
	// If this needs to be run from the game thread then we need to add a critical section
	check(PS4Subsystem->GetAsyncTaskManager()->IsInOnlineThread());

	ETrophyServiceRegistrationStatus Result = ETrophyServiceRegistrationStatus::NotRegistered;

	const ETrophyServiceRegistrationStatus* const FoundStatus = RegistrationStatus.Find(UserId.GetUserId());
	if (FoundStatus)
	{
		Result = *FoundStatus;
	}
	return Result;
}

void FOnlineAchievementsPS4::SetUserTrophyRegistrationStatus(const FUniqueNetIdPS4& UserId, const ETrophyServiceRegistrationStatus Status)
{
	// If this needs to be run from the game thread then we need to add a critical section
	check(PS4Subsystem->GetAsyncTaskManager()->IsInOnlineThread());

	const ETrophyServiceRegistrationStatus PreviousStatus = GetUserTrophyRegistrationStatus(UserId);
	if (PreviousStatus != Status)
	{
		UE_LOG_ONLINE_ACHIEVEMENTS(Log, TEXT("Setting trophy registration status for user=[%s] from [%s] to [%s]"), *UserId.ToDebugString(), LexToCString(PreviousStatus), LexToCString(Status));
		RegistrationStatus.Emplace(UserId.GetUserId(), Status);
	}
}

void FOnlineAchievementsPS4::SetUserAchievements(const FUniqueNetIdPS4& UserId, FOnlineUserAchievementsMapPS4&& Achievements)
{
	check(IsInGameThread());
	UserAchievements.Emplace(UserId.GetUserId(), MoveTemp(Achievements));
}

void FOnlineAchievementsPS4::SetAchievementDescriptions(FOnlineAchievementDescMapPS4&& NewAchievementDescriptions)
{
	check(IsInGameThread());
	AchievementDescriptions = MoveTemp(NewAchievementDescriptions);
}

void FOnlineAchievementsPS4::UpdateAchievementProgress(const FUniqueNetIdPS4& UserId, const SceNpTrophyId TrophyId, const double NewProgress)
{
	check(IsInGameThread());
	FOnlineUserAchievementsMapPS4* const FoundUserAchievements = UserAchievements.Find(UserId.GetUserId());
	if (FoundUserAchievements)
	{
		FOnlineAchievement* const Achievement = FoundUserAchievements->Find(TrophyId);
		if (Achievement)
		{
			UE_LOG_ONLINE_ACHIEVEMENTS(VeryVerbose, TEXT("UpdateAchievementProgress: user=[%s] achievementid=%d updated to new progress=%f"), *UserId.ToDebugString(), static_cast<int32>(TrophyId), NewProgress);
			Achievement->Progress = NewProgress;
		}
		else
		{
			UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("UpdateAchievementProgress: user=[%s] achievementid=%d not found"), *UserId.ToDebugString(), static_cast<int32>(TrophyId));
		}
	}
	else
	{
		UE_LOG_ONLINE_ACHIEVEMENTS(Warning, TEXT("UpdateAchievementProgress: user=[%s] not found"), *UserId.ToDebugString());
	}
}

void FOnlineAchievementsPS4::PopulateBlankAchievements(FOnlineUserAchievementsMapPS4& Achievements) const
{
	Achievements.Empty(AchievementsConfig.AchievementMap.Num());
	for (const TPair<FString, int32>& ConfigAchievement : AchievementsConfig.AchievementMap)
	{
		const SceNpTrophyId TrophyId = static_cast<SceNpTrophyId>(ConfigAchievement.Value);
		FOnlineAchievement Achievement;
		Achievement.Id = FString::FromInt(ConfigAchievement.Value);
		Achievement.Progress = 0.0;
		Achievements.Emplace(TrophyId, MoveTemp(Achievement));
	}
}

#undef LOCTEXT_NAMESPACE
#undef ONLINE_ERROR_NAMESPACE 