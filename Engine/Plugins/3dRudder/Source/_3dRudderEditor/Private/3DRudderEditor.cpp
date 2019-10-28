/************************************************************************************

	Copyright © 2014-2019, 3dRudder SA, All rights reserved
	For terms of use: https://3drudder-dev.com/docs/introduction/sdk_licensing_agreement/
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met :

	* Redistributions of source code must retain the above copyright notice, and
	this list of conditions.
	* Redistributions in binary form must reproduce the above copyright
	notice and this list of conditions.
	* The name of 3dRudder may not be used to endorse or promote products derived from
	this software without specific prior written permission.

    Copyright 1998-2019 Epic Games, Inc. All Rights Reserved

************************************************************************************/

#include "3dRudderEditor.h"
 
// Settings
#include "3dRudderSettings.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ISettingsContainer.h"

// Asset
#include "AxesParamAssetFactory.h"
#include "AxesParamAsset.h"
#include "AxesParamAssetDetails.h"
#include "AxesParamAssetCustomization.h"
#include "PropertyEditorModule.h"

// Editor
#include "EditorViewportClient.h"
#include "Editor.h"

IMPLEMENT_MODULE(F3dRudderEditorModule, _3dRudderEditor);

DEFINE_LOG_CATEGORY_STATIC(_3dRudderEditor, Log, All)
#define LOCTEXT_NAMESPACE "3dRudderEditor"
 
ns3dRudder::CSdk* F3dRudderEditorModule::s_pSdk;

void F3dRudderEditorModule::StartupModule()
{
	UE_LOG(_3dRudderEditor, Warning, TEXT("3dRudderEditor: Log Started"));

	// 3dRudder SDK
	ns3dRudder::ErrorCode error = ns3dRudder::LoadSDK(_3DRUDDER_SDK_LAST_COMPATIBLE_VERSION);
	if (error == ns3dRudder::Success && s_pSdk == nullptr)
	{
		s_pSdk = ns3dRudder::GetSDK();		
		s_pSdk->Init();
		UE_LOG(_3dRudderEditor, Log, TEXT("3dRudder version %x"), s_pSdk->GetSDKVersion());
		if (s_pSdk->GetSDKVersion() < _3DRUDDER_SDK_VERSION)
		{
			FText title = FText::FromString("Warning: 3dRudder SDK no up to date");
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Please, you have to update the 3dRudder dashboard"), &title);			
		}
		else if (s_pSdk->GetSDKVersion() > _3DRUDDER_SDK_VERSION)
		{
			FText title = FText::FromString("Warning: 3dRudder plugin no up to date");
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Please, you have to update the 3dRudder plugin"), &title);
		}
	}
	else
	{
		FString errorText = FString(ANSI_TO_TCHAR(ns3dRudder::GetErrorText(error)));
		FText title = FText::FromString("Warning: 3dRudder SDK failing to load");		
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(errorText), &title);
		FPlatformProcess::LaunchURL(TEXT("https://www.3drudder.com/start/"), NULL, NULL);
	}
	
	RegisterCustomizations();
	RegisterSettings();
	RegisterStyle();

	IAssetTools &AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetCategory3dRudder = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("3dRudder")), LOCTEXT("3dRudder", "3dRudder"));
	{
		TSharedRef<IAssetTypeActions> ACT_UMyItemsDatabase = MakeShareable(new FAxesParamAssetTypeActions);
		AssetTools.RegisterAssetTypeActions(ACT_UMyItemsDatabase);
	}
}

void F3dRudderEditorModule::RegisterStyle()
{
	// Create Slate style set.
	if (!StyleSet.IsValid())
	{
		// Create Slate style set.	
		StyleSet = MakeShareable(new FSlateStyleSet(TEXT("3dRudderStyle")));

		// Note, these sizes are in Slate Units. Slate Units do NOT have to map to pixels.
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon64x64(64.0f, 64.0f);
		//const FVector2D Icon128x128(128.0f, 128.0f);

		// TODO: use EnginePLuginsDir after EnginePluginsDir()
		static FString IconsDir = FPaths::ProjectPluginsDir() / TEXT("3dRudder/Resources/");

		// Register the Asset icon
		StyleSet->Set("ClassIcon.AxesParamAsset", new FSlateImageBrush(IconsDir + TEXT("Icon128.png"), Icon16x16));
		StyleSet->Set("ClassThumbnail.AxesParamAsset", new FSlateImageBrush(IconsDir + TEXT("Icon128.png"), Icon64x64));

		// Register Slate style.
		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
	}
}

void F3dRudderEditorModule::RegisterCustomizations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("AxesParamAsset", FOnGetDetailCustomizationInstance::CreateStatic(&FAxesParamAssetDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("AxesParamAsset", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAxesParamAssetCustomization::MakeInstance));
}

void F3dRudderEditorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		// Create the new category
		ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer("Editor");

		SettingsContainer->DescribeCategory("3dRudder",
			LOCTEXT("RuntimeWDCategoryName", "3dRudder"),
			LOCTEXT("RuntimeWDCategoryDescription", "Editor configuration for the Plugin 3dRudder module"));

		// Register the settings
		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Editor", "3dRudder", "Viewport",
			LOCTEXT("RuntimeGeneralSettingsName", "Viewport"),
			LOCTEXT("RuntimeGeneralSettingsDescription", "Move the camera in viewport with the 3dRudder controller"),
			GetMutableDefault<U3dRudderSettings>()
		);
		GetMutableDefault<U3dRudderSettings>()->LoadAxesParam();
	}
}

void F3dRudderEditorModule::Tick(float DeltaTime)
{		
	//UE_LOG(_3dRudderEditor, Warning, TEXT("tick %f"), DeltaTime);
	if (s_pSdk == nullptr)
		return;
	// Only one device (0)
	uint32 i = 0;
	if (s_pSdk->IsDeviceConnected(i))
	{
		// Axis : X, Y, Z, rZ
		ns3dRudder::AxesValue axesValue;		
		ns3dRudder::IAxesParam* pAxesParam = GetDefault<U3dRudderSettings>()->pAxesParam;
		if (pAxesParam != nullptr)
		{
			ns3dRudder::ErrorCode code = s_pSdk->GetAxes(i, pAxesParam, &axesValue);
			if (code == ns3dRudder::Success)
			{
				// Status of 3dRudder		
				ns3dRudder::Status status = s_pSdk->GetStatus(i);
				if (status == ns3dRudder::InUse && GetDefault<U3dRudderSettings>()->bActive)
				{
					U3dRudderSettings* settings = GetMutableDefault<U3dRudderSettings>();
					FVector speed = GetDefault<U3dRudderSettings>()->Translation;
					float speedRotation = GetDefault<U3dRudderSettings>()->Rotation;
					float angle = 0.0f;
					FVector trans = FVector(0,0,0);

					if (settings->Smooth.ForwardBackward.Enable)
						trans.X = settings->Smooth.ForwardBackward.ComputeSpeed(speed.X * axesValue.Get(ns3dRudder::ForwardBackward), DeltaTime);
					else
						trans.X = speed.X * axesValue.Get(ns3dRudder::ForwardBackward);

					if (settings->Smooth.LeftRight.Enable)
						trans.Y = settings->Smooth.LeftRight.ComputeSpeed(speed.Y * axesValue.Get(ns3dRudder::LeftRight), DeltaTime);
					else
						trans.Y = speed.Y * axesValue.Get(ns3dRudder::LeftRight);

					if (settings->Smooth.UpDown.Enable)
						trans.Z = settings->Smooth.UpDown.ComputeSpeed(speed.Z * axesValue.Get(ns3dRudder::UpDown), DeltaTime);
					else
						trans.Z = speed.Z * axesValue.Get(ns3dRudder::UpDown);

					if (settings->Smooth.Rotation.Enable)
						angle = settings->Smooth.Rotation.ComputeSpeed(speedRotation * axesValue.Get(ns3dRudder::Rotation), DeltaTime);
					else
						angle = speedRotation * axesValue.Get(ns3dRudder::Rotation);
					
					UpdateViewportCamera(trans, angle);
				}
			}
		}
	}
	else
	{
		ns3dRudder::ErrorCode error = s_pSdk->GetLastError();
		if (error > ns3dRudder::NotReady)
		{
			UE_LOG(_3dRudderEditor, Warning, TEXT("%s"), ANSI_TO_TCHAR(ns3dRudder::GetErrorText(error)));
		}
	}
}

void F3dRudderEditorModule::UpdateViewportCamera(const FVector& translation, float fRotation)
{
	//UE_LOG(_3dRudderEditor, Warning, TEXT("tick %f"), fRotation);
	if (translation.IsZero() && fRotation == 0)
		return;

	if (GEditor != nullptr && GEditor->GetActiveViewport() != nullptr && GEditor->GetActiveViewport()->GetClient() != nullptr)
	{
		FViewportClient*  client = GEditor->GetActiveViewport()->GetClient();
		FEditorViewportClient* Eclient = (FEditorViewportClient*)client;
		EWorldType::Type nType = client->GetWorld()->WorldType;

		if (client != nullptr && nType != EWorldType::PIE && !Eclient->Viewport->IsPlayInEditorViewport())
		{			
			
			// X Y local
			FVector local(translation.X, translation.Y, 0);
			FVector world = Eclient->GetViewRotation().RotateVector(local);
			// Z world
			world += FVector(0.0f, 0.0f, translation.Z);
			// Pitch Yaw Roll
			FRotator rotation(0, fRotation, 0);
			// Move Camera of Viewport with 3dRudder
			//Eclient->BeginCameraMovement(bHasMovement);
			Eclient->MoveViewportCamera(world, rotation);
			Eclient->Invalidate(true, true);
		}
	}
}

void F3dRudderEditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "3dRudder", "General");
	}
}

void F3dRudderEditorModule::UnregisterCustomizations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout("AxesParamAsset");
	PropertyModule.UnregisterCustomPropertyTypeLayout("AxesParamAsset");
}

void F3dRudderEditorModule::unRegisterStyle()
{
	// Unregister Slate style set.
	if (StyleSet.IsValid())
	{
		// Unregister Slate style.
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());

		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

void F3dRudderEditorModule::ShutdownModule()
{
	UE_LOG(_3dRudderEditor, Warning, TEXT("3dRudderEditor: Log Ended"));

	UnregisterSettings();
	UnregisterCustomizations();
	unRegisterStyle();

	if (s_pSdk != nullptr)
		s_pSdk->Stop();
}
 
#undef LOCTEXT_NAMESPACE