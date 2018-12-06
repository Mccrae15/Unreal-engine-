#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

struct FAssetData;
class SCurveEditor;

/** Customize the appearance of an FSlateSound */
class FAxesParamAssetCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	FText GetRollToYawCompensation();
	FText GetNonSymmetricalPitch();

	UAxesParamAsset* GetAsset() const { return pAsset; }

protected:
	/** Called when the resource object used by this FSlateSound has been changed */
	void OnObjectChanged(const FAssetData&);
	
	UAxesParamAsset* pAsset;
};
