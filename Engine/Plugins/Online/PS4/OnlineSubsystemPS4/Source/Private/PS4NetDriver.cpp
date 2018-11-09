// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4NetDriver.h"
#include "OnlineSubsystemPS4Private.h"

UPS4NetDriver::UPS4NetDriver( const FObjectInitializer & ObjectInitializer ) : Super( ObjectInitializer )
{
}

FSocket * UPS4NetDriver::CreateSocket()
{
	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();

	if (SocketSubsystem == NULL)
	{
		UE_LOG(LogNet, Warning, TEXT("UPS4NetDriver::CreateSocket: Unable to find socket subsystem"));
		return NULL;
	}

	return SocketSubsystem->CreateSocket( FName( "UDPP2P" ), TEXT( "Unreal" ) );
}
