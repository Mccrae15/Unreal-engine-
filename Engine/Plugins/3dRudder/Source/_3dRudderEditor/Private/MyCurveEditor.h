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

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UAxesParamAsset;
class ns3dRudder::Curve;

class SMyCurveEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMyCurveEditor)
		: _Asset(NULL)
	{}

	SLATE_ARGUMENT(ns3dRudder::Axes, Axes)
	SLATE_ARGUMENT(UAxesParamAsset*, Asset)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);	

	//~ Begin SWidget Interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

protected:
	// current 3dRudder
	uint32 PortNumber;
	// current axes
	ns3dRudder::Axes Axes;
	// current asset
	UAxesParamAsset*	pAsset;

	float CalcNonSymmetricalPitch(uint32 nPortNumber, float fNormalizedV, ns3dRudder::Curve *pCurve) const;

	static const FLinearColor DEADZONE_COLOR;
	static const FLinearColor SATURATIONZONE_COLOR;
	static const FLinearColor CURVE_COLOR;
};
