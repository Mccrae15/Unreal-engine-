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

#include "MyCurveEditor.h"
#include "Widgets/Layout/SBorder.h"
#include "EditorStyleSet.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"
#include "SCurveEditor.h"
#include "3dRudderEditor.h"

DEFINE_LOG_CATEGORY_STATIC(_3dRudderCurve, Log, All)
#define LOCTEXT_NAMESPACE "3dRudderCurve"

const FLinearColor SMyCurveEditor::DEADZONE_COLOR = FLinearColor(0.0f, 0.0f, 1.0f, 0.25f);
const FLinearColor SMyCurveEditor::SATURATIONZONE_COLOR = FLinearColor(1.0f, 0.0f, 0.0f, 0.25f);
const FLinearColor SMyCurveEditor::CURVE_COLOR = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
const float StepSize = 1.0f / 50.0f;

void SMyCurveEditor::Construct(const FArguments& InArgs)
{
	pAsset = InArgs._Asset;
	Axes = InArgs._Axes;

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.DesiredSizeScale(FVector2D(1.0f, 5.0f))
		.Padding(0.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
		]
	];

}

int32 SMyCurveEditor::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Rendering info
	bool bEnabled = ShouldBeEnabled(bParentEnabled);
	ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FSlateBrush* TimelineAreaBrush = FEditorStyle::GetBrush("CurveEd.TimelineArea");
	const FSlateBrush* WhiteBrush = FEditorStyle::GetBrush("WhiteTexture");

	FGeometry CurveAreaGeometry = AllottedGeometry;

	// Draw background to indicate valid timeline area
	FTrackScaleInfo ScaleInfo(-1.0f, 1.0f, -1.0f, 1.0f, CurveAreaGeometry.GetLocalSize());
	float ZeroInputX = ScaleInfo.InputToLocalX(0.f);
	float MinInputX = ScaleInfo.InputToLocalX(-1.f);
	float MaxInputX = ScaleInfo.InputToLocalX(1.f);
	float ZeroOutputY = ScaleInfo.OutputToLocalY(0.f);

	// timeline background
	int32 BackgroundLayerId = LayerId;
	float TimelineMaxX = ScaleInfo.InputToLocalX(1.0f);
	FSlateDrawElement::MakeBox
	(
		OutDrawElements, BackgroundLayerId,
		CurveAreaGeometry.ToPaintGeometry(FVector2D(MinInputX, 0), FVector2D(CurveAreaGeometry.Size.X, CurveAreaGeometry.GetLocalSize().Y)),
		TimelineAreaBrush, DrawEffects, TimelineAreaBrush->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
	);

	// time=0 line
	int32 ZeroLineLayerId = BackgroundLayerId + 1;
	TArray<FVector2D> ZeroLinePoints;
	ZeroLinePoints.Add(FVector2D(ZeroInputX, 0));
	ZeroLinePoints.Add(FVector2D(ZeroInputX, CurveAreaGeometry.GetLocalSize().Y));
	FSlateDrawElement::MakeLines(OutDrawElements, ZeroLineLayerId, AllottedGeometry.ToPaintGeometry(), ZeroLinePoints, DrawEffects, FLinearColor::White, false);

	// value=0 line
	FSlateDrawElement::MakeBox
	(
		OutDrawElements, ZeroLineLayerId, CurveAreaGeometry.ToPaintGeometry(FVector2D(0, ZeroOutputY), FVector2D(CurveAreaGeometry.Size.X, 1)),
		WhiteBrush, DrawEffects, WhiteBrush->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
	);
	
	int32 CurveLayerId = ZeroLineLayerId + 1;
	int32 InfoLayerId = CurveLayerId + 1;
	TArray<FVector2D> LinePoints;
	

	ns3dRudder::IAxesParam* pAxesParam = pAsset->GetAxesParam();
	if (pAxesParam != nullptr && F3dRudderEditorModule::s_pSdk != nullptr)
	{		
		pAxesParam->UpdateParam(pAsset->Test ? pAsset->PortNumber : -1);
		ns3dRudder::Curve* curve = pAxesParam->GetCurve(Axes);
		
		float StartX = -1.0f;
		float EndX = 1.0f;
		for (; StartX < EndX; StartX += StepSize)
		{
			float CurveOut = F3dRudderEditorModule::s_pSdk->CalcCurveValue(curve->GetDeadZone(), curve->GetXSat(), curve->GetExp(), StartX);
			LinePoints.Add(FVector2D(ScaleInfo.InputToLocalX(StartX), ScaleInfo.OutputToLocalY(CurveOut)));
		}
		
		FSlateDrawElement::MakeLines(OutDrawElements, CurveLayerId, AllottedGeometry.ToPaintGeometry(), LinePoints, DrawEffects, SMyCurveEditor::CURVE_COLOR * InWidgetStyle.GetColorAndOpacityTint());
		LinePoints.Empty();

		// DeadZone		
		float DeadZone = ZeroInputX - ScaleInfo.InputToLocalX(curve->GetDeadZone());
		FSlateDrawElement::MakeBox
		(
			OutDrawElements, InfoLayerId,
			CurveAreaGeometry.ToPaintGeometry(FVector2D(DeadZone * 2.0f, CurveAreaGeometry.Size.Y), FSlateLayoutTransform(FVector2D(ZeroInputX - DeadZone, 0))),			
			WhiteBrush, DrawEffects, SMyCurveEditor::DEADZONE_COLOR * InWidgetStyle.GetColorAndOpacityTint()
		);

		// Raw value of 3dRudder
		if (pAsset->Test)
		{
			ns3dRudder::AxesValue axes;
			ns3dRudder::AxesParamNormalizedLinear axesParamNormalized;
			ns3dRudder::ErrorCode error = F3dRudderEditorModule::s_pSdk->GetAxes(pAsset->PortNumber, &axesParamNormalized, &axes);
			if (error == ns3dRudder::Success)
			{
				float value = axes.Get(Axes);
				float valueNonSymm = value;
				// tricks for non symmetrical pitch
				if (pAsset->NonSymmetricalPitch && Axes == ns3dRudder::ForwardBackward)
					valueNonSymm = CalcNonSymmetricalPitch(pAsset->PortNumber, value, curve);
				//valueNormalized = FMath::Max(0.0f, valueNormalized);
				float valueNormalized = ScaleInfo.InputToLocalX(valueNonSymm);
				LinePoints.Add(FVector2D(valueNormalized, 0));
				LinePoints.Add(FVector2D(valueNormalized, CurveAreaGeometry.GetLocalSize().Y));
				FSlateDrawElement::MakeLines(OutDrawElements, InfoLayerId, AllottedGeometry.ToPaintGeometry(), LinePoints, DrawEffects, FLinearColor::Red, false);

				// Debug info axes
				FString TimeStr = FString::Printf(TEXT("x:%.2f/y:%.2f"), valueNonSymm, F3dRudderEditorModule::s_pSdk->CalcCurveValue(curve->GetDeadZone(), curve->GetXSat(), curve->GetExp(), valueNonSymm));
				FSlateDrawElement::MakeText(OutDrawElements, InfoLayerId, AllottedGeometry.MakeChild(FVector2D(0.0, 0.0), FVector2D(1.0f, 1.0f)).ToPaintGeometry(), TimeStr,
					FCoreStyle::GetDefaultFontStyle("Regular", 8), DrawEffects, FLinearColor::White);
			}
		}

		// SaturationZone
		float SaturationZone = FMath::Max(MaxInputX - ScaleInfo.InputToLocalX(curve->GetXSat()), 0.0f);
		// Left
		FSlateDrawElement::MakeBox
		(
			OutDrawElements, InfoLayerId,
			CurveAreaGeometry.ToPaintGeometry(FVector2D(SaturationZone, CurveAreaGeometry.Size.Y), FSlateLayoutTransform(FVector2D(0, 0))),
			WhiteBrush, DrawEffects, SMyCurveEditor::SATURATIONZONE_COLOR * InWidgetStyle.GetColorAndOpacityTint()
		);
		// Right
		FSlateDrawElement::MakeBox
		(
			OutDrawElements, InfoLayerId,
			CurveAreaGeometry.ToPaintGeometry(FVector2D(SaturationZone, CurveAreaGeometry.Size.Y), FSlateLayoutTransform(FVector2D(MaxInputX - SaturationZone, 0))),
			WhiteBrush, DrawEffects, SMyCurveEditor::SATURATIONZONE_COLOR * InWidgetStyle.GetColorAndOpacityTint()
		);		
	}
	return InfoLayerId + 1;
}

float SMyCurveEditor::CalcNonSymmetricalPitch(uint32 nPortNumber, float fNormalizedV, ns3dRudder::Curve *pCurve) const
{	
	if (F3dRudderEditorModule::s_pSdk != nullptr && F3dRudderEditorModule::s_pSdk->GetStatus(nPortNumber) == ns3dRudder::InUse)
	{
		ns3dRudder::DeviceInformation *pInfo = F3dRudderEditorModule::s_pSdk->GetDeviceInformation(nPortNumber);

		ns3dRudder::AxesValue axes;
		F3dRudderEditorModule::s_pSdk->GetUserOffset(nPortNumber, &axes);

		float fPitchOffset = axes.Get(ns3dRudder::ForwardBackward) / pInfo->GetMaxPitch();		
																						
		float reduced_amplitude = 1;													

		if ((fPitchOffset > 0) && (fNormalizedV > 0))									
			reduced_amplitude = 1 - fPitchOffset;										
		else if ((fPitchOffset < 0) && (fNormalizedV < 0))								
			reduced_amplitude = 1 + fPitchOffset;

#define MIN_AMPLITUDE 0.1f														
		if (reduced_amplitude < MIN_AMPLITUDE)
			reduced_amplitude = MIN_AMPLITUDE;

		if (reduced_amplitude < pCurve->GetXSat())										
			fNormalizedV = fNormalizedV / reduced_amplitude;
	}
	return fNormalizedV;
}
#undef LOCTEXT_NAMESPACE