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

#include "AxesParamAssetDetails.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "AxesParamAsset.h"
#include "Widgets/Input/SButton.h"
#include "MyCurveEditor.h"
#include "Components/Viewport.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "AdvancedPreviewScene.h"

DEFINE_LOG_CATEGORY_STATIC(_3dRudderDetails, Log, All)
#define LOCTEXT_NAMESPACE "AxesParamDetails"

TSharedRef<IDetailCustomization> FAxesParamAssetDetails::MakeInstance()
{
	return MakeShareable(new FAxesParamAssetDetails);
}

void FAxesParamAssetDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{

	static const FName BindingsCategory = TEXT("CustomParam");
	// Properties
	const TSharedPtr<IPropertyHandle> AxesParamTypePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAxesParamAsset, AxesParamType));
	const TSharedPtr<IPropertyHandle> NonSymmetricalPitchPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAxesParamAsset, NonSymmetricalPitch));
	const TSharedPtr<IPropertyHandle> RollToYawCompensationPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAxesParamAsset, RollToYawCompensation));
	const TSharedRef<IPropertyHandle> LeftRightPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAxesParamAsset, LeftRight));
	const TSharedRef<IPropertyHandle> ForwardBackwardPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAxesParamAsset, ForwardBackward));
	const TSharedRef<IPropertyHandle> UpDownPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAxesParamAsset, UpDown));
	const TSharedRef<IPropertyHandle> RotationPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAxesParamAsset, Rotation));
	// Visibility
	TAttribute<EVisibility> CustomVisibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FAxesParamAssetDetails::IsCustom, AxesParamTypePropertyHandle));
	// Get current asset
	TArray< TWeakObjectPtr<UObject> > SelectedObjects = DetailBuilder.GetSelectedObjects();
	UAxesParamAsset* asset = NULL;
	for (int i = 0; i < SelectedObjects.Num(); ++i)
	{
		asset = Cast<UAxesParamAsset>(SelectedObjects[i].Get());
		if (asset)
			break;
	}
	check(asset);
	// Category
	IDetailCategoryBuilder& CustomCurvesCategoryBuilder = DetailBuilder.EditCategory(TEXT("AxesParamAsset"));
	CustomCurvesCategoryBuilder.AddProperty(AxesParamTypePropertyHandle);
	CustomCurvesCategoryBuilder.AddProperty(NonSymmetricalPitchPropertyHandle).Visibility(CustomVisibility);
	CustomCurvesCategoryBuilder.AddProperty(RollToYawCompensationPropertyHandle).Visibility(CustomVisibility);	

	IDetailPropertyRow& rowLeftRight = CustomCurvesCategoryBuilder.AddProperty(LeftRightPropertyHandle).Visibility(CustomVisibility);
	CustomizeProperty(rowLeftRight, LeftRightPropertyHandle, asset, ns3dRudder::LeftRight);

	IDetailPropertyRow& rowForwardBackward = CustomCurvesCategoryBuilder.AddProperty(ForwardBackwardPropertyHandle).Visibility(CustomVisibility);
	CustomizeProperty(rowForwardBackward, ForwardBackwardPropertyHandle, asset, ns3dRudder::ForwardBackward);

	IDetailPropertyRow& rowUpDown = CustomCurvesCategoryBuilder.AddProperty(UpDownPropertyHandle).Visibility(CustomVisibility);
	CustomizeProperty(rowUpDown, UpDownPropertyHandle, asset, ns3dRudder::UpDown);

	IDetailPropertyRow& rowRotation = CustomCurvesCategoryBuilder.AddProperty(RotationPropertyHandle).Visibility(CustomVisibility);
	CustomizeProperty(rowRotation, RotationPropertyHandle, asset, ns3dRudder::Rotation);

	/*FDetailWidgetRow& custom = CustomCurvesCategoryBuilder.AddCustomRow(FText::FromString("Viewer 3D"));
	FSoftObjectPath model = FSoftObjectPath(FString("/3dRudder/3dR_Low_Poly_Unity_Ref3.3dR_Low_Poly_Unity_Ref3"));
	UObject* mesh = LoadObject<UObject>(nullptr, *model.ToString());
	
	custom.ValueContent()
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SAssignNew(ViewportWidget, SViewport)
		.IgnoreTextureAlpha(false)
		.EnableBlending(false)
	];

	ViewportClient = MakeShareable(new FUMGViewportClient(&PreviewScene));
	Viewport = MakeShareable(new FSceneViewport(ViewportClient.Get(), ViewportWidget));
	ViewportWidget->SetViewportInterface(Viewport.ToSharedRef());
	*/
	//PreviewScene.AddComponent(mest->Ge, , FTransform::Identity)
}

void FAxesParamAssetDetails::CustomizeProperty(IDetailPropertyRow& row, const TSharedRef<IPropertyHandle> PropertyHandle, UAxesParamAsset* asset, ns3dRudder::Axes axes)
{
	row.CustomWidget()
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(500.0f)
	.MaxDesiredWidth(0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()			
			.Padding(0, 5)		
			[
				SNew(SProperty, PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMyCurve, DeadZone)))				
			
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)			
			[
				SNew(SProperty, PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMyCurve, Sensitivity)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SProperty, PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMyCurve, Shape)))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(10, 0)
		[
			SNew(SMyCurveEditor)
			.Asset(asset)
			.Axes(axes)
		]
	];
}

EVisibility FAxesParamAssetDetails::IsCustom(TSharedPtr<IPropertyHandle> Property) const
{
	if (Property.IsValid())
	{
		uint8 ValueAsByte;
		FPropertyAccess::Result Result = Property->GetValue(/*out*/ ValueAsByte);

		if (Result == FPropertyAccess::Success)
		{
			return (((E3dRudderAxesParam)ValueAsByte) == E3dRudderAxesParam::Custom) ? EVisibility::Visible : EVisibility::Hidden;
		}
	}

	// If there are multiple values, show all properties
	return EVisibility::Visible;
}
#undef LOCTEXT_NAMESPACE