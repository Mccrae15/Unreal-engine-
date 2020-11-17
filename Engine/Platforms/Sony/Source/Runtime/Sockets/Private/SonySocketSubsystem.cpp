// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonySocketSubsystem.h"
#include "SocketSubsystemModule.h"
#include "Modules/ModuleManager.h"
#include <libnetctl.h>
#include <net.h>


/* Static initialization
*****************************************************************************/

FSonySocketSubsystem* FSonySocketSubsystem::SocketSingleton = nullptr;

/* FSocketBSD implementation
*****************************************************************************/

bool FSonySocket::SetNonBlocking(bool bIsNonBlocking)
{
	int32 Param = bIsNonBlocking ? 1 : 0;
	return sceNetSetsockopt(Socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &Param, sizeof(Param)) == SCE_OK;
}

bool FSonySocket::SetNoDelay(bool bIsNoDelay)
{
	if (GetSocketType() == SOCKTYPE_Streaming)
	{
		int32 ParamVal = bIsNoDelay ? 1 : 0;
		return sceNetSetsockopt(Socket, SCE_NET_IPPROTO_TCP, SCE_NET_TCP_NODELAY, &ParamVal, sizeof(ParamVal));
	}
	return true;
}

ESocketBSDReturn FSonySocket::HasState(ESocketBSDParam State, FTimespan WaitTime)
{
	ESocketBSDReturn RetVal = ESocketBSDReturn::EncounteredError;
	
	const SceNetId PollID = (State == ESocketBSDParam::CanRead) ? EpollInId : EpollOutId;

	// return false on error
	if (PollID >= 0)
	{
		// register for error and readability
		SceNetEpollEvent OutEvent;
		// get result, blocking the desired amount of time
		const int WaitTimeUsec = int(WaitTime.GetTotalMicroseconds());

		int Result = sceNetEpollWait(PollID, &OutEvent, 1, WaitTimeUsec);
		if (Result > 0)
		{
			// at this point, the check was successful, so we are either Yes or No
			if (State == ESocketBSDParam::CanRead && (OutEvent.events & SCE_NET_EPOLLIN))
			{
				RetVal = ESocketBSDReturn::Yes;
			}
			else if (State == ESocketBSDParam::CanWrite && (OutEvent.events & SCE_NET_EPOLLOUT))
			{
				RetVal = ESocketBSDReturn::Yes;
			}
			else
			{
				RetVal = ESocketBSDReturn::No;
			}
		}
		else if (Result == 0)	// do not treat timeout as an error condition
		{			
			RetVal = ESocketBSDReturn::No;
		}
	}

	// return final status
	return RetVal;
}

bool FSonySocket::HasPendingData(uint32& PendingDataSize)
{
	if (HasState(ESocketBSDParam::CanRead) == ESocketBSDReturn::Yes)
	{
		// See if there is any pending data on the read socket
		SceNetSockInfo Info;
		if (sceNetGetSockInfo(Socket, &Info, 1, 0) >= 0)
		{
			PendingDataSize = Info.recv_queue_length;

			return (PendingDataSize > 0);
		}
	}

	return false;
}

void FSonySocket::ConvertP2PToBSD(const SceNetSockaddrIn & Source, FInternetAddr & Dest)
{
	FInternetAddrSony& DestPR = static_cast<FInternetAddrSony&>(Dest);
	DestPR.SetIp(Source.sin_addr);
	DestPR.SetRawPort(ntohs(Source.sin_vport));
	DestPR.SetPlatformPortNetworkOrder(Source.sin_port);
}


FName CreateSocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	FName SubsystemName(PLATFORM_SOCKETSUBSYSTEM);
	// Create and register our singleton factor with the main online subsystem for easy access
	FSonySocketSubsystem* SocketSubsystem = FSonySocketSubsystem::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);
		return SubsystemName;
	}
	else
	{
		FSonySocketSubsystem::Destroy();
		return NAME_None;
	}
}


void DestroySocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	SocketSubsystemModule.UnregisterSocketSubsystem(PLATFORM_SOCKETSUBSYSTEM);
	FSonySocketSubsystem::Destroy();
}


FSonySocketSubsystem* FSonySocketSubsystem::Create()
{
	if (SocketSingleton == nullptr)
	{
		SocketSingleton = new FSonySocketSubsystem();
	}

	return SocketSingleton;
}


void FSonySocketSubsystem::Destroy()
{
	if (SocketSingleton != nullptr)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = nullptr;
	}
}


bool FSonySocketSubsystem::Init(FString& Error)
{
	// create a memblock for the resolver (4k as the docs say)
	MemBlockId = sceNetPoolCreate("ResolverMemBlock", 4 * 1024, 0);

	// create a resolver object
	ResolverId = sceNetResolverCreate("SonyResolver", MemBlockId, 0);

	return true;
}


void FSonySocketSubsystem::Shutdown(void)
{
	// toss the resolver if we have it
	if (ResolverId >= 0)
	{
		sceNetResolverDestroy(ResolverId);
		sceNetPoolDestroy(MemBlockId);
	}

	sceNetCtlTerm();

#if PLATFORM_PS4 
	sceNetTerm();
#endif
}


bool FSonySocketSubsystem::HasNetworkDevice()
{
	return true;
}


FSocket * FSonySocketSubsystem::CreateSocket(const FName& SocketType, const FString& SocketDescription, const FName& ProtocolType)
{
	bool bIsP2PUDP = (SocketType == FName("UDPP2P"));
	if (bIsP2PUDP || SocketType == FName("TCPP2P"))
	{
		SOCKET Socket = sceNetSocket(TCHAR_TO_ANSI(*SocketType.ToString()), SCE_NET_AF_INET, bIsP2PUDP ? SCE_NET_SOCK_DGRAM_P2P : SCE_NET_SOCK_STREAM_P2P, 0 );

		return ( Socket != INVALID_SOCKET ) ? new FSonySocket( Socket, bIsP2PUDP ? SOCKTYPE_Datagram : SOCKTYPE_Streaming, SocketDescription,
			FNetworkProtocolTypes::IPv4, true, this ) : nullptr;
	}

	return FSocketSubsystemBSD::CreateSocket( SocketType, SocketDescription, ProtocolType );
}


class FSocketBSD* FSonySocketSubsystem::InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription, const FName& SocketProtocol)
{
	// return a new socket object
	return new FSonySocket(Socket, SocketType, SocketDescription, SocketProtocol, false, this);
}

FAddressInfoResult FSonySocketSubsystem::GetAddressInfo(const TCHAR* HostName, const TCHAR* ServiceName,
	EAddressInfoFlags QueryFlags, const FName ProtocolTypeName, ESocketType SocketType)
{
	FAddressInfoResult AddrData = FAddressInfoResult(HostName, ServiceName);
	AddrData.QueryHostName = HostName;

	// make sure the resolver exists
	if (ResolverId < 0)
	{
		UE_LOG(LogSockets, Warning, TEXT("Could not get address information for %s, had no resolver"), HostName);
		return AddrData;
	}

	SceNetResolverInfo AddressResults;
	SceNetId Result = sceNetResolverStartNtoaMultipleRecords(ResolverId, TCHAR_TO_ANSI(HostName), &AddressResults, 0, 0, 0);

	UE_LOG(LogSockets, Verbose, TEXT("Executed getaddrinfo with HostName: %s Return: %d"), HostName, (int32)Result);
	int32 PortValue = (AddrData.QueryServiceName.IsNumeric()) ? FCString::Atoi(ServiceName) : -1;
	if (Result >= 0 && AddressResults.records > 0)
	{
		for (int32 i = 0; i < AddressResults.records; ++i)
		{
			TSharedRef<FInternetAddrSony> NewAddress = MakeShareable(new FInternetAddrSony(this));
			const SceNetInAddr& AddressRecord = AddressResults.addrs[i].un.addr;

			NewAddress->SetIp(AddressRecord);
			if (PortValue >= 0)
			{
				NewAddress->SetPort(PortValue);
			}

			AddrData.Results.Add(FAddressInfoResultData(NewAddress, sizeof(AddressRecord), FNetworkProtocolTypes::IPv4, SocketType));
			UE_LOG(LogSockets, Verbose, TEXT("# Address: %s"), *(NewAddress->ToString(true)));
		}

		AddrData.ReturnCode = SE_NO_ERROR;
	}
	else
	{
		AddrData.ReturnCode = TranslateErrorCode(Result);
		UE_LOG(LogSockets, Warning, TEXT("Could not get address information for %s, got error code %d"), HostName, (uint32)AddrData.ReturnCode);
	}

	return AddrData;
}

bool FSonySocketSubsystem::GetHostName(FString& HostName)
{
	// query the NetCtl library for host name info
	SceNetCtlInfo Info;
	if (sceNetCtlGetInfo(SCE_NET_CTL_INFO_DHCP_HOSTNAME, &Info) == SCE_OK)
	{
		// Sony may return success but still have a blank host name.
		if (strlen(Info.dhcp_hostname) > 0)
		{
			HostName = ANSI_TO_TCHAR(Info.dhcp_hostname);
			return true;
		}
	}
	if (sceNetCtlGetInfo(SCE_NET_CTL_INFO_IP_ADDRESS, &Info) == SCE_OK)
	{
		HostName = ANSI_TO_TCHAR(Info.ip_address);
		return true;
	}
	else
	{
		// if we couldn't get DHCP hostname or IP address, just give up
		HostName = TEXT("Unknown");
		return false;
	}
}


TSharedRef<FInternetAddr> FSonySocketSubsystem::CreateInternetAddr()
{
	return MakeShareable(new FInternetAddrSony(this));
}

TSharedRef<FInternetAddr> FSonySocketSubsystem::CreateInternetAddr(const FName RequestedProtocol)
{
	return MakeShareable(new FInternetAddrSony(this, RequestedProtocol));
}

bool FSonySocketSubsystem::GetLocalAdapterAddresses(TArray<TSharedPtr<FInternetAddr>>& OutAddresses)
{
	bool bSuccess = true;
	TSharedRef<FInternetAddr> Addr = CreateInternetAddr();
	Addr->SetAnyAddress();

	SceNetCtlInfo Info;
	SceNetInAddr NewAddr;
	// Pull the IP Address and throw it into a NetInAddr so that we can set the IP Address.
	if (sceNetCtlGetInfo(SCE_NET_CTL_INFO_IP_ADDRESS, &Info) == SCE_OK && sceNetInetPton(SCE_NET_AF_INET, Info.ip_address, (void*)&NewAddr) > 0)
	{
		StaticCastSharedRef<FInternetAddrSony>(Addr)->SetIp((const SceNetInAddr)NewAddr);
	}
	else
	{
		UE_LOG(LogSockets, Warning, TEXT("Unable to find an IP address. This probably indicates that only the DEV LAN is connected, not LAN or WiFi. Returning 0.0.0.0 as the local host address."));
		bSuccess = false;
	}

	OutAddresses.Add(Addr);
	return bSuccess;
}


TArray<TSharedRef<FInternetAddr>> FSonySocketSubsystem::GetLocalBindAddresses()
{
	TArray<TSharedRef<FInternetAddr>> BindingAddresses;

	TSharedRef<FInternetAddr> MultihomeAddr = CreateInternetAddr();
	if (!GetMultihomeAddress(MultihomeAddr))
	{
		MultihomeAddr->SetAnyAddress();
	}
	BindingAddresses.Add(MultihomeAddr);
	return BindingAddresses;
}

ESocketErrors FSonySocketSubsystem::TranslateErrorCode(int32 Code)
{
	switch (Code)
	{
	case SCE_NET_EINACTIVEDISABLED: return ESocketErrors::SE_ECONNABORTED;
	};

	return FSocketSubsystemBSD::TranslateErrorCode(Code);
}
