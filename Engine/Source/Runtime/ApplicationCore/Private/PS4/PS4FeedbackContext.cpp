// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4FeedbackContext.h"

FPS4FeedbackContext::FPS4FeedbackContext()
	: FFeedbackContext()
{ }

void FPS4FeedbackContext::Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	if( !GLog->IsRedirectingTo( this ) )
	{
		GLog->Serialize( V, Verbosity, Category );
	}
}

bool FPS4FeedbackContext::YesNof(const FText& Question)
{
	if( ( GIsSilent != true ) && ( FApp::IsUnattended() != true ) )
	{
		FPlatformMisc::LowLevelOutputDebugStringf( *(Question.ToString()) );
		return false;
	}
	else
	{
		return false;
	}
}
