// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4ShareVideo.h"
#include "Async/TaskGraphInterfaces.h"
#include <video_recording.h>
#include <libsysmodule.h>
#include <content_export.h>
#include <user_service.h>

// Memory allocation wrappers for the ContentExport module
static void* ContentExportMalloc(size_t DataSize, void* UserData)
{
	return FMemory::Malloc(DataSize);
}

static void ContentExportFree(void* Memory, void* UserData)
{
	FMemory::Free(Memory);
}


/**
 * RAII wrapper for a single piece of content being exported by the PS4 ContentExport module.
 */
class FPS4ContentExportTask
{
public:
	FPS4ContentExportTask() :
		ExportID(sceContentExportStart())
	{
		
	}

	void Start(const ANSICHAR* Title, const ANSICHAR* Comment, const ANSICHAR* ContentType, const ANSICHAR* FilePath)
	{
		SceContentExportParam ExportParam;
		FCStringAnsi::Strncpy( ExportParam.title, Title, SCE_CONTENT_EXPORT_LENGTH_TITLE );
		FCStringAnsi::Strncpy( ExportParam.comment, Comment, SCE_CONTENT_EXPORT_LENGTH_COMMENT );
		FCStringAnsi::Strncpy( ExportParam.contenttype, ContentType, SCE_CONTENT_EXPORT_LENGTH_CONTENTTYPE );

		const int32 ExportResult = sceContentExportFromFile(ExportID, &ExportParam, FilePath, nullptr, 0);

		if (ExportResult != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("FPS4ContentExportTask::Start: sceContentExportFromFile failed. Error code: 0x%08x"), ExportResult);
		}
	}

	~FPS4ContentExportTask()
	{
		sceContentExportFinish(ExportID);
	}

private:
	int32 ExportID;
};

FPS4ShareVideo::FPS4ShareVideo () :
	bIsInitialized(false),
	bIsEnabled(false),
	RecordState(EVideoRecordingState::None)
{
	Initialize();
}

FPS4ShareVideo::~FPS4ShareVideo()
{
	Shutdown();
}

void FPS4ShareVideo::EnableRecording(bool bEnableRecording)
{
	if (bIsInitialized)
	{
		bIsEnabled = bEnableRecording;
		if (bIsEnabled)
		{
			static const int EnabledVideo = 0;
			int32 Result = sceVideoRecordingSetInfo(SCE_VIDEO_RECORDING_INFO_CHAPTER, &EnabledVideo, sizeof(int32_t));
			if (Result != SCE_OK)
			{
				UE_LOG(LogPS4, Error, TEXT("sceVideoRecordingSetInfo failed. Error code: 0x%08x"), Result);
			}
		}
		else
		{
			static const int DisabledVideo = SCE_VIDEO_RECORDING_CHAPTER_PROHIBIT;
			int32 Result = sceVideoRecordingSetInfo(SCE_VIDEO_RECORDING_INFO_CHAPTER, &DisabledVideo, sizeof(int32_t));
			if (Result != SCE_OK)
			{
				UE_LOG(LogPS4, Error, TEXT("sceVideoRecordingSetInfo failed. Error code: 0x%08x"), Result);
			}
		}
	}
	else
	{
		UE_LOG(LogPS4, Error, TEXT("FPS4ShareVideo not initialized"));
	}
}

bool FPS4ShareVideo::IsEnabled() const
{
	return bIsEnabled;
}

bool FPS4ShareVideo::NewRecording(const TCHAR* DestinationFileName)
{
	if (!bIsEnabled)
	{
		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::NewRecording: can't open a new recording, recording is disabled."));
		return false;
	}

	if (!bIsInitialized)
	{
		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::NewRecording: can't open a new recording, recording is not initialized."));
		return false;
	}

	if (RecordState != EVideoRecordingState::None)
	{
		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::NewRecording: can't open a new recording, one is already in progress."));
		return false;
	}

	SceVideoRecordingParam2 Param;
	sceVideoRecordingParamInit2(&Param);

	// Customize members of Param here if desired

	const int32 BufferSize = sceVideoRecordingQueryMemSize2(&Param);

	if (BufferSize == SCE_VIDEO_RECORDING_ERROR_INVALID_VALUE)
	{
		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::NewRecording: failed to get required buffer size."));
		return false;
	}

	RecordBuffer.SetNumUninitialized(BufferSize, true);
	
	RecordedFileName = SCE_VIDEO_RECORDING_PATH_GAME;
	RecordedFileName += DestinationFileName;
	RecordedFileName += TEXT(".mp4");

	UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::NewRecording: opening new recording with filename %s"), *RecordedFileName);

	auto ConvertedString = StringCast<ANSICHAR>(*RecordedFileName);

	const int32 OpenResult = sceVideoRecordingOpen2(ConvertedString.Get(), &Param, RecordBuffer.GetData(), RecordBuffer.Num());

	if (OpenResult != SCE_OK)
	{
		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::NewRecording: sceVideoRecordingOpen2 failed. Error code: 0x%08x"), OpenResult);
		return false;
	}

	RecordState = EVideoRecordingState::Paused;
	return true;
}

void FPS4ShareVideo::StartRecording()
{
	if (RecordState != EVideoRecordingState::Paused)
	{
		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::StartRecording: can't start recording, invalid state."));
		return;
	}

	UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::StartRecording: starting a recording"));

	const int32 StartResult = sceVideoRecordingStart();

	if (StartResult != SCE_OK)
	{
		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::StartRecording: sceVideoRecordingStart failed. Error code: 0x%08x"), StartResult);
		FinalizeRecording(false, FText::GetEmpty(), FText::GetEmpty());
		return;
	}

	RecordState = EVideoRecordingState::Starting;
}

void FPS4ShareVideo::PauseRecording()
{
	if (RecordState != EVideoRecordingState::Recording && RecordState != EVideoRecordingState::Pausing && RecordState != EVideoRecordingState::Starting)
	{
		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::PauseRecording: can't pause recording, invalid state."));
		return;
	}

	UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::PauseRecording: pausing a recording"));

	const int32 StopResult = sceVideoRecordingStop();

	if (StopResult != SCE_OK)
	{
		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::PauseRecording: sceVideoRecordingStop failed. Error code: 0x%08x"), StopResult);
		RecordState = EVideoRecordingState::Pausing;
		FinalizeRecording(false, FText::GetEmpty(), FText::GetEmpty());
		return;
	}

	RecordState = EVideoRecordingState::Pausing;
}

void FPS4ShareVideo::FinalizeRecording(const bool bSaveRecording, const FText& Title, const FText& Comment)
{
	if (RecordState == EVideoRecordingState::None)
	{
		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::FinalizeRecording: can't finalize recording, invalid state."));
		return;
	}

	if (RecordState != EVideoRecordingState::Paused && RecordState != EVideoRecordingState::Pausing)
	{
		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::FinalizeRecording: pausing a recording before closing it"));
		PauseRecording();
	}
	
	RecordState = EVideoRecordingState::Finalizing;

	// Closing the video and exporting it can take a few seconds, so do it in a task.
	FGraphEventRef CloseEvent = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateRaw(this, &FPS4ShareVideo::PS4VideoExportDelegate, bSaveRecording, RecordedFileName, Title, Comment),
		TStatId()
	);
}

void FPS4ShareVideo::FinalizeVideoOnGameThread()
{
	RecordState = EVideoRecordingState::None;
}

void FPS4ShareVideo::PS4VideoExportDelegate(const bool bWasSuccessful, FString FileName, FText Title, FText Comment)
{
	UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::PS4VideoExportDelegate: closing a recording"));

	const int32 CloseResult = sceVideoRecordingClose(bWasSuccessful ? 0 : 1);

	if (CloseResult != SCE_OK)
	{
		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::PS4VideoExportDelegate: sceVideoRecordingClose failed. Error code: 0x%08x"), CloseResult);
	}
	else if (bWasSuccessful)
	{
		// Caller wants to save the video and close was successful
		auto ConvertedFilename = StringCast<ANSICHAR>(*FileName);
		ANSICHAR* ConvertedTitle = TCHAR_TO_UTF8(*Title.ToString());
		ANSICHAR* ConvertedComment = TCHAR_TO_UTF8(*Comment.ToString());

		FPS4ContentExportTask ExportTask;
		ExportTask.Start(ConvertedTitle, ConvertedComment, SCE_CONTENT_EXPORT_FORMAT_VIDEO_MP4, ConvertedFilename.Get());

		UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::PS4VideoExportDelegate: finished exporting."), *FileName);
	}

	FGraphEventRef FinalizeEvent = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateRaw(this, &FPS4ShareVideo::FinalizeVideoOnGameThread),
		TStatId(),
		nullptr,
		ENamedThreads::GameThread);
}

void FPS4ShareVideo::Tick(float DeltaTime)
{
	if (RecordState != EVideoRecordingState::None && RecordState != EVideoRecordingState::Finalizing)
	{
		const int32 LibraryStatus = sceVideoRecordingGetStatus();

		// All error codes are negative. As per the SDK docs, stop and finalize the recording if there is an error.
		if (LibraryStatus < 0)
		{
			UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::Tick: sceVideoRecordingGetStatus returned error code 0x%08x"), LibraryStatus);
			PauseRecording();
			FinalizeRecording(false, FText::GetEmpty(), FText::GetEmpty());
			return;
		}

		// Update our state enum based on the library's progress.
		if (RecordState == EVideoRecordingState::Starting && LibraryStatus == SCE_VIDEO_RECORDING_STATUS_RUNNING)
		{
			UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::Tick: state change from Starting to Recording"));
			RecordState = EVideoRecordingState::Recording;
		}
		else if (RecordState == EVideoRecordingState::Pausing && LibraryStatus == SCE_VIDEO_RECORDING_STATUS_NONE)
		{
			UE_LOG(LogPS4, Log, TEXT("PS4ShareVideo::Tick: state change from Pausing to Paused"));
			RecordState = EVideoRecordingState::Paused;
		}
	}
}

EVideoRecordingState FPS4ShareVideo::GetRecordingState() const
{
	return RecordState;
}

TStatId FPS4ShareVideo::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FPS4ShareVideo, STATGROUP_Tickables);
}

bool FPS4ShareVideo::Initialize()
{
	if (!bIsInitialized)
	{
		// Load the system modules
		const int32 VideoRecordingResult = sceSysmoduleLoadModule(SCE_SYSMODULE_VIDEO_RECORDING);
  
  		if (VideoRecordingResult != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_VIDEO_RECORDING) failed. Error code: 0x%08x"), VideoRecordingResult);

			// Don't continue setting things up
			return false;
  		}

		// ContentExport is required to have videos show up for uploading via the system software.
		// ContentExport requires the user service library to be initialized.
		sceUserServiceInitialize(nullptr);
		const int32 ContentExportResult = sceSysmoduleLoadModule(SCE_SYSMODULE_CONTENT_EXPORT);
  
  		if (ContentExportResult != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_CONTENT_EXPORT) failed. Error code: 0x%08x"), ContentExportResult);

			// Don't continue setting things up
			return false;
  		}

		// Set up allocator for ContentExport
		SceContentExportInitParam ContentExportInitParam;
		ContentExportInitParam.mallocfunc = &ContentExportMalloc;
		ContentExportInitParam.freefunc = &ContentExportFree;
		ContentExportInitParam.userdata = nullptr;
		sceContentExportInit(&ContentExportInitParam);

		bIsInitialized = true;
	}
	
	return bIsInitialized;
}

void FPS4ShareVideo::Shutdown()
{
	if (bIsInitialized)
	{
		sceContentExportTerm();

		const int32 ContentExportResult = sceSysmoduleUnloadModule(SCE_SYSMODULE_CONTENT_EXPORT);
  		if (ContentExportResult != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_CONTENT_EXPORT) failed. Error code: 0x%08x"), ContentExportResult);
  		}

		const int32 VideoRecordingResult = sceSysmoduleUnloadModule(SCE_SYSMODULE_VIDEO_RECORDING);
  		if (VideoRecordingResult != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_VIDEO_RECORDING) failed. Error code: 0x%08x"), VideoRecordingResult);
  		}
		
		// Assume that we terminated and unloaded the library successfully
		bIsInitialized = false;
	}
}
