// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/ITargetDeviceOutput.h"


class FPS4TargetDeviceOutput : public ITargetDeviceOutput
{
public:
	bool Init( const FString& HostName, FOutputDevice* Output);
	
private:
	TUniquePtr<FRunnableThread> DeviceOutputThread;
};
