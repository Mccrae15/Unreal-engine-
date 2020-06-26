// Copyright 2016-2017 Hunter Delattre. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Private/RecordGesture.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "GestureMeshComponent.generated.h"


UCLASS( ClassGroup=(GestureTrackerVR) )
class GESTURETRACKERVR_API UGestureMeshComponent : public USplineComponent
{
    GENERATED_BODY()

public:
    UPROPERTY()
    TArray<USplineMeshComponent*> pathMeshes;

    // Sets default values for this component's properties
    UGestureMeshComponent();

    // Called when the game starts
    virtual void BeginPlay() override;
    
    // Called every frame
    virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

    virtual void DestroyComponent(bool bPromoteChildren = false) override;

    /**
    * Update the draw mesh and materials with new ones.
    * Leave an input blank to leave it unchanged.
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    void UpdateMaterials(UStaticMesh *mesh, UMaterialInterface *baseMaterial, UMaterialInterface *successMaterial);

    /**
    * Applies the sucessMaterial to the entire Gesture mesh
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    void Success();
    
    void Setup(UStaticMesh *mesh, UMaterialInterface *baseMaterial, UMaterialInterface *successMaterial, int maxLength);

    USplineMeshComponent* DrawSegment(int index, UMaterialInterface *material);

    /**
    * Tracked Draw
    */
    void StreamGesture(const Gesture &gesture);

    /**
    * Predictive Draw
    */
    void SetDrawPath(const RecordGesture &drawGesture);

    void UpdateDrawToIndex(int index);

    void UpdateProgress(int TrackIndex, int updateDist);


    /**
    * Record Gesture Draw
    */
    void DrawGesture(const RecordGesture &drawGesture);


    /**
    * General Draw Functions
    */
    void Erase();


private:
    bool ready;

    int maxLength;
    int drawStreamIndex;

    UPROPERTY()
    UStaticMesh *mesh = NULL;
    UPROPERTY()
    UMaterialInterface *baseMaterial = NULL;
    UPROPERTY()
    UMaterialInterface *successMaterial = NULL;
    
};
