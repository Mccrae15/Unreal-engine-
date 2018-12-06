#pragma once

#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "AssetTypeActions_Base.h"
#include "AxesParamAsset.h"
#include "AxesParamAssetFactory.generated.h"

static EAssetTypeCategories::Type AssetCategory3dRudder;

UCLASS()
class UAxesParamAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()
protected:
	virtual bool IsMacroFactory() const { return false; }
public:
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};

class FAxesParamAssetTypeActions : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override { return NSLOCTEXT("3dRudder", "AxesParams", "Axes Params");}
	virtual uint32 GetCategories() override { return AssetCategory3dRudder; }
	virtual FColor GetTypeColor() const override { return FColor(51, 197, 171); }
	virtual FText GetAssetDescription(const FAssetData &AssetData) const override { return NSLOCTEXT("3dRudder", "AxesParamsDesc", "Contains custom parameters and curves"); }
	virtual UClass* GetSupportedClass() const override { return UAxesParamAsset::StaticClass(); }	
};