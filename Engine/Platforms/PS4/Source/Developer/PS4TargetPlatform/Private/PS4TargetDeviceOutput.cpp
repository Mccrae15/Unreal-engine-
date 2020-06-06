// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4TargetDeviceOutput.h"
#include "HAL/Runnable.h"
#include "HAL/PlatformProcess.h"


class FPS4TargetDeviceOutputReaderRunnable : public FRunnable
{
public:
	FPS4TargetDeviceOutputReaderRunnable( const FString& InHostName, FOutputDevice* InOutput )
		: HostName(InHostName)
		, Output(InOutput)
		, bStopRequest(false)
	{
	}

	virtual void Stop() override
	{
		bStopRequest = true;
	}

	virtual uint32 Run() override
	{
		// spawn logging process and capture the output
		void* ReadPipe;
		void* WritePipe;
		FPlatformProcess::CreatePipe( ReadPipe, WritePipe );

		FString Params = FString::Printf(TEXT(" console user \"%s\""), *HostName);
		FProcHandle RunningProc = FPlatformProcess::CreateProc( TEXT( "orbis-ctrl" ), *Params, true, false, false, NULL, 0, NULL, WritePipe );

		// keep logging the captured output until it is time to stop
		FString PartialLine;
		while( FPlatformProcess::IsProcRunning(RunningProc) && !bStopRequest )
		{
			LogPipeToOutput(ReadPipe, PartialLine);
			FPlatformProcess::Sleep( 0.25 );
		}

		//make sure the logging process is terminated and clean up
		if( FPlatformProcess::IsProcRunning(RunningProc) )
		{
			FPlatformProcess::TerminateProc(RunningProc);
		}

		FPlatformProcess::CloseProc( RunningProc );
		FPlatformProcess::ClosePipe( ReadPipe, WritePipe );

		return 0;
	}

	void LogPipeToOutput( void* InReadPipe, FString& InOutPartialLine ) const
	{
		FString NewLine = FPlatformProcess::ReadPipe( InReadPipe );
		if( NewLine.Len() > 0 )
		{
			//add previous incomplete line to the start of the new data
			if( InOutPartialLine.Len() > 0 )
			{
				NewLine = InOutPartialLine + NewLine;
				InOutPartialLine = "";
			}

			// process the string to break it up in to lines
			TArray<FString> StringArray;
			int32 NumLines = NewLine.ParseIntoArray( StringArray, TEXT( "\n" ), true );
			if( NumLines > 0 )
			{
				//output all lines except the last one
				for( int32 Index = 0; Index < NumLines-1; ++Index )
				{
					Output->Serialize( *StringArray[Index], ELogVerbosity::Log, NAME_None );
				}

				//only output the last line if the newly read string has a carriage return, otherwise save it for next time as the line is incomplete
				if( NewLine.EndsWith( TEXT("\n") ) )
				{
					Output->Serialize( *StringArray[NumLines-1], ELogVerbosity::Log, NAME_None );
				}
				else
				{
					InOutPartialLine = StringArray[NumLines-1];
				}
			}
		}
	}

private:

	FString HostName;
	FOutputDevice* Output;
	FEvent* StopRequestEvent;
	bool bStopRequest;
};







bool FPS4TargetDeviceOutput::Init( const FString& HostName, FOutputDevice* Output)
{
	check(Output);
	// Output will be produced by background thread
	check(Output->CanBeUsedOnAnyThread());
	
	auto* Runnable = new FPS4TargetDeviceOutputReaderRunnable( HostName, Output );
	DeviceOutputThread = TUniquePtr<FRunnableThread>(FRunnableThread::Create(Runnable, TEXT("FPS4DeviceOutputReaderRunnable"), 0, TPri_BelowNormal ));
	return true;
}

