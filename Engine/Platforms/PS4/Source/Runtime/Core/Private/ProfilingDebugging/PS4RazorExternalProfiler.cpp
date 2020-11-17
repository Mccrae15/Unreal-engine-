// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/ExternalProfiler.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "Templates/UniquePtr.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Misc/CommandLine.h"

#if PS4_MEM_ENABLE_MAT
#include <mat.h>
#endif

#if SONY_PROFILING_ENABLED && UE_EXTERNAL_PROFILING_ENABLED

// Include PS4 SCE header for Razor performance profiling
#include <perf.h>


/**
 * PS4 Razer implementation of FExternalProfiler
 */
class FPS4RazorExternalProfiler : public FExternalProfiler
{

public:

	enum EAutoCaptureState
	{
		ERequestStart = 0,
		ERequestStop = 1,
		ECapturing = 2,
		EInactive = 3
	};

	enum EAutoCaptureMode
	{
		ECSV = 0,
		ENONE = 1
	};

	struct AutoCaptureSettings
	{
		AutoCaptureSettings()
		{
			Mode = ENONE;
			State = EInactive;
			FrameStart = 0;
			FrameEnd = 0xFFFFFFFF;
		}

		EAutoCaptureMode Mode;
		EAutoCaptureState State;
		int32 FrameStart;
		int32 FrameEnd;
	};

	/** Constructor */
	FPS4RazorExternalProfiler()
	{
		// Register as a modular feature
		IModularFeatures::Get().RegisterModularFeature( FExternalProfiler::GetFeatureName(), this );
	}


	/** Destructor */
	virtual ~FPS4RazorExternalProfiler()
	{
#if CSV_PROFILER
		if (CaptureSettings.Mode == ECSV && FCsvProfiler::Get())
		{
			FCsvProfiler::Get()->OnCSVProfileStart().Remove(OnRazorStartCaptureHandle);
			FCsvProfiler::Get()->OnCSVProfileEnd().Remove(OnRazorStopCaptureHandle);
		}
#endif // CSV_PROFILER
		IModularFeatures::Get().UnregisterModularFeature( FExternalProfiler::GetFeatureName(), this );
	}

	virtual void Register()
	{
		FString FrameRange;
		if (FParse::Value(FCommandLine::Get(), TEXT("-csvExtProfCpu="), FrameRange))
		{
			FString Start, End;
			if (FrameRange.Split(TEXT(":"), &Start, &End))
			{
				CaptureSettings.FrameStart = FCString::Atoi(*Start);
				CaptureSettings.FrameEnd = FCString::Atoi(*End);
			}
			CaptureSettings.Mode = ECSV;
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("-csvExtProfCpu")))
		{
			CaptureSettings.Mode = ECSV;
		}

		if (CaptureSettings.Mode == ECSV)
		{
#if CSV_PROFILER
			OnRazorStartCaptureHandle = FCsvProfiler::Get()->OnCSVProfileStart().AddRaw(this, &FPS4RazorExternalProfiler::StartDelegate);
			OnRazorStopCaptureHandle = FCsvProfiler::Get()->OnCSVProfileEnd().AddRaw(this, &FPS4RazorExternalProfiler::EndDelegate);
#endif // CSV_PROFILER
		}
	}

	/** Mark where the profiler should consider the frame boundary to be. */
	virtual void FrameSync() override
	{
		if( FPlatformMisc::IsRunningOnDevKit() )
		{
			sceRazorCpuSync();

			SyncCaptureMode();

#if PS4_MEM_ENABLE_MAT
			sceMatNewFrame();
#endif
		}
	}

	/** Gets the name of this profiler as a string.  This is used to allow the user to select this profiler in a system configuration file or on the command-line */
	virtual const TCHAR* GetProfilerName() const override
	{
		return TEXT( "PS4Razor" );
	}


	/** Pauses profiling. */
	virtual void ProfilerPauseFunction() override
	{
		if( FPlatformMisc::IsRunningOnDevKit() )
		{
			// @todo: Razor currently has no "Pause/Resume" API, so this will actually stop and finalize the profiling trace
			sceRazorCpuStopCapture();
		}
	}


	/** Resumes profiling. */
	virtual void ProfilerResumeFunction() override
	{
		if( FPlatformMisc::IsRunningOnDevKit() )
		{
			sceRazorCpuStartCapture();
		}
	}

	void StartDelegate()
	{
		if (CaptureSettings.Mode != ENONE)
		{
			CaptureSettings.State = ERequestStart;
		}
	}

	void EndDelegate()
	{
		if (CaptureSettings.Mode != ENONE)
		{
			CaptureSettings.State = ERequestStop;
		}
	}

private:

	void SyncCaptureMode()
	{
		if (CaptureSettings.Mode == ECSV)
		{
			// check if we need to start the capture
			if (CaptureSettings.State == ERequestStart)
			{
#if CSV_PROFILER
				if (FCsvProfiler::Get()->GetCaptureFrameNumber() >= CaptureSettings.FrameStart)
				{
					CSV_EVENT_GLOBAL(TEXT("Razor Capture Start"));
					ProfilerResumeFunction();
					CaptureSettings.State = ECapturing;
				}
#endif // CSV_PROFILER
			}
			// check if we need to stop on a request
			else if (CaptureSettings.State == ERequestStop && CaptureSettings.State == ECapturing)
			{
				CSV_EVENT_GLOBAL(TEXT("Razor Capture Stop"));
				ProfilerPauseFunction();
				CaptureSettings.State = EInactive;
			}
#if CSV_PROFILER
			// check if we need to stop on a frame range
			else if (FCsvProfiler::Get()->IsCapturing() && FCsvProfiler::Get()->GetCaptureFrameNumber() >= CaptureSettings.FrameEnd && CaptureSettings.State == ECapturing)
			{
				CSV_EVENT_GLOBAL(TEXT("Razor Capture Stop"));
				ProfilerPauseFunction();
				CaptureSettings.State = EInactive;
			}
#endif // CSV_PROFILER
		}
	}

	AutoCaptureSettings CaptureSettings;
	FDelegateHandle OnRazorStartCaptureHandle;
	FDelegateHandle OnRazorStopCaptureHandle;
};


namespace PS4RazorProfiler
{
	struct FAtModuleInit
	{
		FAtModuleInit()
		{
			static TUniquePtr<FPS4RazorExternalProfiler> ProfilerPS4Razor = MakeUnique<FPS4RazorExternalProfiler>();
		}
	};

	static FAtModuleInit AtModuleInit;
}

#endif	// SONY_PROFILING_ENABLED && UE_EXTERNAL_PROFILING_ENABLED
