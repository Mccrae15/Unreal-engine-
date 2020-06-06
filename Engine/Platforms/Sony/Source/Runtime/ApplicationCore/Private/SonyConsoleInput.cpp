// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyConsoleInput.h"
#include "SonyApplication.h"
#include "HAL/RunnableThread.h"

#include <stdio.h>
#include <unistd.h>

#if !UE_BUILD_SHIPPING

/**
 * Reader thread waits on a kernel event queue.
 * When it's awoken it then pulls reads out of 
 * STDIN and reports back via task on the game
 * thread.
 */

FSonyConsoleInputReader::FSonyConsoleInputReader( int32 StdInHandle )
	: NewFileHandle( StdInHandle )
	, EventCount( INDEX_NONE )
	, Queue( nullptr )
{
}

uint32 FSonyConsoleInputReader::Run()
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

			SSIZE_T ReadSize = sceKernelRead( NewFileHandle, Buffer, AvailableData );
			if ( ReadSize > 0 )
			{
				Buffer[ ReadSize ] = '\0';
				TGraphTask< FSonyConsoleInputReporter >::CreateTask().ConstructAndDispatchWhenReady( FString( ANSI_TO_TCHAR( Buffer ) ) );
			}
			
			delete[] Buffer;
			Buffer = nullptr;
		}
	}

	Result = sceKernelDeleteReadEvent( Queue, NewFileHandle );
	SCE_DBG_ASSERT( Result == SCE_OK );

	return 0;
}

void FSonyConsoleInputReader::Stop()
{
	int32 Result = sceKernelDeleteEqueue( Queue );
	SCE_DBG_ASSERT( Result == SCE_OK );
}

void FSonyConsoleInputReader::Exit()
{

}

/**
 * Reporter is dispatched from the reader thread onto
 * the game thread with an FString of whatever the 
 * Reader was able to pull off of STDIN
 */
FSonyConsoleInputReporter::FSonyConsoleInputReporter( FString&& InCommand )
	: Command( InCommand.TrimEnd() )
{

}

void FSonyConsoleInputReporter::DoTask( ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent )
{
	checkf( CurrentThread == ENamedThreads::GameThread, TEXT( "This task can only safely be run on the game thread" ) );
	if ( FSonyApplication* Application = FSonyApplication::GetSonyApplication() )
	{
		Application->AddPendingConsoleCommand( Command );
	}
}

/**
 * Manager maintains ownership of the Reader thread.
 */
FSonyConsoleInputManager::FSonyConsoleInputManager()
	: InputReader( nullptr )
	, Thread( nullptr )
{
}

void FSonyConsoleInputManager::Initialize() 
{
	InputReader = new FSonyConsoleInputReader( STDIN_FILENO );
	Thread = FRunnableThread::Create( InputReader, TEXT( "SonyConsoleInputReader" ), 0, EThreadPriority::TPri_BelowNormal );
	checkf( Thread, TEXT( "Failed to create Ps4ConsoleInputReader thread" ) );
}

void FSonyConsoleInputManager::Finalize()
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
