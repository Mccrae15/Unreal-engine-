// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tickable.h"
#include "VideoRecordingSystem.h"

class FPS4ShareVideo : public IVideoRecordingSystem, public FTickableGameObject
{
public:
	FPS4ShareVideo();
	virtual ~FPS4ShareVideo();
	
	// IVideoRecordingSystem interface
	virtual void EnableRecording(const bool bEnableRecording);
	virtual bool IsEnabled() const override;
	virtual bool NewRecording(const TCHAR* DestinationFileName) override;
	virtual void StartRecording() override;
	virtual void PauseRecording() override;
	virtual void FinalizeRecording(const bool bSaveRecording, const FText& Title, const FText& Comment) override;
	virtual EVideoRecordingState GetRecordingState() const override;

	// FTickableGameObject interface
	void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override;

private:
	/**
	 * Loads the library and initializes
	 */
	bool Initialize();

	/**
	 * Unloads the associated library
	 */
	void Shutdown();

	void PS4VideoExportDelegate(const bool bWasSuccessful, FString FileName, FText Title, FText Comment);
	void FinalizeVideoOnGameThread();

private:
	bool bIsInitialized;
	bool bIsEnabled;

	FString RecordedFileName;
	EVideoRecordingState RecordState;
	TArray<uint32> RecordBuffer;
};
