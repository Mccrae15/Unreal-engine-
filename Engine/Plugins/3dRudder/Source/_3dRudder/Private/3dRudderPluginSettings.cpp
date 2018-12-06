
#include "3dRudderPluginSettings.h"

DEFINE_LOG_CATEGORY_STATIC(_3dRudderPluginSettings, Log, All)
#define LOCTEXT_NAMESPACE "3dRudderPluginSettings"

U3dRudderPluginSettings::U3dRudderPluginSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{	
	AxesParamsClassName = FSoftObjectPath(FString("/3dRudder/Default.Default"));
	LoadAxesParam();
}

void U3dRudderPluginSettings::LoadAxesParam()
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
void U3dRudderPluginSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
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