// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include <companion_httpd.h>
#include <user_service.h>

// list of features to start
#ifndef PS4_FEATURE_COMPANION_APP
#define PS4_FEATURE_COMPANION_APP 0
#endif

class FPS4CompanionServer : public FTickableGameObject
{
public:
	FPS4CompanionServer();


	// FTickableGameObhect interface
	void Tick(float DeltaTime) override;

	virtual ETickableTickType GetTickableTickType() const override
	{
		return ETickableTickType::Always;
	}

	virtual bool IsTickableWhenPaused() const override
	{
		return true;
	}

	virtual TStatId GetStatId() const override;

private:

#if PS4_FEATURE_COMPANION_APP
	// start the server
	bool StartServer();

	// request callback
	static int32 RequestCallback(SceUserServiceUserId UserId, const SceCompanionHttpdRequest* HttpRequest,
		SceCompanionHttpdResponse* HttpResponse, void* UserData);

	// The number of connected companion apps
	uint32 NumConnections;

	// The location to look for disk files to be read in by GFileManager - if this is empty, then the OS will look in /app/sce_companion_httpd/html for the files
	FString WebServerRootDir;
#endif
};

