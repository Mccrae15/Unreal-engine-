// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
 
#include "OnlineSubsystemPS4Private.h"
#include "OnlineUserCloudInterface.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineSubsystemPS4Package.h"

// There can be only MAX_USER_FILES (16) configured TUS slots, per PS4 TUS documentation.
#define MAX_USER_FILES 16

// Specialized versions of the file header and file, customized for the PS4 user cloud implementation
struct FCloudFileHeaderPS4 : public FCloudFileHeader
{
	int SlotId;

	FCloudFileHeaderPS4(const FString& InFileName, const FString& InDLName, int32 InFileSize, int InSlotId) :
		FCloudFileHeader(InFileName, InDLName, InFileSize),
		SlotId(InSlotId)
	{
	}
};

struct FCloudFilePS4 : public FCloudFile
{
	int        SlotId;
	SceRtcTick TimeLastChanged;

	FCloudFilePS4 (const FString& InFileName, int InSlotId) :
		FCloudFile(InFileName),
		SlotId(InSlotId)
	{
		TimeLastChanged.tick = 0L;
	}
};

// NOTE: These arrays are fixed length, since each can be only MAX_USER_FILES (16) long, per the PS4 Tus libary.
// NOTE: These are TArray instead of regular C++ arrays because the FCloudFile structures don't lend themselves to a shallow copy
typedef TArray<FCloudFileHeaderPS4> FCloudFileHeaderArray;
typedef TArray<FCloudFilePS4> FCloudFileArray;

/** Mapping from user id to list of enumerated file headers */
typedef TMap<FUniqueNetIdString, FCloudFileHeaderArray > FCloudFileHeaderMap;
/** Mapping from user id to list of loaded files */
typedef TMap<FUniqueNetIdString, FCloudFileArray > FCloudFileMap;
/** Mapping from filename to slot id, statically for all users */
typedef TMap<FString, SceNpTusSlotId> FSlotMapping;

/** 
 *  Async task for enumerating all cloud files for a given user
 */
class FOnlineAsyncTaskPS4EnumerateUserFiles : public FOnlineAsyncTaskPS4
{
	/** UserId for file enumeration */
	TSharedRef<FUniqueNetIdPS4 const> UserId;
	
	/** The entire enumerated list of file data, stored locally */
	FCloudFileHeaderArray FileDirectory;

	/** The destination for storing the resulting data in the finalize method */
	FCloudFileHeaderMap *FileMetaDataPtr;

	/** The request id used for fetching the data */
	int RequestId;

	/** The user cloud implementation that issued this enumeration task */
	class FOnlineUserCloudPS4 *CloudPtr;

	/** Storage for the accessory query results*/
	SceNpTusDataStatusA StatusData[MAX_USER_FILES];

	/** Hidden on purpose */
	FOnlineAsyncTaskPS4EnumerateUserFiles() = delete;

	/** Calls the PS4 API to fetch the accessory data */
	void FetchAccessoryData();

public:
	FOnlineAsyncTaskPS4EnumerateUserFiles(class FOnlineSubsystemPS4* InSubsystem, class FOnlineUserCloudPS4 *InCloud, const FUniqueNetIdPS4& InUserId, FCloudFileHeaderMap* OutMetaData ) :
		FOnlineAsyncTaskPS4(InSubsystem),
		UserId(InUserId.AsShared()),
		FileMetaDataPtr(OutMetaData),
		RequestId(-1),
		CloudPtr(InCloud)
	{
		FetchAccessoryData();
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;
	
	/**
	 * Marshal the accumlated data into its final home
	 */
	virtual void Finalize() override;

	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

/** 
 *  Async task for reading into memory a single cloud files for a given user
 */
class FOnlineAsyncTaskPS4ReadUserFile : public FOnlineAsyncTaskPS4
{
PACKAGE_SCOPE:

	/** UserId making the request */
	TSharedRef<FUniqueNetIdPS4 const> UserId;
	
	/** Filename shared */
	FString FileName;

	/** Local storage space for the file contents */
	TArray<uint8> FileData;

	/** The request id used for fetching the data */
	int RequestId;

	/** The user cloud implementation that issued this task */
	class FOnlineUserCloudPS4 *CloudPtr;

	/** Destination for the enumeration phase */
	SceNpTusDataStatusA StatusData;

	/** Currently active phase, since the operation has two phases */
	enum ReadPhase
	{
		SEND_REQUEST,
		READ_STATUS,
		READ_CONTENTS
	};
	ReadPhase CurrentPhase;

	/** Title context id, cached since we make multiple requests */
	int TitleContextId;

	/** The id of the slot being read */
	SceNpTusSlotId SlotId;

	/** The timestamp of the last change to the file at the start of the task, 0 if unknown */
	SceRtcTick OldTimeLastChanged;

	/** The timestamp of the last change to the file after the initial query */
	SceRtcTick NewTimeLastChanged;


	/** Hidden on purpose */
	FOnlineAsyncTaskPS4ReadUserFile() = delete;

	void RequestFileContents();

public:

	FOnlineAsyncTaskPS4ReadUserFile(class FOnlineSubsystemPS4* InSubsystem, class FOnlineUserCloudPS4 *InCloud, const FUniqueNetIdPS4& InUserId, const FString& InFileName) :
		FOnlineAsyncTaskPS4(InSubsystem),
		UserId(InUserId.AsShared()), 
		FileName(InFileName),
		RequestId(-1),
		CloudPtr(InCloud),
		CurrentPhase(SEND_REQUEST),
		SlotId(0)
	{
		OldTimeLastChanged.tick = 0L;
		NewTimeLastChanged.tick = 0L;
		RequestFileContents();
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;
	
	/**
	 * Clean up resources
	 */
	virtual void Finalize() override;

	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

/** 
 *  Async task for writing a single cloud file to disk for a given user
 */
class FOnlineAsyncTaskPS4WriteUserFile : public FOnlineAsyncTaskPS4
{
PACKAGE_SCOPE:

	/** Copy of the data to write */
	TArray<uint8> Contents;
	/** UserId making the request */
	TSharedRef<FUniqueNetIdPS4 const> UserId;
	/** File being written */
	FString FileName;

	/** Data used to make the request */
	SceNpTusDataInfo RequestInfo;

	/** The destination slot being written to */
	SceNpTusSlotId SlotId;

	/** The request id used for fetching the data */
	int RequestId;

	/** Flag indicating that the request has been sent */
	bool bWasRequestSent;

	/** The user cloud implementation that issued this enumeration task */
	class FOnlineUserCloudPS4 *CloudPtr;

	/** Hidden on purpose */
	FOnlineAsyncTaskPS4WriteUserFile() = delete;

	/**
	 * Write the specified user file to the network platform's file store
	 *
	 * @param UserId User owning the storage
	 * @param FileToWrite the name of the file to write
	 * @param FileContents the out buffer to copy the data into
	 */
	void WriteUserFile(const FUniqueNetId& InUserId, const FString& InFileToWrite, const TArray<uint8>& InContents);

public:

	FOnlineAsyncTaskPS4WriteUserFile(class FOnlineSubsystemPS4* InSubsystem,  class FOnlineUserCloudPS4 *InCloud, const FUniqueNetIdPS4& InUserId, const FString& InFileName, const TArray<uint8>& InContents) :
		FOnlineAsyncTaskPS4(InSubsystem),
		UserId(InUserId.AsShared()),
		bWasRequestSent(false),
		CloudPtr(InCloud)
	{
		WriteUserFile(InUserId, InFileName, InContents);
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;
	
	/**
	 * Clean up resources
	 */
	virtual void Finalize() override;

	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

/** 
 *  Async task for deleting a single cloud file for a given user
 */
class FOnlineAsyncTaskPS4DeleteUserFile : public FOnlineAsyncTaskPS4
{
	/** Should the file be deleted from the cloud record */
	bool bShouldCloudDelete;
	/** Should the local copy of the file be deleted */
	bool bShouldLocallyDelete;
	/** UserId making the request */
	TSharedRef<FUniqueNetIdPS4 const> UserId;
	/** File being deleted */
	FString FileName;

	/** The request id for the call */
	int RequestId;

	/** The user cloud implementation that issued this enumeration task */
	class FOnlineUserCloudPS4 *CloudPtr;

	/** Hidden on purpose */
	FOnlineAsyncTaskPS4DeleteUserFile() = delete;

	void IssueDeleteRequest();

public:
	FOnlineAsyncTaskPS4DeleteUserFile(class FOnlineSubsystemPS4* InSubsystem, class FOnlineUserCloudPS4 *InCloud, const FUniqueNetIdPS4& InUserId, const FString& InFileName, bool bInShouldCloudDelete, bool bInShouldLocallyDelete) :
		FOnlineAsyncTaskPS4(InSubsystem),
		bShouldCloudDelete(bInShouldCloudDelete),
		bShouldLocallyDelete(bInShouldLocallyDelete),
		UserId(InUserId.AsShared()), 
		FileName(InFileName),
		RequestId(-1),
		CloudPtr(InCloud)
	{
		IssueDeleteRequest();
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;
	
	/**
	 * Clean up resources
	 */
	virtual void Finalize() override;

	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

#if 0
/**
 * Class encapsulating NP title contexts, by user id
 */
#define MAX_USER_CLOUD_CONTEXTS 32

class FOnlineUserCloudPS4ContextCache
{
private:
	struct CacheEntry
	{
		SceNpId			UniqueNetId;
		int				TitleContextId;

		CacheEntry();
		bool Create(SceNpServiceLabel TusServiceLabel, const SceNpId &NpId);
		void Destroy();
	};

	CacheEntry			Entries[MAX_USER_CLOUD_CONTEXTS];

	typedef TDoubleLinkedList<int> IntList;
	IntList FreeList;
	IntList UsedList;

public:
	FOnlineUserCloudPS4ContextCache();
	~FOnlineUserCloudPS4ContextCache();

	/** Frees all allocated contexts */
	void DestroyAll();

	/** Fetches the title context id for the given user, creating if needs be. Use as a replacement for sceNpTusCreateNpTitleCtxA() */
	// NOTE: Don't cache the returned value here, as it will be cached internally to this class and returned the next time it is
	// requested.
	int GetTitleContextId (SceNpServiceLabel TusServiceLabel, const FUniqueNetIdPS4& UserId); 
};
#else
typedef FOnlinePS4ContextCache FOnlineUserCloudPS4ContextCache;
#endif

/**
 * Provides access to per user cloud file storage
 */

class FOnlineUserCloudPS4 : public IOnlineUserCloud
{
private:
	/** Per-user listing of cloud file directories */
	FCloudFileHeaderMap FileMetaData;

	/** Per-user listing of loaded files */
	FCloudFileMap FileData;

	/** Reference to the main PS4 subsystem */
	class FOnlineSubsystemPS4* PS4Subsystem;

PACKAGE_SCOPE:

	/** User/Title context handling */
	FOnlineUserCloudPS4ContextCache ContextCache;

	// Information about the configured slots, in a form usable directly by the TUS API where possible

	/** The configured slots, indexed by filename */
	FSlotMapping SlotsByFileName;

	/** List of slots being queried */
	SceNpTusSlotId SlotList[MAX_USER_FILES];

	/** The count of configured slots */
	int SlotCount;

	/** The configured service label to use on all calls */
	SceNpServiceLabel TusServiceLabel;

private:
	FOnlineUserCloudPS4() :
		PS4Subsystem(NULL),
		ContextCache(sceNpTusCreateNpTitleCtxA, sceNpTusDeleteNpTitleCtx),
		SlotCount(0),
		TusServiceLabel(0)
	{
	}

PACKAGE_SCOPE:

	FOnlineUserCloudPS4(class FOnlineSubsystemPS4* InSubsystem) :
		PS4Subsystem(InSubsystem),
		ContextCache(sceNpTusCreateNpTitleCtxA, sceNpTusDeleteNpTitleCtx),
		SlotCount(0),
		TusServiceLabel(0)
	{
	}

	/** Initialize the underlying PS4 system */
	bool Init();

	/** Cleanup resources and close out the underlying PS4 system */
	void Shutdown();

	/** Get the slot for a given filename */
	int FindSlotForFileName(const FUniqueNetId& UserId, const FString& FileName);

	/** Get the cached file for a given slot */
	FCloudFilePS4 *FindCachedFileForSlot(const FUniqueNetId& UserId, int SlotId, bool CreateIfNeeded=false);
	
	/** Fetch the slot setup from the configuration file */
	void ConfigureSlots();

public:
	
	virtual ~FOnlineUserCloudPS4();

	// IOnlineUserCloud
	virtual bool GetFileContents(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents) override;
	virtual bool ClearFiles(const FUniqueNetId& UserId) override;
	virtual bool ClearFile(const FUniqueNetId& UserId, const FString& FileName) override;
	virtual void EnumerateUserFiles(const FUniqueNetId& UserId) override;
	virtual void GetUserFileList(const FUniqueNetId& UserId, TArray<FCloudFileHeader>& UserFiles) override;
	virtual bool ReadUserFile(const FUniqueNetId& UserId, const FString& FileName) override;
	virtual bool WriteUserFile(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents) override;
	virtual void CancelWriteUserFile(const FUniqueNetId& UserId, const FString& FileName) override;
	virtual bool DeleteUserFile(const FUniqueNetId& UserId, const FString& FileName, bool bShouldCloudDelete, bool bShouldLocallyDelete) override;
	virtual bool RequestUsageInfo(const FUniqueNetId& UserId) override;
	virtual void DumpCloudState(const FUniqueNetId& UserId) override;
	virtual void DumpCloudFileState(const FUniqueNetId& UserId, const FString& FileName) override;

};

typedef TSharedPtr<FOnlineUserCloudPS4, ESPMode::ThreadSafe> FOnlineUserCloudPS4Ptr;