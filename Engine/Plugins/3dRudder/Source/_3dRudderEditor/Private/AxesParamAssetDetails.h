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
#include "IDetailCustomization.h"

class IDetailPropertyRow;
class SViewport;
class FUMGViewportClient;
class FPreviewScene;

class FAxesParamAssetDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;

	EVisibility IsCustom(TSharedPtr<IPropertyHandle> Property) const;

private:
	void CustomizeProperty(IDetailPropertyRow& row, const TSharedRef<IPropertyHandle> PropertyHandle, UAxesParamAsset* asset, ns3dRudder::Axes axes);
	/** Viewport widget*/
	TSharedPtr<SViewport> ViewportWidget;
	TSharedPtr<FUMGViewportClient> ViewportClient;
	TSharedPtr<FSceneViewport> Viewport;
	/** preview scene */
	FPreviewScene PreviewScene;
};