// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4SaveGameSystem.h"
#include "GameDelegates.h"
#include "SonyApplication.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "SonySaveGameSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPS4SaveGame, Log, All);

FPS4SaveGameSystem::FPS4SaveGameSystem()
{
}

FPS4SaveGameSystem::~FPS4SaveGameSystem()
{
}

const TCHAR* FPS4SaveGameSystem::GetSaveFileName()
{
	return TEXT("ue4savegame.ps4.sav");
}

int32 FPS4SaveGameSystem::Mount(const SceSaveDataMountMode Mode, SceUserServiceUserId UserId, SceSaveDataDirName& DirName, SceSaveDataBlocks Blocks, SceSaveDataMountResult& MountResult)
{
	SceSaveDataMount2 Mount;
	FMemory::Memzero(&Mount, sizeof(SceSaveDataMount2));
	Mount.userId = UserId;
	Mount.dirName = &DirName;
	Mount.blocks = Blocks;
	Mount.mountMode = Mode;
	FMemory::Memzero(&MountResult, sizeof(MountResult));
	return sceSaveDataMount2(&Mount, &MountResult);
}

int32 FPS4SaveGameSystem::Unmount(SceSaveDataMountPoint* MountPoint, bool bCommit)
{
	return sceSaveDataUmount(MountPoint);
}

SceSaveDataDialogSystemMessageType FPS4SaveGameSystem::GetNoSpaceMessageType()
{
	return SCE_SAVE_DATA_DIALOG_SYSMSG_TYPE_NOSPACE;
}
