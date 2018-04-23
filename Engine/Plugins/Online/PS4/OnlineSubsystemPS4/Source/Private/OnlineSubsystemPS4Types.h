// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemPS4Package.h"
#include "Containers/List.h"

/** 
 * Key functions classes to allow SceNpOnlineId to be used as the key in TSet and TMap.
 */
template <typename ValueType>
struct TSceNpOnlineIdMap_KeyFuncs : BaseKeyFuncs<TPair<SceNpOnlineId, ValueType>, SceNpOnlineId, false>
{
	static FORCEINLINE SceNpOnlineId const&	GetSetKey	(TPair<SceNpOnlineId, ValueType> const& Element)	{ return Element.Key; }
	static FORCEINLINE uint32				GetKeyHash	(SceNpOnlineId const& Key)							{ return FCrc::StrCrc32(Key.data); }
	static FORCEINLINE bool					Matches		(SceNpOnlineId const& A, SceNpOnlineId const& B)	{ return FCStringAnsi::Strcmp(A.data, B.data) == 0; }
};

struct FSceNpOnlineIdSet_KeyFuncs : BaseKeyFuncs<SceNpOnlineId, SceNpOnlineId, false>
{
	static FORCEINLINE SceNpOnlineId const&	GetSetKey	(SceNpOnlineId const& Element)						{ return Element; }
	static FORCEINLINE uint32				GetKeyHash	(SceNpOnlineId const& Key)							{ return FCrc::StrCrc32(Key.data); }
	static FORCEINLINE bool					Matches		(SceNpOnlineId const& A, SceNpOnlineId const& B)	{ return FCStringAnsi::Strcmp(A.data, B.data) == 0; }
};

template <typename ValueType>
using TSceNpOnlineIdMap = TMap<SceNpOnlineId, ValueType, FDefaultSetAllocator, TSceNpOnlineIdMap_KeyFuncs<ValueType>>;
using FSceNpOnlineIdSet = TSet<SceNpOnlineId, FSceNpOnlineIdSet_KeyFuncs>;

/**
 * Key functions classes to allow SceNpAccountId to be used as the key in TSet and TMap.
 */
template <typename ValueType>
struct TSceNpAccountIdMap_KeyFuncs : BaseKeyFuncs<TPair<SceNpAccountId, ValueType>, SceNpAccountId, false>
{
	static_assert(sizeof(SceNpAccountId) == sizeof(uint64), "SceNpAccountId must be the same size as uint64.");

	static FORCEINLINE SceNpAccountId const&	GetSetKey	(TPair<SceNpAccountId, ValueType> const& Element)	{ return Element.Key; }
	static FORCEINLINE uint32					GetKeyHash	(SceNpAccountId const& Key)							{ return GetTypeHash((uint64)Key); }
	static FORCEINLINE bool						Matches		(SceNpAccountId const& A, SceNpAccountId const& B)	{ return A == B; }
};

struct FSceNpAccountIdSet_KeyFuncs : BaseKeyFuncs<SceNpAccountId, SceNpAccountId, false>
{
	static_assert(sizeof(SceNpAccountId) == sizeof(uint64), "SceNpAccountId must be the same size as uint64.");

	static FORCEINLINE SceNpAccountId const&	GetSetKey	(SceNpAccountId const& Element)						{ return Element; }
	static FORCEINLINE uint32					GetKeyHash	(SceNpAccountId const& Key)							{ return GetTypeHash((uint64)Key); }
	static FORCEINLINE bool						Matches		(SceNpAccountId const& A, SceNpAccountId const& B)	{ return A == B; }
};

template <typename ValueType>
using TSceNpAccountIdMap = TMap<SceNpAccountId, ValueType, FDefaultSetAllocator, TSceNpAccountIdMap_KeyFuncs<ValueType>>;
using FSceNpAccountIdSet = TSet<SceNpAccountId, FSceNpAccountIdSet_KeyFuncs>;

/** 
 * 1:1 associative container for online ids and account ids.
 */
class FOnlineIdMap
{
	TSceNpAccountIdMap<SceNpOnlineId> AccountIdToOnlineIdMap;
	TSceNpOnlineIdMap<SceNpAccountId> OnlineIdToAccountIdMap;

public:
	inline void Insert(SceNpAccountId AccountId, SceNpOnlineId OnlineId)
	{
		OnlineIdToAccountIdMap.FindOrAdd(OnlineId) = AccountId;
		AccountIdToOnlineIdMap.FindOrAdd(AccountId) = OnlineId;
	}

	inline bool GetOnlineId(SceNpAccountId AccountId, SceNpOnlineId& OutOnlineId) const
	{
		SceNpOnlineId const* pOnlineId = AccountIdToOnlineIdMap.Find(AccountId);
		if (pOnlineId)
		{
			OutOnlineId = *pOnlineId;
			return true;
		}

		return false;
	}

	inline bool GetAccountId(SceNpOnlineId OnlineId, SceNpAccountId& OutAccountId) const
	{
		SceNpAccountId const* pAccountId = OnlineIdToAccountIdMap.Find(OnlineId);
		if (pAccountId)
		{
			OutAccountId = *pAccountId;
			return true;
		}

		return false;
	}

	inline bool Remove(SceNpOnlineId OnlineId)
	{
		SceNpAccountId* pAccountId = OnlineIdToAccountIdMap.Find(OnlineId);
		if (pAccountId)
		{
			AccountIdToOnlineIdMap.FindAndRemoveChecked(*pAccountId);
			OnlineIdToAccountIdMap.FindAndRemoveChecked(OnlineId);
			return true;
		}

		return false;
	}

	inline bool Remove(SceNpAccountId AccountId)
	{
		SceNpOnlineId* pOnlineId = AccountIdToOnlineIdMap.Find(AccountId);
		if (pOnlineId)
		{
			OnlineIdToAccountIdMap.FindAndRemoveChecked(*pOnlineId);
			AccountIdToOnlineIdMap.FindAndRemoveChecked(AccountId);
			return true;
		}

		return false;
	}
};

/** Possible login states */
//@todo not all of these will be valid for PS4 taken from steam
namespace EPS4Session
{
	enum Type
	{
		/** Session is undefined */
		None,
		/** Session managed as a lobby on PSN */
		LobbySession,
		/** Session managed as a room on PSN */
		RoomSession,
		/** Unamanaged session not associated with a room or lobby */
		StandaloneSession
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EPS4Session::Type SessionType)
	{
		switch (SessionType)
		{
		case None:
			{
				return TEXT("Session undefined");
			}
		case LobbySession:
			{
				return TEXT("Lobby session");
			}
		case RoomSession:
			{
				return TEXT("Room session");
			}
		case StandaloneSession:
			{
				return TEXT("Standalone session");
			}
		}
		return TEXT("");
	}
}

/**
 * PS4 specific implementation of the unique net id
 */
class FUniqueNetIdPS4 : public FUniqueNetId
{
	static FCriticalSection MasterMutex;
	static TMap<SceUserServiceUserId, TWeakPtr<FUniqueNetIdPS4>> LocalIdMap;
	static TSceNpAccountIdMap<TWeakPtr<FUniqueNetIdPS4>> AccountIdMap;

	enum class EState
	{
		/** The ID is invalid, and does not represent a user. */
		Invalid,

		/** Only the UserId field is valid. No Account ID. */
		LocalOnly,

		/** Both fields are valid (UserId, AccountId) */
		LocalWithOnline,

		/** Only the AccountId field is valid. This is a remote player with no local UserId. */
		OnlineOnly,
	};

	/** Unique ID for a user on the local device. These are not unique between devices. May correctly be invalid for IDs from other devices. */
	SceUserServiceUserId UserId;

	/** The unique PSN account primary key for a signed up user. Guaranteed unique across all of PSN. Only set if the player is signed into PSN. */
	SceNpAccountId AccountId;

	/** Determines which fields are valid in this ID. */
	EState State;
	
	/** Main constructor. */
	FUniqueNetIdPS4(SceUserServiceUserId InUserId, SceNpAccountId const* InAccountId);

public:
	/** Returns a singleton instance of net id to represent an invalid (null) user. */
	static FUniqueNetIdPS4 const& GetInvalidUser();

	/** Functions for down casting to the PS4 net ID implementation. */
	static inline FUniqueNetIdPS4&					Cast(FUniqueNetId&					BaseNetId) { return static_cast<FUniqueNetIdPS4&>(BaseNetId); }
	static inline FUniqueNetIdPS4 const&			Cast(FUniqueNetId const&			BaseNetId) { return static_cast<FUniqueNetIdPS4 const&>(BaseNetId); }
	static inline TSharedPtr<FUniqueNetIdPS4>		Cast(TSharedPtr<FUniqueNetId>		BaseNetId) { return StaticCastSharedPtr<FUniqueNetIdPS4>(BaseNetId); }
	static inline TSharedRef<FUniqueNetIdPS4>		Cast(TSharedRef<FUniqueNetId>		BaseNetId) { return StaticCastSharedRef<FUniqueNetIdPS4>(BaseNetId); }
	static inline TSharedPtr<FUniqueNetIdPS4 const>	Cast(TSharedPtr<FUniqueNetId const>	BaseNetId) { return StaticCastSharedPtr<FUniqueNetIdPS4 const>(BaseNetId); }
	static inline TSharedRef<FUniqueNetIdPS4 const>	Cast(TSharedRef<FUniqueNetId const>	BaseNetId) { return StaticCastSharedRef<FUniqueNetIdPS4 const>(BaseNetId); }

	/** Construction functions. */
	static TSharedRef<FUniqueNetIdPS4> FindOrCreate(SceUserServiceUserId SceUserId);
	static TSharedRef<FUniqueNetIdPS4> FindOrCreate(SceNpAccountId AccountId);
	static TSharedRef<FUniqueNetIdPS4> FindOrCreate(SceNpAccountId AccountId, SceNpOnlineId OnlineId);
	static TSharedPtr<FUniqueNetIdPS4> FromString(FString const& Str);

	/** Shortcuts for casting a net ID reference to a TSharedRef. */
	inline TSharedRef<FUniqueNetIdPS4> AsShared() { return Cast(FUniqueNetId::AsShared()); }
	inline TSharedRef<FUniqueNetIdPS4 const> AsShared() const { return Cast(FUniqueNetId::AsShared()); }

	/** Public accessors. */
	inline SceNpAccountId GetAccountId() const { return AccountId; }
	inline SceUserServiceUserId GetUserId() const { return UserId; }

	/** Delete the copy/move assignment operators/constructors. The class is immutable and immovable. */
	FUniqueNetIdPS4() = delete;
	FUniqueNetIdPS4(FUniqueNetIdPS4&&) = delete;
	FUniqueNetIdPS4(FUniqueNetIdPS4 const&) = delete;
	FUniqueNetIdPS4& operator = (FUniqueNetIdPS4&) = delete;
	FUniqueNetIdPS4& operator = (FUniqueNetId&) = delete;
	FUniqueNetIdPS4& operator = (FUniqueNetIdPS4 const&) = delete;
	FUniqueNetIdPS4& operator = (FUniqueNetId const&) = delete;
	FUniqueNetIdPS4& operator = (FUniqueNetIdPS4&&) = delete;
	FUniqueNetIdPS4& operator = (FUniqueNetId&&) = delete;

	virtual ~FUniqueNetIdPS4();

	/** Called when a local user logs into PSN, to obtain the user's Account ID. */
	bool UpgradeLocal();

	/**
	 * Get the raw byte representation of this net id
	 * This data is platform dependent and shouldn't be manipulated directly
	 *
	 * @return byte array of size GetSize()
	 */
	virtual inline uint8 const* GetBytes() const override final
	{
		check(IsOnlineId()); // Can only call this if we have a valid account ID.
		return reinterpret_cast<uint8 const*>(&AccountId);
	}

	/**
	 * Get the size of the id
	 *
	 * @return size in bytes of the id representation
	 */
	virtual inline int32 GetSize() const override final
	{
		return sizeof(AccountId);
	}

	/**
	 * Check the validity of the id
	 *
	 * @return true if this is a well formed ID, false otherwise
	 */
	virtual inline bool IsValid() const override final
	{
		return State != EState::Invalid;
	}

	/** PS4 Ids can represent device-local players with no PSN account, or players with a PSN account and a valid account/online ID. */
	inline bool IsOnlineId() const { return State == EState::LocalWithOnline || State == EState::OnlineOnly; }
	inline bool IsLocalId() const { return State == EState::LocalWithOnline || State == EState::LocalOnly; }

	/**
	 * Platform specific conversion to string representation of data
	 *
	 * @return data in string form
	 */
	virtual inline FString ToString() const override final
	{
		switch (State)
		{
		default:
		case EState::Invalid:
			checkNoEntry();
			return FString();

		case EState::LocalOnly:
			return FString::Printf(TEXT("local_%d"), UserId);

		case EState::LocalWithOnline:
		case EState::OnlineOnly:
			return PS4AccountIdToString(AccountId);
		}
	}

	/**
	 * Get a human readable representation of the net id
	 * Shouldn't be used for anything other than logging/debugging
	 *
	 * @return id in string form
	 */
	virtual inline FString ToDebugString() const override final
	{
		return (State == EState::Invalid) ? FString(TEXT("INVALID")) : ToString();
	}

	virtual inline bool Compare(FUniqueNetId const& Other) const override final
	{
		// One unique instance of this ID exists for each possible ID. Just compare by address.
		return reinterpret_cast<uintptr_t>(this) == reinterpret_cast<uintptr_t>(&Other);
	}
};

/** 
 * Implementation of session information
 */
class FOnlineSessionInfoPS4 : public FOnlineSessionInfo
{
protected:
	
	/** Hidden on purpose */
	FOnlineSessionInfoPS4(const FOnlineSessionInfoPS4& Src)
	{
	}

	/** Hidden on purpose */
	FOnlineSessionInfoPS4& operator=(const FOnlineSessionInfoPS4& Src)
	{
		return *this;
	}

PACKAGE_SCOPE:

	/** Constructor for LAN sessions */
//	FOnlineSessionInfoPS4();

	/** Constructor for sessions that represent a PS4 lobby? or an advertised server session */
	FOnlineSessionInfoPS4(EPS4Session::Type SessionType, const SceNpMatching2WorldId InWorldId, const SceNpMatching2WorldId InLobbyId, const SceNpMatching2RoomId InRoomId, const FUniqueNetIdString& InSessionId = FUniqueNetIdString(TEXT("INVALID")));

	/** 
	 * Initialize a PS4 session info with the address of this machine
	 */
	void Init();

	/** 
	 * Initialize a PS4 session info with the address of this machine
	 */
//	void InitLAN();

	/** Unique Id for this session */
	FUniqueNetIdString SessionId;
	/** Type of session this is, affects interpretation of id below */
	EPS4Session::Type SessionType;
	/** The ip & port that the host is listening on (valid for LAN/GameServer) */
//	TSharedPtr<class FInternetAddr> HostAddr;
	/** The PS4 P2P address that the host is listening on (valid for GameServer/Lobby) */
//	TSharedPtr<class FInternetAddr> PS4P2PAddr;

	/** The WorldId for which this room belongs */
	SceNpMatching2WorldId  WorldId;

	/** The LobbyId for which this room belongs */
	SceNpMatching2LobbyId  LobbyId;

	/** The WorldId for which this room belongs */
	SceNpMatching2RoomId  RoomId;

	/** The ip & port that the host is listening on */
	TSharedPtr< class FInternetAddr > HostAddr;

public:

	virtual ~FOnlineSessionInfoPS4() {}

	/**
	 *	Comparison operator
	 */
 	bool operator==(const FOnlineSessionInfoPS4& Other) const
 	{
 		return false;
 	}

	/** 
	 * Get the raw byte representation of this session
	 * This data is platform dependent and shouldn't be manipulated directly
	 *
	 * @return byte array of size GetSize()
	 */
	virtual const uint8* GetBytes() const override
	{
		return NULL;
	}

	/** 
	 * Get the size of this session
	 *
	 * @return size in bytes of the id representation
	 */
	virtual int32 GetSize() const override
	{
		return sizeof(uint64) + 
			sizeof(EPS4Session::Type) +
			sizeof(TSharedPtr<class FInternetAddr>) +
			sizeof(TSharedPtr<class FInternetAddr>) + 
			sizeof(FUniqueNetIdPS4);
	}

	/** 
	 * Check the validity of this session
	 *
	 * @return true if this is a well formed ID, false otherwise
	 */
	virtual bool IsValid() const override
	{
		switch (SessionType)
		{
		case EPS4Session::LobbySession:
			return (WorldId != 0) && (LobbyId != 0);
		case EPS4Session::RoomSession:
			return (WorldId != 0) && (RoomId != 0);
		case EPS4Session::StandaloneSession:
			return true;
		case EPS4Session::None:
		default:
			// LAN case
			return false;
		}
	}

	/** 
	 * Platform specific conversion to string representation of data
	 *
	 * @return data in string form 
	 */
	//@todo need to find out where this is called from,
	virtual FString ToString() const override
	{
		return TEXT("");
	}

	/** 
	 * Get a human readable representation of this session
	 * Shouldn't be used for anything other than logging/debugging
	 *
	 * @return handle in string form 
	 */
	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("SessionId: %s WorldId: %i RoomId: %i Type: %s"), 
			*SessionId.ToDebugString(), WorldId, RoomId, EPS4Session::ToString(SessionType));
	}

	/**
	 * Get the session id associated with this session
	 *
	 * @return session id for this session
	 */
	virtual const FUniqueNetId& GetSessionId() const override
	{
		return SessionId;
	}
};

/** Caching mechanism for title contexts */
#define MAX_ONLINE_CONTEXTS 32

class FOnlinePS4ContextCache
{
public:
	typedef int ContextCreateFunction(SceNpServiceLabel ServiceLabel, SceUserServiceUserId SelfId);
	typedef int ContextDestroyFunction(int32_t TitleContextId);

private:
	struct CacheEntry
	{
		SceUserServiceUserId	UserId;
		int						TitleContextId;

		CacheEntry();
		bool Create(ContextCreateFunction *Creator, SceNpServiceLabel ServiceLabel, SceUserServiceUserId SelfId);
		void Destroy(ContextDestroyFunction *Destroyer);
	};
	CacheEntry Entries[MAX_ONLINE_CONTEXTS];

	typedef TDoubleLinkedList<int> IntList;
	IntList FreeList;
	IntList UsedList;

	ContextCreateFunction *CreateFunc;
	ContextDestroyFunction *DestroyFunc;

public:
	FOnlinePS4ContextCache(ContextCreateFunction *Creator, ContextDestroyFunction *Destroyer);
	~FOnlinePS4ContextCache();

	/** Allow the creation and destruction functions to change after construction */
	void SetFunctions (ContextCreateFunction *Creator, ContextDestroyFunction *Destroyer);

	/** Frees all allocated contexts */
	void DestroyAll();

	/** Fetches the title context id for the given user, creating if needs be via CreateFunc(). */
	// NOTE: Don't cache the returned value here, as it will be cached internally to this class and returned the next time it is
	// requested.
	int GetTitleContextId (SceNpServiceLabel ServiceLabel, const FUniqueNetIdPS4& UserId); 
};
