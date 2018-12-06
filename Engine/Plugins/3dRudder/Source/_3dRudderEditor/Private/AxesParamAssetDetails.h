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