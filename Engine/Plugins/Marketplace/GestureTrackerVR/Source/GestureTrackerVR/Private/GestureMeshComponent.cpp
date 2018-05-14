// Copyright 2016-2017 Hunter Delattre. All Rights Reserved.

#include "GestureMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"

UGestureMeshComponent::UGestureMeshComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SetComponentTickEnabled(false);
    SetCastShadow(false);
    SetSimulatePhysics(false);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetMobility(EComponentMobility::Movable);
    SetCastShadow(false);

    ready = false;
}


// Called when the game starts
void UGestureMeshComponent::BeginPlay()
{
    Super::BeginPlay();
    
}


// Called every frame
void UGestureMeshComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
    Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

    // ...
}







/***********************************
* BP FUNCTIONS
************************************/

void UGestureMeshComponent::DestroyComponent(bool bPromoteChildren)
{
    for (int ii = 0; ii < pathMeshes.Num(); ii++) {
        if (pathMeshes[ii]) {
            pathMeshes[ii]->DestroyComponent();
        }
    }
    pathMeshes.Empty();
    Super::DestroyComponent(bPromoteChildren);
}

void UGestureMeshComponent::UpdateMaterials(UStaticMesh *mesh, UMaterialInterface *baseMaterial, UMaterialInterface *successMaterial)
{
    for (int ii = 0; ii < pathMeshes.Num(); ii++) {
        if (pathMeshes[ii]) {
            UMaterial *currentMaterial = pathMeshes[ii]->GetMaterial(0)->GetMaterial();

            // new mesh, reapply all materials
            if (mesh) {
                pathMeshes[ii]->SetStaticMesh(mesh);

                if (currentMaterial == this->successMaterial) {
                    if (successMaterial) {
                        pathMeshes[ii]->SetMaterial(0, successMaterial);
                    }
                    else {
                        pathMeshes[ii]->SetMaterial(0, this->successMaterial);
                    }
                }
                else if (baseMaterial) {
                    pathMeshes[ii]->SetMaterial(0, baseMaterial);
                }
            }
            // same mesh, only update necessary materials
            else {
                if (successMaterial && currentMaterial == this->successMaterial) {
                        pathMeshes[ii]->SetMaterial(0, successMaterial);
                }
                else if (baseMaterial) {
                    pathMeshes[ii]->SetMaterial(0, baseMaterial);
                }
            }
        }
    }
    if (mesh) {
        this->mesh = mesh;
    }
    if (baseMaterial) {
        this->baseMaterial = baseMaterial;
    }
    if (successMaterial) {
        this->successMaterial = successMaterial;
    }
}

void UGestureMeshComponent::Success()
{
    if (successMaterial) {
        for (int ii = 0; ii < pathMeshes.Num(); ii++) {
            if (pathMeshes[ii] && pathMeshes[ii]->GetMaterial(0)->GetMaterial() != successMaterial) {
                pathMeshes[ii]->SetMaterial(0, successMaterial);
            }
        }
    }
}






/***********************************
* Setup Functions
************************************/

void UGestureMeshComponent::Setup(UStaticMesh *mesh, UMaterialInterface *baseMaterial, UMaterialInterface *successMaterial, int maxLength)
{
    this->mesh = mesh;
    this->baseMaterial = baseMaterial;
    this->successMaterial = successMaterial;
    this->maxLength = maxLength;

    if (maxLength > 0) {
        this->pathMeshes.Init(NULL, maxLength);
    }
    else {
        this->pathMeshes.Reserve(64);
    }

    this->ClearSplinePoints();
    this->AddSplinePoint(FVector(0, 0, 0), ESplineCoordinateSpace::Local);

    drawStreamIndex = 0;

    this->ready = true;
}

USplineMeshComponent* UGestureMeshComponent::DrawSegment(int index, UMaterialInterface *material)
{
    FVector startLocation;
    FVector startTangent;
    FVector endLocation;
    FVector endTangent;

    // Start location & Tangent
    this->GetLocationAndTangentAtSplinePoint(index, startLocation, startTangent, ESplineCoordinateSpace::Local);

    // End location & Tangent
    this->GetLocationAndTangentAtSplinePoint(index + 1, endLocation, endTangent, ESplineCoordinateSpace::Local);

    // Store data to build element
    USplineMeshComponent *retVal = NewObject<USplineMeshComponent>(this);
    retVal->SetStaticMesh(mesh);
    if (material) {
        retVal->SetMaterial(0, material);
    }
    retVal->SetMobility(EComponentMobility::Movable);
    retVal->SetComponentTickEnabled(false);
    retVal->SetCastShadow(false);
    retVal->SetStaticMesh(mesh);
    retVal->SetStartAndEnd(startLocation, startTangent, endLocation, endTangent);
    retVal->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
    retVal->RegisterComponent();
    return retVal;
}


// For tracked gestures

void UGestureMeshComponent::StreamGesture(const Gesture &gesture)
{
    FVector newSegment = gesture.PathHead();
     
    int lastSplineIndex = this->GetNumberOfSplinePoints();

    // only update if new head is different
    if (newSegment != this->GetLocationAtSplinePoint(lastSplineIndex, ESplineCoordinateSpace::Local)) {
        
        // add new gesture points to spline
        this->AddSplinePoint(newSegment, ESplineCoordinateSpace::Local);

        // get index to draw from last spline point to the newly added one
        int drawIndex = this->GetNumberOfSplinePoints() - 2;

        // redraw last segment with the proper end tangent
        USplineMeshComponent*& pathMesh = pathMeshes[drawStreamIndex];
        if (pathMesh) {
            pathMesh->DestroyComponent();
            pathMesh = DrawSegment(drawIndex - 1, baseMaterial);
        }

        // remove beginning of spline if past max length
        if (lastSplineIndex > maxLength) {
            this->RemoveSplinePoint(0);
            drawIndex--;
        }

        // update draw indices
        drawStreamIndex = (drawStreamIndex + 1) % maxLength;

        // destroy old segment (the tail) and replace with new one (the head)
        if (pathMeshes[drawStreamIndex]) {
            pathMeshes[drawStreamIndex]->DestroyComponent();
        }
        pathMeshes[drawStreamIndex] = DrawSegment(drawIndex, baseMaterial);
    }
}



// For predicted gestures

void UGestureMeshComponent::SetDrawPath(const RecordGesture &drawGesture)
{
    if (ready) {
        this->SetSplinePoints(drawGesture.Path(), ESplineCoordinateSpace::Local);
    }
}

void UGestureMeshComponent::UpdateDrawToIndex(int index)
{
    if (ready) {
        int drawLength = FMath::Min(this->GetNumberOfSplinePoints() - 1, index);

        for (int ii = pathMeshes.Num(); ii < drawLength; ii++) {
            pathMeshes.Add(DrawSegment(ii, baseMaterial));
        }
    }
}


void UGestureMeshComponent::UpdateProgress(int TrackIndex, int updateDist)
{
    UpdateDrawToIndex(TrackIndex + updateDist);

    if (successMaterial) {
        int drawLength = pathMeshes.Num() - 1;
        // Update recognized path
        for (int ii = FMath::Min(drawLength, TrackIndex); ii >= 0; ii--) {
            USplineMeshComponent*& pathMesh = pathMeshes[ii];
            if (pathMesh) {
                // if success segment then break
                if (pathMesh->GetMaterial(0)->GetMaterial() == successMaterial) {
                    break;
                }
                // make this segment a success segment
                pathMesh->SetMaterial(0, successMaterial);
            }
            else {
                pathMesh = DrawSegment(ii, successMaterial);
            }
        }
        // Update unrecognized path
        for (int ii = FMath::Max(0, TrackIndex); ii <= drawLength; ii++) {
            USplineMeshComponent*& pathMesh = pathMeshes[ii];
            if (pathMesh) {
                // if path segment then break 
                if (pathMesh->GetMaterial(0)->GetMaterial() != successMaterial) {
                    break;
                }
                // make this segment a path segment
                if (baseMaterial) {
                    pathMesh->SetMaterial(0, baseMaterial);
                }
                else {
                    pathMesh->SetMaterial(0, mesh->GetMaterial(0));
                }
            }
            else {
                pathMesh = DrawSegment(ii, baseMaterial);
            }
        }
    }
}



void UGestureMeshComponent::DrawGesture(const RecordGesture &drawGesture)
{
    if (ready) {
        this->SetSplinePoints(drawGesture.Path(), ESplineCoordinateSpace::Local);

        UpdateDrawToIndex(this->GetNumberOfSplinePoints());
    }
}



void UGestureMeshComponent::Erase()
{
    this->ClearSplinePoints();
    for (int ii = 0; ii < pathMeshes.Num(); ii++) {
        USplineMeshComponent*& pathMesh = pathMeshes[ii];
        if (pathMesh) {
            pathMesh->DestroyComponent();
            pathMesh = NULL;
        }
    }
}