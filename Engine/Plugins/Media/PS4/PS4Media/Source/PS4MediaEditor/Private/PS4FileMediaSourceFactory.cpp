// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4FileMediaSourceFactory.h"

#include "FileMediaSource.h"
#include "UObject/UObjectGlobals.h"


/* UPS4FileMediaSourceFactory structors
 *****************************************************************************/

UPS4FileMediaSourceFactory::UPS4FileMediaSourceFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Formats.Add(TEXT("3gpp;3GPP Multimedia File"));
	Formats.Add(TEXT("aac;MPEG-2 Advanced Audio Coding File"));
	Formats.Add(TEXT("mp4;MPEG-4 Movie"));

	SupportedClass = UFileMediaSource::StaticClass();
	bEditorImport = true;
}


/* UFactory overrides
 *****************************************************************************/

bool UPS4FileMediaSourceFactory::FactoryCanImport(const FString& Filename)
{
	return true;
}


UObject* UPS4FileMediaSourceFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UFileMediaSource* MediaSource = NewObject<UFileMediaSource>(InParent, InClass, InName, Flags);
	MediaSource->SetFilePath(CurrentFilename);

	return MediaSource;
}
