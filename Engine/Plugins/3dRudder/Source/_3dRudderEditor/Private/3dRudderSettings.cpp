
#include "3dRudderSettings.h"

DEFINE_LOG_CATEGORY_STATIC(_3dRudderSettings, Log, All)
#define LOCTEXT_NAMESPACE "3dRudderSettings"

U3dRudderSettings::U3dRudderSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bActive = false;
	Translation = FVector::OneVector;
	Rotation = 1.0f;
	AxesParamsClassName = FSoftObjectPath(FString("/3dRudder/Default.Default"));
	LoadAxesParam();
}

void U3dRudderSettings::LoadAxesParam()
{
	if (AxesParamsClassName.IsValid())
	{
		UAxesParamAsset* pAxesParamAsset = LoadObject<UAxesParamAsset>(nullptr, *AxesParamsClassName.ToString());
		if (pAxesParamAsset != nullptr)
			pAxesParam = pAxesParamAsset->GetAxesParam();
		else
		{
			AxesParamsClassName = FSoftObjectPath(FString("/3dRudder/Default.Default"));
			pAxesParamAsset = LoadObject<UAxesParamAsset>(nullptr, *AxesParamsClassName.ToString());
			pAxesParam = pAxesParamAsset->GetAxesParam();
		}
	}
}

#if WITH_EDITOR
void U3dRudderSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	//UE_LOG(_3DRudderEditor, Warning, TEXT("property change: %s"), *PropertyChangedEvent.Property->GetNameCPP());
	if (Name.ToString() == "AxesParamsClassName")
	{		
		LoadAxesParam();
	}
}
#endif

#undef LOCTEXT_NAMESPACE