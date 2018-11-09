// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4TargetSettingsDetails.h"

#include "SExternalImageReference.h"
#include "EditorDirectories.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "UObject/UnrealType.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PS4TargetSettingsDetails"

TSharedRef<IDetailCustomization> FPS4TargetSettingsDetails::MakeInstance()
{
	return MakeShareable(new FPS4TargetSettingsDetails);
}

void FPS4TargetSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	AudioPluginManager.BuildAudioCategory(DetailBuilder, EAudioPlatform::Playstation4);
}

#undef LOCTEXT_NAMESPACE
