// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4NetDriver.h"
#include "OnlineSubsystemPS4Private.h"

UPS4NetDriver::UPS4NetDriver( const FObjectInitializer & ObjectInitializer ) : Super( ObjectInitializer )
{
}

FSocket * UPS4NetDriver::CreateSocket()
{
	// This is a deprecated function with unsafe socket lifetime management. The Release call is intentional and for backwards compatiblity only.
	return CreateSocketForProtocol(NAME_None).Release();
}

FUniqueSocket UPS4NetDriver::CreateSocketForProtocol(const FName& ProtocolTypeName)
{
	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();

	if (SocketSubsystem == NULL)
	{
		UE_LOG(LogNet, Warning, TEXT("UPS4NetDriver::CreateSocket: Unable to find socket subsystem"));
		return NULL;
	}

	return SocketSubsystem->CreateUniqueSocket(FName("UDPP2P"), TEXT("Unreal"), ProtocolTypeName);
}
