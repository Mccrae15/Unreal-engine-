// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4ConsoleInput.h"
#include "PS4Application.h"
#include "HAL/RunnableThread.h"

#if PLATFORM_PS4
#include <stdio.h>
#include <unistd.h>
#endif

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST 

/**
 * Reader thread waits on a kernel event queue.
 * When it's awoken it then pulls reads out of 
 * STDIN and reports back via task on the game
 * thread.
 */

FPS4ConsoleInputReader::FPS4ConsoleInputReader( int32 StdInHandle )
	: NewFileHandle( StdInHandle )
	, EventCount( INDEX_NONE )
	, Queue( nullptr )
{
}

uint32 FPS4ConsoleInputReader::Run()
{
	int32 Result = SCE_OK;
	Result = sceKernelCreateEqueue( &Queue, "stdin_reader" );
	SCE_DBG_ASSERT( Result == SCE_OK );

	Result = sceKernelAddReadEvent( Queue, NewFileHandle, 1, nullptr );
	SCE_DBG_ASSERT( Result == SCE_OK );

	SceKernelEvent QueueEvent;
	while ( true )
	{
		Result = sceKernelWaitEqueue( Queue, &QueueEvent, 1, &EventCount, nullptr );
		if ( Result != SCE_KERNEL_ERROR_ETIMEDOUT )
		{
			intptr_t AvailableData = sceKernelGetEventData( &QueueEvent );
			char* Buffer = new char[ AvailableData + 1 ];

			SSIZE_T ReadSize = read( NewFileHandle, Buffer, AvailableData );
			if ( ReadSize > 0 )
			{
				Buffer[ ReadSize ] = '\0';
				TGraphTask< FPS4ConsoleInputReporter >::CreateTask().ConstructAndDispatchWhenReady( FString( ANSI_TO_TCHAR( Buffer ) ) );
			}
			
			delete[] Buffer;
			Buffer = nullptr;
		}
	}

	Result = sceKernelDeleteReadEvent( Queue, NewFileHandle );
	SCE_DBG_ASSERT( Result == SCE_OK );

	return 0;
}

void FPS4ConsoleInputReader::Stop()
{
	int32 Result = sceKernelDeleteEqueue( Queue );
	SCE_DBG_ASSERT( Result == SCE_OK );
}

void FPS4ConsoleInputReader::Exit()
{

}

/**
 * Reporter is dispatched from the reader thread onto
 * the game thread with an FString of whatever the 
 * Reader was able to pull off of STDIN
 */
FPS4ConsoleInputReporter::FPS4ConsoleInputReporter( FString&& InCommand )
	: Command( InCommand.TrimEnd() )
{

}

void FPS4ConsoleInputReporter::DoTask( ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent )
{
	checkf( CurrentThread == ENamedThreads::GameThread, TEXT( "This task can only safely be run on the game thread" ) );
	if ( FPS4Application* Application = FPS4Application::GetPS4Application() )
	{
		Application->AddPendingConsoleCommand( Command );
	}
}

/**
 * Manager maintains ownership of the Reader thread.
 */
FPS4ConsoleInputManager::FPS4ConsoleInputManager()
	: InputReader( nullptr )
	, Thread( nullptr )
{
}

void FPS4ConsoleInputManager::Initialize() 
{
	InputReader = new FPS4ConsoleInputReader( STDIN_FILENO );
	Thread = FRunnableThread::Create( InputReader, TEXT( "PS4ConsoleInputReader" ), 0, EThreadPriority::TPri_BelowNormal );
	checkf( Thread, TEXT( "Failed to create Ps4ConsoleInputReader thread" ) );
}

void FPS4ConsoleInputManager::Finalize()
{
	if ( Thread )
	{
		Thread->Kill( true );
		delete Thread;

		delete InputReader;
		InputReader = nullptr;
	}
}

#endif