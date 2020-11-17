// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BSDSockets/IPAddressBSD.h"
#include "SocketSubsystemPackage.h"
#include <np/np_common.h>
#include <libnet/nettypes.h>
#include <libnet/in.h>
#include <libnet/inet.h>

/**
* Represents an internet ip address, using the relatively standard SOCKADDR_IN structure. All data is in network byte order
*/
class FInternetAddrSony : public FInternetAddrBSD
{
PACKAGE_SCOPE:
	using FInternetAddrBSD::SetIp;

	/**
	 * Sets the ip address from a PS4 Net structure
	 *
	 * @param InAddr the structure containing the new ip to use
	 */
	void SetIp(const SceNetInAddr& InAddr)
	{
		sockaddr_storage CompatibleFormat;
		FMemory::Memzero(CompatibleFormat);

		sockaddr_in& Ipv4Formatted = (sockaddr_in&)CompatibleFormat;
		Ipv4Formatted.sin_family = AF_INET;
		Ipv4Formatted.sin_len = sizeof(SceNetInAddr);
		Ipv4Formatted.sin_addr.s_addr = InAddr.s_addr;
		FInternetAddrBSD::SetIp(CompatibleFormat);
	}


public:
	// set signalled port to 0 rather than SCE_NP_PORT because this addr might be used with an actual BSD socket.
	FInternetAddrSony() : FInternetAddrBSD(nullptr), SignalledPort(0) {}
	FInternetAddrSony(FSocketSubsystemBSD* InSocketSubsystem, FName RequestedProtocol = NAME_None) : 
		FInternetAddrBSD(InSocketSubsystem, RequestedProtocol), 
		SignalledPort(0) 
	{
	}

	// must jam both ports together so that get/set port operations don't lose information.
	virtual int32 GetPort() const override
	{
		return (GetPlatformPort() << 16) | (FInternetAddrBSD::GetPort());
	}

	// For Ease of Usage
	int32 GetRawPort() const
	{
		return FInternetAddrBSD::GetPort();
	}

	// Same ease of use
	void SetRawPort(int32 InPort)
	{
		FInternetAddrBSD::SetPort(InPort);
	}

	virtual void SetPort(int32 Port) override
	{
		// Port may be coming from an FURL created from the ToString() result of one of these addresses which shoves both ports
		// into the port field for cross-platform compatability.  We need to extract the top bits if necessary.
		int32 VirtualPort = Port & 0xFFFF;
		int32 PlatformPort = Port >> 16;

		FInternetAddrBSD::SetPort(VirtualPort);
		SetPlatformPort(PlatformPort);
	}

	virtual void SetPlatformPort(int32 InPort) override
	{
		SignalledPort = htons(InPort);
	}

	virtual int32 GetPlatformPort() const override
	{
		return ntohs(SignalledPort);
	}

	/**
	 * Get Platform port without converting to host byte order.
	 */
	virtual int32 GetPlatformPortNetworkOrder() const
	{
		return SignalledPort;
	}

	/**
	 * Set Platform port without converting to network byte order.
	 */
	virtual void SetPlatformPortNetworkOrder(int32 InPort)
	{
		SignalledPort = InPort;
	}

	/**
	 * Sets the ip address from a string ("A.B.C.D")
	 *
	 * @param InAddr the string containing the new ip address to use
	 */
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) override
	{
		int32 Port = 0;
		int32 PlatformPort = 0;

		FString AddressString = InAddr;

		TArray<FString> PortTokens;
		AddressString.ParseIntoArray(PortTokens, TEXT(":"), true);

		// look for a port number
		if (PortTokens.Num() > 1)
		{
			int32 CombinedPort = FCString::Atoi(*PortTokens[1]);
			Port = CombinedPort & 0xFFFF;
			PlatformPort = (CombinedPort >> 16);
		}

		SceNetInAddr NewAddressData;
		if (sceNetInetPton(AF_INET, TCHAR_TO_ANSI(*PortTokens[0]), (void*)&NewAddressData) > 0)
		{
			SetIp(NewAddressData);

			if (Port != 0)
			{
				SetPort(Port);
			}

			if (PlatformPort != 0)
			{
				SetPlatformPort(PlatformPort);
			}

			bIsValid = true;
		}
		else
		{
			bIsValid = false;
		}
	}

	/**
	 * Converts this internet ip address to string form
	 *
	 * @param bAppendPort whether to append the port information or not
	 */
	virtual FString ToString(bool bAppendPort) const override
	{
		char ntopBuffer[SCE_NET_INET_ADDRSTRLEN];
		in_addr AddressData;
		GetIp(AddressData);
		FMemory::Memzero(ntopBuffer, sizeof(ntopBuffer));

		if (sceNetInetNtop(AF_INET, (void*)&AddressData, ntopBuffer, sizeof(ntopBuffer)) != NULL)
		{
			FString IPAddress(ANSI_TO_TCHAR(ntopBuffer));
			if (bAppendPort)
			{
				// have to combine the port because this string representation gets filtered through FURL which will lose extra fields.
				// since ports are only 16 bits anyway for BSD sockets, and FURL stores as 32bits this should be fine.
				int32 CombinedPort = GetPlatformPort() << 16 | GetPort();
				IPAddress += FString::Printf(TEXT(":%d"), CombinedPort);
			}

			return IPAddress;
		}

		return TEXT("");
	}

	/**
	 * Compares two internet ip addresses for equality
	 *
	 * @param Other the address to compare against
	 */
	virtual bool operator==(const FInternetAddr& Other) const override
	{
		const FInternetAddrSony& OtherAddress = static_cast<const FInternetAddrSony&>(Other);
		return FInternetAddrBSD::operator==(Other) && SignalledPort == OtherAddress.SignalledPort;
	}

	/**
	 * Clones the data from given FInternetAddr into this structure.
	 */
	virtual TSharedRef<FInternetAddr> Clone() const override
	{
		TSharedRef<FInternetAddrSony> NewAddress = MakeShareable(new FInternetAddrSony(SocketSubsystem, GetProtocolType()));
		NewAddress->SetRawIp(GetRawIp());
		NewAddress->SetPort(GetPort());
		return NewAddress;
	}

private:

	int32 SignalledPort;
};