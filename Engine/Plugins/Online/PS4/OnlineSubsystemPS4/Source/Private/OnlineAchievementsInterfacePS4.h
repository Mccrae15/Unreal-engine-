// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlineAchievementsInterface.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4Package.h"
#include "OnlineJsonSerializer.h"

class FAchievementsConfig : public FOnlineJsonSerializable
{
public:
	FJsonSerializableKeyValueMapInt	AchievementMap;

	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE_MAP( "AchievementMap", AchievementMap );
	END_ONLINE_JSON_SERIALIZER
};

/**
 *	FOnlineAchievementsPS4 - Interface class for trophies
 */
class FOnlineAchievementsPS4 : public IOnlineAchievements
{
private:

	/** Reference to the main GameCenter subsystem */
	class FOnlineSubsystemPS4 *									PS4Subsystem;

	/** Maps SceUserServiceUserId to FTrophyContextInfo */
	TMap< SceUserServiceUserId, SceNpTrophyContext >			ContextMap;

	/** Config settings initialized from json file */
	FAchievementsConfig											AchievementsConfig;

	/** Mapping of players to their achievements */
	TMap< SceUserServiceUserId, TArray< FOnlineAchievement > >	PlayerAchievements;

	/** Cached achievement descriptions for an Id */
	TMap< FString, FOnlineAchievementDesc >						AchievementDescriptions;

	/** hide the default constructor, we need a reference to our OSS */
	FOnlineAchievementsPS4() {};

	/** Shutdown and cleanup resources */
	void Shutdown();

	/**
	 * Initializes the config object
	 *
	 * @param JsonConfigName - Name of config file to load
	 */
	bool LoadAndInitFromJsonConfig( const TCHAR* JsonConfigName );

	/**
	 * Listens to user sign-in events
	 *
	 * @param bLoggingIn - True if the user is signing in, false otherwise
	 * @param UserID - Service user id
	 * @param UserIndex - User index slot
	 */
	void UserLoginEventHandler( bool bLoggingIn, int32 UserID, int32 UserIndex );

	/**
	 *	Async task that creates and registers a trophy context for a user
	 */
	class FAsyncTrophyTask : public FOnlineAsyncTask
	{
	public:
		enum EContextRegisterState
		{
			Idle,
			Pending,
			Complete,
			Failed
		};

		FOnlineSubsystemPS4 *		SubsystemPS4;			// PS4 main subsystem
		FOnlineAchievementsPS4 *	AchievementsPS4;		// Pointer to the PS4 achievements OSS
		SceUserServiceUserId		ServiceUserId;			// This is the user we are unlocking an achievement for
		EContextRegisterState		State;					// Current state

		/**
		 * Constructor.
		 */
		FAsyncTrophyTask( 
			FOnlineSubsystemPS4 *		InSubsystemPS4, 
			FOnlineAchievementsPS4 *	InAchievementsPS4, 
			const SceUserServiceUserId	InServiceUserId ) :
			SubsystemPS4( InSubsystemPS4 ),
			AchievementsPS4( InAchievementsPS4 ),
			ServiceUserId( InServiceUserId ),
			State( Idle )
		{
		}

		virtual bool	IsDone() override;
		virtual bool	WasSuccessful() override;
		virtual FString	ToString() const override;

		bool			EnsureTrophyContext();
		bool			RegisterTrophyContext();
	};

	/**
	 *	Async task that unlocks a trophy for a user
	 */
	class FAsyncUnlockTrophyTask : public FAsyncTrophyTask
	{
	public:

		SceNpTrophyId				TrophyId;				// This is the trophy index we are unlocking
		FOnAchievementsWrittenDelegate WriteCallbackDelegate;

		/**
		 * Constructor.
		 */
		FAsyncUnlockTrophyTask( 
			FOnlineSubsystemPS4 *		InSubsystemPS4, 
			FOnlineAchievementsPS4 *	InAchievementsPS4, 
			const SceUserServiceUserId	InServiceUserId,
			const SceNpTrophyId			InTrophyId,
			const FOnAchievementsWrittenDelegate& InWriteCallbackDelegate) :
			FAsyncTrophyTask( InSubsystemPS4, InAchievementsPS4, InServiceUserId ),
			TrophyId( InTrophyId ),
			WriteCallbackDelegate( InWriteCallbackDelegate )
		{
		}

		virtual void Tick() override;
		virtual void TriggerDelegates() override;
	};

	/**
	 *	Async task that queries trophies for a user
	 */
	class FAsyncQueryTrophiesTask : public FAsyncTrophyTask
	{
	public:
		SceNpTrophyFlagArray					TrophyFlagArray;
		uint32									TrophyCount;
		TArray< SceNpTrophyDetails >			TrophyDetails;

		TSharedRef<FUniqueNetIdPS4 const> const PlayerId;
		FOnQueryAchievementsCompleteDelegate	Delegate;

		/**
		 * Constructor.
		 */
		FAsyncQueryTrophiesTask( 
			FOnlineSubsystemPS4 *		InSubsystemPS4, 
			FOnlineAchievementsPS4 *	InAchievementsPS4, 
			FUniqueNetIdPS4 const&		InPlayerId,
			const FOnQueryAchievementsCompleteDelegate& InDelegate ) :
			FAsyncTrophyTask( InSubsystemPS4, InAchievementsPS4, InPlayerId.GetUserId() ),
			PlayerId( InPlayerId.AsShared() ),
			Delegate( InDelegate )
		{
		}

		virtual void	Tick() override;
		virtual void	TriggerDelegates() override;
	};

	class FAsyncUserSignoutTask : public FAsyncTrophyTask
	{
	public:
		/**
		 * Constructor.
		 */
		FAsyncUserSignoutTask( 
			FOnlineSubsystemPS4 *		InSubsystemPS4, 
			FOnlineAchievementsPS4 *	InAchievementsPS4, 
			const SceUserServiceUserId	InServiceUserId ) :
			FAsyncTrophyTask( InSubsystemPS4, InAchievementsPS4, InServiceUserId )
		{
		}

		virtual void Tick() override;
	};

public:

	//~ Begin IOnlineAchievements Interface
	virtual void WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate = FOnAchievementsWrittenDelegate()) override;
	virtual void QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate()) override;
	virtual void QueryAchievementDescriptions( const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate() ) override;
	virtual EOnlineCachedResult::Type GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement) override;
	virtual EOnlineCachedResult::Type GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement> & OutAchievements) override;
	virtual EOnlineCachedResult::Type GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc) override;
#if !UE_BUILD_SHIPPING
	virtual bool ResetAchievements( const FUniqueNetId& PlayerId ) override;
#endif // !UE_BUILD_SHIPPING
	//~ End IOnlineAchievements Interface


	/**
	 * Constructor
	 *
	 * @param InSubsystem - A reference to the owning subsystem
	 */
	FOnlineAchievementsPS4( class FOnlineSubsystemPS4* InSubsystem );

	
	/**
	 * Default destructor
	 */
	virtual ~FOnlineAchievementsPS4();
};

typedef TSharedPtr<FOnlineAchievementsPS4, ESPMode::ThreadSafe> FOnlineAchievementsPS4Ptr;
