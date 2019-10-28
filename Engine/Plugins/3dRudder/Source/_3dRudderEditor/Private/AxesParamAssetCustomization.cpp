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

#include "AxesParamAssetCustomization.h"
#include "PropertyHandle.h"
#include "AxesParamAsset.h"
#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "SCurveEditor.h"
#include "Curves/CurveFloat.h"
#include "MyCurveEditor.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "3dRudderCustom"

TSharedRef<IPropertyTypeCustomization> FAxesParamAssetCustomization::MakeInstance()
{
	return MakeShareable(new FAxesParamAssetCustomization());
}

FText FAxesParamAssetCustomization::GetRollToYawCompensation()
{ 
	return FText::Format(LOCTEXT("RollToYawCompensationStr", "Roll To Yaw Compensation: {0}"), FText::AsNumber(pAsset->RollToYawCompensation));
}

FText FAxesParamAssetCustomization::GetNonSymmetricalPitch()
{
	return FText::Format(LOCTEXT("NonSymmetricalPitchStr", "Non Symmetrical Pitch: {0}"), FText::FromString(pAsset->NonSymmetricalPitch ? "true" : "false"));
}

void FAxesParamAssetCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	UObject* Value;
	FPropertyAccess::Result R = StructPropertyHandle->GetValue(Value);
	pAsset = Cast<UAxesParamAsset>(Value);
	check(pAsset);

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0.0f)
	[
		SNew(SVerticalBox)		
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SObjectPropertyEntryBox)
			.PropertyHandle(StructPropertyHandle)
			.AllowedClass(UAxesParamAsset::StaticClass())
			.ThumbnailPool(StructCustomizationUtils.GetThumbnailPool())
			.OnObjectChanged(this, &FAxesParamAssetCustomization::OnObjectChanged)
		]
	];
}

void FAxesParamAssetCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TAttribute<FText> RollToYawCompensation = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(this, &FAxesParamAssetCustomization::GetRollToYawCompensation));
	TAttribute<FText> NonSymmetricalPitch = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(this, &FAxesParamAssetCustomization::GetNonSymmetricalPitch));	

	FMargin marge(0, 10, 0, 10);
	StructBuilder.AddCustomRow(LOCTEXT("AxesParam", "AxesParam"))
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0.0f)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(marge)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(NonSymmetricalPitch)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(RollToYawCompensation)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(marge)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString("Left / Right"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SMyCurveEditor)
				.Asset(pAsset)
				.Axes(ns3dRudder::LeftRight)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(marge)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString("Forward / Backward"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SMyCurveEditor)
				.Asset(pAsset)
				.Axes(ns3dRudder::ForwardBackward)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(marge)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString("Up / Down"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SMyCurveEditor)
				.Asset(pAsset)
				.Axes(ns3dRudder::UpDown)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(marge)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString("Rotation"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SMyCurveEditor)
				.Asset(pAsset)
				.Axes(ns3dRudder::Rotation)
			]
		]
	];
}

void FAxesParamAssetCustomization::OnObjectChanged(const FAssetData& InAssetData)
{
	pAsset = Cast<UAxesParamAsset>(InAssetData.GetAsset());
	check(pAsset);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.NotifyCustomizationModuleChanged();
}

#undef LOCTEXT_NAMESPACE