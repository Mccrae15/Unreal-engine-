// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4ShareVideo.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/LowLevelMemTracker.h"
#include <video_recording.h>
#include <libsysmodule.h>
#include <content_export.h>
#include <user_service.h>

DEFINE_VIDEOSYSTEMRECORDING_STATS

DECLARE_CYCLE_STAT(TEXT("FPS4ShareVideo.FPS4ContentExportDelegate"), STAT_FPS4ShareVideo_PS4VideoExportDelegate, STATGROUP_VideoRecordingSystem);
DECLARE_CYCLE_STAT(TEXT("FPS4ShareVideo.FinalizeVideoOnGameThread"), STAT_FPS4ShareVideo_FinalizeVideoOnGameThread, STATGROUP_VideoRecordingSystem);

// Memory allocation wrappers for the ContentExport module
static void* ContentExportMalloc(size_t DataSize, void* UserData)
{
	LLM_SCOPE(ELLMTag::VideoRecording);
	return FMemory::Malloc(DataSize);
}

static void ContentExportFree(void* Memory, void* UserData)
{
	LLM_SCOPE(ELLMTag::VideoRecording);
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

	FString Start(const ANSICHAR* Title, const ANSICHAR* Comment, const ANSICHAR* ContentType, const ANSICHAR* FilePath)
	{
		SceContentExportParam ExportParam;
		FCStringAnsi::Strncpy( ExportParam.title, Title, SCE_CONTENT_EXPORT_LENGTH_TITLE );
		FCStringAnsi::Strncpy( ExportParam.comment, Comment, SCE_CONTENT_EXPORT_LENGTH_COMMENT );
		FCStringAnsi::Strncpy( ExportParam.contenttype, ContentType, SCE_CONTENT_EXPORT_LENGTH_CONTENTTYPE );

		ANSICHAR OutputFilePath[PATH_MAX];
		const int32 ExportResult = sceContentExportFromFile(ExportID, &ExportParam, FilePath, OutputFilePath, 0);

		if (ExportResult != SCE_OK)
		{
			UE_LOG(LogSony, Error, TEXT("FPS4ContentExportTask::Start: sceContentExportFromFile failed. Error code: 0x%08x"), ExportResult);
		}

		return OutputFilePath;
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
	SCOPE_CYCLE_COUNTER(STAT_VideoRecordingSystem_EnableRecording);
	check(IsInGameThread());

	if (bIsInitialized)
	{
		bIsEnabled = bEnableRecording;
		if (bIsEnabled)
		{
			static const int EnabledVideo = 0;
			int32 Result = sceVideoRecordingSetInfo(SCE_VIDEO_RECORDING_INFO_CHAPTER, &EnabledVideo, sizeof(int32_t));
			if (Result != SCE_OK)
			{
				UE_LOG(LogSony, Error, TEXT("sceVideoRecordingSetInfo failed. Error code: 0x%08x"), Result);
			}
		}
		else
		{
			static const int DisabledVideo = SCE_VIDEO_RECORDING_CHAPTER_PROHIBIT;
			int32 Result = sceVideoRecordingSetInfo(SCE_VIDEO_RECORDING_INFO_CHAPTER, &DisabledVideo, sizeof(int32_t));
			if (Result != SCE_OK)
			{
				UE_LOG(LogSony, Error, TEXT("sceVideoRecordingSetInfo failed. Error code: 0x%08x"), Result);
			}
		}
	}
	else
	{
		UE_LOG(LogSony, Error, TEXT("FPS4ShareVideo not initialized"));
	}
}

bool FPS4ShareVideo::IsEnabled() const
{
	return bIsEnabled;
}

bool FPS4ShareVideo::OpenRecording()
{
	LLM_SCOPE(ELLMTag::VideoRecording);

	SceVideoRecordingParam2 Param;
	sceVideoRecordingParamInit2(&Param);
	if (Parameters.RecordingLengthSeconds > 0)
	{
		Param.ringSec = Parameters.RecordingLengthSeconds;
	}

	const int32 BufferSize = sceVideoRecordingQueryMemSize2(&Param);

	if (BufferSize == SCE_VIDEO_RECORDING_ERROR_INVALID_VALUE)
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::NewRecording: failed to get required buffer size."));
		return false;
	}

	RecordBuffer.SetNumUninitialized(BufferSize, true);

	UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::NewRecording: opening new recording with filename %s"), *CurrentRecordingFileName);

	auto ConvertedString = StringCast<ANSICHAR>(*CurrentRecordingFileName);

	const int32 OpenResult = sceVideoRecordingOpen2(ConvertedString.Get(), &Param, RecordBuffer.GetData(), RecordBuffer.Num());

	if (OpenResult != SCE_OK)
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::NewRecording: sceVideoRecordingOpen2 failed. Error code: 0x%08x"), OpenResult);
		return false;
	}

	return true;
}

void FPS4ShareVideo::NextRecording()
{
	if (Parameters.bAutoContinue)
	{
		CurrentRecordingFileName = FString::Printf(TEXT(SCE_VIDEO_RECORDING_PATH_GAME) TEXT("%s_%llu.mp4"), *BaseFileName, RecordingIndex++);
	}
	else
	{
		CurrentRecordingFileName = FString::Printf(TEXT(SCE_VIDEO_RECORDING_PATH_GAME) TEXT("%s.mp4"), *BaseFileName);
	}
}

bool FPS4ShareVideo::NewRecording(const TCHAR* DestinationFileName, FVideoRecordingParameters InParameters)
{
	SCOPE_CYCLE_COUNTER(STAT_VideoRecordingSystem_NewRecording);
	check(IsInGameThread());

	if (!bIsEnabled)
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::NewRecording: can't open a new recording, recording is disabled."));
		return false;
	}

	if (!bIsInitialized)
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::NewRecording: can't open a new recording, recording is not initialized."));
		return false;
	}

	if (RecordState != EVideoRecordingState::None)
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::NewRecording: can't open a new recording, one is already in progress."));
		return false;
	}

	if (InParameters.RecordingLengthSeconds > GetMaximumRecordingSeconds())
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::NewRecording: length greater than platform maximum of %ds."), GetMaximumRecordingSeconds());
		return false;
	}

	Parameters = InParameters;

	RecordingIndex = 0;
	BaseFileName = "recording";
	CyclesBeforePausing = 0;
	CurrentStartRecordingCycles = 0;

	if (DestinationFileName != nullptr)
	{
		BaseFileName = DestinationFileName;
	}

	NextRecording();
	if (OpenRecording())
	{
		RecordState = EVideoRecordingState::Paused;
		if (Parameters.bAutoStart)
		{
			StartRecording();
		}
		return true;
	}
	
	return false;
}

void FPS4ShareVideo::StartRecording()
{
	SCOPE_CYCLE_COUNTER(STAT_VideoRecordingSystem_StartRecording);
	LLM_SCOPE(ELLMTag::VideoRecording);
	check(IsInGameThread());

	if (RecordState != EVideoRecordingState::Paused)
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::StartRecording: can't start recording, invalid state."));
		return;
	}

	UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::StartRecording: starting a recording"));

	const int32 StartResult = sceVideoRecordingStart();

	if (StartResult != SCE_OK)
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::StartRecording: sceVideoRecordingStart failed. Error code: 0x%08x"), StartResult);
		FinalizeRecording(false, FText::GetEmpty(), FText::GetEmpty(), true);
		return;
	}

	RecordState = EVideoRecordingState::Starting;
	CurrentStartRecordingCycles = FPlatformTime::Cycles64();
}

void FPS4ShareVideo::PauseRecording()
{
	SCOPE_CYCLE_COUNTER(STAT_VideoRecordingSystem_PauseRecording);
	LLM_SCOPE(ELLMTag::VideoRecording);
	check(IsInGameThread());

	if (RecordState != EVideoRecordingState::Recording && RecordState != EVideoRecordingState::Pausing && RecordState != EVideoRecordingState::Starting)
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::PauseRecording: can't pause recording, invalid state."));
		return;
	}

	UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::PauseRecording: pausing a recording"));

	CyclesBeforePausing += FPlatformTime::Cycles64() - CurrentStartRecordingCycles;
	const int32 StopResult = sceVideoRecordingStop();

	if (StopResult != SCE_OK)
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::PauseRecording: sceVideoRecordingStop failed. Error code: 0x%08x"), StopResult);
		RecordState = EVideoRecordingState::Pausing;
		FinalizeRecording(false, FText::GetEmpty(), FText::GetEmpty(), true);
		return;
	}

	RecordState = EVideoRecordingState::Pausing;
}

float FPS4ShareVideo::GetCurrentRecordingSeconds() const
{
	return (FPlatformTime::Cycles64() - CurrentStartRecordingCycles + CyclesBeforePausing) * FPlatformTime::GetSecondsPerCycle();
}

void FPS4ShareVideo::FinalizeRecording(const bool bSaveRecording, const FText& Title, const FText& Comment, const bool bStopAutoContinue)
{
	SCOPE_CYCLE_COUNTER(STAT_VideoRecordingSystem_FinalizeRecording);
	check(IsInGameThread());

	if (RecordState == EVideoRecordingState::None)
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::FinalizeRecording: can't finalize recording, invalid state."));
		return;
	}

	if (RecordState != EVideoRecordingState::Paused && RecordState != EVideoRecordingState::Pausing)
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::FinalizeRecording: pausing a recording before closing it"));
		PauseRecording();
	}
	
	RecordState = EVideoRecordingState::Finalizing;

	// Closing the video and exporting it can take a few seconds, so do it in a task.
	FGraphEventRef CloseEvent = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateRaw(this, &FPS4ShareVideo::PS4VideoExportDelegate, bSaveRecording, Parameters.bAutoContinue && !bStopAutoContinue, Title, Comment),
		GET_STATID(STAT_FPS4ShareVideo_PS4VideoExportDelegate)
	);
}

void FPS4ShareVideo::FinalizeVideoOnGameThread(EVideoRecordingState NewState, uint64 NewStartCycles, FString OutputFilePath)
{
	RecordState = NewState;
	CurrentStartRecordingCycles = NewStartCycles;
	CyclesBeforePausing = 0;

	OnVideoRecordingFinalized.Broadcast(!OutputFilePath.IsEmpty(), OutputFilePath);
}

void FPS4ShareVideo::PS4VideoExportDelegate(bool bSaveRecording, bool bAutoContinue, FText Title, FText Comment)
{
	UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::PS4VideoExportDelegate: closing a recording"));
	LLM_SCOPE(ELLMTag::VideoRecording);

	const int32 CloseResult = sceVideoRecordingClose(bSaveRecording ? 0 : 1);
	EVideoRecordingState NewState = EVideoRecordingState::None;
	uint64 NewStartCycles = 0;

	FString OutputFilePath;
	if (CloseResult == SCE_OK)
	{
		FString PreviousRecordingFileName = CurrentRecordingFileName;

		if (bAutoContinue)
		{
			NextRecording();
			if (OpenRecording())
			{
				const int32 StartResult = sceVideoRecordingStart();
				if (StartResult == SCE_OK)
				{
					NewStartCycles = FPlatformTime::Cycles64();
					NewState = EVideoRecordingState::Recording;
				}
				else
				{
					sceVideoRecordingClose(1);
					UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::PS4VideoExportDelegate: sceVideoRecordingStart failed. Error code: 0x%08x"), StartResult);
				}
			}
		}

		if (bSaveRecording && Parameters.bExportToLibrary)
		{
			// Caller wants to save the video and close was successful
			auto ConvertedFilename = StringCast<ANSICHAR>(*PreviousRecordingFileName);
			ANSICHAR* ConvertedTitle = TCHAR_TO_UTF8(*Title.ToString());
			ANSICHAR* ConvertedComment = TCHAR_TO_UTF8(*Comment.ToString());

			FPS4ContentExportTask ExportTask;
			OutputFilePath = ExportTask.Start(ConvertedTitle, ConvertedComment, SCE_CONTENT_EXPORT_FORMAT_VIDEO_MP4, ConvertedFilename.Get());

			UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::PS4VideoExportDelegate: finished exporting %s."), *OutputFilePath);
		}
		else if (bSaveRecording)
		{
			OutputFilePath = PreviousRecordingFileName;
		}
	}
	else
	{
		UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::PS4VideoExportDelegate: sceVideoRecordingClose failed. Error code: 0x%08x"), CloseResult);
	}

	FGraphEventRef FinalizeEvent = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateRaw(this, &FPS4ShareVideo::FinalizeVideoOnGameThread, NewState, NewStartCycles, OutputFilePath),
		GET_STATID(STAT_FPS4ShareVideo_FinalizeVideoOnGameThread),
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
			UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::Tick: sceVideoRecordingGetStatus returned error code 0x%08x"), LibraryStatus);
			PauseRecording();
			FinalizeRecording(false, FText::GetEmpty(), FText::GetEmpty(), true);
			return;
		}

		// Update our state enum based on the library's progress.
		if (RecordState == EVideoRecordingState::Starting && LibraryStatus == SCE_VIDEO_RECORDING_STATUS_RUNNING)
		{
			UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::Tick: state change from Starting to Recording"));
			RecordState = EVideoRecordingState::Recording;
		}
		else if (RecordState == EVideoRecordingState::Pausing && LibraryStatus == SCE_VIDEO_RECORDING_STATUS_NONE)
		{
			UE_LOG(LogSony, Log, TEXT("PS4ShareVideo::Tick: state change from Pausing to Paused"));
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
			UE_LOG(LogSony, Error, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_VIDEO_RECORDING) failed. Error code: 0x%08x"), VideoRecordingResult);

			// Don't continue setting things up
			return false;
  		}

		// ContentExport is required to have videos show up for uploading via the system software.
		// ContentExport requires the user service library to be initialized.
		sceUserServiceInitialize(nullptr);
		const int32 ContentExportResult = sceSysmoduleLoadModule(SCE_SYSMODULE_CONTENT_EXPORT);
  
  		if (ContentExportResult != SCE_OK)
		{
			UE_LOG(LogSony, Error, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_CONTENT_EXPORT) failed. Error code: 0x%08x"), ContentExportResult);

			// Don't continue setting things up
			return false;
  		}

		// Set up allocator for ContentExport
		SceContentExportInitParam2 ContentExportInitParam = {};
		ContentExportInitParam.mallocfunc = &ContentExportMalloc;
		ContentExportInitParam.freefunc = &ContentExportFree;
		ContentExportInitParam.userdata = nullptr;
		ContentExportInitParam.bufsize = 0;
		sceContentExportInit2(&ContentExportInitParam);

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
			UE_LOG(LogSony, Error, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_CONTENT_EXPORT) failed. Error code: 0x%08x"), ContentExportResult);
  		}

		const int32 VideoRecordingResult = sceSysmoduleUnloadModule(SCE_SYSMODULE_VIDEO_RECORDING);
  		if (VideoRecordingResult != SCE_OK)
		{
			UE_LOG(LogSony, Error, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_VIDEO_RECORDING) failed. Error code: 0x%08x"), VideoRecordingResult);
  		}
		
		// Assume that we terminated and unloaded the library successfully
		bIsInitialized = false;
	}
}
