// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4SaveGameSystem.h"
#include "GameDelegates.h"
#include <libsysmodule.h>
#include <fios2.h>
#include "PS4Application.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RenderingThread.h"

DEFINE_LOG_CATEGORY_STATIC(LogPS4SaveGame, Log, All);

#define SAVE_SLOT_TEMPLATE "ue4Game%04x"
#define SAVE_FILE_NAME TEXT("ue4savegame.ps4.sav")

// A pad used when calculating saved game block sizes. Accounts for system data
#define EXTRA_BLOCK_COUNT 5

// Slowest measured save speed was 6604990 bytes/sec, with slow HDD emulation turned on

// This is the save game block count used when the save game delegate does not provide a size.
// Is was chosen, based on empirical write speed test data, to take slightly less than 15 seconds to write out
// at full size.
// An alternative would be 1200, which is slightly less than 5 seconds of writing time
#define DEFAULT_BLOCK_COUNT 3000

// The maximum allowed save game size.
// NOTE: This is less than Sony's mandated 1GB limit. It is set based on empirical measurements of 
// save game writing speed. This size prevents violating the 15 sec save data mount time requirement.
#define MAX_SAVE_SIZE (DEFAULT_BLOCK_COUNT * SCE_SAVE_DATA_BLOCK_SIZE)

// NOTE: Enable to measure save file write performance
#define MEASURE_SAVE_BANDWIDTH 0

//
// Local classes
//

// This class is a scoped handler for save game dialogs. It ensures that the save game dialog is terminated when an instance
// falls out of scope
class FDialogScope
{
public:
	FDialogScope()
	{
		bIsInitialized = false;

		// Initialize the save game dialog system, in case it hasn't been initialized yet.
		// NOTE: This is not initialized in the main Initialize() method because it is dependent on
		// the common dialog initialization, which the docs state can take a while to complete.
		int32 Result = sceSaveDataDialogInitialize();
		if (Result == SCE_OK || Result == SCE_COMMON_DIALOG_ERROR_ALREADY_INITIALIZED)
		{
			// These are the acceptable return values, so we are initialized
			bIsInitialized = true;
		}
		else
		{
			// The initialization failed. Generate sensible output for likely conditions, rather than
			// simply logging the raw error code
			switch (Result)
			{
				case SCE_COMMON_DIALOG_ERROR_NOT_SYSTEM_INITIALIZED:
					UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataDialogInitialize failed in SaveGame() because the common dialog system is not initialized."));
					break;

				case SCE_COMMON_DIALOG_ERROR_BUSY:
					UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataDialogInitialize failed in SaveGame() because another common dialog is running."));
					break;

				case SCE_COMMON_DIALOG_ERROR_OUT_OF_MEMORY:
					UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataDialogInitialize failed in SaveGame() due to lack of memory."));
					break;

				case SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL:
					// Fall through to default case
				default:
					UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataDialogInitialize failed in SaveGame() for an unknown reason. Error code: 0x%x"), Result);
					break;
			}

			// Didn't initialize
		}
	}

	~FDialogScope()
	{
		if (bIsInitialized)
		{
			sceSaveDataDialogTerminate();
			bIsInitialized = false;
		}
	}

private:
	bool bIsInitialized;
};


//
// Utilities
//

// Help make the code a bit more readable
// typedef SceUserServiceUserId UserId;
// #define SCE_USER_SERVICE_USER_ID_INVALID SCE_USER_SERVICE_USER_ID_INVALID;

/**
 * Check for a valid user id
 */
static inline bool IsUserIdValid(SceUserServiceUserId Id)
{
	return Id != SCE_USER_SERVICE_USER_ID_INVALID;
}

/**
 * Implements the local policy for fetching the title id
 */
static inline SceSaveDataTitleId* GetTitleId()
{
	// NOTE: Returning NULL since we are only interested in this title's saved games
	// If access to other saved games is required, then this will need to return a proper title id and the fingerprint of the
	// mount structure will need to be set as well
	return NULL;
}

/**
 * Implements local policy for building a save game path from a given mount point
 */
static FString GetSaveGamePath(SceSaveDataMountPoint* MountPoint)
{
	// NOTE: The mount point is a char * (UTF-8), while all of the string handling in the engine is TCHAR.
	// return FString::Printf(TEXT("%hs/%s"), MountPoint->data, SAVE_FILE_NAME);  // this works
	// return FString::Printf(TEXT("%S/%s"), MountPoint->data, SAVE_FILE_NAME);  // this does not work. %S is not recognized and is treated as literal
	return FString::Printf(TEXT("%s/%s"), UTF8_TO_TCHAR(MountPoint->data), SAVE_FILE_NAME);
}

bool FPS4SaveGameSystem::PlatformHasNativeUI()
{
	// Only has native UI if the appropriate module succesfully loads, otherwise treat as if there was no native UI
	return bHasNativeUI;
}

ISaveGameSystem::ESaveExistsResult FPS4SaveGameSystem::DoesSaveGameExistWithResult(const TCHAR* Name, const int32 UserIndex)
{
	// Attempt to mount the directory
	SceUserServiceUserId Id = GetSaveGameUserId(UserIndex);
	if (!IsUserIdValid(Id))
	{
		// Can't mount because we don't have user, so say that the save game doesn't exist
		UE_LOG(LogPS4SaveGame, Warning, TEXT("Returning that save game doesn't exist because we don't have a user for mounting save data"));
		return ESaveExistsResult::DoesNotExist;
	}
	SetCurrentUser(Id);

	SetMountDirName(Name);

	SceSaveDataMount2 MountData;
	FillOutSceSaveDataMount (SCE_SAVE_DATA_MOUNT_MODE_RDONLY, &MountData);

	SceSaveDataMountResult MountResult;
	FMemory::Memzero(&MountResult, sizeof(MountResult));
	int32 result = sceSaveDataMount2(&MountData, &MountResult);
	if(result < SCE_OK)
	{
		// The mount failed. If the reason was that the data didn't exist, that is expected.
		// Otherwise we need to log the reason.
		if (result == SCE_SAVE_DATA_ERROR_NOT_FOUND)
		{
			// Expected result. No need to log
		}
		else
		{
			UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataMount failed in DoesSaveGameExist(). Error code 0x%x"), result);

			// Only need to display an error if the save was corrupt.
			if (result == SCE_SAVE_DATA_ERROR_BROKEN)
			{
				// Corrupt. Show error dialog (just use load dialog).
				FDialogScope Scope;
				DisplayCorruptDialog(SCE_SAVE_DATA_DIALOG_TYPE_LOAD);
			}
		}

		// Regardless of the failure reason, the directory doesn't exist
		return PlatformResultToGenericResult(result);
	}

	// If here, then we were able to mount, so there should be save data.
	// As a final test, check to see if the save file name exists.
	bool bDoesSaveFileExist = true;

	SceSaveDataMountPoint* MountPoint = &MountResult.mountPoint;
#if 0
	// NOTE: This code is disabled because using it will cause a disconnect between
	// what the PS4 reports as saved data and this function, i.e. the system menus will
	// show a saved game but the it won't exist if requested.
	// Rather that persist this condition, we'll instead report that it exists and
	// have it fail (and potentially deleted) when a save-game load is attempted. Thus the system and game
	// views of saved data stay in sync, although the system won't see the saved game as
	// corrupt while the game will.
	FString SaveGamePath = GetSaveGamePath(MountPoint);

	if (!FPaths::FileExists(SaveGamePath))
	{
		// Although the directory exists, the save data file does not.
		// Log this condition and fail the existence check.
		UE_LOG(LogPS4SaveGame, Error, TEXT("PS4 save data dir exists but does not have save file (%s)"), SAVE_FILE_NAME);

		// There is no save data, so it doesn't exist
		bDoesSaveFileExist = false;
	}

	// *** TODO: Add any other checks here for a properly configured save game directory
#endif

	// Unmount
	result = sceSaveDataUmount(MountPoint);
	if (result < 0 )
	{
		UE_LOG(LogPS4SaveGame, Error, TEXT("sceSaveDataUmount failed in DoesSaveGameExist(). Error code 0x%x"), result);
		// Don't display an error here because this is a utility call and not general directly user initiated
	}
	return bDoesSaveFileExist ? ESaveExistsResult::OK : ESaveExistsResult::DoesNotExist;
}

int FPS4SaveGameSystem::FindSaveSlot()
{
	// Design Notes:
	//    Although this is O(N) in existing saved games, any algorithm will be O(N) since at least one traversal
	// of the save slots is required to detect collisions. You could cache the result here, but the cache would need to
	// be cleared after each call to FindSavedGames(), which must be called between save game system invocations
	// to ensure that the data is up-to-date. This this function is only called once per invocation
	// of the saved game system, caching won't provide any benefits.

	// Run through the list of known saved games, attempting to parse the slot out of each one.
	// Keep a running count of the highest slot number seen.
	int32 HighestSlot = 0;
	for (int32 SaveGameIndex = 0; SaveGameIndex < SavedGameCount; ++SaveGameIndex)
	{
		int SlotNumber;
		if (sscanf (SavedGameDirs[SaveGameIndex].data, SAVE_SLOT_TEMPLATE, &SlotNumber) == 1)
		{
			if (SlotNumber > HighestSlot)
			{
				HighestSlot = SlotNumber;
			}
		}
	}

	// Return a slot just beyond the highest found slot
	return HighestSlot + 1;
}

bool FPS4SaveGameSystem::SelectOrCreateSavedGame(SceSaveDataParam &MetaData)
{
	bool bDidSucceed = false;
	while (true)
	{
		// Fill out the basic information for a save game dialog
		FillBasicSaveGameDialogData();

		// Customize the dialog before display
		SaveDialogAnimParam.userOK = SCE_SAVE_DATA_DIALOG_ANIMATION_OFF;
		SaveDialogAnimParam.userCancel = SCE_SAVE_DATA_DIALOG_ANIMATION_ON;

		// Display the dialog. The results will have been fetched by the time this function returns.
		if (!DisplayDialog())
		{
			UE_LOG(LogPS4SaveGame, Warning, TEXT("DisplayDialog() failed in SaveGame(). See previous log entries for reasons."));
			return false;
		}

		// The results of the dialog are now in the SaveDialogResults member.
		// Examine the results and react accordingly.
		if (SaveDialogResults.result == SCE_COMMON_DIALOG_RESULT_OK)
		{
			char* DirName = NULL;
			SceSaveDataDirName NewDirName;

			// The user selected a save game (directory) so make that the name to use
			// If the selected name is zero length, then it is a request to create a new save slot.
			// If the dirname is NULL, that means a new item was requested. Create a new save game slot here
			if (SaveDialogResults.dirName == NULL || SaveDialogResults.dirName->data == NULL || SaveDialogResults.dirName->data[0] == '\0')
			{
				int32 NewSlotIndex = FindSaveSlot();
				sprintf(NewDirName.data, SAVE_SLOT_TEMPLATE, NewSlotIndex);
				DirName = NewDirName.data;

				// NOTE: MetaData will be filled in from the save game delegate
				// For now, leave blank, which will result in system defaults if the save game delegate
				// does not provide them
			}
			else
			{
				// TRC [R4096]?
				// This is overwriting an existing save game. Confirm that this is desired before proceeding.
				// First, copy the directory name since popping up the message dialog will destroy the current SaveDialogResults field.
				FMemory::Memcpy(NewDirName.data, SaveDialogResults.dirName->data, sizeof(NewDirName));

				// Fill out the basic information for a save game dialog
				FillBasicSaveGameDialogData();

				// Make the dialog actually ask the user about the slot they just selected.  Not the default slot from FillBasicSaveGameDialogData
				SaveDialogItems.dirName = &NewDirName;
				SaveDialogItems.dirNameNum = 1;

				// Customize the dialog before display
				SaveDialogAnimParam.userOK = SCE_SAVE_DATA_DIALOG_ANIMATION_ON;
				SaveDialogAnimParam.userCancel = SCE_SAVE_DATA_DIALOG_ANIMATION_ON;

				// Customize the dialog to display a message prior to display
				SaveDialogParam.mode = SCE_SAVE_DATA_DIALOG_MODE_SYSTEM_MSG;
				SaveDialogParam.dispType = SCE_SAVE_DATA_DIALOG_TYPE_SAVE;

				FMemory::Memzero(&SaveDialogMsgParam, sizeof(SaveDialogMsgParam));
				SaveDialogMsgParam.sysMsgType = SCE_SAVE_DATA_DIALOG_SYSMSG_TYPE_OVERWRITE;

				SaveDialogParam.sysMsgParam = &SaveDialogMsgParam;
		
				// Display the dialog. The results will have been fetched by the time this function returns.
				if (!DisplayDialog())
				{
					UE_LOG(LogPS4SaveGame, Warning, TEXT("DisplayDialog() failed in SaveGame(). See previous log entries for reasons."));
					return false;
				}

				// React to the player's choice
				if (SaveDialogResults.result == SCE_COMMON_DIALOG_RESULT_OK)
				{
					// The dialog box closed normally, but check that the player said yes
					if (SaveDialogResults.buttonId == SCE_SAVE_DATA_DIALOG_BUTTON_ID_YES)
					{
						// Safe to overwrite, so process as planned
						DirName = NewDirName.data;
					}
					else 
					{
						// The player declined to overwrite, so loop through the process of asking the player for a filename again.
						continue;
					}
				}
				else if (SaveDialogResults.result == SCE_COMMON_DIALOG_RESULT_USER_CANCELED)
				{
					// The player declined to overwrite, so loop through the process of asking the player for a filename again.
					continue;
				}
				else
				{
					// This should not happen
					UE_LOG(LogPS4SaveGame, Warning, TEXT("Unknown result for 'overwrite' message: 0x%x. See prior log entries for reason(s)"), SaveDialogResults.result);
					return false;
				}
			}

			// Set this selected name as the directory to mount and break out of the loop to return success
			SetMountDirName(DirName);
			bDidSucceed = true;
			break;
		}
		else
		{
			// User canceled the save, so return failure
			return false;
		}
	}
	return bDidSucceed;
}

void FPS4SaveGameSystem::DisplaySaveError(int32 ErrorCode)
{
	DisplayError(ErrorCode, SCE_SAVE_DATA_DIALOG_TYPE_SAVE);
}

void FPS4SaveGameSystem::DisplayLoadError(int32 ErrorCode)
{
	DisplayError(ErrorCode, SCE_SAVE_DATA_DIALOG_TYPE_LOAD);
}

void FPS4SaveGameSystem::DisplayError(int32 ErrorCode, SceSaveDataDialogType DialogType)
{
	// Fill out the basic information for a save game dialog
	FillBasicSaveGameDialogData();

	// Customize the dialog to display an error message prior to invoking it
	SaveDialogParam.mode = SCE_SAVE_DATA_DIALOG_MODE_ERROR_CODE;
	SaveDialogParam.dispType = DialogType;

	FMemory::Memzero(&SaveDialogErrorParam, sizeof(SaveDialogErrorParam));
	SaveDialogErrorParam.errorCode = ErrorCode;
	SaveDialogParam.errorCodeParam = &SaveDialogErrorParam;

	// Display the dialog and wait for it to complete
	if (!DisplayDialog())
	{
		UE_LOG(LogPS4SaveGame, Warning, TEXT("DisplayDialog() failed in DisplayError(). See previous log entries for reasons."));
	}
}

bool FPS4SaveGameSystem::DisplayDeleteConfirm()
{
	return DisplayConfirm(SCE_SAVE_DATA_DIALOG_TYPE_DELETE);
}

bool FPS4SaveGameSystem::DisplaySystemMessageDialog (SceSaveDataDialogType DialogType, SceSaveDataDialogSystemMessageType MessageType)
{
	// Fill out the basic information for a save game dialog
	FillBasicSaveGameDialogData();

	// Customize the dialog before display
	SaveDialogAnimParam.userOK = SCE_SAVE_DATA_DIALOG_ANIMATION_ON;
	SaveDialogAnimParam.userCancel = SCE_SAVE_DATA_DIALOG_ANIMATION_ON;

	// Customize the dialog to display a message prior to display
	SaveDialogParam.mode = SCE_SAVE_DATA_DIALOG_MODE_SYSTEM_MSG;
	SaveDialogParam.dispType = DialogType;

	FMemory::Memzero(&SaveDialogMsgParam, sizeof(SaveDialogMsgParam));
	SaveDialogMsgParam.sysMsgType = MessageType;

	SaveDialogParam.sysMsgParam = &SaveDialogMsgParam;
		
	// Display the dialog. The results will have been fetched by the time this function returns.
	if (!DisplayDialog())
	{
		UE_LOG(LogPS4SaveGame, Warning, TEXT("DisplayDialog() failed in DisplayConfirm(). See previous log entries for reasons."));
		return false;
	}

	// Only confirm if the dialog closed via user action and that action was the yes button
	return (SaveDialogResults.result == SCE_COMMON_DIALOG_RESULT_OK && SaveDialogResults.buttonId == SCE_SAVE_DATA_DIALOG_BUTTON_ID_YES);
}

bool FPS4SaveGameSystem::DisplayConfirm(SceSaveDataDialogType DialogType)
{
	return DisplaySystemMessageDialog(DialogType, SCE_SAVE_DATA_DIALOG_SYSMSG_TYPE_CONFIRM);
}

void FPS4SaveGameSystem::DisplayCorruptDialog(SceSaveDataDialogType DialogType)
{
	DisplaySystemMessageDialog(DialogType, SCE_SAVE_DATA_DIALOG_SYSMSG_TYPE_FILE_CORRUPTED);
}

// Returns true if the dialog was successful, false if an error occured.
// NOTE: The player electing to cancel the delete is considered a success, despite nothing being deleted
bool FPS4SaveGameSystem::DisplayDeleteDialog()
{
	// Keep asking to delete a file until one is actually deleted or the player cancels the dialog
	while (true)
	{
		// Fill out the basic information for a save game dialog
		FillBasicSaveGameDialogData();

		// Customize the dialog prior to invoking it
		SaveDialogParam.mode = SCE_SAVE_DATA_DIALOG_MODE_LIST;
		SaveDialogParam.dispType = SCE_SAVE_DATA_DIALOG_TYPE_DELETE;

		// Display the dialog and wait for it to complete
		if (!DisplayDialog())
		{
			UE_LOG(LogPS4SaveGame, Warning, TEXT("DisplayDialog() failed in DisplayDeleteDialog(). See previous log entries for reasons."));
			return false;
		}

		if (SaveDialogResults.result == SCE_COMMON_DIALOG_RESULT_OK)
		{
			// Make the selected name the current directory name to mount/delete
			// NOTE: This is done before confirmation so that it can be shown in the dialog
			// and to ensure that it does not get destroyed by the confirmation call
			SetMountDirName(SaveDialogResults.dirName->data);

			// Confirm the deletion
			if (DisplayDeleteConfirm())
			{
				if (DeleteSavedGame())
				{
					// All is well. Return success
					return true;
				}
				else
				{
					// Failed to delete, so log and give up
					UE_LOG(LogPS4SaveGame, Warning, TEXT("DEBUG: DeleteSavedGame() failed in DisplayDeleteDialog(). See prior log entries for more information."));
					return false;
				}
			}
			else
			{
				// The player changed their mind about deleting that file, but this action does not mean that they
				// don't want to delete another. In other words they haven't declined to delete a file, so we need
				// to loop back and display the list dialog again
			}
		}
		else
		{
			// The player decided to cancel the delete dialog, so break out.
			// 
			break;
		}
	}

	// User choose not to delete, which is still a success
	return true;
}

//
// NOTE: This will silently overwrite the save game if the ui is not requested and the given name already exists.
//
bool FPS4SaveGameSystem::SaveGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, const TArray<uint8>& Data)
{
	// If the data is > 1GB, fail. The limit is per TRC[R4100].
	if (Data.Num() > 1024 * 1024 * 1024)
	{
		UE_LOG(LogPS4SaveGame, Error, TEXT("Failing SaveGame becaause save data is bigger than the TRC[R4100] limit."));
		return false;
	}

	bool bWasGameSaved = false;

	// Get the id of the user for whom the game will be saved
	SceUserServiceUserId Id = GetSaveGameUserId(UserIndex);
	if (!IsUserIdValid(Id))
	{
		// Can't mount because we don't have user, so say that the save game doesn't exist
		UE_LOG(LogPS4SaveGame, Warning, TEXT("Failing SaveGame because we don't have a user for mounting save data"));
		return false;
	}
	SetCurrentUser(Id);

	// Request the icon for the saved game. The delegate will return the full path to an icon file to use.
	// This code will then load the data into memory and write it back out using the appropriate Sony function.
	// The icon data must conform to the Sony requirements:
	//  * 24bit PNG (no transparency)
	//  * SCE_SAVE_DATA_ICON_WIDTH (228) wide
	//  * SCE_SAVE_DATA_ICON_HEIGHT (128) high
	//  * SCE_SAVE_DATA_ICON_FILE_MAXSIZE (116736) maximum icon file size
	// If the delegate does not return success, no icon is written and the default system icon will be used
	// NOTE: This does not delete the written icon file, so an extra copy of the icon file will live in the saved game directory and its size must
	// be factored into the save game size.
	char IconFilename[1024];
	TArray<uint8> IconFileData;
	if (GetSaveGameParameter(Name, EGameDelegates_SaveGame::Icon, IconFilename, sizeof(IconFilename)))
	{
		// Load the named file contents
		if (!FFileHelper::LoadFileToArray(IconFileData, UTF8_TO_TCHAR(IconFilename)))
		{
			UE_LOG(LogPS4SaveGame, Error, TEXT("Error reading icon file \"%s\", or icon file is not valid."), UTF8_TO_TCHAR(IconFilename));
		}
	}

	// Start a potential dialog scope for this entire function call
	{
		FDialogScope DialogScope;

		SceSaveDataParam MetaData;
		SceSaveDataMountResult MountResult;
		int32 BlockCount;
		int32 Result;

		// This will loop until the saved game directory mounts properly, the effort errors out, or the player cancels
		while (true)
		{
			// Storage for the metadata, filled out at various points through this function
			FMemory::Memzero(&MetaData, sizeof(MetaData));

			// Fetch the latest directory of saved games.
			FindSavedGames();

			// Now proceed with saving
			if (bAttemptToUseUI)
			{
				// Run the native UI for selecting from the existing saved games or creating a new one.
				// NOTE: This function call does a TRC complient flow, prompting for overwriting if an existing slot
				// is selected
				if (!SelectOrCreateSavedGame(MetaData))
				{
					return false;
				}
			}
			else
			{
				// No attempt to use the UI here.
				// Set the current directory name to mount (i.e. the save game directory) to what was passed in
				SetMountDirName(Name);
			}

			// We now have the name of a directory to either create or overwrite. Proceed with the save operations
			// Attempt to mount the configured directory

			// Determine the largest block count for the saved data by first checking with the saved game delegate, falling
			// back on a default if the delegate doesn't return a value.
			// NOTE: All saved games are the same size, which should be the biggest possible size for a saved game plus some padding for
			// the system files.
			// NOTE: SaveGameSize is specified in bytes, so it needs to be converted to blocks
			BlockCount = DEFAULT_BLOCK_COUNT;
			char SaveGameSizeStr[256];
			if (GetSaveGameParameter((const TCHAR *) NULL, EGameDelegates_SaveGame::MaxSize, SaveGameSizeStr, sizeof(SaveGameSizeStr)))
			{
				int count = atoi(SaveGameSizeStr);
				if (count != 0)
				{
					BlockCount = (count + SCE_SAVE_DATA_BLOCK_SIZE - 1) / SCE_SAVE_DATA_BLOCK_SIZE;
				}
			}
			else
			{
				// No parameter provided, so stick with the default.
				// NOTE: Can't do the code below because newly created saved games can't change their size. This means we need all the saved games
				// created as big as they will ever get in order to be able to overwrite an existing saved game.
				// blockCount = Data.Num() / SCE_SAVE_DATA_BLOCK_SIZE + EXTRA_BLOCK_COUNT + 1;
			}
		
			// Sony demands a minimum block count for saved games. Enforce that limit here
			if (BlockCount < SCE_SAVE_DATA_BLOCKS_MIN2)
			{
				BlockCount = SCE_SAVE_DATA_BLOCKS_MIN2;
			}

			// Add padding for system data
			BlockCount += EXTRA_BLOCK_COUNT;

			// NOTE: There is a bug in the PS4 system software that effectively rounds save games with block counts >= 4096 to floor(count/4096).
			// This will cause saved game writing to fail if a full size write is attempted.
			// Technote: https://ps4.scedev.net/technotes/view/213
			// To ensure that writes succeed, this rounds block counts greater than 4096 up to the nearest 4096 multiple
			// @todo Remove this code when the bug is fixed.
			if (BlockCount > 0x1000)
			{
				// Round up to the nearest multiple of 0x1000
				int remainder = (BlockCount & 0x0FFF);
				if (remainder > 0)
				{
					BlockCount += (0x1000 - remainder);
				}
			}
		
			// Fill out the mount information, indicating that we want to create the save game.
			// NOTE: This is expected to fail if the saved game already exists, which will trigger the overwriting code below
			SceSaveDataMount2 MountData;
			FillOutSceSaveDataMount(SCE_SAVE_DATA_MOUNT_MODE_RDWR | SCE_SAVE_DATA_MOUNT_MODE_CREATE, &MountData);
			MountData.blocks = BlockCount;

			// Now attempt to mount the configured directory
			FMemory::Memzero(&MountResult, sizeof(MountResult));
			Result = sceSaveDataMount2(&MountData, &MountResult);

			// If the mount failed because the directory already exists and we were trying to create it, try mounting again without the create flag
			if (Result == SCE_SAVE_DATA_ERROR_EXISTS)
			{
				// This is an overwrite situation.
				// If here, either the user explicitly requested (and confirmed) an overwrite or the
				// name was directly provided, implying a silent overwrite. Either way we can just
				// proceed with overwriting now.
				// Remount write-only so we can overwrite the data.

				MountData.mountMode = SCE_SAVE_DATA_MOUNT_MODE_RDWR;
				FMemory::Memzero(&MountResult, sizeof(MountResult));
				Result = sceSaveDataMount2(&MountData, &MountResult);

				// Fall through
			}

			// If the mount failed due to a corrupted save, deal with it here by deleting the corrupted data and creating it anew
			if (Result == SCE_SAVE_DATA_ERROR_BROKEN)
			{
				// Save data is corrupted.
				// TRC[R4096] states that the user should be notified if corrupted data is going to be deleted.
				// Deleting the data here would violate that recommendation. But there is no means of communicating
				// back to the caller a reason for failure. So to meet this recommendation would require a dialog call here.

				// This post (https://ps4.scedev.net/forums/thread/19969/) has a flow that asks for an overwrite confirmation, failing the call
				// if the player does not confirm and proceeding with the delete if they confirm.
				// We'll do this even if no UI was specified because we need to inform the player of the corruption and offer the chance to fail out.

				// However, since the TRC makes it a recommendation and not a requirement, we'll silently overwrite if the saved game was corrupted.

				if (DeleteSavedGame())
				{
					// Save game is now deleted.
					// Attempt to create the saved game again, now that the corrupted version has been deleted
					MountData.mountMode = SCE_SAVE_DATA_MOUNT_MODE_RDWR | SCE_SAVE_DATA_MOUNT_MODE_CREATE;
					FMemory::Memzero(&MountResult, sizeof(MountResult));
					Result = sceSaveDataMount2(&MountData, &MountResult);

					// Fall through
				}
				else
				{
					// Failed to delete, so log and give up
					UE_LOG(LogPS4SaveGame, Warning, TEXT("DEBUG: DeleteSavedGame() failed in SaveGame(). See prior log entries for more information."));
					return false;
				}
			}

			// If the mount failed due to lack of hard drive space, follow the specific directions from Sony
			if (/*Result == SCE_SAVE_DATA_ERROR_NO_SPACE ||*/ Result == SCE_SAVE_DATA_ERROR_NO_SPACE_FS)
			{
				// Out of storage space.
				// TRC[R4099] states that a message should be displayed via sceSaveDataDialogOpen()
				// The TRC also indicates that there are two possible legal resolutions here: continue displaying this message until
				// there is enough space, or simply fail the save and keep running.
				// The sceSaveDataDialogOpen() function just provides a message, it does not provide a means of manipulating storage,
				// so based on the API here our only recourse is to fail the save. The user will then have to go out to the PS4
				// system menu to free up storage.

				// Fill out the basic information for a save game dialog
				FillBasicSaveGameDialogData();

				// Customize the dialog to display a message prior to display
				SaveDialogParam.mode = SCE_SAVE_DATA_DIALOG_MODE_SYSTEM_MSG;
				SaveDialogParam.dispType = SCE_SAVE_DATA_DIALOG_TYPE_SAVE;

				FMemory::Memzero(&SaveDialogMsgParam, sizeof(SaveDialogMsgParam));
				SaveDialogMsgParam.sysMsgType = SCE_SAVE_DATA_DIALOG_SYSMSG_TYPE_NOSPACE;
				SaveDialogMsgParam.value = MountResult.requiredBlocks;

				// NOTE: There is some problem with the standard dialog if there is no space and there are no saved games
				// Passing in no saved game directories causes the dialog to fail. So we'll put in a garbage name to make the dialog
				// work. This duplicates the conditions that work properly in Sony's demo code.
				if (SavedGameCount <= 0)
				{
					SaveDialogItems.dirName = SavedGameDirs;
					SavedGameDirs[0].data[0] = 'A';
					SavedGameDirs[0].data[1] = '\0';
					SaveDialogItems.dirNameNum = 1;
				}

				SaveDialogParam.sysMsgParam = &SaveDialogMsgParam;
		
				// Display the dialog. The results will have been fetched by the time this function returns.
				if (!DisplayDialog())
				{
					UE_LOG(LogPS4SaveGame, Warning, TEXT("DisplayDialog() failed in SaveGame(). See previous log entries for reasons."));
					return false;
				}

				// The message only presents one button ("OK") but there is a cancel button.
				// The OK button will invoke the delete dialog, while anything else will result in a failure of the function.
				if (SaveDialogResults.result == SCE_COMMON_DIALOG_RESULT_OK)
				{
					// If there are no saved games, we can't delete anything, so just fail this call.
					if (SavedGameCount <= 0)
					{
						return false;
					}

					// If the UI flag was specified, invoke the dialog presenting the player with the option to delete saved games.
					if (!DisplayDeleteDialog())
					{
						// Failed or aborted deletion. Return failure
						// *** We don't really want to fail the save if they player elects to abort the deletion. They may have changed
						// *** their mind about overwriting and want to try a save again. In that case we should return true
						return false;
					}

					// If here, continue the loop to try and save again
					continue;
				}
				else if (SaveDialogResults.result == SCE_COMMON_DIALOG_RESULT_USER_CANCELED)
				{
					// The user cancelled the dialog, which we will interpret as the user not wanting to attempt to free save game space
					return false;
				}
				else
				{
					// This is an unknown failure, so immediately fail here instead of following the standard behavior
					UE_LOG(LogPS4SaveGame, Warning, TEXT("Fetching results for 'out of storage space' message failed. Error code: 0x%x"), Result);
					return false;
				}
			}

			// If here, then none of the above cases triggered, so exit the loop
			break;
		}

		// If we've failed here it is for a reason we can't recover from. So just log the error
		if (Result < SCE_OK)
		{
			// The mount failed, so we can't save
			UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataMount failed in SaveGame(). Error code 0x%x"), Result);
			DisplaySaveError(Result);
			return false;
		}

		// The directory is mounted, so we can write out the file. The path to write combines the mount point with the standard filename
		SceSaveDataMountPoint *MountPoint = &MountResult.mountPoint;
		FString SaveGamePath = GetSaveGamePath(MountPoint);

#if MEASURE_SAVE_BANDWIDTH
		{
			// Create a maximal buffer and write it out, measuring the time taken to arrive at a bytes/sec write speed
			TArray<uint8> hackData;
			// hackData.AddUninitialized(SCE_SAVE_DATA_BLOCK_SIZE * SCE_SAVE_DATA_BLOCKS_MIN);
			hackData.AddUninitialized(SCE_SAVE_DATA_BLOCK_SIZE * BlockCount);

			SceKernelTimespec startTime, endTime;
			sceKernelClockGettime(SCE_KERNEL_CLOCK_MONOTONIC, &startTime);
			bWasGameSaved = FFileHelper::SaveArrayToFile(hackData, *SaveGamePath);

			sceKernelClockGettime(SCE_KERNEL_CLOCK_MONOTONIC, &endTime);
			float totalTime = (endTime.tv_sec - startTime.tv_sec) + (endTime.tv_nsec - startTime.tv_nsec) / 1000000000.0f;
			printf("Save game: Bytes/sec = %.2f (%d bytes in %.2f sec)\n", hackData.Num() / totalTime, hackData.Num(), totalTime);

			// NOTE: The file that was just written will immediately be overwritten with the real data below. We wrote it here just to test
			// write speeds
		}
#endif

		// Save the data BLOB to the selected/given filename
		// NOTE: Not writing to a temporary file because doing so would require the save game directory size to be twice as large than
		// is actually required
		Result = SaveArrayToSceFile (Data, *SaveGamePath);
		bWasGameSaved = (Result == SCE_OK);

		if (!bWasGameSaved)
		{
			// We failed to write the file.
			UE_LOG(LogPS4SaveGame, Error, TEXT("Failed to write save data file %s"), *SaveGamePath);

			// Display the error code as required by TRC
			DisplaySaveError(Result);

			// Since the write failed, the save data directory is actually corrupt. However, when we unmount
			// below, the system will think that the directory is not corrupt.
			// See below for handling of game save failures.
		}
		else 
		{
			// If we have icon data, write it out using the Sony's prescribed method
			if (IconFileData.Num())
			{
				SceSaveDataIcon Icon = {};
				Icon.buf = IconFileData.GetData();
				Icon.bufSize = IconFileData.Num();
				Icon.dataSize = IconFileData.Num();  // size of the actual data in the buffer
				Result = sceSaveDataSaveIcon(MountPoint, &Icon);
				if (Result != SCE_OK)
				{
					UE_LOG(LogPS4SaveGame, Error, TEXT("sceSaveDataSaveIcon failed in SaveGame(). Error code 0x%x"), Result);

					// This is not a failure to save, so keep going
				}
			}

			// Write any delegate provided data out to the save data parameters
			// NOTE: The default values will result in all saved games looking identical so the game should really provide meaningful
			// values here
			// NOTE: There is no TRC associated with saving the data below, although there is one about how the saved games need to be
			// distinguishable from each other, and the defaults are all the same. the subtitle and detail seem like they are optional,
			// but the title seems like a requirement.
			// Regardless, we will not consider these failures to be a failure to save.

			if (GetSaveGameParameter(Name, EGameDelegates_SaveGame::Title, MetaData.title, sizeof(MetaData.title)))
			{
				Result = sceSaveDataSetParam(MountPoint, SCE_SAVE_DATA_PARAM_TYPE_TITLE, &MetaData.title, sizeof(MetaData.title));
				if (Result != SCE_OK)
				{
					UE_LOG(LogPS4SaveGame, Error, TEXT("sceSaveDataSetParam failed in SaveGame() for 'Title'. Error Code 0x%x"), Result);
				}
			}

			if (GetSaveGameParameter(Name, EGameDelegates_SaveGame::SubTitle, MetaData.subTitle, sizeof(MetaData.subTitle)))
			{
				Result = sceSaveDataSetParam(MountPoint, SCE_SAVE_DATA_PARAM_TYPE_SUB_TITLE, &MetaData.subTitle, sizeof(MetaData.subTitle));
				if (Result != SCE_OK)
				{
					UE_LOG(LogPS4SaveGame, Error, TEXT("sceSaveDataSetParam failed in SaveGame() for 'SubTitle'. Error Code 0x%x"), Result);
				}
			}

			if (GetSaveGameParameter(Name, EGameDelegates_SaveGame::Detail, MetaData.detail, sizeof(MetaData.detail)))
			{
				Result = sceSaveDataSetParam(MountPoint, SCE_SAVE_DATA_PARAM_TYPE_DETAIL, &MetaData.detail, sizeof(MetaData.detail));
				if (Result != SCE_OK)
				{
					UE_LOG(LogPS4SaveGame, Error, TEXT("sceSaveDataSetParam failed in SaveGame() for 'Detail'. Error Code 0x%x"), Result);
				}
			}
		}

		// Unmount the directory
		// NOTE: This is where the data actually gets committed. Failure to unmount will lead to corrupted save data
		Result = sceSaveDataUmount(MountPoint);
		if (Result < 0 )
		{
			UE_LOG(LogPS4SaveGame, Error, TEXT("sceSaveDataUmount failed in SaveGame(). Error code 0x%x"), Result);
			// TRC requirement to display save failure with the associated error code
			DisplaySaveError(Result);
			
			// Returning false (i.e. failed to save) here because a failure to unmount might cause data corruption
			return false;
		}

		// The only way to reach here without saving the game is due to a failed write, which will leave the save game directory
		// as valid in the Sony UI but unloadable by LoadGame(). To prevent this corrupted state corrupted
		// we'll delete the saved game in this case, as there is no way to recover. Delete the saved game without asking
		if (!bWasGameSaved)
		{
			DeleteSavedGame();
		}
	}

	// Tell the caller if the game save was successful.
	return bWasGameSaved;
}

bool FPS4SaveGameSystem::LoadGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, TArray<uint8>& Data)
{
	bool bWasGameLoaded = false;
	int32 Result;

	// The name is known, so attempt to load it by mounting the named directory
	SceUserServiceUserId Id = GetSaveGameUserId(UserIndex);
	if (!IsUserIdValid(Id))
	{
		// Can't mount because we don't have user, so say that the save game doesn't exist
		UE_LOG(LogPS4SaveGame, Warning, TEXT("Failing LoadGame because we don't have a user for mounting save data"));
		return false;
	}
	SetCurrentUser(Id);

	// Fetch the latest directory of saved games.
	FindSavedGames();

	// If there are no saved games, return immediately
	if (SavedGameCount <= 0)
	{
		UE_LOG(LogPS4SaveGame, Log, TEXT("LoadGame() called with no saved games."));
		return false;
	}

	// Start a potential dialog scope for this entire function call
	{
		FDialogScope DialogScope;

		if (bAttemptToUseUI)
		{
			// Set the save game dialog on the given user id
			FillBasicSaveGameDialogData();

			// Customize the dialog before display
			SaveDialogParam.mode = SCE_SAVE_DATA_DIALOG_MODE_LIST;
			SaveDialogParam.dispType = SCE_SAVE_DATA_DIALOG_TYPE_LOAD;

			// Display the dialog. The results will have been fetched by the time this function returns.
			if (!DisplayDialog())
			{
				UE_LOG(LogPS4SaveGame, Warning, TEXT("DisplayDialog() failed in LoadGame(). See previous log entries for reasons."));
				return false;
			}

			if (SaveDialogResults.result == SCE_COMMON_DIALOG_RESULT_OK)
			{
				if (SaveDialogResults.dirName == NULL || SaveDialogResults.dirName->data == NULL || SaveDialogResults.dirName->data[0] == '\0')
				{
					UE_LOG(LogPS4SaveGame, Warning, TEXT("Invalid directory name returned from sceSaveDataDialogGetResult() in LoadGame()"));
					return false;
				}

				// Set this selected name as the directory to mount
				SetMountDirName(SaveDialogResults.dirName->data);
			}
			else
			{
				// User canceled, so return failure
				return false;
			}
		}
		else
		{
			// Set the current directory name to mount (i.e. the save game directory) to what was passed in
			SetMountDirName(Name);
		}

		// Fill out the mount information, indicating that we want to mount read-only
		SceSaveDataMount2 MountData;
		FillOutSceSaveDataMount (SCE_SAVE_DATA_MOUNT_MODE_RDONLY, &MountData);

		// Now attempt to mount the configured directory
		SceSaveDataMountResult MountResult;
		FMemory::Memzero(&MountResult, sizeof(MountResult));
		Result = sceSaveDataMount2(&MountData, &MountResult);

		// If the mount failed because the save game doesn't exist, return failure.
		if (Result == SCE_SAVE_DATA_ERROR_NOT_FOUND)
		{
			// The mount point will exist if selected via the UI, so this will only occur when the name is passed in.
			// Therefore, simply log the occurance and fail the call
			UE_LOG(LogPS4SaveGame, Log, TEXT("sceSaveDataMount failed in LoadGame() because save file '%s' does not exist."), Name);
			return false;
		}

		// If the mount failed due to a corrupted save, deal with it here.
		if (Result == SCE_SAVE_DATA_ERROR_BROKEN)
		{
			// The given save data is corrupted.

			// This post (https://ps4.scedev.net/forums/thread/19969/) has a flow that has the load failing if corrupt, with notification
			// of the corruption but no deletion.

			// Display the corrupted file during load notification, per TRC
			DisplayCorruptDialog(SCE_SAVE_DATA_DIALOG_TYPE_LOAD);

			// Since this is a load, corrupted data is a failure to load. Log and punt.
			UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataMount failed in LoadGame() due to corrupted save data"));
			return false;
		}

		// If there is any other error mounting the save file, log it and report failure
		// NOTE: Since the failure was not that the save data directory didn't exist or was corrupt, this is an
		// unrecoverable error and therefore should be display to the player, regardless of the requested UI state.
		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4SaveGame, Log, TEXT("sceSaveDataMount failed in LoadGame(). Error code: 0x%x"), Result);
			DisplayLoadError(Result);
			return false;
		}

		// Attempt to load the save file.
		SceSaveDataMountPoint* MountPoint = &MountResult.mountPoint;
		FString SaveGamePath = GetSaveGamePath(MountPoint);
		Result = LoadSceFileToArray(Data, *SaveGamePath);
		bWasGameLoaded = (Result == SCE_OK);

		// If there was an error reading the file, report it as an error, per TRC
		if (Result != SCE_OK)
		{
			DisplayLoadError(Result);
		}

		// Unmount the directory, regardless of the success or failure of the load
		Result = sceSaveDataUmount(MountPoint);
		if (Result < 0 )
		{
			UE_LOG(LogPS4SaveGame, Error, TEXT("sceSaveDataUmount failed in LoadGame(). Error code 0x%x"), Result);
			DisplayLoadError(Result);
			
			// Returning false (i.e. failed to save) here because a failure to unmount might cause data corruption
			return false;
		}
	}

	return bWasGameLoaded;
}

//
// Implementation members
//

FPS4SaveGameSystem::FPS4SaveGameSystem() :
	bIsInitialized(false),
	bHasNativeUI(false),
	SavedGameCount(0)
{
	Initialize();
}

FPS4SaveGameSystem::~FPS4SaveGameSystem()
{
	Shutdown();
}

bool FPS4SaveGameSystem::Initialize()
{
	// Initialize the save data library
	int32 Result = sceSaveDataInitialize3(nullptr);
	if (Result < SCE_OK)
	{
		UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataInitialize() failed. Error code 0x%x"), Result);
	}
	else
	{
		bIsInitialized = true;
	}

	// Initialize the save dialog system.
	Result = sceSysmoduleLoadModule(SCE_SYSMODULE_SAVE_DATA_DIALOG);
	if (Result == SCE_OK)
	{
		// We can use the native UI functions
		bHasNativeUI = true;

		// Start up the common dialog library. 
		// NOTE: Sony states this could take a while and should be done at startup, which is why the code is here
		Result = sceCommonDialogInitialize();
		if ( Result < SCE_OK )
		{
			if (Result == SCE_COMMON_DIALOG_ERROR_ALREADY_SYSTEM_INITIALIZED)
			{
				// The only possible error at this time is that the common dialog system is already initialized,
				// so we'll just log the event as a debugging tool.
				UE_LOG(LogPS4SaveGame, Log, TEXT("sceCommonDialogInitialize() called with common dialog already initialized"));
			}
			else
			{
				// Should not be possible according to the documentations
				UE_LOG(LogPS4SaveGame, Warning, TEXT("sceCommonDialogInitialize() failed. Error code 0x%x"), Result);
			}
		}
	}
	else
	{
		UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSysmoduleLoadModule() failed. Error code 0x%x"), Result);

		// Since the UI failed to load, we'll not use it
		bHasNativeUI = false;
	}

	// Clear out the data members
	FMemory::Memzero(&UserList, sizeof(UserList));
	FMemory::Memzero(&SaveDialogAnimParam, sizeof(SaveDialogAnimParam));
	CurrentUser = SCE_USER_SERVICE_USER_ID_INVALID;

	return bIsInitialized;
}

void FPS4SaveGameSystem::Shutdown()
{
	int32 Result;
	if (bHasNativeUI)
	{
		Result = sceSysmoduleUnloadModule(SCE_SYSMODULE_SAVE_DATA_DIALOG);
		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSysmoduleUnloadModule() failed. Error code 0x%x"), Result);
		}
		bHasNativeUI = false;
	}

	if (bIsInitialized)
	{
		Result = sceSaveDataTerminate();
		if (Result < 0 )
		{
			UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataTerminate() failed. Error code 0x%x"), Result);
		}
		// NOTE: Despite the failure, say that we are no longer initialized
		bIsInitialized = false;
	}
}

void FPS4SaveGameSystem::SetCurrentUser(const SceUserServiceUserId User)
{
	// Set the given user as the one to use for subsequent calls to sceXXX functions
	if (IsUserIdValid(User))
	{
		CurrentUser = User;
	}
}

SceUserServiceUserId FPS4SaveGameSystem::GetSaveGameUserId(const int32 UserIndex)
{
	// This function implements the policy for getting the id of the user to be used for saving a game.
	// Currently the only valid policy is to ask the PS4Application based on the UserIndex	
	const FPS4Application* PS4Application = FPS4Application::GetPS4Application();
	return PS4Application->GetUserID(UserIndex);	
}

void FPS4SaveGameSystem::FillOutSceSaveDataMount(const SceSaveDataMountMode Mode, SceSaveDataMount2 *Mount)
{
	FMemory::Memzero(Mount, sizeof(SceSaveDataMount2));
	Mount->userId = CurrentUser;
	Mount->dirName = &MountDirName;
	Mount->blocks = SCE_SAVE_DATA_BLOCKS_MIN2;  // Fill out the minimum number of blocks for now. Expect caller to override
	Mount->mountMode = Mode;
}

void FPS4SaveGameSystem::SetMountDirName(const TCHAR *DirName)
{
	FTCHARToUTF8 Converter(DirName);
	const char *utf8Path = (const char *)(ANSICHAR*)Converter.Get();
	SetMountDirName(utf8Path);
}

void FPS4SaveGameSystem::SetMountDirName(const char *DirName)
{
	//Filter invalid non-alphanumeric characters from DirName while copying to MountDirName	
	int MountDirLen = sizeof(MountDirName.data);
	int DirLen = FCStringAnsi::Strlen(DirName);
	int MountDirIndex = 0;

	for(int j = 0; (j < DirLen) && (MountDirIndex < (MountDirLen - 1)); ++j)
	{
		if(FChar::IsAlnum(DirName[j]))
		{
			MountDirName.data[MountDirIndex++] = DirName[j];
		}
	}

	MountDirName.data[MountDirIndex] = '\0';
}

void FPS4SaveGameSystem::FindSavedGames()
{
	// Get all the saved games for the given user and current title id, caching them for later use
	SceSaveDataDirNameSearchCond cond;
	FMemory::Memzero(&cond, sizeof(cond));
	cond.userId = CurrentUser;
	cond.titleId = GetTitleId();
	cond.dirName = NULL;  // all files
	cond.key = SCE_SAVE_DATA_SORT_KEY_DIRNAME;
	cond.order = SCE_SAVE_DATA_SORT_ORDER_ASCENT;
 
	SceSaveDataDirNameSearchResult searchResults;
	FMemory::Memzero(&searchResults, sizeof(searchResults));
 
	searchResults.dirNames = SavedGameDirs;
	// SCE_SAVE_DATA_DIRNAME_MAX_COUNT can be the maximum number of save data directories the application will create
	searchResults.dirNamesNum = ARRAY_COUNT(SavedGameDirs);

	int32 result = sceSaveDataDirNameSearch(&cond, &searchResults);
	if (result != SCE_OK)
	{
		UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataDirNameSearch() failed in FindSavedGames(). Error code: 0x%x"), result);
		
		// Failed to search for some reason, so set the known directory names to 0
		SavedGameCount = 0;
	}
	else
	{
		SavedGameCount = searchResults.hitNum;
	}
}

// Fills out the basic save game dialog structures
void FPS4SaveGameSystem::FillBasicSaveGameDialogData()
{
	sceSaveDataDialogParamInitialize(&SaveDialogParam);

	// Make some assumptions about mode and display type, which can be overridden by the caller if needs be
	SaveDialogParam.mode = SCE_SAVE_DATA_DIALOG_MODE_LIST;
	SaveDialogParam.dispType = SCE_SAVE_DATA_DIALOG_TYPE_SAVE;

	// Fill the items with all the known saved games. This assumes that we've searched for saved games already
	FMemory::Memzero(&SaveDialogItems, sizeof(SaveDialogItems));
	SaveDialogItems.userId = CurrentUser;
	SaveDialogItems.titleId = GetTitleId();
	if (SavedGameCount > 0)
	{
		SaveDialogItems.dirName = SavedGameDirs;
		SaveDialogItems.dirNameNum = SavedGameCount;
	}

	// Fill in information about new items, if that is possible.
	// NOTE: This implements a policy of any number of saved games, limited only by the PS4 hardware and system software
	if (SavedGameCount < SCE_SAVE_DATA_DIRNAME_MAX_COUNT)
	{
		FMemory::Memzero(&SaveDialogNewItem, sizeof(SaveDialogNewItem));
//		static char defaultTitle[] = "New UE4 Save Game";
//		SaveDialogNewItem.title = defaultTitle;
		// *** TODO: Make this fetch the title from the saved game delegate, allowing the game to override the default.
		SaveDialogItems.newItem = &SaveDialogNewItem;
	}

	SaveDialogParam.items = &SaveDialogItems;

	SaveDialogParam.animParam = &SaveDialogAnimParam;
}

// Displays the currently configured dialog and returns when the dialog exits.
// Returns false if there was some issue launching the dialog, otherwise returns true
bool FPS4SaveGameSystem::DisplayDialog()
{
	//
	// Display the save game dialog
	// NOTE: This assumes that the SaveDialogParam has already been set up appropriately
	//

	// Flush rendering so that the submitdone thread can safely pump submit done
	if( IsInGameThread() )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND( FlushCommand,
		{
			GRHICommandList.GetImmediateCommandList().ImmediateFlush( EImmediateFlushType::FlushRHIThreadFlushResources );
			RHIFlushResources();
		}
		);
		FlushRenderingCommands();
	}


	int32 result = sceSaveDataDialogOpen(&SaveDialogParam);
	if (result != SCE_OK)
	{
		UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataDialogOpen() failed in DisplayDialog(). Error code: 0x%x"), result);
		return false;
	}

	// We now have to spin, polling the status of the save data dialog.
	SceCommonDialogStatus status;
	while (true)
	{
		status = sceSaveDataDialogUpdateStatus();
		if (status == SCE_COMMON_DIALOG_STATUS_NONE || status == SCE_COMMON_DIALOG_STATUS_FINISHED)
		{
			break;
		}

		// Otherwise the dialog is still running.		

		// Don't hog all the time
		sceKernelUsleep (10000);
	}

	if (status == SCE_COMMON_DIALOG_STATUS_NONE)
	{
		// We didn't even get started, so fail out
		return false;
	}

	// The dialog is finished. Now process the results.
	FMemory::Memzero(&SelectedDirName, sizeof(SelectedDirName));
	FMemory::Memzero(&DialogSaveDataParam, sizeof(DialogSaveDataParam));

	FMemory::Memzero(&SaveDialogResults, sizeof(SaveDialogResults));
	SaveDialogResults.dirName = &SelectedDirName;
	SaveDialogResults.param = &DialogSaveDataParam;

	result = sceSaveDataDialogGetResult(&SaveDialogResults);
	if (result != SCE_OK)
	{
		UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataDialogGetResult() failed in DisplayDialog(). Error code: 0x%x"), result);
		return false;
	}

	// Return that we successfully got results
	return true;
}

bool FPS4SaveGameSystem::GetSaveGameParameter (const TCHAR* SaveName, EGameDelegates_SaveGame Key, char *KeyValue, int KeyValueSize)
{
	FString Value;
	FGameDelegates::Get().GetExtendedSaveGameInfoDelegate().ExecuteIfBound(SaveName, Key, Value);
	if (Value.Len() > 0)
	{
		// We got a value, so convert to UTF8 and copy into the destination buffer
		FTCHARToUTF8 Converter(*Value);
		const char *utf8String = (const char *)(ANSICHAR*)Converter.Get();
		FCStringAnsi::Strncpy(KeyValue, utf8String, KeyValueSize);
		return true;
	}
	
	// Failed to get the value
	return false;
}

bool FPS4SaveGameSystem::GetSaveGameParameter (const char * SaveName, EGameDelegates_SaveGame Key, char * KeyValue, int KeyValueSize)
{
	FUTF8ToTCHAR Converter(SaveName);
	const TCHAR *wideSaveName = (const TCHAR *)Converter.Get();
	return GetSaveGameParameter(wideSaveName, Key, KeyValue, KeyValueSize);
}

// Deletes the save game specified in the last SetMountDirName() call
bool FPS4SaveGameSystem::DeleteSavedGame()
{
	SceSaveDataDelete del;
	FMemory::Memzero(&del, sizeof(del));
	del.userId = CurrentUser;
	del.titleId = GetTitleId();
	del.dirName = &MountDirName;

	int32 result = sceSaveDataDelete(&del);
	if ( result == SCE_OK )
	{
		// All is well, so return success
		return true;
	}

	// Failed to delete, so log and return failure
	UE_LOG(LogPS4SaveGame, Warning, TEXT("sceSaveDataDelete failed in DeleteSavedGame(). Error code 0x%x. MountDirName='%s'"), result, UTF8_TO_TCHAR(MountDirName.data));
	return false;
}

//
// Replacements for FFileHelper::SaveArrayToFile() and FFileHelper::LoadFileToArray() that
//  * Return Sony error codes instead of simple success or failure
//  * Perform the file I/O in another thread
//
// NOTE: This assumes that FIOS2 has already been initialized.

static inline int32 WaitForCompletion(SceFiosOp op)
{
	int32 result;
	if (op > 0)
	{
		while (!sceFiosOpIsDone(op))
		{
			// Don't hog all the time
			sceKernelUsleep (10000);
		}
		result = sceFiosOpGetError(op);
		sceFiosOpDelete(op);
	}
	else
	{
		// NOTE: This will always return SCE_FIOS_ERROR_BAD_OP since op is <= 0
		result = sceFiosOpGetError(op);
	}
	return result;
}

/**
 * Log an SCE error code with a human readable description
 */
static inline void LogErrorCode(int32 errorCode, const TCHAR *innerContext, const TCHAR *outerContext)
{
	if (errorCode != SCE_OK)
	{
		char errorDescBuffer[1024];
		UE_LOG(LogPS4SaveGame, Warning, TEXT("%s failed in %s. Reason: %s"), innerContext, outerContext, UTF8_TO_TCHAR(sceFiosDebugDumpError(errorCode, errorDescBuffer, sizeof(errorDescBuffer))));
	}
}

int32 FPS4SaveGameSystem::SaveArrayToSceFile(const TArray<uint8>& Array, const TCHAR *Filename)
{
	// Use FIOS2 to open the file in create mode.
	const uint8 *Data = Array.GetData();
	int32 DataLength = Array.Num();

	FTCHARToUTF8 Converter(Filename);
	const char *utf8Path = (const char *)(ANSICHAR*)Converter.Get();
	int32 result;

	// Make an FIOS2 call to write the contents of the array the given filename, then poll for completion	
	SceFiosOp op = sceFiosFileWrite(NULL, utf8Path, Data, DataLength, 0);
	result = WaitForCompletion(op);

	// DEBUG - Dump the result code here, for debugging purposes
	if (result != SCE_OK)
	{
		LogErrorCode(result, TEXT("sceFiosFileWrite()"), TEXT("SaveArrayToSceFile()"));
	}

	// Simply return the result and let the caller deal with the fallout
	return result;
}

int32 FPS4SaveGameSystem::LoadSceFileToArray(TArray<uint8>& Array, const TCHAR *Filename)
{
	FTCHARToUTF8 Converter(Filename);
	const char *utf8Path = (const char *)(ANSICHAR*)Converter.Get();

	// Make an FIOS2 call to find the size of the given file.
	int32 result = SCE_OK;

	SceFiosSize fileSize;
	fileSize = sceFiosFileGetSizeSync(NULL, utf8Path);
	if (fileSize < 0)
	{
		// The returned value is an error, so store it in the result
		result = fileSize;
	}
	else
	{
		// Reserve enough memory to hold the entire file then attempt to load it
		Array.Reset(fileSize);
		Array.SetNum(fileSize);
		uint8 *Data = Array.GetData();

		SceFiosOp op;
		op = sceFiosFileRead(NULL, utf8Path, Data, fileSize, 0);
		result = WaitForCompletion(op);
	}

	// DEBUG - Dump the result code here, for debugging purposes
	if (result != SCE_OK)
	{
		LogErrorCode(result, TEXT("sceFiosFileWrite()"), TEXT("SaveArrayToSceFile()"));
	}

	// Simply return the result and let the caller deal with the fallout
	return result;
}
