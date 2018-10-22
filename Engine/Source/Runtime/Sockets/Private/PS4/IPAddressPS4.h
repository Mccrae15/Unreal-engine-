// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "../BSDSockets/IPAddressBSD.h"
#include <np/np_common.h>

/**
* Represents an internet ip address, using the relatively standard SOCKADDR_IN structure. All data is in network byte order
*/
class FInternetAddrPS4 : public FInternetAddrBSD
{
public:
	// set signalled port to 0 rather than SCE_NP_PORT because this addr might be used with an actual BSD socket.
	FInternetAddrPS4() : SignalledPort(0) {}

	// must jam both ports together so that get/set port operations don't lose information.
	virtual int32 GetPort() const override
	{
		return (GetPlatformPort() << 16) | (FInternetAddrBSD::GetPort());
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
		int32 A, B, C, D;
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

		// now split the part before the : into a.b.c.d
		TArray<FString> AddrTokens;
		PortTokens[0].ParseIntoArray(AddrTokens, TEXT("."), true);

		if (AddrTokens.Num() < 4)
		{
			bIsValid = false;
			return;
		}

		A = FCString::Atoi(*AddrTokens[0]);
		B = FCString::Atoi(*AddrTokens[1]);
		C = FCString::Atoi(*AddrTokens[2]);
		D = FCString::Atoi(*AddrTokens[3]);

		// Make sure the address was valid
		if ((A & 0xFF) == A && (B & 0xFF) == B && (C & 0xFF) == C && (D & 0xFF) == D)
		{
			FInternetAddrBSD::SetIp((A << 24) | (B << 16) | (C << 8) | (D << 0));

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
			//debugf(TEXT("Invalid IP address string (%s) passed to SetIp"),InAddr);
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
		uint32 LocalAddr = ntohl(Addr.sin_addr.s_addr);

		// Get the individual bytes
		const int32 A = (LocalAddr >> 24) & 0xFF;
		const int32 B = (LocalAddr >> 16) & 0xFF;
		const int32 C = (LocalAddr >> 8) & 0xFF;
		const int32 D = (LocalAddr >> 0) & 0xFF;
		if (bAppendPort)
		{
			// have to combine the port because this string representation gets filtered through FURL which will lose extra fields.
			// since ports are only 16 bits anyway for BSD sockets, and FURL stores as 32bits this should be fine.
			int32 CombinedPort = GetPlatformPort() << 16 | GetPort();
			return FString::Printf(TEXT("%i.%i.%i.%i:%i"), A, B, C, D, CombinedPort);
		}
		else
		{
			return FString::Printf(TEXT("%i.%i.%i.%i"), A, B, C, D);
		}
	}

	/**
	* Compares two internet ip addresses for equality
	*
	* @param Other the address to compare against
	*/
	virtual bool operator==(const FInternetAddr& Other) const override 
	{
		FInternetAddrPS4& OtherBSD = (FInternetAddrPS4&)Other;
		return Addr.sin_addr.s_addr == OtherBSD.Addr.sin_addr.s_addr &&
			Addr.sin_port == OtherBSD.Addr.sin_port &&
			Addr.sin_family == OtherBSD.Addr.sin_family &&
			SignalledPort == OtherBSD.SignalledPort;
	}

private:

	int32 SignalledPort;
};