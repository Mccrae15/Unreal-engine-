// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineTournamentInterface.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineAsyncTaskManager.h"
#include "WebApiPS4Task.h"

class FOnlineSubsystemPS4;
class FUniqueNetIdPS4;
struct FPS4NpQueryTournamentDetailsTournamentDetail;
struct FPS4NpEntity;
struct FPS4NpQueryMatchDetailsMatchDetail;
struct FPS4NpRegisteredUser;
struct FPS4NpRegisteredRoster;

using FOnlineTournamentPlayerIdPS4 = FUniqueNetIdPS4;

template <typename KeyType, typename ValueType>
struct TSharedRefMapKeyFuncs : public TDefaultMapKeyFuncs<TSharedRef<KeyType>, ValueType, false>
{
	static FORCEINLINE TSharedRef<KeyType>	GetSetKey(TPair<TSharedRef<KeyType>, ValueType> const& Element) { return Element.Key; }
	static FORCEINLINE uint32				GetKeyHash(TSharedRef<KeyType> const& Key) {	return GetTypeHash(*Key); }
	static FORCEINLINE bool					Matches(TSharedRef<KeyType> const& A, TSharedRef<KeyType> const& B) { return (A == B) || (*A == *B); }
};

template <typename KeyType, typename ValueType>
using TSharedRefMap = TMap<TSharedRef<KeyType>, ValueType, FDefaultSetAllocator, TSharedRefMapKeyFuncs<KeyType, ValueType>>;

class FOnlineTournamentIdPS4
	: public FUniqueNetIdString
{
public:
	virtual ~FOnlineTournamentIdPS4() = default;

	//~ Begin FUniqueNetId Interface
	virtual FName GetType() const override final;
	//~ End FUniqueNetId Interface

	friend uint32 GetTypeHash(const FOnlineTournamentIdPS4& TournamentId);

	friend bool operator==(const FOnlineTournamentIdPS4& A, const FOnlineTournamentIdPS4& B);
	friend bool operator!=(const FOnlineTournamentIdPS4& A, const FOnlineTournamentIdPS4& B);

PACKAGE_SCOPE:
	FOnlineTournamentIdPS4(FString&& InTournamentId);
	FOnlineTournamentIdPS4(const FString& InTournamentId);

	static FName GetType_Internal();
};

class FOnlineTournamentMatchIdPS4
	: public FUniqueNetIdString
{
public:
	virtual ~FOnlineTournamentMatchIdPS4() = default;

	//~ Begin FUniqueNetId Interface
	virtual FName GetType() const override;
	//~ End FUniqueNetId Interface

	friend uint32 GetTypeHash(const FOnlineTournamentMatchIdPS4& EventId);

	friend bool operator==(const FOnlineTournamentMatchIdPS4& A, const FOnlineTournamentMatchIdPS4& B);
	friend bool operator!=(const FOnlineTournamentMatchIdPS4& A, const FOnlineTournamentMatchIdPS4& B);

PACKAGE_SCOPE:
	FOnlineTournamentMatchIdPS4(FString&& InMatchId);
	FOnlineTournamentMatchIdPS4(const FString& InMatchId);

	static FName GetType_Internal();
};

class FOnlineTournamentTeamIdPS4
	: public FOnlineTournamentTeamId
{
public:
	virtual ~FOnlineTournamentTeamIdPS4() = default;

	//~ Begin FUniqueNetId Interface
	virtual FName GetType() const override final;
	virtual const uint8* GetBytes() const override final;
	virtual int32 GetSize() const override final;
	virtual bool IsValid() const override final;
	virtual FString ToString() const override final;
	virtual FString ToDebugString() const override final;
	//~ End FUniqueNetId Interface

	friend uint32 GetTypeHash(const FOnlineTournamentTeamIdPS4& OtherTeamId);

	friend bool operator==(const FOnlineTournamentTeamIdPS4& A, const FOnlineTournamentTeamIdPS4& B);
	friend bool operator!=(const FOnlineTournamentTeamIdPS4& A, const FOnlineTournamentTeamIdPS4& B);

PACKAGE_SCOPE:
	FOnlineTournamentTeamIdPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, const FString& InFullTeamId);
	FOnlineTournamentTeamIdPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, FString&& InFullTeamId);
	FOnlineTournamentTeamIdPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, const FString& InPlatform, const FString& InTeamId);
	FOnlineTournamentTeamIdPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, FString&& InPlatform, FString&& InTeamId);

	TSharedRef<const FOnlineTournamentId> GetTournamentId() const;
	const FString& GetPlatform() const;
	const FString& GetTeamId() const;

PACKAGE_SCOPE:
	/** TeamId concatenated with the tournament id and team vendor */
	FString FullTeamId;

	/** What tournament this team belongs to */
	TSharedRef<const FOnlineTournamentId> TournamentId;
	/** What backend vendor this team belongs to */
	FString Platform;
	/** Team Id (requires Platform to have meaning) */
	FString TeamId;
};

class FOnlineTournamentDetailsPS4
	: public IOnlineTournamentDetails
{
public:
	~FOnlineTournamentDetailsPS4() = default;

	//~ Begin IOnlineTournamentDetails Interface
	virtual TSharedRef<const FOnlineTournamentId> GetTournamentId() const override final;
	virtual const FString& GetTitle() const override final;
	virtual const FString& GetDescription() const override final;
	virtual EOnlineTournamentState GetState() const override final;
	virtual EOnlineTournamentFormat GetFormat() const override final;
	virtual EOnlineTournamentParticipantType GetParticipantType() const override final;
	virtual const TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>& GetParticipants() const override final;
	virtual TOptional<FDateTime> GetRegistrationStartDateUTC() const override final;
	virtual TOptional<FDateTime> GetRegistrationEndDateUTC() const override final;
	virtual TOptional<FDateTime> GetStartDateUTC() const override final;
	virtual TOptional<FTimespan> GetCheckInTimespan() const override final;
	virtual TOptional<FDateTime> GetEndDateUTC() const override final;
	virtual TOptional<FDateTime> GetLastUpdatedDateUTC() const override final;
	virtual TOptional<bool> RequiresPremiumSubscription() const override;
	virtual TOptional<FVariantData> GetAttribute(const FName AttributeName) const override final;
	//~ End IOnlineTournamentDetails Interface

PACKAGE_SCOPE:
	FOnlineTournamentDetailsPS4() = delete;
	explicit FOnlineTournamentDetailsPS4(FString&& EventId, FPS4NpQueryTournamentDetailsTournamentDetail&& EventDetails);

PACKAGE_SCOPE:
	TSharedRef<const FOnlineTournamentId> TournamentId;
	FString Title;
	FString Description;
	EOnlineTournamentState State;
	EOnlineTournamentFormat Format;
	EOnlineTournamentParticipantType ParticipantType;
	TArray<TSharedRef<const IOnlineTournamentParticipantDetails>> Participants;
	FDateTime RegistrationStartDate;
	FDateTime RegistrationEndDate;
	FDateTime StartDate;
	FTimespan CheckinTimespan;
	FDateTime EndDate;
	FDateTime LastUpdatedDate;
	bool bRequiresPSPlus;
	TMap<FName, FVariantData> Attributes;
};

struct FOnlineTournamentMatchDetailsPS4
	: public IOnlineTournamentMatchDetails
{
public:
	virtual ~FOnlineTournamentMatchDetailsPS4() = default;

	//~ Begin IOnlineTournamentMatchDetails Interface
	virtual TSharedRef<const FOnlineTournamentMatchId> GetMatchId() const override final;
	virtual EOnlineTournamentParticipantType GetParticipantType() const override final;
	virtual EOnlineTournamentMatchState GetMatchState() const override final;
	virtual TOptional<FString> GetBracket() const override final;
	virtual TOptional<int32> GetRound() const override final;
	virtual TOptional<FDateTime> GetStartDateUTC() const override final;
	virtual TArray<TSharedRef<const IOnlineTournamentParticipantDetails>> GetParticipants() const override final;
	virtual TOptional<FVariantData> GetAttribute(const FName AttributeName) const override final;
	//~ End IOnlineTournamentMatchDetails Interface

PACKAGE_SCOPE:
	FOnlineTournamentMatchDetailsPS4(FString&& MatchId, FPS4NpQueryMatchDetailsMatchDetail&& MatchDetails);

PACKAGE_SCOPE:
	TSharedRef<const FOnlineTournamentMatchId> MatchId;
	EOnlineTournamentParticipantType ParticipantType;
	EOnlineTournamentMatchState MatchState;
	FString Bracket;
	int32 Round;
	FDateTime StartDateUTC;
	TSharedRefMap<const FUniqueNetId, TSharedRef<const IOnlineTournamentParticipantDetails>> Participants;
	TMap<FName, FVariantData> Attributes;
};

struct FOnlineTournamentParticipantDetailsPS4
	: public IOnlineTournamentParticipantDetails
{
public:
	virtual ~FOnlineTournamentParticipantDetailsPS4() = default;

	//~ Begin IOnlineTournamentParticipantDetails Interface
	virtual TSharedRef<const FOnlineTournamentId> GetTournamentId() const /*override*/ final;
	virtual TSharedPtr<const FUniqueNetId> GetPlayerId() const override final;
	virtual TSharedPtr<const FOnlineTournamentTeamId> GetTeamId() const override final;
	virtual const FString& GetDisplayName() const override final;
	virtual EOnlineTournamentParticipantState GetState() const override final;
	virtual TOptional<int32> GetPosition() const override final;
	virtual TOptional<FVariantData> GetScore() const override final;
	virtual TOptional<FVariantData> GetAttribute(const FName AttributeName) const override final;
	//~ End IOnlineTournamentParticipantDetails Interface

PACKAGE_SCOPE:
	FOnlineTournamentParticipantDetailsPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, const EOnlineTournamentParticipantType InParticipantType, const FPS4NpEntity& InRankedEntity, const EOnlineTournamentParticipantState InState);
	FOnlineTournamentParticipantDetailsPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, FPS4NpRegisteredUser&& InUser);
	FOnlineTournamentParticipantDetailsPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, FPS4NpRegisteredRoster&& InRoster);

PACKAGE_SCOPE:
	TSharedRef<const FOnlineTournamentId> TournamentId;
	TSharedPtr<const FUniqueNetId> PlayerId;
	TSharedPtr<const FOnlineTournamentTeamId> TeamId;
	FString DisplayName;
	EOnlineTournamentParticipantState State;
	TOptional<int32> Position;
	TOptional<FVariantData> Score;
	TMap<FName, FVariantData> Attributes;
};

struct FOnlineTournamentTeamDetailsPS4
	: public IOnlineTournamentTeamDetails
{
public:
	virtual ~FOnlineTournamentTeamDetailsPS4() = default;

	/** Get the TeamId of this team */
	virtual TSharedRef<const FOnlineTournamentTeamId> GetTeamId() const override final;
	/** Get the player ids of this team (if they are known) */
	virtual TOptional<TArray<TSharedRef<const FUniqueNetId>>> GetPlayerIds() const override final;
	/** Get the display name of this Team */
	virtual FString GetDisplayName() const override final;
	/** Get an attribute for this team (varies by online platform) */
	virtual TOptional<FVariantData> GetAttribute(const FName AttributeName) const override final;

PACKAGE_SCOPE:
	FOnlineTournamentTeamDetailsPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, FPS4NpEntity&& TeamDetails, TArray<FPS4NpEntity>&& Roster);

PACKAGE_SCOPE:
	TSharedRef<const FOnlineTournamentTeamId> TeamId;
	TArray<TSharedRef<const FUniqueNetId>> Roster;
	FString DisplayName;
	TMap<FName, FVariantData> Attributes;

};

using FOnlineAsyncTaskBasicPS4 = FOnlineAsyncTaskBasic<FOnlineSubsystemPS4>;

class FOnlineAsyncWebTaskPS4
	: public FOnlineAsyncTaskBasicPS4
{
public:
	FOnlineAsyncWebTaskPS4(FOnlineSubsystemPS4& InSubsystem);
	virtual ~FOnlineAsyncWebTaskPS4();

	/**
	 * Create a WebApi Task to be ran in the background
	 *
	 * @return A WebApiTask to be processed
	 */
	virtual TOptional<FWebApiPS4Task> CreateWebTask() = 0;

	/**
	 * Process our result now on the online thread, now that it has completed.
	 */
	virtual void ProcessResult(FWebApiPS4Task& CompletedTask) = 0;

private:
	virtual void Initialize() final;
	virtual void Tick() final;

private:
	TUniquePtr<FAsyncTask<FWebApiPS4Task>> WebAsyncTask;
};


class FOnlineAsyncWebTaskListPS4
	: public FOnlineAsyncTaskBasicPS4
{
public:
	FOnlineAsyncWebTaskListPS4(FOnlineSubsystemPS4& InSubsystem);
	virtual ~FOnlineAsyncWebTaskListPS4();

	virtual void Initialize() final;
	virtual void Tick() final;

	/**
	 * Create an array of WebApi Tasks to be ran in the background
	 *
	 * @return An array of WebApiTasks to be processed
	 */
	virtual TArray<FWebApiPS4Task> CreateWebTasks() = 0;

	/**
	 * Process a single result now that it has completed.  This will be called for each tasks as they are completed.
	 */
	virtual void ProcessResult(FWebApiPS4Task& CompletedTask, const int32 TaskIndex) = 0;

	/**
	 * Process any results that need to be processed on the online thread, now that all tasks have completed.
	 *
	 * This is not called if no tasks are created, or if our aysnc task is cancelled.
	 */
	virtual void OnAllTasksComplete() {};

private:
	TArray<TUniquePtr<FAsyncTask<FWebApiPS4Task>>> WebAsyncTasks;
};
