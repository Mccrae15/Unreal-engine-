// Copyright 2016-2017 Hunter Delattre. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Private/GestureLibrary.h"
#include "GestureMeshComponent.h"
#include "GestureTracker.generated.h"

const float GESTRACK_DEFAULT_GESTURE_RESOLUTION = 3;
const float GESTRACK_DEFAULT_ROTATION_OFFSET = 0;
const float GESTRACK_DEFAULT_GESTURE_MIN_LENGTH = 15;
const float GESTRACK_DEFAULT_GESTURE_MAX_LENGTH = 500;
const float GESTRACK_DEFAULT_TRACK_RATIO = 5;
const float GESTRACK_DEFAULT_CONTINUOUS_TRACK_RATIO = 2;
const float GESTRACK_DEFAULT_MAX_TRACK_DRAW_LENGTH = 50;
const float GESTRACK_DEFAULT_PREDICT_DISTANCE = 30;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FGestureRecognizedDelegate, int, id, FString, name, float, recognitionDuration, float, lengthRatio);

UENUM(BlueprintType)
enum class EGestureRecordResult : uint8 
{
    Recorded UMETA(DisplayName = "Recorded"),
    Failed UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EGestureRecognizeResult : uint8 
{
    Recognized UMETA(DisplayName = "Recognized"),
    Failed UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EGestureLibFunctionResult : uint8 
{
    Success UMETA(DisplayName = "Success"),
    Failed UMETA(DisplayName = "Failed")
};

/**
* Gesture Tracker provides gesture recognition and drawing.
* Attach it to your motion controller component (or any moving component)
* and setup inputs for its Record/Recognition/Draw functions.
* Check the parameters in the details panel to tweak tracking, recognition, and drawing.
*/
UCLASS(BlueprintType, ClassGroup = (GestureTrackerVR), meta = (BlueprintSpawnableComponent))
class GESTURETRACKERVR_API UGestureTracker : public USceneComponent
{
    GENERATED_BODY()

public:

    /**
    * Triggered whenever a gesture is recognized during continuous recognition.
    * @param id   The id of the recognized gesture
    * @param name The name of the recognized gesture
    * @param recognitionDuration The duration of the recognition period in seconds (approximately from the time the player started the gesture to when it was recognized).
    * @param lengthRatio The ratio of the tracked gesture length to the reference gesture length (e.g. 2 means the tracked gesture was twice as long as the recognized reference).
    */
    UPROPERTY(BlueprintAssignable, Category = "Recognition Events", Meta = (DisplayName = "Continuous Recognition Gesture Recognized"))
    FGestureRecognizedDelegate ContinuousGestureRecognized;

    /**
    * Triggered whenever a gesture is recognized during normal recognition.
    * Can be used to trigger a gesture effect as soon as a gesture is detected instead
    * of when FinishRecognition is called. Multiple gestures may be recognized during one
    * recognition period (e.g. another gesture is a prefix of the one being tracked).
    * @param id   The id of the recognized gesture
    * @param name The name of the recognized gesture
    * @param recognitionDuration The duration of the recognition period in seconds (from StartRecognition() to when the gesture was recognized).
    * @param lengthRatio The ratio of the tracked gesture length to the reference gesture length (e.g. 2 means the tracked gesture was twice as long as the recognized reference).
    */
    UPROPERTY(BlueprintAssignable, Category = "Recognition Events", Meta = (DisplayName = "Regular Recognition Gesture Recognized"))
    FGestureRecognizedDelegate GestureRecognized;

    /**
    * Use the Gesture Tracker's: world location for tracking if true
    *                          : location relative to the attach root if false
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Tracking")
    bool bUseWorldLocation = false;

    /**
    * If None (default) the gesturing orientation will be interpreted using the GestureTracker rotation. 
    * If set, will override normal orientation detection and use the rotation from the BodyLocationComponent
    * to the GestureTracker to orient gestures. This will keep orientation more consistent when switching between 
    * different motion controller orientations. 
    * For example, if a player is pointing the controller forward but holding it with a 90 degree twist in their 
    * wrist the GestureTracker will think they're pointing left. Setting this component will resolve this issue.
    * Generally you should set this to point to your pawn's Camera.
    * @note RotationOffset will still apply if this is set.
    */
    UPROPERTY(BlueprintReadWrite, Transient, Category = "Parameters|Tracking")
    const USceneComponent* BodyLocationComponent = nullptr;

    /**
    * The number of units between each recorded gesture point (or the length of a segment of the gesture)
    * Smaller values mean a finer resolution, less performance.
    * It is highly recommended that you keep this above 1. If you record every .25 units you'll be recording
    * movements as small as the shaking of a person's hand and using up a lot of resources unnecessarily.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Tracking", Meta = (ClampMin = 0.25, ClampMax = 20, ExposeOnSpawn = true))
    float gestureResolution = GESTRACK_DEFAULT_GESTURE_RESOLUTION;

    /**
    * If true, rotation of gestures will be normalized by pitch (looking up or down) in addition to yaw.
    * e.g. A forward punch gesture will be recognized if you punch up.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Tracking", Meta = (ExposeOnSpawn = true))
    bool bNormalizePitchRotation = false;

    /**
    * If BodyLocationComponent is set and we're normalizing pitch rotation, this sets whether pitch should be determined by the
    * GestureTracker rotation or the angle from the GestureTracker to the BodyLocationComponent. Using the BodyLocation
    * for pitch is probably undesirable if you've set BodyLocationComponent to point to your VR camera since your hands
    * are around 40 degrees below the camera at rest, so the standard pitch will be around -40 instead of around 0.
    * This won't be a problem if gestures are recorded with this set to true since that -40 pitch bias will be in both
    * the recorded and tracked gestures.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Tracking", Meta = (ExposeOnSpawn = true, EditCondition = bNormalizePitchRotation))
    bool bUseBodyLocationForPitch = false;

    /**
    * Offset the yaw rotation of performed gestures by this many degrees, useful if the gesture
    * tracker is pointing in a different direction than the player will be for any reason.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Tracking", Meta = (ClampMin = -180, ClampMax = 180, ExposeOnSpawn = true))
    float rotationOffset = GESTRACK_DEFAULT_ROTATION_OFFSET;

    /**
    * Timing information will use system time if true, game time if false
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Tracking", Meta = (ExposeOnSpawn = true))
    bool bUseSystemTime = true;

    /**
    * Minimum length of a recorded gesture in units
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Recording", Meta = (ClampMin = 2, ClampMax = 200))
    float minGestureLength = GESTRACK_DEFAULT_GESTURE_MIN_LENGTH;

    /**
    * Maximum length of a recorded gesture in units
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Recording", Meta = (ClampMin = 2, ClampMax = 1000))
    float maxGestureLength = GESTRACK_DEFAULT_GESTURE_MAX_LENGTH;

    /**
    * Gesture similarity value above which recognition will be acceptable.
    * If a tracked gesture is acceptably similar to the current portion
    * of a reference gesture then recognition will progress to the next 
    * portion of that gesture.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Recognition", Meta = (ClampMin = -1, ClampMax = 1, ExposeOnSpawn = true))
    float acceptableSimilarity = GESLIB_DEFAULT_TOLERANCE;

    /**
    * Gesture similarity value below which recognition be rejected.
    * If a tracked gesture is unacceptably similar to the current 
    * portion of a reference gesture for too long then that gesture
    * will be disqualified.
    * Completed gestures will also be disqualified in this way.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Recognition", Meta = (ClampMin = -1, ClampMax = 1, ExposeOnSpawn = true))
    float unacceptableSimilarity = GESLIB_DEFAULT_INTOLERANCE;

    /**
    * The ratio of the maximum size of the recognized gesture to the tracked gesture for regular recognition.
    * One unit of the tracked gesture can represent up to ratio units of a reference gesture.
    */ 
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Recognition", Meta = (ClampMin = 1, ClampMax = 100, ExposeOnSpawn = true))
    float trackRecognitionRatio = GESTRACK_DEFAULT_TRACK_RATIO;

    /**
    * Same as trackRecognitionRatio but for continuous recognition
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Recognition", Meta = (ClampMin = 1, ClampMax = 100, ExposeOnSpawn = true))
    float continuousRecognitionRatio = GESTRACK_DEFAULT_CONTINUOUS_TRACK_RATIO;

    /**
    * The distance you can go off the gesture path before a gesture will fail recognition
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Recognition", Meta = (ClampMin = 0, ExposeOnSpawn = true))
    float trackFailDistance = GESLIB_STRIKEOUT_DISTANCE;

    /**
    * Same as trackFailDistance but for continuous recognition
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Recognition", Meta = (ClampMin = 0, ExposeOnSpawn = true))
    float continuousFailDistance = GESLIB_CONTINUOUS_STRIKEOUT_DISTANCE;

    /**
    * Length of the portion of a gesture that will be checked for acceptable similarity 
    * to the currently tracked gesture. If the gesture motion being tracked is acceptably
    * similar to any part of the reference portion then recognition will progress.
    * 
    * Effectively smooths tracking so that players don't have to perfectly mimick sharp corners.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Recognition", Meta = (ClampMin = 0, ClampMax = 50, ExposeOnSpawn = true))
    float smoothing = GESLIB_DEFAULT_SMOOTHING;

    /**
    * Number of units the tracker can move before any potential gestures can be disqualified
    * during normal recognition. Lets players move randomly for a bit after recognition starts
    * so they don't have to begin performing the gesture the moment they trigger recognition.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Recognition", Meta = (ClampMin = 0, ClampMax = 100, ExposeOnSpawn = true))
    float patience = GESLIB_DEFAULT_PATIENCE;

    /**
    * The percentage of a reference gesture that must be completed to be recognized
    */ 
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Recognition", Meta = (ClampMin = 0, ClampMax = 1, ExposeOnSpawn = true))
    float completionPercentage = GESLIB_DEFAULT_COMPLETION;

    /**
    * The maximum length of the tracked gesture mesh in units. 
    * The end of the tracked gesture will start being erased past this length.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Drawing", Meta = (ClampMin = 1, ClampMax = 100, ExposeOnSpawn = true))
    float maxTrackDrawDistance = GESTRACK_DEFAULT_MAX_TRACK_DRAW_LENGTH;

    /**
    * The percentage of a gesture that must be completed before it can be the predicted gesture
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Drawing", Meta = (ClampMin = 0, ClampMax = 1, ExposeOnSpawn = true))
    float predictPercentage = GESLIB_DEFAULT_PREDICT_PERCENT;

    /**
    * The distance in units ahead of your current recognition progress that the predicted gesture will be drawn
    * This is the "ghost" image displayed during predictive drawing with the default materials
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters|Drawing", Meta = (ClampMin = 1, ClampMax = 100, ExposeOnSpawn = true))
    float predictDistance = GESTRACK_DEFAULT_PREDICT_DISTANCE;

    // Sets default values for this component's properties
    UGestureTracker();

    // Called when the game starts
    virtual void BeginPlay() override;

    // Called every frame
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;




    /**************************
    * RECOGNITION
    ***************************/

    /**
    * Initialize tracking to start recording a gesture
    * @param force Force restart tracking even if currently recording/recognizing
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    void StartRecording(bool force=false);

    /**
    * Save the gesture being recorded. Fails if the gesture is too short or too long, or called while not recording.
    * If you record using the same id as a previous recording, the previous recording will be overwritten.
    * @param id The unique id to assign to the recorded gesture. Enter a negative number to have id automatically assigned.
    * @param name (optional) A non unique name to assign to the recorded gesture.
    * @param assignedId The id assigned to the recorded gesture. -1 if recording failed.
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR", Meta = (ExpandEnumAsExecs = "result"))
    void FinishRecording(EGestureRecordResult &result, int &assignedId, int id=-1, const FString &name="");

    /**
     * Initialize tracking to start recognizing a gesture
     * @param trackIds A list of ids to track (or not, depending on trackIdsIsBlacklist) for recognition
     * @param trackIdsIsBlacklist trackIds is a blacklist if true, whitelist if false
     * @param force Force restart tracking even if currently recording/recognizing
     */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR", Meta = (AutoCreateRefTerm = "trackIds"))
    void StartRecognition(const TArray<int> &trackIds, bool trackIdsIsBlacklist = true, bool force = false);

    /**
    * End recognition and check for a successfully recognized gesture
    * @param id The unique id of the recognized gesture
    * @param name The non-unique name of the recognized gesture.
    * @param recognitionDuration The time from StartRecognition to FinishRecognition in real seconds (not game time).
    * @param lengthRatio The ratio of the tracked gesture length to the reference gesture length (e.g. 2 means the tracked gesture was twice as long as the recognized reference).
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR", Meta = (ExpandEnumAsExecs = "result"))
    void FinishRecognition(EGestureRecognizeResult &result, int &id, FString &name, float &recognitionDuration, float &lengthRatio);

    /**
    * After starting continuous recognition the recognition event will fire every time a recorded gesture is completed.
    * @param trackIds A list of ids to track (or not, depending on trackIdsIsBlacklist) for recognition
    * @param trackIdsIsBlacklist trackIds is a blacklist if true, whitelist if false
    * @param force Force restart tracking even if currently recording/recognizing
    * @param resetOnRecognized If true, recognition of a gesture will restart immediately after it is recognized, else it will wait until the gesture has stopped being performed
    */ 
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR", Meta = (AutoCreateRefTerm = "trackIds"))
    void StartContinuousRecognition(const TArray<int> &trackIds, bool trackIdsIsBlacklist = true, bool force = false, bool resetOnRecognized = false);

    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    void StopContinuousRecognition();

    /**
    * Save the currently recorded gestures to file. The root directory is your game's content folder.
    *
    * NOTE: If you want to package default gestures with your game, save the gestures to their own directory
    * and then add that directory to "Project Settings > Packaging > Additional Non-Asset Directories to Package".
    * (e.g. save with a filename "Gestures/default_right_gestures" and then add the "Gestures" directory to this list)
    * @param filename The path/filename to save the gestures to within the content folder.
    * @return true if the save was successful
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR", Meta = (ExpandEnumAsExecs = "result"))
    bool SaveGestures(const FString &filename, EGestureLibFunctionResult &result);

    /**
    * Load gesture file, set as current recorded gestures
    * Stops any recording/recognition
    * @param filename The path/filename to load the gestures from within the content folder.
    * @return true if the load was successful
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR", Meta = (ExpandEnumAsExecs = "result"))
    bool LoadGestures(const FString &filename, EGestureLibFunctionResult &result);

    /**
    * Clear all recorded gestures
    * Stops any recognition (but not recording)
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    void ClearGestures();

    /**
    * Returns the number of stored gestures
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    int GetNumGestures() const;

    /**
    * Returns an array containing the Ids of stored gestures
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    TArray<int> GetIds() const;

    /**
    * @param id The id of the gesture whose path we want
    * @return A vector array describing the path of the specified gesture
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    TArray<FVector> GetGesturePath(int id) const;

    /**
    * @return A vector array describing the path of the currently predicted gesture
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    TArray<FVector> GetPredictedGesturePath() const;

    /**
    * Use to update the trackIds at any time so that you can stop
    * recognizing a gesture in the middle of recognition, and start/stop
    * recognizing gestures during continuous recognition.
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR", Meta = (AutoCreateRefTerm = "trackIds"))
    void UpdateTrackIds(const TArray<int> trackIds, bool trackIdsIsBlacklist = true);

    /**
    * Get the id of the current predicted gesture, or -1 if no gesture is predicted
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    int GetPredictedGestureId() const;

    /**
    * Get the current percentage complete of the gesture with the given id.
    * i.e. If you're halfway through performing the gesture when this is called it will return .5
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    float GetPercentageComplete(int id) const;


    /****************************
    * DRAWING
    *****************************/

    /**
    * Start drawing the gesture that is currently being tracked
    * Will use the default or last assigned mesh and materials if left blank
    * IMPORTANT: Materials must have "Use with spline meshes" checked
    * @param mesh            A mesh used for segments of the drawn gesture.
    * @param baseMaterial    (optional) A material to replace the mesh's material
    * @param successMaterial A material to apply when the tracked gesture is recognized.
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    void StartDrawTrackedGesture(UStaticMesh *mesh, UMaterialInterface *baseMaterial);

    /**
    * Stop drawing the gesture that is currently being tracked
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    void StopDrawTrackedGesture();

    /**
    * Start drawing the predicted gesture during recognition
    * Will use default or last assigned mesh and materials if left blank
    * IMPORTANT: Materials must have "Use with spline meshes" checked
    * @param mesh                A mesh used for segments of the drawn gesture.
    * @param baseMaterial        (optional) A material to replace the mesh's material
    * @param successMaterial     A material to apply to the completed segments of the predicted gesture.
    * @param drawIds             A array of gesture Ids to draw predictively (or not, depending on the value of drawIdsIsBlackList)
    * @param drawIdsIsBlacklist  DrawIds is a blacklist if true, whitelist if false
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR", Meta = (AutoCreateRefTerm = "drawIds"))
    void StartDrawPredictedGesture(UStaticMesh *mesh, UMaterialInterface *baseMaterial, UMaterialInterface *successMaterial, const TArray<int> &drawIds, bool drawIdsIsBlacklist = true);

    /**
    * Stop drawing the predicted gesture during recognition
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    void StopDrawPredictedGesture();

    /**
    * Draws the gesture with the given id at the Gesture Tracker's current position and rotation.
    * IMPORTANT: The drawResult must be destroyed manually, so it is recommended that you store a reference to it.
    * @param name The id of the gesture to draw.
    * @param mesh A mesh used for segments of the drawn gesture.
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR", Meta = (ExpandEnumAsExecs = "result"))
    void DrawGestureById(int id, UStaticMesh *mesh, EGestureLibFunctionResult &result, UGestureMeshComponent *&drawResult);

    /**
    * Draws the gesture with the given name at the Gesture Tracker's current position and rotation.
    * If multiple Gestures with the same name exist only the first recorded will be drawn.
    * IMPORTANT: The drawResult must be destroyed manually, so it is recommended that you store a reference to it.
    * @param name The name of the gesture to draw.
    * @param mesh A mesh used for segments of the drawn gesture.
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR", Meta = (ExpandEnumAsExecs = "result"))
    void DrawGestureByName(const FString &name, UStaticMesh *mesh, EGestureLibFunctionResult &result, UGestureMeshComponent *&drawResult);

    /**
    * Update tracking gesture draw materials with the supplied ones.
    * Leave an input blank to leave it unchanged.
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    void UpdateTrackDrawMaterials(UStaticMesh *mesh, UMaterialInterface *baseMaterial);

    /**
    * Update the predicted gesture draw materials with the supplied ones.
    * Leave an input blank to leave it unchanged.
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR")
    void UpdatePredictDrawMaterials(UStaticMesh *mesh, UMaterialInterface *baseMaterial, UMaterialInterface *successMaterial);

    /**
    * Use to update the drawIds at any time so that you can start/stop
    * drawing a gesture in the middle of recognition
    */
    UFUNCTION(BlueprintCallable, Category = "GestureTrackerVR", Meta = (AutoCreateRefTerm = "drawIds"))
    void UpdateDrawIds(const TArray<int> drawIds, bool drawIdsIsBlacklist = true);

private:

    // The Gesture object that holds tracking data
    Gesture trackGesture;
    // The Gesture object that holds recording data
    RecordGesture recordGesture;
    // Library stores and recognizes gestures
    GestureLibrary gestureLib;
    // The index of the last recognized gesture, or -1
    int gestureIndex;

    FVector trackLocation;
    FRotator trackRotation;

    double trackStartTime;

    // true if any tracking mode is active (recording, regular/continuous recognition)
    UPROPERTY(BlueprintReadOnly, Category = "Tracking Status Booleans", meta = (AllowPrivateAccess = "true"))
    bool bTracking;

    // true if recording is active
    UPROPERTY(BlueprintReadOnly, Category = "Tracking Status Booleans", meta = (AllowPrivateAccess = "true"))
    bool bRecording;

    // true if continuous recognition is active
    UPROPERTY(BlueprintReadOnly, Category = "Tracking Status Booleans", meta = (AllowPrivateAccess = "true"))
    bool bContinuous;

    // if true, recognition resets immediately after finishing a gesture during continuous recognition
    // if false, recognition resets only when a gesture has stopped being performed
    UPROPERTY(BlueprintReadOnly, Category = "Tracking Status Booleans", meta = (AllowPrivateAccess = "true"))
    bool bContinuousResetOnRecognized;

    struct ContinuousTrack 
    {
        Gesture gesture;
        double startTime;

        ContinuousTrack() {}

        void Reset(const FVector &startLocation, const FRotator &rotation, float gestureResolution, double time)
        {
            gesture.Reset(startLocation, rotation, gestureResolution);
            startTime = time;
        }
    };
    TArray<ContinuousTrack, TInlineAllocator<GESLIB_RESERVED_MEM>> continuousTracks;

    // true if the tracked gesture will be drawn during tracking
    UPROPERTY(BlueprintReadOnly, Category = "Tracking Status Booleans", meta = (AllowPrivateAccess = "true"))
    bool bDrawingTracked;

    // true if the predicted gesture will be drawn during recognition
    UPROPERTY(BlueprintReadOnly, Category = "Tracking Status Booleans", meta = (AllowPrivateAccess = "true"))
    bool bDrawingPredicted;

    int drawIndex;
    FVector trackDrawStartLocation;
    FVector predictDrawStartLocation;

    UPROPERTY()
    TSet<int> drawIds;
    bool bDrawIdsIsBlacklist = false;

    // The gesture mesh object for tracked drawing
    UPROPERTY(BlueprintReadOnly, Category = "Draw Meshes", meta = (AllowPrivateAccess = "true"))
    UGestureMeshComponent *trackGestureMesh = NULL;

    // The gesture mesh object for predictive drawing
    UPROPERTY(BlueprintReadOnly, Category = "Draw Meshes", meta = (AllowPrivateAccess = "true"))
    UGestureMeshComponent *predictGestureMesh = NULL;
    
    UPROPERTY()
    UStaticMesh *trackMesh = NULL;
    UPROPERTY()
    UMaterialInterface *trackBaseMaterial = NULL;
    UPROPERTY()
    UStaticMesh *predictMesh = NULL;
    UPROPERTY()
    UMaterialInterface *predictBaseMaterial = NULL;
    UPROPERTY()
    UMaterialInterface *predictSuccessMaterial = NULL;
    
    // Update the location used for tracking to be current.
    void UpdateTrackLocation();

    // Update the rotation used for tracking to be current.
    void UpdateTrackRotation();

    /**
    * Starts tracking a new gesture. Can be called at any
    * time to restart tracking with new parameters.
    */
    void InitTracking(bool record, bool continuous=false);

    /**
    * Perform one tick of tracking
    */
    void Track();


    /**
    * Update one tick of regular recognition
    */
    void RecognitionUpdate();

    /**
    * Update tracking on all ContinuousTracks for one tick of continuous recognition.
    */
    void ContinuousRecognitionUpdate();

    /**
    * Stops all tracking and drawing
    */
    void EndTracking();

    /**
    * Stores the currently tracked gesture for later recognition
    * @param id The unique id to be assigned to this gesture
    * @param name (Optional) A non unique name to assign to this gesture
    */
    void StoreRecording(int id, const FString &name);

    /**
    * Attempt to recognize the tracked gesture
    * Returns the recognized Gesture's id and name
    */
    void Invoke(int &id, FString &name);






    /** 
    * Draw the currently tracked gesture
    */
    void DrawTrackGestureMesh();

    /**
    * Draw the recorded gesture at index
    */
    void DrawPredictGestureMesh(int index);

    /**
    * Creates an registers a GestureMeshComponent with the given properties
    */
    UGestureMeshComponent* MakeGestureMesh(const FVector &location, const FRotator &rotation, UStaticMesh *mesh, UMaterialInterface *baseMaterial, UMaterialInterface *successMaterial, int maxLength = 0);

    /**
    * Deletes the GestureMeshComponent set its pointer to NULL
    */
    void ResetGestureMesh(UGestureMeshComponent *&smc);

    /**
    * Calculates the appropriate location to start Gesture drawing depending on bUseWorldLocation
    */
    FORCEINLINE FVector GetDrawLocation();

    /**
    * Updates draw start locations to be current
    */
    FORCEINLINE void UpdateDrawLocation();

    /**
    * Update the predicted gesture draw to be the latest most likely gesture
    */
    void DrawMostLikely();

    /**
    * Rotates a rotator around the z axis (yaw) by degrees.
    * Used for normalizing the rotation of gestures
    */
    static FRotator TurnByDegrees(const FRotator &rotator, float degrees);

    FORCEINLINE double GetTimeSeconds() const;

    FORCEINLINE float GetLengthRatio(const Gesture& trackedGesture, int index) const;

};
