#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AxesParamAsset.h"
#include "3dRudderPluginSettings.generated.h"

/**
* Setting object used to hold both config settings and editable ones in one place
* To ensure the settings are saved to the specified config file make sure to add
* props using the globalconfig or config meta.
*/
UCLASS(config = Input, defaultconfig)
class U3dRudderPluginSettings : public UObject
{
	GENERATED_BODY()

public:

	U3dRudderPluginSettings(const FObjectInitializer& ObjectInitializer);
	void LoadAxesParam();

	// UObject overrides
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif	

	/** Path to the axes param asset*/
	UPROPERTY(config, EditAnywhere, meta = (AllowedClasses = "AxesParamAsset", DisplayName = "Input Axes Param"), Category = "Rudder")
		FSoftObjectPath AxesParamsClassName;

	ns3dRudder::IAxesParam* pAxesParam;
};