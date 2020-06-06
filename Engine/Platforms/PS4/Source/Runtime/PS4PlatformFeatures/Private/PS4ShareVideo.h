// Copyright Epic Games, Inc. All Rights Reserved.

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
	virtual bool NewRecording(const TCHAR* DestinationFileName, FVideoRecordingParameters Parameters = FVideoRecordingParameters()) override;
	virtual void StartRecording() override;
	virtual void PauseRecording() override;
	virtual uint64 GetMinimumRecordingSeconds() const override { return 1; };
	virtual uint64 GetMaximumRecordingSeconds() const override { return 900; };
	virtual float GetCurrentRecordingSeconds() const override;
	virtual void FinalizeRecording(const bool bSaveRecording, const FText& Title, const FText& Comment, const bool bStopAutoContinue = true) override;
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

	void PS4VideoExportDelegate(bool bSaveRecording, bool bAutoContinue, FText Title, FText Comment);
	void FinalizeVideoOnGameThread(EVideoRecordingState NewState, uint64 NewStartCycles, FString OutputFilePath);
	bool OpenRecording();
	void NextRecording();

private:
	bool bIsInitialized;
	bool bIsEnabled;
	FVideoRecordingParameters Parameters;

	FString BaseFileName;
	FString CurrentRecordingFileName;
	EVideoRecordingState RecordState;
	TArray<uint32> RecordBuffer;
	uint64 RecordingIndex;
	uint64 CurrentStartRecordingCycles;
	uint64 CyclesBeforePausing;
};
