// Copyright 2016-2017 Hunter Delattre. All Rights Reserved.

#include "GestureTracker.h"
#include "ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

// Sets default values for this component's properties
UGestureTracker::UGestureTracker()
    : gestureLib({ acceptableSimilarity, unacceptableSimilarity, smoothing, patience, trackFailDistance, continuousFailDistance, completionPercentage, predictPercentage })
{
    PrimaryComponentTick.bCanEverTick = true;
    bVisible = false;
    bAutoActivate = true;

    // Load default draw meshes if none are supplied
    if (!trackMesh) {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> trackMeshObj(TEXT("/GestureTrackerVR/TrackMesh.TrackMesh"));
        if (trackMeshObj.Succeeded()) {
            trackMesh = (UStaticMesh*)trackMeshObj.Object;
        }
    }
    if (!predictMesh) {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> predictMeshObj(TEXT("/GestureTrackerVR/PredictMesh.PredictMesh"));
        if (predictMeshObj.Succeeded()) {
            predictMesh = (UStaticMesh*)predictMeshObj.Object;
        }
    }
    if (!predictSuccessMaterial) {
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> predictSuccessMaterialObj(TEXT("/GestureTrackerVR/PredictSuccessMaterial.PredictSuccessMaterial"));
        if (predictSuccessMaterialObj.Succeeded()) {
            predictSuccessMaterial = (UMaterialInterface*)predictSuccessMaterialObj.Object;
        }
    }

    drawIndex = INDEX_NONE;
    trackGestureMesh = NULL;
    predictGestureMesh = NULL;

    bTracking = false;
    bRecording = false;
    bContinuous = false;

    bDrawingTracked = false;
    bDrawingPredicted = false;
}


// Called when the game starts
void UGestureTracker::BeginPlay()
{
    Super::BeginPlay();

}


// Called every frame
void UGestureTracker::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bTracking) {
        Track();
    }
}






/***************************
* BP Functions
****************************/

void UGestureTracker::StartRecording(bool force)
{
    if (force || !bTracking) {
        InitTracking(true, false);
    }
}

void UGestureTracker::FinishRecording(EGestureRecordResult &result, int &assignedId, int id, const FString &name)
{
    if (!bRecording) {
        assignedId = NULL_ID;
        result = EGestureRecordResult::Failed;
    }
    else {
        EndTracking();
        int gestureLength = recordGesture.Size() * gestureResolution;
        if (gestureLength >= minGestureLength && gestureLength <= maxGestureLength) {
            if (id < 0) {
                assignedId = gestureLib.GetAvailableId();
            }
            else {
                assignedId = id;
            }
            StoreRecording(assignedId, name);
            result = EGestureRecordResult::Recorded;
        }
        else {
            assignedId = NULL_ID;
            result = EGestureRecordResult::Failed;
        }
    }
}

void UGestureTracker::StartRecognition(const TArray<int> &trackIds, bool trackIdsIsBlacklist, bool force)
{
    if (force || !bTracking) {
        gestureLib.SetTrackIds(TSet<int>(trackIds), trackIdsIsBlacklist);
        InitTracking(false, false);
    }
}

void UGestureTracker::FinishRecognition(EGestureRecognizeResult &result, int &id, FString &name, float &recognitionDuration, float &lengthRatio)
{
    if (bTracking && !bContinuous) {
        EndTracking();

        recognitionDuration = GetTimeSeconds() - trackStartTime;

        Invoke(id, name);
        if (id >= 0) {
            result = EGestureRecognizeResult::Recognized;
            lengthRatio = GetLengthRatio(trackGesture, gestureIndex);
            return;
        }
    }
    id = NULL_ID;
    name = "";
    recognitionDuration = 0.f;
    lengthRatio = 0.f;
    result = EGestureRecognizeResult::Failed;
}

void UGestureTracker::StartContinuousRecognition(const TArray<int> &trackIds, bool trackIdsIsBlacklist, bool force, bool resetOnRecognized)
{
    if (!bTracking || force) {
        gestureLib.SetTrackIds(TSet<int>(trackIds), trackIdsIsBlacklist);
        bContinuousResetOnRecognized = resetOnRecognized;
        InitTracking(false, true);
    }
}

void UGestureTracker::StopContinuousRecognition()
{
    if (bTracking && bContinuous) {
        EndTracking();
    }
}

bool UGestureTracker::SaveGestures(const FString &filename, EGestureLibFunctionResult &result)
{
    bool success = gestureLib.Save(filename);
    if (success) {
        result = EGestureLibFunctionResult::Success;
    }
    else {
        result = EGestureLibFunctionResult::Failed;
    }
    return success;
}

bool UGestureTracker::LoadGestures(const FString &filename, EGestureLibFunctionResult &result)
{
    EndTracking();

    bool success = gestureLib.Load(filename);
    if (success) {
        result = EGestureLibFunctionResult::Success;
    }
    else {
        result = EGestureLibFunctionResult::Failed;
    }
    return success;
}

void UGestureTracker::ClearGestures()
{
    if (!bRecording) {
        EndTracking();
    }

    gestureLib.Empty();
}

int UGestureTracker::GetNumGestures() const
{
    return gestureLib.Num();
}

TArray<int> UGestureTracker::GetIds() const
{
    return gestureLib.GetIds();
}

TArray<FVector> UGestureTracker::GetGesturePath(int id) const
{
    int gestureIndex = gestureLib.GetIndexById(id);
    if (gestureIndex == INDEX_NONE) {
        return TArray<FVector>();
    }
    return gestureLib.Get(gestureIndex).Path();
}

TArray<FVector> UGestureTracker::GetPredictedGesturePath() const
{
    int gestureIndex = gestureLib.MostLikely();
    if (gestureIndex == INDEX_NONE) {
        return TArray<FVector>();
    }
    return gestureLib.Get(gestureIndex).Path();
}

void UGestureTracker::StartDrawTrackedGesture(UStaticMesh *mesh, UMaterialInterface *baseMaterial)
{
    bDrawingTracked = true;
    if (mesh) {
        trackMesh = mesh;
    }
    if (baseMaterial) {
        trackBaseMaterial = baseMaterial;
    }
}

void UGestureTracker::StopDrawTrackedGesture()
{
    ResetGestureMesh(trackGestureMesh);
    bDrawingTracked = false;
}

void UGestureTracker::StartDrawPredictedGesture(UStaticMesh *mesh, UMaterialInterface *baseMaterial, UMaterialInterface *successMaterial, const TArray<int> &drawIds, bool drawIdsIsBlacklist)
{
    bDrawingPredicted = true;
    if (mesh) {
        predictMesh = mesh;
    }
    if (baseMaterial) {
        predictBaseMaterial = baseMaterial;
    }
    if (successMaterial) {
        predictSuccessMaterial = successMaterial;
    }
    this->drawIds = TSet<int>(drawIds);
    this->bDrawIdsIsBlacklist = drawIdsIsBlacklist;
}

void UGestureTracker::StopDrawPredictedGesture()
{
    ResetGestureMesh(predictGestureMesh);
    bDrawingPredicted = false;
}


void UGestureTracker::DrawGestureById(int id, UStaticMesh *mesh, EGestureLibFunctionResult &result, UGestureMeshComponent *&drawResult)
{
    UpdateDrawLocation();
    UpdateTrackRotation();

    int namedIndex = gestureLib.GetIndexById(id);
    if (namedIndex >= 0) {
        drawResult = MakeGestureMesh(predictDrawStartLocation, trackRotation, mesh, NULL, NULL);
        drawResult->DrawGesture(gestureLib.Get(namedIndex));
        result = EGestureLibFunctionResult::Success;
    }
    else {
        drawResult = NULL;
        result = EGestureLibFunctionResult::Failed;
    }
}

void UGestureTracker::DrawGestureByName(const FString &name, UStaticMesh *mesh, EGestureLibFunctionResult &result, UGestureMeshComponent *&drawResult)
{
    UpdateDrawLocation();
    UpdateTrackRotation();

    int namedIndex = gestureLib.GetIndexByName(name);
    if (namedIndex >= 0) {
        drawResult = MakeGestureMesh(predictDrawStartLocation, trackRotation, mesh, NULL, NULL);
        drawResult->DrawGesture(gestureLib.Get(namedIndex));
        result = EGestureLibFunctionResult::Success;
    }
    else {
        drawResult = NULL;
        result = EGestureLibFunctionResult::Failed;
    }
}

void UGestureTracker::UpdateTrackDrawMaterials(UStaticMesh *mesh, UMaterialInterface *baseMaterial)
{
    if (mesh) {
        this->trackMesh = mesh;
    }
    if (baseMaterial) {
        this->trackBaseMaterial = baseMaterial;
    }
    if (trackGestureMesh) {
        trackGestureMesh->UpdateMaterials(mesh, baseMaterial, NULL);
    }
}

void UGestureTracker::UpdatePredictDrawMaterials(UStaticMesh *mesh, UMaterialInterface *baseMaterial, UMaterialInterface *successMaterial)
{
    if (mesh) {
        this->predictMesh = mesh;
    }
    if (baseMaterial) {
        this->predictBaseMaterial = baseMaterial;
    }
    if (successMaterial) {
        this->predictSuccessMaterial = successMaterial;
    }
    if (predictGestureMesh) {
        predictGestureMesh->UpdateMaterials(mesh, baseMaterial, successMaterial);
    }
}

void UGestureTracker::UpdateTrackIds(const TArray<int> trackIds, bool trackIdsIsBlacklist)
{
    gestureLib.SetTrackIds(TSet<int>(trackIds), trackIdsIsBlacklist);
}

void UGestureTracker::UpdateDrawIds(const TArray<int> drawIds, bool drawIdsIsBlacklist)
{
    this->drawIds = TSet<int>(drawIds);
    this->bDrawIdsIsBlacklist = drawIdsIsBlacklist;

    // clear the current prediction draw if it is now allowed by the new drawIds
    if (drawIdsIsBlacklist == this->drawIds.Contains(drawIndex)) {
        ResetGestureMesh(predictGestureMesh);
    }
}

int UGestureTracker::GetPredictedGestureId() const
{
    int mostLikelyIndex = gestureLib.MostLikely();
    if (mostLikelyIndex == INDEX_NONE) {
        return NULL_ID;
    }
    else {
        return gestureLib.Id(gestureLib.MostLikely());
    }
}

float UGestureTracker::GetPercentageComplete(int id) const
{
    int gestureIndex = gestureLib.GetIndexById(id);

    if (gestureIndex == INDEX_NONE) {
        return 0.f;
    }
    else {
        return gestureLib.PercentageComplete(gestureIndex);
    }
}





/*************************
* Private Functions
**************************/


// Gesture Tracking Functions

void UGestureTracker::InitTracking(bool record, bool continuous)
{
    this->bRecording = record;
    this->bContinuous = continuous;

    UpdateTrackLocation();
    UpdateTrackRotation();

    // reset gesture objects and search
    trackGesture.Reset(trackLocation, trackRotation, gestureResolution);
    if (record) {
        recordGesture.Reset(trackLocation, trackRotation, gestureResolution, FMath::CeilToInt(maxGestureLength / gestureResolution));
    }

    gestureLib.ResetSearch();
    gestureIndex = INDEX_NONE;

    // continuous tracking setup
    if (continuous) {
        continuousTracks.SetNum(gestureLib.Num());

        for (int ii = 0; ii < continuousTracks.Num(); ii++) {
            continuousTracks[ii].Reset(trackLocation, trackRotation, gestureResolution, GetTimeSeconds());
        }
    }

    // reset drawing
    ResetGestureMesh(trackGestureMesh);
    ResetGestureMesh(predictGestureMesh);
    UpdateDrawLocation();

    this->trackStartTime = GetTimeSeconds();

    this->bTracking = true;
}

void UGestureTracker::Track()
{
    UpdateTrackLocation();
    bool trackPathUpdated = trackGesture.AddToPath(trackLocation);

    // Update the tracked draw mesh
    if (bDrawingTracked && trackPathUpdated) {
        DrawTrackGestureMesh();
    }

    if (bContinuous) {
        ContinuousRecognitionUpdate();
    }
    else if (trackPathUpdated) {
        if (bRecording) {
            recordGesture.AddToPath(trackLocation);
        }
        else {
            RecognitionUpdate();
        }
    }

    // Update the predicted draw mesh
    if (bDrawingPredicted && trackPathUpdated) {
        DrawMostLikely();
    }
}

void UGestureTracker::RecognitionUpdate()
{
    int searchResultIndex = gestureLib.Search(trackGesture, trackRecognitionRatio * trackGesture.PathHeadLength());

    if (searchResultIndex >= 0) {
        GestureRecognized.Broadcast(gestureLib.Id(searchResultIndex), gestureLib.Name(searchResultIndex), GetTimeSeconds() - trackStartTime, GetLengthRatio(trackGesture, searchResultIndex));
        gestureIndex = searchResultIndex;
    }
}

void UGestureTracker::ContinuousRecognitionUpdate()
{
    UpdateTrackRotation();

    int maxTracks = FMath::Min(continuousTracks.Num(), gestureLib.Num());
    for (int ii = 0; ii < maxTracks; ii++) {

        // Update tracking
        if (!gestureLib.Banned(ii) && continuousTracks[ii].gesture.AddToPath(trackLocation)) {

            const Gesture& trackedGesture = continuousTracks[ii].gesture;

            // if bContinuousResetOnRecognized is false then we need to check finished gestures for disqualification
            if (gestureLib.Finished(ii)) {
                gestureLib.CheckForDisqualification(ii, trackedGesture);
            }
            else if (gestureLib.ContinuousTrack(ii, trackedGesture, continuousRecognitionRatio * trackedGesture.PathHeadLength())) {
                // Broadcast the action id of a successful continuous gesture
                ContinuousGestureRecognized.Broadcast(gestureLib.Id(ii), gestureLib.Name(ii), GetTimeSeconds() - continuousTracks[ii].startTime, GetLengthRatio(continuousTracks[ii].gesture, ii));

                // Reset search immediately instead of waiting for disqualification
                if (bContinuousResetOnRecognized) {
                    gestureLib.ResetTracking(ii);
                    continuousTracks[ii].Reset(trackLocation, trackRotation, gestureResolution, GetTimeSeconds());
                }

                // Reset draw locations
                UpdateDrawLocation();

                // Reset predictive draw mesh, erasing it
                ResetGestureMesh(predictGestureMesh);

                break;
            }

            // If tracking did not result in a recognized gesture then check if it was disqualified, reset tracking if so
            if (gestureLib.Disqualified(ii)) {
                gestureLib.ResetTracking(ii);
                continuousTracks[ii].Reset(trackLocation, trackRotation, gestureResolution, GetTimeSeconds());
            }
        }
    }
}

void UGestureTracker::EndTracking()
{
    if (bContinuous) {
        continuousTracks.Empty();
    }

    ResetGestureMesh(trackGestureMesh);
    ResetGestureMesh(predictGestureMesh);

    bTracking = false;
    bRecording = false;
    bContinuous = false;
}

void UGestureTracker::StoreRecording(int id, const FString &name)
{
    gestureLib.Add(id, name, recordGesture);
}

void UGestureTracker::Invoke(int &id, FString &name)
{
    // gesture search successful check
    if (!bRecording && gestureIndex >= 0 && !gestureLib.Disqualified(gestureIndex)) {
        id = gestureLib.Id(gestureIndex);
        name = gestureLib.Name(gestureIndex);
    }
    else {
        id = NULL_ID;
        name = "";
    }
}







// Drawing Functions

void UGestureTracker::DrawTrackGestureMesh()
{
    if (trackGestureMesh) {
        trackGestureMesh->StreamGesture(trackGesture);
    }
    else {
        trackGestureMesh = MakeGestureMesh(trackDrawStartLocation, trackGesture.Rotator(), trackMesh, trackBaseMaterial, NULL, FMath::RoundToInt(maxTrackDrawDistance / gestureResolution));
        trackGestureMesh->StreamGesture(trackGesture);
    }
}


void UGestureTracker::DrawPredictGestureMesh(int index)
{
    // Only draw if valid index and not in blacklist/in whitelist
    if (bDrawIdsIsBlacklist != drawIds.Contains(gestureLib.Id(index))) {
        // Same gesture predicted, update draw
        if (predictGestureMesh && drawIndex == index) {
            if (predictSuccessMaterial) {
                predictGestureMesh->UpdateProgress(gestureLib.TrackIndex(index), FMath::RoundToInt(predictDistance / gestureLib.Resolution(index)));
            }
        }
        // New gesture predicted, start new draw
        else {
            drawIndex = index;
            ResetGestureMesh(predictGestureMesh);

            FRotator rotation;
            if (bContinuous) {
                predictDrawStartLocation = continuousTracks[index].gesture.StartLocation();
                rotation = continuousTracks[index].gesture.Rotator();
            }
            else {
                rotation = trackGesture.Rotator();
            }

            predictGestureMesh = MakeGestureMesh(predictDrawStartLocation, rotation, predictMesh, predictBaseMaterial, predictSuccessMaterial);
            predictGestureMesh->SetDrawPath(gestureLib.Get(index));
        }
    }
}

void UGestureTracker::DrawMostLikely()
{
    int mostLikelyIndex = gestureLib.MostLikely();

    if (mostLikelyIndex < 0 || (bContinuous && gestureLib.Finished(mostLikelyIndex))) {
        ResetGestureMesh(predictGestureMesh);
    }
    else {
        DrawPredictGestureMesh(mostLikelyIndex);
    }
}

UGestureMeshComponent* UGestureTracker::MakeGestureMesh(const FVector &location, const FRotator &rotation, UStaticMesh *mesh, UMaterialInterface *baseMaterial, UMaterialInterface *successMaterial, int maxLength)
{
    UGestureMeshComponent *retVal = NewObject<UGestureMeshComponent>(this);
    retVal->RegisterComponent();

    if (bUseWorldLocation) {
        retVal->SetWorldLocation(location);
        retVal->SetRelativeRotation(rotation);
    }
    else {
        retVal->AttachToComponent(this->GetAttachmentRoot(), FAttachmentTransformRules::KeepWorldTransform);
        retVal->SetWorldLocation(location + this->GetAttachmentRoot()->GetComponentLocation());
        retVal->SetRelativeRotation(TurnByDegrees(rotation, -this->GetAttachmentRootActor()->GetActorRotation().Yaw));
    }

    retVal->Setup(mesh, baseMaterial, successMaterial, maxLength);

    return retVal;
}

void UGestureTracker::ResetGestureMesh(UGestureMeshComponent *&smc)
{
    if (smc) {
        smc->DestroyComponent();
        smc = NULL;
    }
}






// Misc Functions

// Updates current hand positions
void UGestureTracker::UpdateTrackLocation()
{
    if (bUseWorldLocation) {
        trackLocation = this->GetComponentLocation();
    }
    else {
        trackLocation = this->GetComponentLocation() - this->GetAttachmentRoot()->GetComponentLocation();
    }
}

void UGestureTracker::UpdateTrackRotation()
{
    if (BodyLocationComponent != nullptr) {
        const FVector bodyToHandDiff = this->GetComponentLocation() - BodyLocationComponent->GetComponentLocation();
        const FRotator bodyRotator = FRotationMatrix::MakeFromX(bodyToHandDiff).Rotator();
        
        const float InterpretedYaw = bodyRotator.Yaw + rotationOffset;

        if (bNormalizePitchRotation) {
            const float InterpretedPitch = bUseBodyLocationForPitch ? bodyRotator.Pitch : this->GetComponentRotation().Pitch;
            trackRotation = FRotator(InterpretedPitch, InterpretedYaw, 0.f);
        }
        else {
            trackRotation = FRotator(0.f, InterpretedYaw, 0.f);
        }
    }
    else {
        const FRotator handRotator = this->GetComponentRotation();

        const float InterpretedYaw = handRotator.Yaw - handRotator.Roll + rotationOffset;
        const float InterpretedPitch = bNormalizePitchRotation ? handRotator.Pitch : 0.f;

        // Use the hand yaw rotation to adjust tracking
        // Adjust for wrist rotation using roll
        trackRotation = FRotator(InterpretedPitch, InterpretedYaw, 0.f);
    }
}

FVector UGestureTracker::GetDrawLocation()
{
    if (bUseWorldLocation) {
        return this->GetComponentLocation();
    }
    else {
        return this->GetComponentLocation() - this->GetAttachmentRoot()->GetComponentLocation();
    }
}

void UGestureTracker::UpdateDrawLocation()
{
    trackDrawStartLocation = GetDrawLocation();
    predictDrawStartLocation = trackDrawStartLocation;
}

FRotator UGestureTracker::TurnByDegrees(const FRotator &rotator, float degrees)
{
    return (rotator.Quaternion() * FRotator(0, degrees, 0).Quaternion()).Rotator();
}

double UGestureTracker::GetTimeSeconds() const
{
    if (bUseSystemTime) {
        return FPlatformTime::Seconds();
    }
    else {
        return UGameplayStatics::GetRealTimeSeconds(GetWorld());
    }
}

float UGestureTracker::GetLengthRatio(const Gesture& trackedGesture, int index) const
{
    return trackedGesture.PathLength() / gestureLib.Get(index).PathLength();
}