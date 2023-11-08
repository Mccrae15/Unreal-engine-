/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * Licensed under the Oculus SDK License Agreement (the "License");
 * you may not use the Oculus SDK except in compliance with the License,
 * which is provided at the time of installation or download, or which
 * otherwise accompanies this software in either electronic or hard copy form.
 *
 * You may obtain a copy of the License at
 *
 * https://developer.oculus.com/licenses/oculussdk/
 *
 * Unless required by applicable law or agreed to in writing, the Oculus SDK
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Alexander Borsboom
 * Date: July 2023
 */

// A class to render the currently connected controller.
// Similar to how hands are tracked.

#pragma once
#include "OculusXRInputFunctionLibrary.h"
#include "OculusXRFunctionLibrary.h"
#include "Components/StaticMeshComponent.h"
#include <Engine/StreamableManager.h>

// Must always be the last include.
#include "OculusXRControllerComponent.generated.h"

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = OculusHand)
class UOculusXRControllerComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UOculusXRControllerComponent();

	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The skeleton that will be loaded */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Properties")
	EOculusXRSide SkeletonType;

	/** Should this controller be rendered when using controller driven hand poses */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Properties")
	bool RenderWhenUsingControllerDrivenHands;

private:
	enum MeshLoadingState
	{
		None,
		Loading,
		Loaded
	};

	UStaticMesh* _runtimeMesh;
	MeshLoadingState _meshLoadingState;
	TSharedPtr<FStreamableHandle> _loadAssetHandle;
	FStreamableManager _streamableManager;
	EOculusXRControllerType _controllerType;
	FSoftObjectPath _runtimeMeshPath;
	EOculusXRControllerDrivenHandPoseTypes _cachedControllerHandType;

	void InitializeMesh();
	void MeshLoaded();
	EOculusXRControllerType GetControllerType();

	const FVector PositionOffsets
		[EOculusXRSideCount]
		[EOculusXRControllerDrivenHandPoseTypesCount];
	const FVector RotationOffsets
		[EOculusXRSideCount]
		[EOculusXRControllerDrivenHandPoseTypesCount];
};
