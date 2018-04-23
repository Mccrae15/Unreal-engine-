// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineUserCloudInterfacePS4.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4.h"

//
// Structure used to hold directory information
struct DirectoryEntry
{
	int8 Magic1;
	int8 Magic2;
	int8 Version;
	int8 SlotId;
	char FileName[];
};

// 'Magic' values, used to validate a directory entry
#define DE_MAGIC_1 'D'
#define DE_MAGIC_2 'E'

// Currently supported version number
#define DE_VERSION 1

FString FOnlineAsyncTaskPS4EnumerateUserFiles::ToString() const
{
	return FString::Printf(TEXT("FOnlineAsyncTaskPS4EnumerateUserFiles bWasSuccessful:%d UserId:%s"),
									bWasSuccessful, *UserId->ToDebugString());
}

void FOnlineAsyncTaskPS4EnumerateUserFiles::FetchAccessoryData()
{
	// Default to having completed the request unsuccessfully.
	bIsComplete = true;
	bWasSuccessful = false;

	// Bail if we don't have any configured slots
	if (CloudPtr->SlotCount == 0)
	{
		return;
	}

	// Create a request ID for this query
	int Result;

	int TitleContextId = CloudPtr->ContextCache.GetTitleContextId(CloudPtr->TusServiceLabel, UserId.Get());
 
	Result = sceNpTusCreateRequest(TitleContextId);
	if (Result < 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceNpTusCreateRequest() failed. Error code: 0x%08x"), Result);
		return;  // isComplete(true), wasSuccessful(false)
	}
	RequestId = Result;

	//
	// Now make the actual request
	//

	FMemory::Memzero(StatusData, sizeof(StatusData));

	// NOTE: Slot list must but 4 byte aligned. StatusData must be 8 byte aligned.
	// The default system allocation policy is that blocks >= 16 bytes are 16 byte aligned and blocks < 16
	// are 8 byte aligned. This guarantees that the data allocated 
	Result = sceNpTusGetMultiSlotDataStatusAAsync
	(
		RequestId,
		UserId->GetAccountId(),
		CloudPtr->SlotList,
		StatusData,
		sizeof(SceNpTusDataStatusA) * CloudPtr->SlotCount,
		CloudPtr->SlotCount,
		NULL
	);

	if (Result != SCE_OK)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceNpTusGetMultiSlotDataStatusAAsync() failed. Error code: 0x%08x"), Result);
		// The call was not successfully launched, so return,
		// which leaves the call complete and unsuccessful
		return;
	}

	// If here, the request was good, so we aren't complete yet.
	bIsComplete = false;
}

void FOnlineAsyncTaskPS4EnumerateUserFiles::Tick()
{
	// NOTE: Assuming here that if bIsComplete was true, Tick() is not called.

	// Poll for completion
	int32 OperationResult = 0;
	int Result = sceNpTusPollAsync(RequestId, &OperationResult);
	if (Result == 0)
	{
		// Processing is complete.
		// NOTE: OperationResult is what would have been returned if the call had been synchronous, i.e. if sceNpTusGetMultiSlotDataStatus()
		// were called. In this case it is the number of slots status values returned.

		if (OperationResult >= 0)
		{
			// Operation result is the count of the slots that have data set on them, be it accessory data or real data, i.e. that have been written to.
			// Walk the status data, adding a file entry for each one that has data.
			for (int i = 0; i < CloudPtr->SlotCount; ++i)
			{
				// Get and validate the directory information.
				SceNpTusDataStatusA &Status = StatusData[i];
				int ValidCount = 0;
				if (Status.hasData)
				{
					// Validate the directory data for this slot index.
					// While not strictly needed since the name->slot mapping is configured in the config files,
					// the information is used to detect files that have been written outside of this code
					int SlotId = CloudPtr->SlotList[i];
					DirectoryEntry *DirEntry = reinterpret_cast<DirectoryEntry *>(Status.info.data);
					if (DirEntry->Magic1 == DE_MAGIC_1 && DirEntry->Magic2 == DE_MAGIC_2)
					{
						if (DirEntry->Version == DE_VERSION)
						{
							// Ensure null termination
							Status.info.data[Status.info.infoSize-1] = 0;

							// Validated, so create a new file directory entry
							const TCHAR *Filename = UTF8_TO_TCHAR(DirEntry->FileName);
							FileDirectory.Add(FCloudFileHeaderPS4(Filename, Filename, Status.dataSize, SlotId));
						}
						else
						{
							UE_LOG_ONLINE(Warning, TEXT("UserCloud directory data version is incorrect. Want %d, got %d. Skipping entry"), DE_VERSION, DirEntry->Version);
						}
					}
					else
					{
						UE_LOG_ONLINE(Error, TEXT("Malformed UserCloud directory data. Skipping entry"));
					}

					// Early out when we've reached the returned valid count
					if (++ValidCount >= OperationResult)
					{
						break;
					}
				}
			}

			// We're done. All is well.
			bWasSuccessful = true;
		}
		else
		{
			// There was an error.
			UE_LOG_ONLINE(Error, TEXT("sceNpTusGetMultiSlotDataStatusAAsync returned an error. Error code: 0x%08x"), OperationResult);
			bWasSuccessful = false;
		}

		// Regardless of success or failure, the task is complete
		bIsComplete = true;
	}
	else if (Result == 1)
	{
		// Processing is still in progress
		bIsComplete = false;
	}
	else
	{
		// There was an error.
		UE_LOG_ONLINE(Error, TEXT("sceNpTusPollAsync failed. Error code: 0x%08x"), Result);

		// In this case, we call it complete and unsuccessful
		bIsComplete = true;
		bWasSuccessful = false;
	}
}

void FOnlineAsyncTaskPS4EnumerateUserFiles::Finalize() 
{
	// assert that we're on the game thread
	FOnlineAsyncTaskPS4::Finalize();

	// If the operation was successful, copy all the metadata from this task into the final destination
	if (WasSuccessful()) 
	{
		// Copy the received data into its file home, now that we are on the main thread again
		if (FileMetaDataPtr) 
		{
			FUniqueNetIdString id(UserId->ToString());
			// NOTE: This copies the FileDirectory array here
			if (FileMetaDataPtr->Contains(id))
			{
				(*FileMetaDataPtr)[id] = FileDirectory;
			}
			else
			{
				FileMetaDataPtr->Add(id, FileDirectory);
			}
		}
	}

	// Clean up any allocated resources
	if (RequestId >= 0)
	{
		int Result = sceNpTusDeleteRequest(RequestId);
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceNpTusDeleteRequest() failed. Error code: 0x%08x"), Result);
		}

		// Regardless of the error or not, clear out the request id
		RequestId = -1;
	}
}

void FOnlineAsyncTaskPS4EnumerateUserFiles::TriggerDelegates()
{ 
	FOnlineAsyncTaskPS4::TriggerDelegates();

	IOnlineUserCloudPtr UserCloudInterface = Subsystem->GetUserCloudInterface();
	UserCloudInterface->TriggerOnEnumerateUserFilesCompleteDelegates(bWasSuccessful, UserId.Get());
}

FString FOnlineAsyncTaskPS4ReadUserFile::ToString() const
{
	return FString::Printf(TEXT("FOnlineAsyncTaskPS4ReadUserFile bWasSuccessful:%d UserId:%s FileName:%s"),
									bWasSuccessful, *UserId->ToDebugString(), *FileName);
}

void FOnlineAsyncTaskPS4ReadUserFile::RequestFileContents()
{
	// Assume a complete failure
	bWasSuccessful = false;
	bIsComplete = true;

	// Attempt to find the slot associated with the given file
	SlotId = CloudPtr->FindSlotForFileName(UserId.Get(), FileName);
	if (SlotId <= 0)
	{
		// This is an unknown file name, so bail
		return; // isComplete(true), wasSuccess(false)
	}

	// We have a slot whose content we can fetch. Let's see if we have a cached copy.
	FCloudFilePS4 *FilePtr = CloudPtr->FindCachedFileForSlot(UserId.Get(), SlotId);
	if (FilePtr)
	{
		// We have a cached file for this slot. Store a copy of the timestamp for comparison later
		OldTimeLastChanged = FilePtr->TimeLastChanged;
	}
	else
	{
		// No cached file, so leave OldTimeLastChanged at its default of 0
	}

	// Proceed with the request
	int Result;

	TitleContextId = CloudPtr->ContextCache.GetTitleContextId(CloudPtr->TusServiceLabel, UserId.Get());
 
	Result = sceNpTusCreateRequest(TitleContextId);
	if (Result < 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceNpTusCreateRequest() failed. Error code: 0x%08x"), Result);
		return;  // isComplete(true), wasSuccessful(false)
	}
	RequestId = Result;

	// If here, the request was good, so we aren't complete yet.
	bIsComplete = false;
}

void FOnlineAsyncTaskPS4ReadUserFile::Tick()
{
	bWasSuccessful = false;

	int Result;
	if (CurrentPhase == SEND_REQUEST)
	{
		//
		// Now make the actual request
		// NOTE: We'll be making two requests - one for the status and size, and the next for the actual contents.
		// The second request is made after the READ_STATUS completes below
		//
		FMemory::Memzero(&StatusData, sizeof(StatusData));
		CurrentPhase = READ_STATUS;

		Result = sceNpTusGetDataAAsync
		(
			 RequestId,
			 UserId->GetAccountId(),
			 SlotId,
			 &StatusData,
			 sizeof(StatusData),
			 NULL,
			 0,
			 NULL
		);

		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceNpTusGetDataAAsync() failed. Error code: 0x%08x"), Result);
			// The call was not successfully launched, so return,
			// which leaves the call complete and unsuccessful
			// NOTE: The request id will be cleaned up in Finalize()
			bIsComplete = true;
			return;
		}

		// Fall through to the polling loop below. Could also just return and poll on the next Tick() call
		// but falling through will be faster on failure.
	}

	// Poll for completion
	int32 OperationResult = 0;
	Result = sceNpTusPollAsync(RequestId, &OperationResult);
	if (Result == 0)
	{
		// We completed an async operation. The next step depends on the current phase
		switch (CurrentPhase)
		{
		case READ_STATUS: // fetched status
			if (OperationResult < 0)
			{
				UE_LOG_ONLINE(Warning, TEXT("sceNpTusGetDataAAsync() returned an error. Error code: 0x%08x"), OperationResult);
				bIsComplete = true;
				return;  // isComplete(true), wasSuccessful(false)
			}

			// If there is no data for this file, complete unsuccessfully
			if (!StatusData.hasData)
			{
				bIsComplete = true;
				return;
			}

			// NOTE: If here, then the slot has data, which means that StatusData.lastChangedDate can't realistically be 0

			// If the last changed time of the file matches what we had when we started, then our cached copy
			// is still valid, so we are done.
			// If we didn't have any cached data, OldTimeLastChanged will be 0, so this compare will fail
			// and we'll fall through.
			NewTimeLastChanged = StatusData.lastChangedDate;
			if (NewTimeLastChanged.tick == OldTimeLastChanged.tick)
			{
				bIsComplete = true;
				bWasSuccessful = true;
				return;
			}

			//
			// Not cached or something has changed, so fetch the results
			//

			// Delete the request id and create another one
			Result = sceNpTusDeleteRequest(RequestId);
			if (Result != SCE_OK)
			{
				UE_LOG_ONLINE(Warning, TEXT("sceNpTusDeleteRequest() failed. Error code: 0x%08x"), Result);
			}

			// Regardless of the error or not, create a new request id
			RequestId = -1;
			Result = sceNpTusCreateRequest(TitleContextId);
			if (Result < 0)
			{
				UE_LOG_ONLINE(Warning, TEXT("sceNpTusCreateRequest() failed. Error code: 0x%08x"), Result);
				bIsComplete = true;
				return;  // isComplete(true), wasSuccessful(false)
			}
			RequestId = Result;

			// Make the new request
			FileData.AddZeroed(StatusData.dataSize);

			Result = sceNpTusGetDataAAsync
			(
				 RequestId,
				 UserId->GetAccountId(),
				 SlotId,
				 &StatusData,
				 sizeof(StatusData),
				 FileData.GetData(),
				 FileData.Num(),
				 NULL
			);

			if (Result != SCE_OK)
			{
				UE_LOG_ONLINE(Warning, TEXT("sceNpTusGetDataAAsync() failed. Error code: 0x%08x"), Result);
				// The call was not successfully launched, so return
				// NOTE: The request id will be cleaned up in Finalize()
				bIsComplete = true;
				return;  // isComplete(true), wasSuccessful(false)
			}
			CurrentPhase = READ_CONTENTS;
			break;

		case READ_CONTENTS: // fetched content
			if (OperationResult < 0)
			{
				UE_LOG_ONLINE(Warning, TEXT("sceNpTusGetDataAAsync() returned an error. Error code: 0x%08x"), OperationResult);
				bIsComplete = true;
				return;  // isComplete(true), wasSuccessful(false)
			}

			// We should now have the data from the file in the local array. OperationResult holds the count of bytes received,
			// which should be exactly what we asked for.
			check(OperationResult == FileData.Num());
			
			// We'll hold on to the data for now, marshalling it over to its final resting place in the Finalize() function.
			bIsComplete = true;
			bWasSuccessful = true;
			break;
		}
	}
	else if (Result < 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceNpTusPollAsync() failed. Error code: 0x%08x"), Result);
		bIsComplete = true;
	}
}

void FOnlineAsyncTaskPS4ReadUserFile::Finalize() 
{
	// assert that we're on the game thread
	FOnlineAsyncTaskPS4::Finalize();

	// If the operation was successful, copy all the metadata from this task into the final destination
	if (WasSuccessful()) 
	{
		if (NewTimeLastChanged.tick == OldTimeLastChanged.tick)
		{
			// Nothing changed since the last time we fetched data, so the cached data is valid. Don't change
			// anything.
		}
		else
		{
			// Find or create the cached file, then fill in its new data
			FCloudFilePS4 *FilePtr = CloudPtr->FindCachedFileForSlot(UserId.Get(), SlotId, true);
			FilePtr->Data = FileData;
			FilePtr->FileName = FileName;
			FilePtr->SlotId = SlotId;
			FilePtr->TimeLastChanged = NewTimeLastChanged;
			FilePtr->AsyncState = EOnlineAsyncTaskState::Done;
		}
	}

	// Clean up any allocated resources
	if (RequestId >= 0)
	{
		int Result = sceNpTusDeleteRequest(RequestId);
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceNpTusDeleteRequest() failed. Error code: 0x%08x"), Result);
		}

		// Regardless of the error or not, clear out the request id
		RequestId = -1;
	}
}

void FOnlineAsyncTaskPS4ReadUserFile::TriggerDelegates()
{ 
	FOnlineAsyncTaskPS4::TriggerDelegates();

	IOnlineUserCloudPtr UserCloudInterface = Subsystem->GetUserCloudInterface();
	UserCloudInterface->TriggerOnReadUserFileCompleteDelegates(bWasSuccessful, UserId.Get(), FileName);
}

void FOnlineAsyncTaskPS4WriteUserFile::WriteUserFile(const FUniqueNetId& InUserId, const FString& InFileToWrite, const TArray<uint8>& InContents)
{
	// Assume we'll fail
	bIsComplete = true;
	bWasSuccessful = false;

	// Find the slot associated with this filename, if any
	SlotId = CloudPtr->FindSlotForFileName(InUserId, InFileToWrite);
	if (SlotId < 0)
	{
		// Don't have a valid slot, so bail
		return;  // bIsComplete(true), bWasSuccessful(false)
	}

	// Make copies of the arguments in local storage
	UserId = FUniqueNetIdPS4::Cast(InUserId.AsShared());
	FileName = InFileToWrite;
	Contents = InContents;

	int TitleContextId = CloudPtr->ContextCache.GetTitleContextId(CloudPtr->TusServiceLabel, UserId.Get());
	
	int Result = sceNpTusCreateRequest(TitleContextId);
	if (Result < 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceNpTusCreateRequest failed for filename \"%s\". Error code: 0x%08x"), *FileName, Result);
		return;  // bIsComplete(true), bWasSuccessful(false)
	}
	RequestId = Result;

	// Format the filename and slot into the directory entry
	FMemory::Memzero(&RequestInfo, sizeof(RequestInfo));

	// BEGIN directory entries based on accessory data
	DirectoryEntry *DirEntry = reinterpret_cast<DirectoryEntry *>(RequestInfo.data);

	DirEntry->Magic1 = DE_MAGIC_1;
	DirEntry->Magic2 = DE_MAGIC_2;
	DirEntry->Version = DE_VERSION;
	DirEntry->SlotId = SlotId;

	char *UTFFileName = TCHAR_TO_UTF8(*FileName);
	int NameLength = strlen(UTFFileName)+1;
	
	int ByteCount = NameLength + sizeof(DirectoryEntry);
	if (ByteCount > SCE_NP_TUS_DATA_INFO_MAX_SIZE)
	{
		NameLength = SCE_NP_TUS_DATA_INFO_MAX_SIZE - sizeof(DirectoryEntry);
		ByteCount = SCE_NP_TUS_DATA_INFO_MAX_SIZE;
	}
	FMemory::Memcpy(DirEntry->FileName, UTFFileName, NameLength);
	DirEntry->FileName[NameLength-1] = '\0';
	RequestInfo.infoSize = ByteCount;
	// END directory entries based on accessory data

	// Successful so far, so not complete
	bIsComplete = false;

	//PS4Subsystem->GetUserCloudInterface()->DumpCloudFileState(UserId, FileToWrite);
}

FString FOnlineAsyncTaskPS4WriteUserFile::ToString() const
{
	return FString::Printf(TEXT("FOnlineAsyncTaskPS4WriteUserFile bWasSuccessful:%d UserId:%s FileName:%s"),
									bWasSuccessful, *UserId->ToDebugString(), *FileName);
}

void FOnlineAsyncTaskPS4WriteUserFile::Tick()
{
	bWasSuccessful = false;

	int Result;

	// NOTE: Assuming here that if bIsComplete was true, Tick() is not called.
	// If the request has not yet been sent, send it now
	if (!bWasRequestSent)
	{
		// Now make the actual write call.
		Result = sceNpTusSetDataAAsync
		(
			RequestId,
			UserId->GetAccountId(),
			SlotId,
			Contents.Num(),
			Contents.Num(),
			Contents.GetData(),
			&RequestInfo,
			sizeof(RequestInfo),
			NULL,
			NULL,
			NULL
		);

		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceNpTusSetDataAAsync failed for filename \"%s\". Error code: 0x%08x"), *FileName, Result);
			bIsComplete = true;
			return;
		}

		bWasRequestSent = true;

		// Fall through to the poll below instead of waiting for the next tick. This will allow failure cases to fail faster.
	}

	// Poll for completion
	int32 OperationResult = 0;
	Result = sceNpTusPollAsync(RequestId, &OperationResult);
	if (Result == 0)
	{
		// Processing is complete.
		// NOTE: OperationResult is what would have been returned if the call had been synchronous, i.e. if sceNpTusSetData()
		// were called.

		if (OperationResult >= 0)
		{
			bWasSuccessful = true;
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("sceNpTusSetDataAAsync failed() for filename \"%s\". Error code: 0x%08x"), *FileName, OperationResult);
			bWasSuccessful = false;
		}

		// Going to be complete no matter what
		bIsComplete = true;
	}
	else if (Result < 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceNpTusPollAsync failed() for filename \"%s\". Error code: 0x%08x"), *FileName, Result);
		bWasSuccessful = false;
		bIsComplete = true;
	}
}

void FOnlineAsyncTaskPS4WriteUserFile::Finalize()
{
	FOnlineAsyncTaskPS4::Finalize();

	// Clean up allocated resources
	// NOTE: Cleaning up here so we are properly cleaned up even if Tick() is never called
	Contents.Empty();

	if (RequestId >= 0)
	{
		int Result = sceNpTusDeleteRequest(RequestId);
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceNpTusDeleteRequest failed for filename \"%s\". Error code: 0x%08x"), *FileName, Result);
		}

		// Regardless of the error or not, clear out the request id
		RequestId = -1;
	}
}

void FOnlineAsyncTaskPS4WriteUserFile::TriggerDelegates()
{ 
	FOnlineAsyncTaskPS4::TriggerDelegates();

	IOnlineUserCloudPtr UserCloudInterface = Subsystem->GetUserCloudInterface();
	UserCloudInterface->TriggerOnWriteUserFileCompleteDelegates(bWasSuccessful, UserId.Get(), FileName);
}

//
// UserCloud interface implementation
//

FOnlineUserCloudPS4::~FOnlineUserCloudPS4()
{
	// ClearFiles();
	Shutdown();
}

void FOnlineUserCloudPS4::ConfigureSlots()
{
	SlotsByFileName.Empty();

	// Fetch the configured list of files
	TArray<FString> SlotSpecs;
	int Count = GConfig->GetArray( TEXT("OnlineSubsystemPS4.UserCloud"), TEXT("SlotSpecs"), SlotSpecs, GEngineIni );
	if (Count == 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("No TUS slots configured. Edit PS4Engine.ini after configuring slots in the SMT for the Title User Storage"));
	}

	// We have some file specifications. They should be of the syntax: Filename SLOT=slotId
	// Parse each out to create a list of slots
	SlotCount = 0;
	for (int i = 0; i < Count; ++i)
	{
		SlotSpecs[i].TrimStartInline();
		const TCHAR *Spec = *SlotSpecs[i];
		FString FileName = FParse::Token(Spec, false);
		int SlotId;
		if (FParse::Value(Spec, TEXT("SLOT="), SlotId))
		{
			// SlotId and FileName are now valid. Store them into the mapping
			SlotsByFileName.Add(FileName, SlotId);

			// Add to the slot list and increment the slot count
			SlotList[SlotCount++] = SlotId;
		}
	}

	// By now, SlotCount is the number of valid slots, SlotList contains the validly configured slots indexed from 0 to SlotCount-1,
	// and SlotsByFilename maps filenames onto SlotId values
}

bool FOnlineUserCloudPS4::Init()
{
	int Result = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TUS);
	if ( Result != SCE_OK )
	{
		UE_LOG_ONLINE(Warning, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TUS) failed. Error code: 0x%08x"), Result);
		return false;
	}

	// The context functions aren't valid until the module is loaded, so set the functions into the context cache now
	ContextCache.SetFunctions(sceNpTusCreateNpTitleCtxA, sceNpTusDeleteNpTitleCtx);

	// Fetch the service label to use
	int32 LabelValue;
	if (!GConfig->GetInt(TEXT("OnlineSubsystemPS4.UserCloud"),TEXT("ServiceLabel"), LabelValue, GEngineIni))
	{
		LabelValue = 0;
	}
	TusServiceLabel = LabelValue;

	// Setup the slot mappings from the configuration file. See PS4Engine.ini
	ConfigureSlots();

	return true;
}

void FOnlineUserCloudPS4::Shutdown()
{
	ContextCache.DestroyAll();

	int Result = sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TUS);
	if (Result != SCE_OK )
	{
		UE_LOG_ONLINE(Warning, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TUS) failed. Error code: 0x%08x"), Result);
	}
}

int FOnlineUserCloudPS4::FindSlotForFileName(const FUniqueNetId& UserId, const FString& FileName)
{
	// Attempt to find the given file for the given user in the MetaData.
	// NOTE: This will require either config-based meta-data or that an enumeration have happened prior
	int SlotId = -1;

	// Static, global mapping version
	if (SlotsByFileName.Contains(FileName))
	{
		SlotId = SlotsByFileName[FileName];
	}

	// Return the found slot
	return SlotId;
}

FCloudFilePS4 *FOnlineUserCloudPS4::FindCachedFileForSlot(const FUniqueNetId& UserId, int SlotId, bool CreateIfNeeded)
{
	FCloudFilePS4 *FilePtr = NULL;
	if (SlotId > 0)
	{
		// Get the file set for this user, creating an empty set if the user doesn't have one yet
		FUniqueNetIdString IdString(UserId.ToString());
		FCloudFileArray *FileSetPtr = NULL;
		if (FileData.Contains(IdString))
		{
			FileSetPtr = &(FileData[IdString]);
		}
		else
		{
			FileSetPtr = &(FileData.Add(IdString, FCloudFileArray()));
		}

		for (FCloudFileArray::TIterator It(*FileSetPtr); It; It++)
		{
			FCloudFilePS4 &File = *(It);
			if (File.SlotId == SlotId)
			{
				FilePtr = &File;
				break;
			}
		}

		// We didn't find a file, so if requested, create one
		if (!FilePtr && CreateIfNeeded)
		{
			int Index = FileSetPtr->Add(FCloudFilePS4("", SlotId));
			FilePtr = &((*FileSetPtr)[Index]);
		}
	}
	return FilePtr;
}

bool FOnlineUserCloudPS4::GetFileContents(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents)
{
	// Get the id of the slot mapped to this filename, failing if it can't be found
	int SlotId = FindSlotForFileName(UserId, FileName);

	if (SlotId < 0)
	{
		return false;
	}

	// Fetch the cached file for this slot
	FCloudFile *FilePtr = FindCachedFileForSlot(UserId, SlotId);
	if (!FilePtr)
	{
		return false;
	}

	// Copy over the contents
	FileContents.Empty();
	FileContents = FilePtr->Data;
	return true;
}

bool FOnlineUserCloudPS4::ClearFiles(const FUniqueNetId& UserId)
{
	// Clear all the cached files for this user
	// NOTE: This won't stop any fetch operations that are currently in progress.

	// Clear the array for this user, if the user has an array of files to clear.
	FUniqueNetIdString IdString(UserId.ToString());
	FCloudFileArray *FileSetPtr = NULL;
	if (FileData.Contains(IdString))
	{
		FileSetPtr = &(FileData[IdString]);
		FileSetPtr->Empty();
	}

	// NOTE: Return true regardless of if the user was valid or not. We don't care if
	// there weren't any files in need of clearing, only if there was a failure to clear
	return true;
}

bool FOnlineUserCloudPS4::ClearFile(const FUniqueNetId& UserId, const FString& FileName)
{
	// Remove this one file from the cached file set
	// NOTE: This won't stop a read operation in progress on this filename
	FUniqueNetIdString IdString(UserId.ToString());
	FCloudFileArray *FileSetPtr = NULL;
	if (FileData.Contains(IdString))
	{
		FileSetPtr = &(FileData[IdString]);
		for (FCloudFileArray::TIterator It(*FileSetPtr); It; It++)
		{
			FCloudFilePS4 &File = *(It);
			if (File.FileName.Equals(FileName, ESearchCase::IgnoreCase))
			{
				FileSetPtr->RemoveAt(It.GetIndex());
				break;
			}
		}
	}

	// NOTE: Return true regardless of if the file exists or not. The only way to return false
	// would be in we failed to delete, and not deleting a non-existing file is not a failure
	return true;
}

void FOnlineUserCloudPS4::EnumerateUserFiles(const FUniqueNetId& UserId)
{
	// Create the request for enumeration and then queue the task so it will get polled
	FOnlineAsyncTaskPS4EnumerateUserFiles *Task = new FOnlineAsyncTaskPS4EnumerateUserFiles(PS4Subsystem, this, FUniqueNetIdPS4::Cast(UserId), &FileMetaData);
	PS4Subsystem->GetAsyncTaskManager()->AddToInQueue( Task );
}

void FOnlineUserCloudPS4::GetUserFileList(const FUniqueNetId& UserId, TArray<FCloudFileHeader>& UserFiles)
{
	// Return the set of files that have been enumerated
	FUniqueNetIdString IdString(UserId.ToString());
	UserFiles.Empty();

	// If we have enumeration data for the given UserId...
	if (FileMetaData.Contains(IdString))
	{
		// NOTE: Not clearing the UserFiles list here in case the caller wants the files appended.
		FCloudFileHeaderArray &FileDirectory = FileMetaData[IdString];
		for (int i = 0; i < FileDirectory.Num(); ++i)
		{
			FCloudFileHeaderPS4 &Entry = FileDirectory[i];
			UserFiles.Add(FCloudFileHeader(Entry.FileName, Entry.DLName, Entry.FileSize));
		}
	}
}

bool FOnlineUserCloudPS4::ReadUserFile(const FUniqueNetId& UserId, const FString& FileName)
{
	FOnlineAsyncTaskPS4ReadUserFile *Task =
		new FOnlineAsyncTaskPS4ReadUserFile(PS4Subsystem, this, FUniqueNetIdPS4::Cast(UserId), FileName);
	PS4Subsystem->GetAsyncTaskManager()->AddToInQueue( Task );
	return true;
}

bool FOnlineUserCloudPS4::WriteUserFile(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents)
{
	// Create the request for writing and then queue it
	FOnlineAsyncTaskPS4WriteUserFile *Task =
		new FOnlineAsyncTaskPS4WriteUserFile(PS4Subsystem, this, FUniqueNetIdPS4::Cast(UserId), FileName, FileContents);
	PS4Subsystem->GetAsyncTaskManager()->AddToInQueue( Task );
	return true;
}

void FOnlineUserCloudPS4::CancelWriteUserFile(const FUniqueNetId& UserId, const FString& FileName)
{
	// Not implemented
}


bool FOnlineUserCloudPS4::DeleteUserFile(const FUniqueNetId& UserId, const FString& FileName, bool bShouldCloudDelete, bool bShouldLocallyDelete)
{
	FOnlineAsyncTaskPS4DeleteUserFile *Task = 
		new FOnlineAsyncTaskPS4DeleteUserFile(PS4Subsystem, this, FUniqueNetIdPS4::Cast(UserId), FileName, bShouldCloudDelete, bShouldLocallyDelete);
	PS4Subsystem->GetAsyncTaskManager()->AddToInQueue( Task );
	return true;
}

bool FOnlineUserCloudPS4::RequestUsageInfo(const FUniqueNetId& UserId)
{
	// Not implemented on PS4 platform
	return false;
}

void FOnlineUserCloudPS4::DumpCloudState(const FUniqueNetId& UserId)
{
	UE_LOG_ONLINE(Verbose, TEXT("PS4 UserCloud has %d files configured"), SlotCount);

	// Get the file set for this user
	FUniqueNetIdString IdString(UserId.ToString());
	FCloudFileArray *FileSetPtr = NULL;
	if (FileData.Contains(IdString))
	{
		FileSetPtr = &(FileData[IdString]);
	}
	else
	{
		UE_LOG_ONLINE(Verbose, TEXT("User does not have any cached files"));
	}

	if (FileSetPtr)
	{
		UE_LOG_ONLINE(Verbose, TEXT("User has %d cached files."), FileSetPtr->Num());
	}
}

void FOnlineUserCloudPS4::DumpCloudFileState(const FUniqueNetId& UserId, const FString& FileName)
{
	if (FileName.Len() > 0)
	{
		UE_LOG_ONLINE(Log, TEXT("Cloud File State file %s:"), *FileName);
		{
			SceNpTusSlotId SlotId = FindSlotForFileName(UserId, FileName);
			if (SlotId <= 0)
			{
				UE_LOG_ONLINE(Log, TEXT("\tFile is not configured!"));
			}
			else
			{
				UE_LOG_ONLINE(Log, TEXT("\tFile is configured for slot %d"), SlotId);
			}

			FCloudFilePS4 *FilePtr = FindCachedFileForSlot(UserId, SlotId);
			if (FilePtr)
			{
				UE_LOG_ONLINE(Log, TEXT("\tFileCache: Filename:%s  CacheSize:%d  SlotId:%d  LastModifiedTick: %#lx"), 
					*FilePtr->FileName, FilePtr->Data.Num(), FilePtr->SlotId, FilePtr->TimeLastChanged.tick);
			}
			else
			{
				UE_LOG_ONLINE(Log, TEXT("\tNo cached content found!"));
			}
		}
	}
}

FString FOnlineAsyncTaskPS4DeleteUserFile::ToString() const
{
	return FString::Printf(TEXT("FOnlineAsyncTaskPS4DeleteUserFile bWasSuccessful:%d UserId:%s FileName:%s"),
								bWasSuccessful, *UserId->ToDebugString(), *FileName);
}

void FOnlineAsyncTaskPS4DeleteUserFile::IssueDeleteRequest()
{
	// Set up a failure state, to be overwritten on success below
	bIsComplete = true;
	bWasSuccessful = false;

	// Fetch the slot id for the filename
	SceNpTusSlotId SlotId = CloudPtr->FindSlotForFileName(UserId.Get(), FileName);
	if (SlotId < 0)
	{
		// Completed unsuccessfully
		return;
	}

	// NOTE: Not caching on local storage, so ignore the bit about deleting locally. But consider it to
	// be a successful completion if local is specified without cloud
	if (!bShouldCloudDelete)
	{
		bWasSuccessful = true;
		return;
	}

	// If here, we need to delete a cloud instance of the file
	int TitleContextId = CloudPtr->ContextCache.GetTitleContextId(CloudPtr->TusServiceLabel, UserId.Get());
	
	int Result = sceNpTusCreateRequest(TitleContextId);
	if (Result < 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceNpTusCreateRequest failed for filename \"%s\". Error code: 0x%08x"), *FileName, Result);
		return;  // isComplete(true), wasSuccessful(false)
	}
	RequestId = Result;

	// Make the call to delete the data for the given slot
	Result = sceNpTusDeleteMultiSlotDataAAsync
	(
		RequestId,
		UserId->GetAccountId(),
		&SlotId,
		1,
		NULL
	);

	if (Result != SCE_OK)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceNpTusDeleteMultiSlotDataAAsync() failed for file \"%s\". Error code: 0x%08x"), *FileName, Result);
		return;  // isComplete(true), wasSuccessful(false)
	}

	// All is well
	bIsComplete = false;
}

void FOnlineAsyncTaskPS4DeleteUserFile::Tick()
{
	bWasSuccessful = false;

	// Poll for completion
	int32 OperationResult = 0;
	int Result = sceNpTusPollAsync(RequestId, &OperationResult);
	if (Result == 0)
	{
		// Processing is complete.
		// NOTE: OperationResult is what would have been returned if the call had been synchronous, i.e. if sceNpTusDeleteMultiSlotData()
		// were called.

		if (OperationResult == SCE_OK)
		{
			bWasSuccessful = true;
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("sceNpTusPollAsync failed() for deleting filename \"%s\". Error code: 0x%08x"), *FileName, OperationResult);
			bWasSuccessful = false;
		}

		// Going to be complete no matter what
		bIsComplete = true;
	}
	else if (Result < 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceNpTusPollAsync failed() for filename \"%s\". Error code: 0x%08x"), *FileName, Result);
		bIsComplete = true;
		bWasSuccessful = false;
	}
}

void FOnlineAsyncTaskPS4DeleteUserFile::Finalize()
{
	FOnlineAsyncTaskPS4::Finalize();

	// Clean up allocated resources
	// NOTE: Cleaning up here so we are properly cleaned up even if Tick() is never called

	if (RequestId >= 0)
	{
		int Result = sceNpTusDeleteRequest(RequestId);
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceNpTusDeleteRequest failed for filename \"%s\". Error code: 0x%08x"), *FileName, Result);
		}

		// Regardless of the error or not, clear out the request id
		RequestId = -1;
	}
}

void FOnlineAsyncTaskPS4DeleteUserFile::TriggerDelegates()
{ 
	FOnlineAsyncTaskPS4::TriggerDelegates();

	IOnlineUserCloudPtr UserCloudInterface = Subsystem->GetUserCloudInterface();
	UserCloudInterface->TriggerOnDeleteUserFileCompleteDelegates(bWasSuccessful, UserId.Get(), FileName);
}
