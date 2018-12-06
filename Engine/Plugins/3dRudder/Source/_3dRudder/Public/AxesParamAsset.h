#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Engine/DataAsset.h"
#define _3DRUDDER_SDK_STATIC
#include "3DRudderSDK.h"
#include "AxesParamAsset.generated.h"

USTRUCT()
struct FMyCurve 
{
	GENERATED_USTRUCT_BODY()
		FMyCurve();
		FMyCurve(float deadzone, float sensitivity, float shape);

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "0.99"), Category = "Rudder")
		float DeadZone;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "2.0"), Category = "Rudder")
		float Sensitivity;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.1", ClampMax = "5.0"), Category = "Rudder")
		float Shape;
};

USTRUCT(Blueprintable)
struct FSmoothFactor
{
	GENERATED_USTRUCT_BODY()

		UPROPERTY(EditAnywhere, Category = "Rudder")
		bool Enable;
	UPROPERTY(EditAnywhere, meta = (EditCondition = "Enable"), Category = "Rudder")
		float Smoothness;

	float CurrentSpeed;

	FSmoothFactor()
	{
		Enable = false;
		Smoothness = 0.15f;
		CurrentSpeed = 0.0f;
	}

	float ComputeSpeed(float input, float deltatime)
	{
		float speedTarget = input; // m/s            
		float acceleration = (speedTarget - CurrentSpeed) / Smoothness; // m/s²            
		CurrentSpeed = CurrentSpeed + (acceleration * deltatime); // m/s            
		return CurrentSpeed;
	}
};

USTRUCT(Blueprintable)
struct FSmoothMovement
{
	GENERATED_USTRUCT_BODY()

		UPROPERTY(EditAnywhere, Category = "Rudder")
		FSmoothFactor LeftRight;

	UPROPERTY(EditAnywhere, Category = "Rudder")
		FSmoothFactor ForwardBackward;

	UPROPERTY(EditAnywhere, Category = "Rudder")
		FSmoothFactor UpDown;

	UPROPERTY(EditAnywhere, Category = "Rudder")
		FSmoothFactor Rotation;
};

/** Defines the Axes Param type*/
UENUM(BlueprintType)
enum class E3dRudderAxesParam : uint8
{
	Default = 0,
	NormalizedLinear,
	Custom,
};

/**
 * Axes param asset for 3dRudder controller
 */
UCLASS(Category = "3dRudder", BlueprintType, Blueprintable)
class _3DRUDDER_API UAxesParamAsset :  public UDataAsset
{
	GENERATED_BODY()

public:

	UAxesParamAsset(const FObjectInitializer& ObjectInitializer);
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;	

	// UObject overrides
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	ns3dRudder::IAxesParam* GetAxesParam() const { return pAxesParam; }

	UPROPERTY(EditAnywhere, Category = "Rudder")
		E3dRudderAxesParam AxesParamType;

	UPROPERTY(EditAnywhere, Category = "Rudder")
		bool NonSymmetricalPitch;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"), Category = "Rudder")
		float RollToYawCompensation;

	UPROPERTY(EditAnywhere, Category = "Rudder")
		FMyCurve LeftRight;

	UPROPERTY(EditAnywhere, Category = "Rudder")
		FMyCurve ForwardBackward;

	UPROPERTY(EditAnywhere, Category = "Rudder")
		FMyCurve UpDown;

	UPROPERTY(EditAnywhere, Category = "Rudder")
		FMyCurve Rotation;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Rudder")
		bool Test;
	
	UPROPERTY(EditAnywhere, AdvancedDisplay, meta = (EditCondition="Test", ClampMin = "0", ClampMax = "4"), Category = "Rudder")
		uint32 PortNumber;

private:
	void CreateAxesParam();
	ns3dRudder::IAxesParam* pAxesParam;
};

class _3DRUDDER_API CAxesParamCustom : public ns3dRudder::IAxesParam
{
public:
	CAxesParamCustom(UAxesParamAsset* passet);	
	ns3dRudder::ErrorCode UpdateParam(uint32_t nPortNumber);

private:
	UAxesParamAsset* asset;
	ns3dRudder::Curve m_Curve[ns3dRudder::MaxAxes];
};