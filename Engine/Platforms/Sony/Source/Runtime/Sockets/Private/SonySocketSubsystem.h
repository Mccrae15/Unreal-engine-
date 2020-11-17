// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocketSubsystem.h"
#include "BSDSockets/SocketSubsystemBSD.h"
#include "BSDSockets/SocketsBSD.h"
#include "IPAddressSony.h"
#include "SocketTypes.h"
#include "SocketSubsystemPackage.h"
#include <net.h>
#include <np.h>  


/**
 * Sony sockets, sub-classed from BSD sockets, needed to override a couple of functions
 */
class FSonySocket
	: public FSocketBSD
{
public:

	FSonySocket(SOCKET InSocket, ESocketType InSocketType, const FString& InSocketDescription, const FName& InProtocolType, const bool bInIsP2P, ISocketSubsystem * InSubsystem)
		: FSocketBSD(InSocket, InSocketType, InSocketDescription, InProtocolType, InSubsystem), bIsP2P(bInIsP2P)
	{
		EventInRequest.events = SCE_NET_EPOLLIN;
		EventInRequest.reserved = 0;

		EventOutRequest.events = SCE_NET_EPOLLOUT;
		EventOutRequest.reserved = 0;

		// create a polling object
		static int32 Counter = 0;

		EpollInId = sceNetEpollCreate(TCHAR_TO_ANSI(*FString::Printf(TEXT("HasInState%d"), Counter)), 0);
		if (EpollInId >= 0)
		{
			int ReturnVal = sceNetEpollControl(EpollInId, SCE_NET_EPOLL_CTL_ADD, Socket, &EventInRequest);
			if (ReturnVal < 0)
			{
				UE_LOG(LogSockets, Warning, TEXT("sceNetEpollControl(ADD) failed (0x%x errno=%d)\n"), ReturnVal, sce_net_errno);
				sceNetEpollDestroy(EpollInId);
				EpollInId = -1;
			}
		}
		else
		{
			UE_LOG(LogSockets, Warning, TEXT("sceNetEpollCreate() failed (0x%x errno=%d)\n"), EpollInId, sce_net_errno);
		}

		EpollOutId = sceNetEpollCreate(TCHAR_TO_ANSI(*FString::Printf(TEXT("HasOutState%d"), Counter)), 0);
		if (EpollOutId >= 0)
		{
			int ReturnVal = sceNetEpollControl(EpollOutId, SCE_NET_EPOLL_CTL_ADD, Socket, &EventOutRequest);
			if (ReturnVal < 0)
			{
				UE_LOG(LogSockets, Warning, TEXT("sceNetEpollControl(ADD) failed (0x%x errno=%d)\n"), ReturnVal, sce_net_errno);
				sceNetEpollDestroy(EpollOutId);
				EpollOutId = -1;
			}
		}
		else
		{
			UE_LOG(LogSockets, Warning, TEXT("sceNetEpollCreate() failed (0x%x errno=%d)\n"), EpollOutId, sce_net_errno);
		}

		Counter++;
	}

	SceNetSockaddrIn ConvertBSDToP2PForBind(const FInternetAddr & Addr)
	{
		SceNetSockaddrIn SceSocket = ConvertBSDToP2P(Addr);
		SceSocket.sin_port = sceNetHtons(SCE_NP_PORT); //for binding we always need to use SCE_NP_PORT
		return SceSocket;
	}

	SceNetSockaddrIn ConvertBSDToP2P(const FInternetAddr & Addr)
	{
		in_addr BSDAddrData;
		const FInternetAddrSony& SonyAddr = static_cast<const FInternetAddrSony&>(Addr);
		SonyAddr.GetIp(BSDAddrData);

		SceNetSockaddrIn P2PAddr = { 0 };

		P2PAddr.sin_len = sizeof(P2PAddr);
		// Sony doesn't have official support for ipv6
		P2PAddr.sin_family = AF_INET;

		P2PAddr.sin_addr.s_addr = BSDAddrData.s_addr;
		P2PAddr.sin_port = SonyAddr.GetPlatformPortNetworkOrder();
		P2PAddr.sin_vport = htons(SonyAddr.GetRawPort());
		return P2PAddr;
	}

	void ConvertP2PToBSD(const SceNetSockaddrIn & Source, FInternetAddr & Dest);

	bool Bind(const FInternetAddr& Addr) override
	{
		if (!bIsP2P)
		{
			// Punt non p2p work to bsd socket code
			return FSocketBSD::Bind(Addr);
		}

		SceNetSockaddrIn P2PAddr = ConvertBSDToP2PForBind(Addr);

		return sceNetBind(Socket, (const SceNetSockaddr *)&P2PAddr, sizeof(P2PAddr)) == 0;
		//return bind( Socket, (const sockaddr*)&P2PAddr, sizeof( P2PAddr) ) == 0;
	}

	bool SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination) override
	{
		if (!bIsP2P)
		{
			// Punt non p2p work to bsd socket code
			return FSocketBSD::SendTo(Data, Count, BytesSent, Destination);
		}

		SceNetSockaddrIn P2PAddrIn = ConvertBSDToP2P(Destination);

		BytesSent = sceNetSendto(Socket, (const char*)Data, Count, 0, (SceNetSockaddr*)&P2PAddrIn, sizeof(P2PAddrIn));
		//BytesSent = sendto(Socket, (const char*)Data, Count, 0, (const sockaddr*)&P2PAddr, sizeof( P2PAddr ) );

		//	NETWORK_PROFILER(FSocket::SendTo(Data,Count,BytesSent,Destination));

		bool Result = BytesSent >= 0;
		if (Result)
		{
			UpdateActivity();
		}
		return Result;
	}

	// This is the same as FSocketBSD::Send with the addition of the MSG_NOSIGNAL flag
	virtual bool Send(const uint8* Data, int32 Count, int32& BytesSent) override
	{
		BytesSent = send(Socket, (const char*)Data, Count, MSG_NOSIGNAL);

		bool Result = BytesSent >= 0;
		if (Result)
		{
			UpdateActivity();
		}
		return Result;
	}

	bool RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags) override
	{
		if (!bIsP2P)
		{
			// Punt non p2p work to bsd socket code
			return FSocketBSD::RecvFrom(Data, BufferSize, BytesRead, Source, Flags);
		}

		SceNetSockaddrIn P2PAddr = { 0 };
		SOCKLEN Size = sizeof(P2PAddr);

		//BytesRead = sceNetRecvfrom( Socket, Data, BufferSize, SCE_NET_MSG_DONTWAIT, reinterpret_cast< SceNetSockaddr * >( &P2PAddr ), &Size );
		// Read into the buffer and set the source address


		BytesRead = sceNetRecvfrom(Socket, (char*)Data, BufferSize, 0, (SceNetSockaddr*)&P2PAddr, &Size);
		//BytesRead = recvfrom(Socket, (char*)Data, BufferSize, Flags, (sockaddr*)&P2PAddr, &Size);
		//	NETWORK_PROFILER(FSocket::RecvFrom(Data,BufferSize,BytesRead,Source));

		ConvertP2PToBSD(P2PAddr, Source);

		bool Result = BytesRead >= 0;
		if (Result)
		{
			UpdateActivity();
		}

		return Result;
	}

	~FSonySocket()
	{
		if (EpollInId >= 0)
		{
			sceNetEpollControl(EpollInId, SCE_NET_EPOLL_CTL_DEL, Socket, &EventInRequest);
			sceNetEpollDestroy(EpollInId);
		}
		EpollInId = -1;

		if (EpollOutId >= 0)
		{
			sceNetEpollControl(EpollOutId, SCE_NET_EPOLL_CTL_DEL, Socket, &EventOutRequest);
			sceNetEpollDestroy(EpollOutId);
		}
		EpollOutId = -1;
	}
	virtual bool SetNonBlocking(bool bIsNonBlocking = true) override;
	virtual bool SetNoDelay(bool bIsNoDelay = true) override;
	bool HasPendingData(uint32& PendingDataSize) override;

protected:

	virtual ESocketBSDReturn HasState(ESocketBSDParam State, FTimespan WaitTime = FTimespan::Zero()) override;

	SceNetId			EpollInId;
	SceNetId			EpollOutId;
	SceNetEpollEvent	EventInRequest;
	SceNetEpollEvent	EventOutRequest;
	bool				bIsP2P;
};


/**
 * Sony specific socket subsystem implementation.
 */
class FSonySocketSubsystem
	: public FSocketSubsystemBSD
{
public:

	/** Default constructor. */
	FSonySocketSubsystem()
		: bTriedToInit(false)
	{ }

	/** Virtual destructor. */
	virtual ~FSonySocketSubsystem() { }

public:

	// FSocketSubsystemBSD overrides

	virtual bool Init(FString& Error) override;
	virtual void Shutdown() override;
	virtual bool HasNetworkDevice() override;
	virtual class FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, const FName& ProtocolType) override;
	virtual class FSocketBSD* InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription, const FName& SocketProtocol) override;
	virtual FAddressInfoResult GetAddressInfo(const TCHAR* HostName, const TCHAR* ServiceName = nullptr,
		EAddressInfoFlags QueryFlags = EAddressInfoFlags::Default,
		const FName ProtocolTypeName = NAME_None,
		ESocketType SocketType = ESocketType::SOCKTYPE_Unknown) override;
	virtual bool GetHostName(FString& HostName) override;
	virtual TSharedRef<FInternetAddr> CreateInternetAddr() override;
	virtual TSharedRef<FInternetAddr> CreateInternetAddr(const FName RequestedProtocol) override;
	virtual bool GetLocalAdapterAddresses(TArray<TSharedPtr<FInternetAddr> >& OutAdresses) override;
	virtual TArray<TSharedRef<FInternetAddr>> GetLocalBindAddresses() override;
	virtual ESocketErrors TranslateErrorCode(int32 Code) override;
	virtual FName GetDefaultSocketProtocolFamily() const override
	{
		return FNetworkProtocolTypes::IPv4;
	}

protected:

	/** Single instantiation of this subsystem. */
	static FSonySocketSubsystem* SocketSingleton;

	/** Whether Init() has been called before or not. */
	bool bTriedToInit;

	/** The name resolver handle, -1 means invalid. */
	SceNetId ResolverId;

	/** The memblock handle for the resolver. */
	int32 MemBlockId;

PACKAGE_SCOPE:

	/**
	 * Singleton interface for this subsystem.
	 *
	 * @return the only instance of this subsystem.
	 */
	static FSonySocketSubsystem* Create();

	/** Performs Windows specific socket clean up. */
	static void Destroy();
};