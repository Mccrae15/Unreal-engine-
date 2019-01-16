#pragma once

#include <CoreMinimal.h>
#include <Components/SceneComponent.h>
#include <MotionControllerComponent.h>
#include <Components/SceneCaptureComponent2D.h>
#include <openvr.h>
#include "Calibration.h"
#include "MixedRealityCanvas.h"
#include "MixedRealityRenderer.generated.h"

UENUM()
enum class ECapturePostprocessType : uint8
{
	NoPostprocess,
	BackgroundPostprocess,
	FullPostprocess,
};

UCLASS(ClassGroup = LIV, BlueprintType, meta = (BlueprintSpawnableComponent))
class LIV_API UMixedRealityRenderer : public USceneComponent
{
	GENERATED_BODY()

public:
	UMixedRealityRenderer();

	/** The type of postprocessing to be used. Full postprocessing uses additional resources. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LIV")
		ECapturePostprocessType CapturePostprocessType;

#ifdef UE_4_17_OR_LATER
	/** Header include for this property is necessary to allow UHT to succeed on older Unreal versions. */
	#include "MixedRealityRendererPrimitiveRenderMode.h"
#endif

	/** The components won't rendered by LIV.*/
	UPROPERTY()
		TArray<TWeakObjectPtr<UPrimitiveComponent>> HiddenComponents;

	/** The actors to hide in LIV. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LIV")
		TArray<AActor*> HiddenActors;

	/** The only components to be rendered by LIV, if PrimitiveRenderMode is set to UseShowOnlyList. */
	UPROPERTY()
		TArray<TWeakObjectPtr<UPrimitiveComponent>> ShowOnlyComponents;

	/** The only actors to be rendered by LIV, if PrimitiveRenderMode is set to UseShowOnlyList. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LIV")
		TArray<AActor*> ShowOnlyActors;

protected:
	UPROPERTY()
		USceneComponent* ExternalCamera;

	UPROPERTY()
		ACalibration* Calibration;

	// Background Scene Capture
	UPROPERTY()
		USceneCaptureComponent2D* BGCapture;

	// Foreground Scene Capture
	UPROPERTY()
		USceneCaptureComponent2D* FGCapture;

	// Postprocessed Foreground Scene Capture
	UPROPERTY()
		USceneCaptureComponent2D* PostFGCapture;

	UPROPERTY()
		UMixedRealityCanvas* MixedRealityCanvas;

	bool bWasProtocolActive = false;

	uint32_t OpenVRDeviceId = vr::k_unTrackedDeviceIndexInvalid;

	bool bIsInitialized = false;
	void Initialize();

	// Returns true if initialized.
	bool InitializeIfPossible();

	void CreateRenderTargets(int Width, int Height);
	void ReleaseRenderTargets();

	static bool IsOpenVRActive();
	FVector GetOpenVRHMDLocation() const;
	static uint32_t GetOpenVRDeviceIndex();
	static int GetOpenVRDeviceWeight(uint32_t DeviceIndex);
	static bool IsDeviceConnected(uint32_t DeviceIndex);

public:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
