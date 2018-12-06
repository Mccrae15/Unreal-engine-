#include "AxesParamAssetFactory.h"

UAxesParamAssetFactory::UAxesParamAssetFactory(const class FObjectInitializer &OBJ) : Super(OBJ) {
	SupportedClass = UAxesParamAsset::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UAxesParamAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UAxesParamAsset::StaticClass()));
	return NewObject<UAxesParamAsset>(InParent, Class, Name, Flags | RF_Transactional, Context);
}