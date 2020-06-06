// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4Package.h"
#include "OnlineJsonSerializer.h"

class FOnlineSubsystemPS4;

#define ACHIEVEMENTS_FAILURE_NOT_REGISTERED TEXT("errors.com.epicgames.achievements.not_registered")

#include "OnlineError.h"
#define LOCTEXT_NAMESPACE "OnlineSubsystemPS4"
#define ONLINE_ERROR_NAMESPACE "errors.com.epicgames.oss.achievements"

namespace OnlineAchievementsPS4
{
#include "OnlineErrorMacros.inl"

	namespace Errors
	{
		inline FOnlineError AchievementNotRegistered() { return ONLINE_ERROR(EOnlineErrorResult::Canceled, TEXT("errors.com.epicgames.achievements.not_registered")); }
		inline FOnlineError RequestFailure(int32 InCode) { return ONLINE_ERROR(EOnlineErrorResult::RequestFailure, FString::Printf(TEXT("0x%08X"), InCode)); }
		inline FOnlineError ResultsError(int32 InCode) { return ONLINE_ERROR(EOnlineErrorResult::InvalidResults, FString::Printf(TEXT("0x%08X"), InCode)); }
	}
}


#undef LOCTEXT_NAMESPACE
#undef ONLINE_ERROR_NAMESPACE

class FAchievementsConfig : public FOnlineJsonSerializable
{
public:
	FJsonSerializableKeyValueMapInt AchievementMap;

	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE_MAP("AchievementMap", AchievementMap);
	END_ONLINE_JSON_SERIALIZER
};

/**
 * Represents the user's registration status with the trophy service
 * A user must be registered with the trophy service in order to be able to interact with the trophy service
 */
enum class ETrophyServiceRegistrationStatus : uint8
{
	/** User is not yet registered */
	NotRegistered,
	/** Registration is in progress */
	Registering,
	/** User is registered */
	Registered,
	/** Failed to register the user - don't retry */
	Failed
};

typedef TMap<SceNpTrophyId, FOnlineAchievement> FOnlineUserAchievementsMapPS4;
typedef TMap<SceNpTrophyId, FOnlineAchievementDesc> FOnlineAchievementDescMapPS4;

/**
 * FOnlineAchievementsPS4 - Interface class for trophies
 */
class FOnlineAchievementsPS4 : public IOnlineAchievements
{
public:
	/**
	 * Constructor
	 *
	 * @param InSubsystem - A reference to the owning subsystem
	 */
	FOnlineAchievementsPS4(FOnlineSubsystemPS4* const InSubsystem);

	/** Destructor */
	virtual ~FOnlineAchievementsPS4();

	//~ Begin IOnlineAchievements Interface
	virtual void WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate = FOnAchievementsWrittenDelegate()) override;
	virtual void QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate()) override;
	virtual void QueryAchievementDescriptions( const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate() ) override;
	virtual EOnlineCachedResult::Type GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement) override;
	virtual EOnlineCachedResult::Type GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement> & OutAchievements) override;
	virtual EOnlineCachedResult::Type GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc) override;
#if !UE_BUILD_SHIPPING
	virtual bool ResetAchievements(const FUniqueNetId& PlayerId) override;
#endif // !UE_BUILD_SHIPPING
	//~ End IOnlineAchievements Interface

	/**
	 * Dump the state of the achievement system to log
	 * Only available in non-shipping builds
	 */
	void Dump() const;

	/**
	 * Get a user's trophy registration status
	 * Only called from the async task thread
	 *
	 * @param UserId the user id to get the registration status for
	 * @return the registration status
	 */
	ETrophyServiceRegistrationStatus GetUserTrophyRegistrationStatus(const FUniqueNetIdPS4& UserId) const;

	/**
	 * Set a user's trophy registration status
	 * Only called from the async task thread
	 *
	 * @param UserId the user id to update
	 * @param Status the new status
	 */
	void SetUserTrophyRegistrationStatus(const FUniqueNetIdPS4& UserId, const ETrophyServiceRegistrationStatus Status);

	/**
	 * Set a user's achievements - intended to be called from an online async task on the game thread
	 *
	 * @param UserId the user id to update
	 * @param UnlockedAchievements the unlocked achievements for the user
	 */
	void SetUserAchievements(const FUniqueNetIdPS4& UserId, FOnlineUserAchievementsMapPS4&& Achievements);

	/**
	 * Set the achievement descriptions - intended to be called from an online async task on the game thread
	 *
	 * @param NewAchievementDescriptions the achievement descriptions to update to
	 */
	void SetAchievementDescriptions(FOnlineAchievementDescMapPS4&& NewAchievementDescriptions);

	/**
	 * Update the progress for an achievement - intended to be called from an online async task on the game thread
	 *
	 * @param UserId the user id to update
	 * @param AchievementId the achievement id to update
	 * @param NewProgress the new progress for the achievement
	 */
	void UpdateAchievementProgress(const FUniqueNetIdPS4& UserId, const SceNpTrophyId TrophyId, const double NewProgress);

	/**
	 * @param Achievements a map of achievements to fill with blank progress from the JSON 
	 */
	void PopulateBlankAchievements(FOnlineUserAchievementsMapPS4& Achievements) const;

private:
	/** Default constructor disabled */
	FOnlineAchievementsPS4() = delete;

	/** Shutdown and cleanup resources */
	void Shutdown();

	/**
	 * Initializes the config object
	 *
	 * @param JsonConfigName - Name of config file to load
	 */
	void LoadAchievementsFromJsonConfig();

	/**
	 * Lookup a trophy id from our achievements map
	 *
	 * @param AchievementId the achievement to find the trophy for
	 * @return the trophy id, or nullptr if not found
	 */
	const SceNpTrophyId* LookupTrophyId(const FString& AchievementId);



private:
	/** Reference to the PS4 online subsystem */
	FOnlineSubsystemPS4* const PS4Subsystem;

	/** Is the trophy module loaded? */
	bool bTrophyModuleLoaded = false;

	/** Have we attempted loading the JSON config? */
	bool bJsonConfigLoadAttempted = false;

	/**
	 * The trophy registration status for a user
	 * This is expected to only be accessed on the async task thread
	 */
	TMap<SceUserServiceUserId, ETrophyServiceRegistrationStatus> RegistrationStatus;

	/** Config settings initialized from json file */
	FAchievementsConfig AchievementsConfig;

	/** Mapping of players to their achievements */
	TMap<SceUserServiceUserId, FOnlineUserAchievementsMapPS4> UserAchievements;

	/** Cached achievement descriptions for an Id */
	TMap<SceNpTrophyId, FOnlineAchievementDesc> AchievementDescriptions;
};

typedef TSharedPtr<FOnlineAchievementsPS4, ESPMode::ThreadSafe> FOnlineAchievementsPS4Ptr;
