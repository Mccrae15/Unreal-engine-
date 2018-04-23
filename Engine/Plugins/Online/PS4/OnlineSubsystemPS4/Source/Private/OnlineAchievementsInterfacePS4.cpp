// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

// Module includes
#include "OnlineAchievementsInterfacePS4.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineAsyncTaskManagerPS4.h"

bool FOnlineAchievementsPS4::FAsyncTrophyTask::EnsureTrophyContext()
{
	// Initialize with a NULL entry if we don't have one for this user
	if ( AchievementsPS4->ContextMap.Find( ServiceUserId ) == NULL )
	{
		SceNpTrophyContext & TrophyContext = AchievementsPS4->ContextMap.Add( ServiceUserId );
		TrophyContext = SCE_NP_TROPHY_INVALID_CONTEXT;
	}

	// Grab or create context info for this user
	SceNpTrophyContext & TrophyContext = AchievementsPS4->ContextMap.FindChecked( ServiceUserId );

	// Make sure we have a trophy context
	if ( TrophyContext == SCE_NP_TROPHY_INVALID_CONTEXT && !RegisterTrophyContext() )
	{
		UE_LOG_ONLINE( Error, TEXT( "FAsyncTrophyTask::EnsureTrophyContext: RegisterTrophyContext FAILED") );
		State = Failed;
		TrophyContext = SCE_NP_TROPHY_INVALID_CONTEXT;
		return false;
	}

	return true;
}

bool FOnlineAchievementsPS4::FAsyncTrophyTask::RegisterTrophyContext()
{
	SceNpTrophyContext & TrophyContext = AchievementsPS4->ContextMap.FindChecked( ServiceUserId );

	const SceNpServiceLabel ServiceLabel = 0;

	// Create the trophy context for this user
	int Result = sceNpTrophyCreateContext( &TrophyContext, ServiceUserId, ServiceLabel, 0 );

	if ( Result < 0 )
	{
		UE_LOG_ONLINE( Error, TEXT( "FAsyncTrophyTask::RegisterTrophyContext: sceNpTrophyCreateContext FAILED (0x%08x)"), Result );
		return false;
	}

	// Create a handle used to register the context
	SceNpTrophyHandle TrophyContextHandle = SCE_NP_TROPHY_INVALID_HANDLE;

	Result = sceNpTrophyCreateHandle( &TrophyContextHandle );

	if ( Result < 0 )
	{
		sceNpTrophyDestroyContext( TrophyContext );
		UE_LOG_ONLINE( Error, TEXT( "FAsyncTrophyTask::RegisterTrophyContext: sceNpTrophyCreateHandle FAILED (0x%08x)"), Result );
		return false;
	}

	// Register the context (this is the slow call)
	Result = sceNpTrophyRegisterContext( TrophyContext, TrophyContextHandle, 0 );

	if ( Result < 0 )
	{
		UE_LOG_ONLINE( Error, TEXT( "FAsyncTrophyTask::RegisterTrophyContext: sceNpTrophyRegisterContext FAILED (0x%08x)"), Result );
		sceNpTrophyDestroyHandle( TrophyContextHandle );
		sceNpTrophyDestroyContext( TrophyContext );
		return false;
	}

	// No longer need this
	sceNpTrophyDestroyHandle( TrophyContextHandle );

	return true;
}

bool FOnlineAchievementsPS4::FAsyncTrophyTask::IsDone()
{
	return ( State == Complete ) || ( State == Failed );
}

bool FOnlineAchievementsPS4::FAsyncTrophyTask::WasSuccessful()
{
	return State == Complete;
}

FString	FOnlineAchievementsPS4::FAsyncTrophyTask::ToString() const
{
	return TEXT( "FAsyncTrophyTask" );
}

void FOnlineAchievementsPS4::FAsyncUnlockTrophyTask::Tick()
{
	if ( State != Idle )
	{
		// We've already started, just waiting on Finalized to be called
		return;
	}

	// Set our state to pending
	State = Pending;

	if ( !EnsureTrophyContext() )
	{
		UE_LOG_ONLINE( Error, TEXT( "FAsyncUnlockTrophyTask::Tick: EnsureTrophyContext FAILED") );
		return;
	}

	// Grab or create context info for this user
	SceNpTrophyContext & TrophyContext = AchievementsPS4->ContextMap.FindChecked( ServiceUserId );

	// Give the trophy to the user
	SceNpTrophyHandle TrophyHandle = SCE_NP_TROPHY_INVALID_HANDLE;

	int32 Result = sceNpTrophyCreateHandle( &TrophyHandle );

	if ( Result < 0 )
	{
		UE_LOG_ONLINE( Error, TEXT( "FAsyncUnlockTrophyTask::Tick: sceNpTrophyCreateHandle FAILED (0x%08x)"), Result );
		State = Failed;
		return;
	}

	SceNpTrophyId PlatinumId = SCE_NP_TROPHY_INVALID_TROPHY_ID;

	Result = sceNpTrophyUnlockTrophy( TrophyContext, TrophyHandle, TrophyId, &PlatinumId );

	// Destroy the handle, done with this
	sceNpTrophyDestroyHandle( TrophyHandle );

	if ( Result != SCE_NP_TROPHY_ERROR_TROPHY_ALREADY_UNLOCKED && Result < 0 )
	{
		UE_LOG_ONLINE( Error, TEXT( "FAsyncUnlockTrophyTask::Tick: sceNpTrophyUnlockTrophy FAILED (0x%08x)"), Result );
		State = Failed;
		return;
	}

	if ( PlatinumId != SCE_NP_TROPHY_INVALID_TROPHY_ID ) 
	{
		// FIXME: Notify game that a platinum trophy was unlocked
	}

	// We're done
	State = Complete;
}

void FOnlineAchievementsPS4::FAsyncUnlockTrophyTask::TriggerDelegates()
{
	FAsyncTrophyTask::TriggerDelegates();
	WriteCallbackDelegate.ExecuteIfBound(FUniqueNetIdPS4::FindOrCreate(ServiceUserId).Get(), (State != Failed));
}

void FOnlineAchievementsPS4::FAsyncQueryTrophiesTask::Tick()
{
	if ( State != Idle )
	{
		// We've already started, just waiting on Finalized to be called
		return;
	}

	// Set our state to pending
	State = Pending;

	if ( !EnsureTrophyContext() )
	{
		UE_LOG_ONLINE( Error, TEXT( "FAsyncQueryTrophiesTask::Tick: EnsureTrophyContext FAILED") );
		return;
	}

	// Grab or create context info for this user
	SceNpTrophyContext & TrophyContext = AchievementsPS4->ContextMap.FindChecked( ServiceUserId );

	// Give the trophy to the user
	SceNpTrophyHandle TrophyHandle = SCE_NP_TROPHY_INVALID_HANDLE;

	int32 Result = sceNpTrophyCreateHandle( &TrophyHandle );

	if ( Result < 0 )
	{
		UE_LOG_ONLINE( Error, TEXT( "FAsyncQueryTrophiesTask::Tick: sceNpTrophyCreateHandle FAILED (0x%08x)"), Result );
		State = Failed;
		return;
	}

	Result = sceNpTrophyGetTrophyUnlockState( TrophyContext, TrophyHandle, &TrophyFlagArray, &TrophyCount );

	// Destroy the handle, done with this
	sceNpTrophyDestroyHandle( TrophyHandle );

	if ( Result < 0 )
	{
		UE_LOG_ONLINE( Error, TEXT( "FAsyncQueryTrophiesTask::Tick: sceNpTrophyGetTrophyUnlockState FAILED (0x%08x)"), Result );
		State = Failed;
		return;
	}

	for ( int32 i = 0; i < TrophyCount; i++ )
	{
		SceNpTrophyDetails	Details;
		SceNpTrophyData		Data;

		FMemory::Memzero( Details );
		FMemory::Memzero( Data );
		
		Details.size = sizeof( Details );
		Data.size = sizeof( Data );

		Result = sceNpTrophyCreateHandle( &TrophyHandle );

		if ( Result < 0 )
		{
			UE_LOG_ONLINE( Error, TEXT( "FAsyncQueryTrophiesTask::Tick: sceNpTrophyCreateHandle FAILED (0x%08x)"), Result );
			State = Failed;
			return;
		}

		Result = sceNpTrophyGetTrophyInfo( TrophyContext, TrophyHandle, i, &Details, &Data );

		// Destroy the handle, done with this
		sceNpTrophyDestroyHandle( TrophyHandle );

		if ( Result < 0 )
		{
			UE_LOG_ONLINE( Error, TEXT( "FAsyncQueryTrophiesTask::Tick: sceNpTrophyGetTrophyInfo FAILED (0x%08x)"), Result );
			State = Failed;
			return;
		}

		TrophyDetails.Add( Details );
	}

	// We're done
	State = Complete;
}

void FOnlineAchievementsPS4::FAsyncQueryTrophiesTask::TriggerDelegates()
{
	if ( State == Failed )
	{
		Delegate.ExecuteIfBound( PlayerId.Get(), false );
		return;
	}

	TArray< FOnlineAchievement > PlayerAchievementsQueried;

	for ( int32 i = 0; i < TrophyCount; i++ )
	{
		FOnlineAchievement NewAchivement;

		NewAchivement.Id		= FString::Printf( TEXT( "%i" ), i );
		NewAchivement.Progress	= SCE_NP_TROPHY_FLAG_ISSET( i, &TrophyFlagArray ) ? 100 : 0;

		PlayerAchievementsQueried.Add(NewAchivement);

		FOnlineAchievementDesc NewDesc;

		NewDesc.Title			= FText::FromString( TrophyDetails[i].name );
		NewDesc.LockedDesc		= FText::FromString( TrophyDetails[i].description );
		NewDesc.UnlockedDesc	= FText::FromString( TrophyDetails[i].description );
		NewDesc.bIsHidden		= TrophyDetails[i].hidden;

		AchievementsPS4->AchievementDescriptions.Add( NewAchivement.Id, NewDesc );
	}

	AchievementsPS4->PlayerAchievements.Add(ServiceUserId, PlayerAchievementsQueried);

	Delegate.ExecuteIfBound( PlayerId.Get(), true );
}

void FOnlineAchievementsPS4::FAsyncUserSignoutTask::Tick()
{
	if ( State != Idle )
	{
		// We've already started, just waiting on Finalized to be called
		return;
	}

	// Set our state to pending
	State = Pending;

	// If we have a valid trophy context for this user, make sure we destroy it
	if ( AchievementsPS4->ContextMap.Find( ServiceUserId ) != NULL )
	{
		SceNpTrophyContext & TrophyContext = AchievementsPS4->ContextMap.FindChecked( ServiceUserId );

		if ( TrophyContext != SCE_NP_TROPHY_INVALID_CONTEXT )
		{
			const int32 Result = sceNpTrophyDestroyContext( TrophyContext );

			if ( Result < 0 )
			{
				UE_LOG_ONLINE( Error, TEXT( "FAsyncUserSignoutTask::Tick: sceNpTrophyDestroyContext FAILED (0x%08x)"), Result );
			}
		}

		// Remove the context from the map
		AchievementsPS4->ContextMap.Remove( ServiceUserId );
	}

	// We're done
	State = Complete;
}

FOnlineAchievementsPS4::FOnlineAchievementsPS4( class FOnlineSubsystemPS4* InSubsystem )
{
	PS4Subsystem = InSubsystem;

	// Load trophy module
	if ( sceSysmoduleLoadModule( SCE_SYSMODULE_NP_TROPHY ) != SCE_OK ) 
	{
		UE_LOG_ONLINE(Error, TEXT( "FOnlineSubsystemPS4::FOnlineAchievementsPS4: sceSysmoduleLoadModule( SCE_SYSMODULE_NP_TROPHY ) != SCE_OK" ) );
	}

	LoadAndInitFromJsonConfig( TEXT( "Achievements.json" ) );

	FCoreDelegates::OnUserLoginChangedEvent.AddRaw(this, &FOnlineAchievementsPS4::UserLoginEventHandler);
}

FOnlineAchievementsPS4::~FOnlineAchievementsPS4()
{
	Shutdown();
}

void FOnlineAchievementsPS4::Shutdown()
{
	// Shutdown trophy module
	if ( sceSysmoduleUnloadModule( SCE_SYSMODULE_NP_TROPHY ) != SCE_OK )
	{
		UE_LOG_ONLINE(Error, TEXT( "FOnlineSubsystemPS4::Shutdown: sceSysmoduleUnloadModule( SCE_SYSMODULE_NP_TROPHY ) != SCE_OK" ) );
	}
}

bool FOnlineAchievementsPS4::LoadAndInitFromJsonConfig( const TCHAR* JsonConfigName )
{
	const FString BaseDir = FPaths::ProjectDir() + TEXT( "Config/OSS/PS4/" );
	const FString JSonConfigFilename = BaseDir + JsonConfigName;;

	FString JSonText;

	if ( !FFileHelper::LoadFileToString( JSonText, *JSonConfigFilename ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineAchievementsPS4: Failed to find json OSS achievements config: %s"), *JSonConfigFilename );
		return false;
	}

	if ( !AchievementsConfig.FromJson( JSonText ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineAchievementsPS4: Failed to parse json OSS achievements config: %s"), *JSonConfigFilename );
		return false;
	}

	return true;
}

void FOnlineAchievementsPS4::UserLoginEventHandler( bool bLoggingIn, int32 UserID, int32 UserIndex )
{
	if ( !bLoggingIn )
	{
		auto NewTask = new FAsyncUserSignoutTask( PS4Subsystem, this, UserID );
		PS4Subsystem->GetAsyncTaskManager()->AddToInQueue( NewTask );
	}
}

void FOnlineAchievementsPS4::QueryAchievements( const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate )
{
	auto NewTask = new FAsyncQueryTrophiesTask( PS4Subsystem, this, FUniqueNetIdPS4::Cast(PlayerId), Delegate );
	PS4Subsystem->GetAsyncTaskManager()->AddToInQueue( NewTask );
}


void FOnlineAchievementsPS4::QueryAchievementDescriptions( const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate )
{
	Delegate.ExecuteIfBound( PlayerId, false );
}


void FOnlineAchievementsPS4::WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate)
{
	FUniqueNetIdPS4 const& PS4User = FUniqueNetIdPS4::Cast(PlayerId);

	bool bResult = true;

	for ( FStatPropertyArray::TConstIterator It( WriteObject->Properties ); It; ++It )
	{
		float Percent = 0.0f;
		It.Value().GetValue( Percent );

		if ( Percent < 100.0f )
		{
			continue;
		}

		// Convert the achievement name to the trophy index
		int32* Index = AchievementsConfig.AchievementMap.Find( It.Key().ToString() );

		if ( Index == NULL )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineAchievementsPS4::WriteAchievements: No mapping for achievement %s"), *It.Key().ToString() );
			bResult = false;
			continue;
		}

		// Create a task to unlock the trophy
		auto NewTask = new FAsyncUnlockTrophyTask( PS4Subsystem, this, PS4User.GetUserId(), *Index, Delegate );
		PS4Subsystem->GetAsyncTaskManager()->AddToInQueue( NewTask );
	}

	if (!bResult)
	{
		Delegate.ExecuteIfBound(PlayerId, false);
	}
}


EOnlineCachedResult::Type FOnlineAchievementsPS4::GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement)
{
	FUniqueNetIdPS4 const& PS4User = FUniqueNetIdPS4::Cast( PlayerId );
	TArray< FOnlineAchievement > * Achievements = PlayerAchievements.Find( PS4User.GetUserId() );

	if ( Achievements == NULL )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineAchievementsPS4::GetCachedAchievement: Achievements have not been read for player %s"), *PlayerId.ToString() );
		return EOnlineCachedResult::NotFound;
	}

	// Look up platform ID from achievement mapping
	int32* Index = AchievementsConfig.AchievementMap.Find( AchievementId );
	if (Index == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineAchievementsLive::GetCachedAchievement: No mapping for achievement %s"), *AchievementId);
		return EOnlineCachedResult::NotFound;
	}

	FString PlatformAchievementId = FString::FromInt(*Index);

	for ( int32 i = 0; i < Achievements->Num(); i++ )
	{
		if ( (*Achievements)[ i ].Id == PlatformAchievementId )
		{
			OutAchievement = (*Achievements)[ i ];
			return EOnlineCachedResult::Success;
		}
	}

	return EOnlineCachedResult::NotFound;
}


EOnlineCachedResult::Type FOnlineAchievementsPS4::GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement>& OutAchievements)
{
	FUniqueNetIdPS4 const& PS4User = FUniqueNetIdPS4::Cast( PlayerId );
	TArray< FOnlineAchievement > * Achievements = PlayerAchievements.Find( PS4User.GetUserId() );

	if ( Achievements == NULL )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineAchievementsPS4::GetCachedAchievement: Achievements have not been read for player %s"), *PlayerId.ToString() );
		return EOnlineCachedResult::NotFound;
	}

	OutAchievements = *Achievements;

	return EOnlineCachedResult::Success;
}


EOnlineCachedResult::Type FOnlineAchievementsPS4::GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc)
{
	// Look up platform ID from achievement mapping
	int32* Index = AchievementsConfig.AchievementMap.Find( AchievementId );
	if (Index == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineAchievementsLive::GetCachedAchievementDescription: No mapping for achievement %s"), *AchievementId);
		return EOnlineCachedResult::NotFound;
	}

	FString PlatformAchievementId = FString::FromInt(*Index);

	FOnlineAchievementDesc * AchievementDesc = AchievementDescriptions.Find( PlatformAchievementId );

	if ( AchievementDesc == NULL )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineAchievementsPS4::GetCachedAchievementDescription: Achievements have not been read for id: %s"), *AchievementId );
		return EOnlineCachedResult::NotFound;
	}

	OutAchievementDesc = *AchievementDesc;
	return EOnlineCachedResult::Success;
}

#if !UE_BUILD_SHIPPING
bool FOnlineAchievementsPS4::ResetAchievements( const FUniqueNetId& PlayerId )
{
	check(!TEXT("ResetAchievements has not been implemented for PS4"));
	return false;
};
#endif // !UE_BUILD_SHIPPING
