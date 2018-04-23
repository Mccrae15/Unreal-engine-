// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

//
// PS4 sockets based implementation of the net driver
//

#pragma once
#include "IpConnection.h"
#include "IPNetDriver.h"
#include "PS4NetDriver.generated.h"


UCLASS( transient, config=Engine )
class UPS4NetDriver : public UIpNetDriver
{
	GENERATED_UCLASS_BODY()

	virtual FSocket * CreateSocket() override;
};
