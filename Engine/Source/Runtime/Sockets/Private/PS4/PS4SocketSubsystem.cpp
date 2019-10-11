// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PS4SocketSubsystem.h"
#include "SocketSubsystemModule.h"
#include "Modules/ModuleManager.h"
#include <libnetctl.h>
#include <net.h>


/* Static initialization
*****************************************************************************/

FPS4SocketSubsystem* FPS4SocketSubsystem::SocketSingleton = nullptr;


/* FSocketBSD implementation
*****************************************************************************/

bool FPS4Socket::SetNonBlocking(bool bIsNonBlocking)
{
	int32 Param = bIsNonBlocking ? 1 : 0;
	return sceNetSetsockopt(Socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &Param, sizeof(Param)) == SCE_OK;
}

ESocketBSDReturn FPS4Socket::HasState(ESocketBSDParam State, FTimespan WaitTime)
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

bool FPS4Socket::HasPendingData(uint32& PendingDataSize)
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


FName CreateSocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	FName SubsystemName(TEXT("PS4"));
	// Create and register our singleton factor with the main online subsystem for easy access
	FPS4SocketSubsystem* SocketSubsystem = FPS4SocketSubsystem::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);
		return SubsystemName;
	}
	else
	{
		FPS4SocketSubsystem::Destroy();
		return NAME_None;
	}
}


void DestroySocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	SocketSubsystemModule.UnregisterSocketSubsystem(FName(TEXT("PS4")));
	FPS4SocketSubsystem::Destroy();
}


FPS4SocketSubsystem* FPS4SocketSubsystem::Create()
{
	if (SocketSingleton == nullptr)
	{
		SocketSingleton = new FPS4SocketSubsystem();
	}

	return SocketSingleton;
}


void FPS4SocketSubsystem::Destroy()
{
	if (SocketSingleton != nullptr)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = nullptr;
	}
}


bool FPS4SocketSubsystem::Init(FString& Error)
{
	// create a memblock for the resolver (4k as the docs say)
	MemBlockId = sceNetPoolCreate("ResolverMemBlock", 4 * 1024, 0);

	// create a resolver object
	ResolverId = sceNetResolverCreate("PS4Resolver", MemBlockId, 0);

	return true;
}


void FPS4SocketSubsystem::Shutdown(void)
{
	// toss the resolver if we have it
	if (ResolverId >= 0)
	{
		sceNetResolverDestroy(ResolverId);
		sceNetPoolDestroy(MemBlockId);
	}

	sceNetCtlTerm();
	sceNetTerm();
}


bool FPS4SocketSubsystem::HasNetworkDevice()
{
	return true;
}


FSocket * FPS4SocketSubsystem::CreateSocket( const FName& SocketType, const FString& SocketDescription, ESocketProtocolFamily ProtocolType )
{
	if ( SocketType == FName( "UDPP2P" ) )
	{
		SOCKET Socket = sceNetSocket( "UDPP2P", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P, 0 ); 

		return ( Socket != INVALID_SOCKET ) ? new FPS4Socket( Socket, SOCKTYPE_Datagram, SocketDescription, ProtocolType, true, this ) : nullptr;
	}

	return FSocketSubsystemBSD::CreateSocket( SocketType, SocketDescription, ProtocolType );
}


class FSocketBSD* FPS4SocketSubsystem::InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription, ESocketProtocolFamily SocketProtocol)
{
	// return a new socket object
	return new FPS4Socket(Socket, SocketType, SocketDescription, SocketProtocol, false, this);
}

FAddressInfoResult FPS4SocketSubsystem::GetAddressInfo(const TCHAR* HostName, const TCHAR* ServiceName,
	EAddressInfoFlags QueryFlags, ESocketProtocolFamily ProtocolType, ESocketType SocketType)
{
	FAddressInfoResult AddrData = FAddressInfoResult(HostName, ServiceName);
	AddrData.QueryHostName = HostName;

	// make sure the resolver exists
	if (ResolverId < 0)
	{
		UE_LOG(LogSockets, Warning, TEXT("Could not get address information for %s, had no resolver"), HostName);
		return AddrData;
	}

	// it's expected this function returns the value, and most resolves happen in a thread, 
	// so we just do a blocking resolve
	SceNetInAddr Addr;
	SceNetId Result = sceNetResolverStartNtoa(ResolverId, TCHAR_TO_ANSI(HostName), &Addr, 0, 0, 0);
	if (Result >= 0)
	{
		char NetAddrBuffer[SCE_NET_INET_ADDRSTRLEN];
		FMemory::Memzero(NetAddrBuffer, sizeof(NetAddrBuffer));
		TSharedRef<FInternetAddrPS4> NewAddress = MakeShareable(new FInternetAddrPS4(this));
		if (sceNetInetNtop(SCE_NET_AF_INET, (void*)&Addr, NetAddrBuffer, sizeof(NetAddrBuffer)) != NULL)
		{
			bool bIsValid;
			NewAddress->SetIp(ANSI_TO_TCHAR(NetAddrBuffer), bIsValid);
			AddrData.Results.Add(FAddressInfoResultData(NewAddress, sizeof(NetAddrBuffer),
				ESocketProtocolFamily::IPv4, SocketType));
		}
		else
		{
			UE_LOG(LogSockets, Warning, TEXT("ntop failed for host name %s with error %d"), HostName, (uint32)TranslateErrorCode(sce_net_errno));
		}	
	}
	else
	{
		UE_LOG(LogSockets, Warning, TEXT("Could not get address information for %s, got error code %d"), HostName, (uint32)TranslateErrorCode(Result));
	}

	return AddrData;
}

bool FPS4SocketSubsystem::GetHostName(FString& HostName)
{
	// query the NetCtl library for host name info
	SceNetCtlInfo Info;
	if (sceNetCtlGetInfo(SCE_NET_CTL_INFO_DHCP_HOSTNAME, &Info) == SCE_OK)
	{
		// PS4 may return success but still have a blank host name.
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


TSharedRef<FInternetAddr> FPS4SocketSubsystem::CreateInternetAddr(uint32 Address, uint32 Port)
{
	TSharedRef<FInternetAddr> Result = MakeShareable(new FInternetAddrPS4(this));
	Result->SetIp(Address);
	Result->SetPort(Port);
	return Result;
}


TSharedRef<FInternetAddr> FPS4SocketSubsystem::GetLocalHostAddr(FOutputDevice& Out, bool& bCanBindAll) 
{
	bool bIsValid;
	TSharedRef<FInternetAddr> Addr = CreateInternetAddr();

	SceNetCtlInfo Info;
	int32 Result = sceNetCtlGetInfo(SCE_NET_CTL_INFO_IP_ADDRESS, &Info);
	if (Result == SCE_OK)
	{
		Addr->SetIp(ANSI_TO_TCHAR(Info.ip_address), bIsValid);
	}
	else
	{
		Out.Logf(TEXT("Unable to find an IP address. This probably indicates that only the DEV LAN is connected, not LAN or WiFi. Returning 0.0.0.0 as the local host address."));
		Addr->SetAnyAddress();
	}
	
	// there really is only one that should be used, so let's say we can bind to all
	// @todo test this somehow
	bCanBindAll = true;
	return Addr;
}


bool FPS4SocketSubsystem::GetLocalAdapterAddresses( TArray<TSharedPtr<FInternetAddr> >& OutAddresses )
{
	bool bCanBindAll;
	OutAddresses.Add(GetLocalHostAddr(*GLog, bCanBindAll));
	return true;
}


ESocketErrors FPS4SocketSubsystem::TranslateErrorCode(int32 Code)
{
	switch (Code)
	{
	case SCE_NET_EINACTIVEDISABLED: return ESocketErrors::SE_ECONNABORTED;
	};

	return FSocketSubsystemBSD::TranslateErrorCode(Code);
}
