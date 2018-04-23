// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4ChunkInstall.h"
#include "Containers/Ticker.h"
#include "Misc/CallbackDevice.h"
#include "Misc/Paths.h"
#include <libsysmodule.h>
#include <playgo.h>
#include "Stats.h"


FPS4ChunkInstall::FPS4ChunkInstall()
{
	bIsPlayGoPackage = false;

	int Ret = 0;

	Ret = sceSysmoduleLoadModule(SCE_SYSMODULE_PLAYGO);
	check(Ret == SCE_OK);

	// initialize PlayGo
	ScePlayGoInitParams initParams;
	memset(&initParams, 0x0, sizeof(initParams));
	PlayGoBuffer = FMemory::Malloc(SCE_PLAYGO_HEAP_SIZE);
	check(PlayGoBuffer);
	initParams.bufAddr = PlayGoBuffer;
	initParams.bufSize = SCE_PLAYGO_HEAP_SIZE;
	Ret = scePlayGoInitialize( &initParams );
	check(Ret == SCE_OK);

	// get our package handle if this app is compatible with PlayGo
	Ret = scePlayGoOpen(&PlayGoHandle, nullptr);
	if (Ret == SCE_OK)
	{
		bIsPlayGoPackage = true;

		// Chunks are implemented as pakfiles. At init time, any available chunks should have already been mounted.
		int32 ChunkID = 0;
		EChunkLocation::Type ChunkLocation;
		do 
		{
			ChunkLocation = GetChunkLocation(ChunkID);
			if (ChunkLocation == EChunkLocation::LocalFast || ChunkLocation == EChunkLocation::LocalSlow)
			{
				MountedChunks.Add(ChunkID);
			}
			++ChunkID;
		} while (ChunkLocation != EChunkLocation::DoesNotExist);
		

		// first chunk is required to start the app.  It's always mounted.
		check(MountedChunks.Find(0));

		AddToTicker();
	}
	else
	{
		ShutDown();
	}	
}


FPS4ChunkInstall::~FPS4ChunkInstall()
{
	ShutDown();
}


void FPS4ChunkInstall::ShutDown()
{
	int Ret = 0;
	RemoveFromTicker();
	if (bIsPlayGoPackage)
	{
		Ret = scePlayGoClose(PlayGoHandle);
		check(Ret == SCE_OK);
	}

	Ret = scePlayGoTerminate();
	check(Ret == SCE_OK);

	Ret = sceSysmoduleUnloadModule(SCE_SYSMODULE_PLAYGO);
	check(Ret == SCE_OK);

	if (PlayGoBuffer)
	{
		FMemory::Free(PlayGoBuffer);
		PlayGoBuffer = nullptr;
	}

	bIsPlayGoPackage = false;
}


EChunkLocation::Type FPS4ChunkInstall::GetChunkLocation( uint32 ChunkID )
{
	if (!bIsPlayGoPackage)
	{
		return EChunkLocation::LocalFast;
	}

	// check for cached location
	FPS4ChunkStatus* CurStatus = ChunkStatus.Find(ChunkID);

	// early out if we already know it's local (should be the most common case)
	if (CurStatus && (CurStatus->Locus == SCE_PLAYGO_LOCUS_LOCAL_FAST))
	{
		return EChunkLocation::LocalFast;
	}

	// request an updated chunk locus
	ScePlayGoChunkId PlayGoChunkID = ChunkID;
	ScePlayGoLocus Locus = SCE_PLAYGO_LOCUS_LOCAL_FAST;

	int Ret = scePlayGoGetLocus(PlayGoHandle, &PlayGoChunkID, 1, &Locus);

	// handle the case of the unknown chunk ID
	if (Ret == SCE_PLAYGO_ERROR_BAD_CHUNK_ID)
	{
		return EChunkLocation::DoesNotExist;
	}

	check(Ret == SCE_OK);

	// update the cached chunk status
	if (CurStatus)
	{
		CurStatus->Locus = Locus;
	}
	else
	{
		FPS4ChunkStatus NewStatus;
		NewStatus.Locus = Locus;
		ChunkStatus.Add(ChunkID, NewStatus);
	}

	// convert the locus to our format
	EChunkLocation::Type ChunkLoc;
	switch (Locus)
	{
	case SCE_PLAYGO_LOCUS_NOT_DOWNLOADED:
		ChunkLoc = EChunkLocation::NotAvailable;
		break;
	case SCE_PLAYGO_LOCUS_LOCAL_SLOW:
		ChunkLoc = EChunkLocation::LocalSlow;
		break;
	case SCE_PLAYGO_LOCUS_LOCAL_FAST:
	default:
		ChunkLoc = EChunkLocation::LocalFast;
		break;
	}

	return ChunkLoc;
}

bool FPS4ChunkInstall::GetProgressReportingTypeSupported( EChunkProgressReportingType::Type ReportType )
{
	bool bSupported = false;
	switch (ReportType)
	{
		case EChunkProgressReportingType::ETA:
		case EChunkProgressReportingType::PercentageComplete:
			bSupported = true;
			break;
		default:
			bSupported = false;
			break;
	}
	return bSupported;
}


float FPS4ChunkInstall::GetChunkProgress( uint32 ChunkID, EChunkProgressReportingType::Type ReportType )
{
	float Val;
	switch(ReportType)
	{
		case EChunkProgressReportingType::ETA:
			Val = GetChunkETA(ChunkID);
			break;
		case EChunkProgressReportingType::PercentageComplete:
			Val = GetChunkPercentComplete(ChunkID);
			break;
		default:
			UE_LOG(LogChunkInstaller, Error, TEXT("Unsupported ProgressReportType: %i"), (int)ReportType);
			Val = 0.0f;
			break;
	}
	return Val;
}


float FPS4ChunkInstall::GetChunkETA(uint32 ChunkID)
{
	if (!bIsPlayGoPackage)
	{
		return 0.0f;
	}

	// early out if we already know it's local
	const FPS4ChunkStatus* CurStatus = ChunkStatus.Find(ChunkID);

	if (CurStatus && (CurStatus->Locus == SCE_PLAYGO_LOCUS_LOCAL_FAST))
	{
		return 0.0f;
	}

	ScePlayGoChunkId PlayGoChunkID = ChunkID;
	ScePlayGoEta Eta = 0;

	int Ret = scePlayGoGetEta(PlayGoHandle, &PlayGoChunkID, 1, &Eta);
	check(Ret == SCE_OK);

	return (float)Eta;
}

FDelegateHandle FPS4ChunkInstall::SetChunkInstallDelgate(uint32 ChunkID, FPlatformChunkInstallCompleteDelegate Delegate)
{
	FPlatformChunkInstallCompleteMultiDelegate* FoundDelegate = DelegateMap.Find(ChunkID);
	if (FoundDelegate)
	{
		return FoundDelegate->Add(Delegate);
	}
	else
	{
		FPlatformChunkInstallCompleteMultiDelegate MC;
		auto RetVal = MC.Add(Delegate);
		DelegateMap.Add(ChunkID, MC);
		return RetVal;
	}
	return FDelegateHandle();
}

void FPS4ChunkInstall::RemoveChunkInstallDelgate(uint32 ChunkID, FDelegateHandle Delegate)
{
	FPlatformChunkInstallCompleteMultiDelegate* FoundDelegate = DelegateMap.Find(ChunkID);
	if (!FoundDelegate)
	{
		return;
	}
	FoundDelegate->Remove(Delegate);
}


float FPS4ChunkInstall::GetChunkPercentComplete(uint32 ChunkID)
{
	if (!bIsPlayGoPackage)
	{
		return 100.0f;
	}

	// early out if we already know it's local
	const FPS4ChunkStatus* CurStatus = ChunkStatus.Find(ChunkID);

	if (CurStatus && (CurStatus->Locus == SCE_PLAYGO_LOCUS_LOCAL_FAST))
	{
		return 100.0f;
	}

	ScePlayGoChunkId PlayGoChunkID = ChunkID;
	ScePlayGoProgress ChunkProgress;	

	int Ret = scePlayGoGetProgress(PlayGoHandle, &PlayGoChunkID, 1, &ChunkProgress);
	check(Ret == SCE_OK);

	float ProgressPct = 0.0f;
	if (ChunkProgress.totalSize > 0)
	{
		ProgressPct = ((float)ChunkProgress.progressSize / (float)ChunkProgress.totalSize) * 100.0f;
	}

	return (float)ProgressPct;
}


EChunkInstallSpeed::Type FPS4ChunkInstall::GetInstallSpeed()
{
	if (!bIsPlayGoPackage)
	{
		return EChunkInstallSpeed::Paused;
	}

	ScePlayGoInstallSpeed PlayGoSpeed = SCE_PLAYGO_INSTALL_SPEED_SUSPENDED;

	int Ret = scePlayGoGetInstallSpeed(PlayGoHandle, &PlayGoSpeed);
	check(Ret == SCE_OK);

	// translate PlayGo speeds to our enum
	switch (PlayGoSpeed)
	{
	case SCE_PLAYGO_INSTALL_SPEED_FULL:
		return EChunkInstallSpeed::Fast;
	case SCE_PLAYGO_INSTALL_SPEED_TRICKLE:
		return EChunkInstallSpeed::Slow;
	default:
		return EChunkInstallSpeed::Paused;
	}
}


bool FPS4ChunkInstall::SetInstallSpeed( EChunkInstallSpeed::Type InstallSpeed )
{
	if (!bIsPlayGoPackage)
	{
		return false;
	}

	ScePlayGoInstallSpeed PlayGoSpeed;

	switch (InstallSpeed)
	{
	case EChunkInstallSpeed::Fast:
		PlayGoSpeed = SCE_PLAYGO_INSTALL_SPEED_FULL;
		break;
	case EChunkInstallSpeed::Paused:
		PlayGoSpeed = SCE_PLAYGO_INSTALL_SPEED_SUSPENDED;
		break;
	case EChunkInstallSpeed::Slow:
	default:
		PlayGoSpeed = SCE_PLAYGO_INSTALL_SPEED_TRICKLE;
		break;
	}

	int Ret = scePlayGoSetInstallSpeed(PlayGoHandle, PlayGoSpeed);
	check(Ret == SCE_OK);

	return (Ret == SCE_OK);
}


bool FPS4ChunkInstall::PrioritizeChunk( uint32 ChunkID, EChunkPriority::Type Priority )
{
	if (!bIsPlayGoPackage || Priority == EChunkPriority::Low)
	{
		return false;
	}

	ScePlayGoChunkId PlayGoChunkID = ChunkID;

	int Ret = scePlayGoPrefetch(PlayGoHandle, &PlayGoChunkID, 1, SCE_PLAYGO_LOCUS_LOCAL_FAST);
	check(Ret == SCE_OK)

	return (Ret == SCE_OK);
}


bool FPS4ChunkInstall::DebugStartNextChunk()
{	
#if !UE_BUILD_SHIPPING
	//cannot call this function in a submission build.  Will fail cert.
	int32 Ret = scePlayGoRequestNextChunk(PlayGoHandle, nullptr);
	check(Ret == SCE_OK);

	return (Ret == SCE_OK);
#endif
	return true;
}

//Adds the installer to the core ticker.
void FPS4ChunkInstall::AddToTicker()
{
	if (TickHandle == FDelegateHandle())
	{
		FTicker& Ticker = FTicker::GetCoreTicker();

		// Register delegate for ticker callback
		FTickerDelegate TickDelegate = FTickerDelegate::CreateRaw(this, &FPS4ChunkInstall::Tick);
		TickHandle = Ticker.AddTicker(TickDelegate, 0.5f);
	}
}

//Removes the installer from the core ticker.
void FPS4ChunkInstall::RemoveFromTicker()
{
	FTicker& Ticker = FTicker::GetCoreTicker();

	// Unregister ticker delegate
	if (TickHandle != FDelegateHandle())
	{
		Ticker.RemoveTicker(TickHandle);
		TickHandle = FDelegateHandle();
	}
}

bool FPS4ChunkInstall::Tick(float DeltaSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(PS4ChunkInstallTick);

	uint32 NumChunks = -1;
	ScePlayGoToDo ToDoItems[SCE_PLAYGO_CHUNK_INDEX_MAX];
	int32 Ret = scePlayGoGetToDoList(PlayGoHandle, ToDoItems, SCE_PLAYGO_CHUNK_INDEX_MAX, &NumChunks);
	if (Ret == SCE_OK)
	{
		// populate chunk location maps from any new chunks in the todo list.
		for (int32 i = 0; i < NumChunks; ++i)
		{
			GetChunkLocation(ToDoItems[i].chunkId);
		}		
	}
	else
	{
		UE_LOG(LogChunkInstaller, Warning, TEXT("scePlayGoGetToDoList failed. 0x%x"), Ret);
	}

	// loop over all the chunks that we know exist so we can mount and fire delegates for any that are newly completed.
	for (auto Iter = ChunkStatus.CreateIterator(); Iter; ++Iter)
	{		
		int32 ChunkID = Iter.Key();		

		//call GetChunkLocation rather than using the value from the iterator because we may need a call to scePlayGoGetLocus
		//to get the latest state.
		EChunkLocation::Type ChunkLoc = GetChunkLocation(ChunkID);

		if (ChunkLoc == EChunkLocation::LocalFast || ChunkLoc == EChunkLocation::LocalSlow)
		{
			if (!MountedChunks.Find(ChunkID))
			{
				FString PakLocation = FString::Printf(TEXT("%sPaks/pakchunk%i-ps4.pak"), *FPaths::ProjectContentDir(), ChunkID);
				bool bMounted = FCoreDelegates::OnMountPak.Execute(PakLocation, 0, nullptr);
				if (bMounted)
				{
					MountedChunks.Add(ChunkID);

					UE_LOG(LogChunkInstaller, Log, TEXT("PlayGo Chunk %i mounted."), ChunkID);
					
					// Inform any listeners that the chunk download has been completed.
					FPlatformChunkInstallCompleteMultiDelegate* FoundDelegate = DelegateMap.Find(ChunkID);
					if (FoundDelegate)
					{
						FoundDelegate->Broadcast(ChunkID);
					}

					InstallDelegate.Broadcast(ChunkID, true);
				}
				else
				{					
					UE_LOG(LogChunkInstaller, Warning, TEXT("PlayGo Chunk %i couldn't be mounted."), ChunkID);

					InstallDelegate.Broadcast(ChunkID, false);
				}
			}
		}
	}

	//if there are no chunks left to download then we can stop ticking entirely.
	if (NumChunks == 0)
	{
		RemoveFromTicker();
	}
	return true;
}
