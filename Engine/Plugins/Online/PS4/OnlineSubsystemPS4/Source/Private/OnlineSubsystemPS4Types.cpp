// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineIdentityInterfacePS4.h"

static FName OSSName(TEXT("PS4"));

//
// FUniqueNetIdPS4 cache methods
//

//
// Context cache methods
//

FOnlinePS4ContextCache::CacheEntry::CacheEntry()
{
	UserId = SCE_USER_SERVICE_USER_ID_INVALID;
	TitleContextId = -1;
}

bool FOnlinePS4ContextCache::CacheEntry::Create(ContextCreateFunction *Creator, SceNpServiceLabel ServiceLabel, SceUserServiceUserId SelfId)
{
	if (TitleContextId < 0)
	{
		int Result = Creator(ServiceLabel, SelfId);
		if (Result >= 0)
		{
			// All is well
			UserId = SelfId;
			TitleContextId = Result;
			return true;
		}
		else
		{
			// @todo: Devise a way of reporting the actual function name
			UE_LOG_ONLINE(Warning, TEXT("sceNpXXXCreateNpTitleCtx() failed. Error code: 0x%08x"), Result);
		}
	}

	// Already created or failed to create
	return false;
}

void FOnlinePS4ContextCache::CacheEntry::Destroy(ContextDestroyFunction *Destroyer)
{
	if (TitleContextId >= 0)
	{
		int Result = Destroyer(TitleContextId);
		if (Result < 0)
		{
			// @todo: Devise a way of reporting the actual function name
			UE_LOG_ONLINE(Warning, TEXT("sceNpXXXDeleteNpTitleCtx() failed. Error code: 0x%08x"), Result);
		}
		TitleContextId = -1;
		UserId = SCE_USER_SERVICE_USER_ID_INVALID;
	}
}

FOnlinePS4ContextCache::FOnlinePS4ContextCache(ContextCreateFunction *Creator, ContextDestroyFunction *Destroyer) :
	CreateFunc(Creator),
	DestroyFunc(Destroyer)
{
	// Form the free list
	for (int i = 0; i < MAX_ONLINE_CONTEXTS; ++i)
	{
		FreeList.AddTail(i);
	}
}

FOnlinePS4ContextCache::~FOnlinePS4ContextCache()
{
	DestroyAll();
}

void FOnlinePS4ContextCache::SetFunctions (ContextCreateFunction *Creator, ContextDestroyFunction *Destroyer)
{
	CreateFunc = Creator;
	DestroyFunc = Destroyer;
}

void FOnlinePS4ContextCache::DestroyAll()
{
	// Destroy each context and reset the used and free lists
	for (IntList::TIterator It(UsedList.GetHead()); It; ++It )
	{
		Entries[*It].Destroy(DestroyFunc);
	}
	UsedList.Empty();
	FreeList.Empty();
	for (int i = 0; i < MAX_ONLINE_CONTEXTS; ++i)
	{
		FreeList.AddTail(i);
	}
}

int FOnlinePS4ContextCache::GetTitleContextId (SceNpServiceLabel ServiceLabel, const FUniqueNetIdPS4& UserId)
{
	// Attempt to find an entry in the used list that matches the given user id.
	for (IntList::TIterator It(UsedList.GetHead()); It; ++It )
	{
		int Index = (*It);
		CacheEntry &Entry = Entries[Index];
		if (Entry.UserId == UserId.GetUserId())
		{
			// We have a match. Move this entry's index to the head of the used list, making the
			// used list in MRU order.
			UsedList.RemoveNode(It.GetNode());
			UsedList.AddHead(Index);

			// Provide the context to the caller
			return Entry.TitleContextId;
		}
	}

	// Not a previously known user, so create a new context, making space in the cache if there is none.
	int NewIndex = -1;
	if (FreeList.Num() == 0)
	{
		// None free
		// Pull out the tail entry in the used list and free its context
		IntList::TDoubleLinkedListNode *UsedTail = UsedList.GetTail();
		NewIndex = UsedTail->GetValue();
		UsedList.RemoveNode(UsedTail);

		Entries[NewIndex].Destroy(DestroyFunc);
	}
	else
	{
		// Use least recently used
		// Pull out the head of the free list as the new index
		IntList::TDoubleLinkedListNode *FreeHead = FreeList.GetHead();
		NewIndex = FreeHead->GetValue();
		FreeList.RemoveNode(FreeHead);
	}
	
	int ContextId = -1;
	if (Entries[NewIndex].Create(CreateFunc, ServiceLabel, UserId.GetUserId()))
	{
		UsedList.AddHead(NewIndex);
		ContextId = Entries[NewIndex].TitleContextId;
	}
	else
	{
		// Failed, so add this new node back to the free list
		FreeList.AddTail(NewIndex);
	}

	check(FreeList.Num() + UsedList.Num() == MAX_ONLINE_CONTEXTS);
	return ContextId;
}

//
// FUniqueNetIdPS4 methods
//

FCriticalSection FUniqueNetIdPS4::MasterMutex;
TMap<SceUserServiceUserId, TWeakPtr<FUniqueNetIdPS4>> FUniqueNetIdPS4::LocalIdMap;
TSceNpAccountIdMap<TWeakPtr<FUniqueNetIdPS4>> FUniqueNetIdPS4::AccountIdMap;

bool FUniqueNetIdPS4::UpgradeLocal()
{
	FScopeLock Lock(&MasterMutex);
	check(State == EState::LocalOnly);
	
	int32 Result;

	// Resolve the local player's Account ID
	SceNpAccountId TempAccountId;
	if ((Result = sceNpGetAccountIdA(UserId, &TempAccountId)) != SCE_OK)
	{
		UE_LOG_ONLINE(Error, TEXT("Local ID upgrade failed. sceNpGetAccountIdA returned 0x%08x"), Result);
		return false;
	}

	// Insert self into the account IDs map.
	AccountIdMap.FindOrAdd(TempAccountId) = AsShared();

	// Update state
	State = EState::LocalWithOnline;
	AccountId = TempAccountId;

	UE_LOG_ONLINE(Log, TEXT("Local ID upgrade successful. UserID: 0x%08x, Account ID: %s"), UserId, *PS4AccountIdToString(AccountId));
	return true;
}

TSharedRef<FUniqueNetIdPS4> FUniqueNetIdPS4::FindOrCreate(SceUserServiceUserId InUserId)
{
	FScopeLock Lock(&MasterMutex);
	check(InUserId != SCE_USER_SERVICE_USER_ID_INVALID);

	TWeakPtr<FUniqueNetIdPS4>* LocalWeakPtr = nullptr;
	TWeakPtr<FUniqueNetIdPS4>* RemoteWeakPtr = nullptr;

	// Find an existing ID in the local IDs map.
	LocalWeakPtr = &LocalIdMap.FindOrAdd(InUserId);
	TSharedPtr<FUniqueNetIdPS4> NetID = LocalWeakPtr->Pin();

	if (NetID.IsValid())
	{
		// Existing account found by SceUserServiceUserId
		return NetID.ToSharedRef();
	}

	// No existing ID found, see if the player is signed into PSN...
	SceNpAccountId AccountId = {};
	SceNpAccountId* pAccountId = nullptr;
	if (sceNpGetAccountIdA(InUserId, &AccountId) == SCE_OK)
	{
		// Player is signed in.
		pAccountId = &AccountId;

		// Check if we've already seen an online-only player with this account ID...
		RemoteWeakPtr = &AccountIdMap.FindOrAdd(AccountId);
		NetID = RemoteWeakPtr->Pin();

		if (NetID.IsValid())
		{
			check(NetID->State == EState::OnlineOnly);

			// We've found an online-only player. Update it with the local user ID.
			NetID->UserId = InUserId;
			NetID->State = EState::LocalWithOnline;

			// Update the local map, so we can resolve this ID by SceUserServiceUserId
			*LocalWeakPtr = NetID;

			return NetID.ToSharedRef();
		}
	}

	// This user is a new, previously unknown local player. Create a new ID...
	NetID = MakeShareable(new FUniqueNetIdPS4(InUserId, pAccountId));

	// Update the maps
	if (RemoteWeakPtr)
		*RemoteWeakPtr = NetID;

	*LocalWeakPtr = NetID;

	return NetID.ToSharedRef();
}

TSharedRef<FUniqueNetIdPS4> FUniqueNetIdPS4::FindOrCreate(SceNpAccountId AccountId)
{
	FScopeLock Lock(&MasterMutex);

	TWeakPtr<FUniqueNetIdPS4>* RemoteWeakPtr = nullptr;
	TWeakPtr<FUniqueNetIdPS4>* LocalWeakPtr = nullptr;

	// Find an existing ID in the local IDs map.
	RemoteWeakPtr = &AccountIdMap.FindOrAdd(AccountId);
	TSharedPtr<FUniqueNetIdPS4> NetID = RemoteWeakPtr->Pin();

	if (NetID.IsValid())
	{
		// Existing account found by AccountId.
		return NetID.ToSharedRef();
	}

	// No existing ID found. Lookup the local user ID by the account ID. This will succeed for local users only.
	SceUserServiceUserId LocalUserId = SCE_USER_SERVICE_USER_ID_INVALID;
	if (sceNpGetUserIdByAccountId(AccountId, &LocalUserId) == SCE_OK)
	{
		// This account ID maps to a local user.
		LocalWeakPtr = &LocalIdMap.FindOrAdd(LocalUserId);
		NetID = LocalWeakPtr->Pin();

		if (NetID.IsValid())
		{
			check(NetID->State == EState::LocalOnly);

			// We've found a local-only player. Update it with the account ID.
			NetID->AccountId = AccountId;
			NetID->State = EState::LocalWithOnline;

			// Update the remote map, so we can resolve this ID by account ID.
			*RemoteWeakPtr = NetID;

			return NetID.ToSharedRef();
		}
	}

	// This user is a new, previously unknown player. Create a new ID...
	NetID = MakeShareable(new FUniqueNetIdPS4(LocalUserId, &AccountId));

	// Update the maps
	if (LocalWeakPtr)
		*LocalWeakPtr = NetID;

	*RemoteWeakPtr = NetID;

	return NetID.ToSharedRef();
}

TSharedRef<FUniqueNetIdPS4> FUniqueNetIdPS4::FindOrCreate(SceNpAccountId AccountId, SceNpOnlineId OnlineId)
{
	// Update the online ID cache for this account ID.
	static_cast<FOnlineSubsystemPS4*>(IOnlineSubsystem::Get(OSSName))->CacheOnlineId(AccountId, OnlineId);

	return FindOrCreate(AccountId);
}

TSharedPtr<FUniqueNetIdPS4> FUniqueNetIdPS4::FromString(const FString& Str)
{
	static const FString LocalPrefix = TEXT("local_");
	if (Str.StartsWith(LocalPrefix))
	{
		// This is a local only net ID.
		SceUserServiceUserId UserId = (SceUserServiceUserId)FCString::Atoi(*Str + LocalPrefix.Len());
		int32 LocalUserIndex = FOnlineIdentityPS4::GetLocalUserIndex(UserId);

		if (LocalUserIndex == INDEX_NONE)
			return nullptr; // No such user signed into the PS4 system.

		return FindOrCreate(UserId);
	}
	else
	{
		// This is a remote net ID.
		return FindOrCreate(PS4StringToAccountId(Str));
	}
}

FUniqueNetIdPS4 const& FUniqueNetIdPS4::GetInvalidUser()
{
	static TSharedRef<FUniqueNetIdPS4 const> InvalidUser = MakeShareable(new FUniqueNetIdPS4(SCE_USER_SERVICE_USER_ID_INVALID, nullptr));
	return *InvalidUser;
}

FUniqueNetIdPS4::FUniqueNetIdPS4(SceUserServiceUserId InUserId, SceNpAccountId const* InAccountId)
	: UserId(InUserId)
{
	if (InAccountId)
	{
		AccountId = *InAccountId;
		State = (UserId == SCE_USER_SERVICE_USER_ID_INVALID) ? EState::OnlineOnly : EState::LocalWithOnline;
	}
	else
	{
		AccountId = {};
		State = (UserId == SCE_USER_SERVICE_USER_ID_INVALID) ? EState::Invalid : EState::LocalOnly;
	}
}

FUniqueNetIdPS4::~FUniqueNetIdPS4()
{
	FScopeLock Lock(&MasterMutex);

	if (IsLocalId())
		verify(!LocalIdMap.FindAndRemoveChecked(UserId).IsValid());

	if (IsOnlineId())
		verify(!AccountIdMap.FindAndRemoveChecked(AccountId).IsValid())
}
