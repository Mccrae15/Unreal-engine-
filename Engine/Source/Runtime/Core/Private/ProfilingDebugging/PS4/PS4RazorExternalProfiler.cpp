// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/ExternalProfiler.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "Templates/ScopedPointer.h"
#include "Templates/UniquePtr.h"

#if PS4_MEM_ENABLE_MAT
#include <mat.h>
#endif

#if PS4_PROFILING_ENABLED && UE_EXTERNAL_PROFILING_ENABLED

// Include PS4 SCE header for Razor performance profiling
#include <perf.h>


/**
 * PS4 Razer implementation of FExternalProfiler
 */
class FPS4RazorExternalProfiler : public FExternalProfiler
{

public:

	/** Constructor */
	FPS4RazorExternalProfiler()
	{
		// Register as a modular feature
		IModularFeatures::Get().RegisterModularFeature( FExternalProfiler::GetFeatureName(), this );
	}


	/** Destructor */
	virtual ~FPS4RazorExternalProfiler()
	{
		IModularFeatures::Get().UnregisterModularFeature( FExternalProfiler::GetFeatureName(), this );
	}

	/** Mark where the profiler should consider the frame boundary to be. */
	virtual void FrameSync() override
	{
		if( FPlatformMisc::IsRunningOnDevKit() )
		{
			sceRazorCpuSync();

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

private:

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

#endif	// PS4_PROFILING_ENABLED && UE_EXTERNAL_PROFILING_ENABLED
