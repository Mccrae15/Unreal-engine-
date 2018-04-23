// Copyright 2016-2017 Hunter Delattre. All Rights Reserved.

#pragma warning(disable: 4458)
#pragma once

#include "CoreMinimal.h"
#include "RecordGesture.h"

const int GESLIB_RESERVED_MEM = 16;

const float GESLIB_DEFAULT_TOLERANCE = .8;
const float GESLIB_DEFAULT_INTOLERANCE = .2;
const float GESLIB_DEFAULT_SMOOTHING = 10;
const float GESLIB_DEFAULT_PATIENCE = 20;
const float GESLIB_DEFAULT_COMPLETION = 1.0;
const float GESLIB_STRIKEOUT_DISTANCE = 10;
const float GESLIB_CONTINUOUS_STRIKEOUT_DISTANCE = 4;
const float GESLIB_DEFAULT_PREDICT_PERCENT = .2;

const int NULL_ID = -1;

struct FGestureRecognitionParams
 {
    const float& acceptableSimilarity;
    const float& unacceptableSimilarity;
    const float& smoothing;
    const float& patience;
    const float& strikeoutDistance;
    const float& continuousStrikeoutDistance;
    const float& completionPercentage;
    const float& predictPercentage;
};

static const FGestureRecognitionParams DEFAULT_GESTURE_RECOGNITION_PARAMS
{
    GESLIB_DEFAULT_TOLERANCE,
    GESLIB_DEFAULT_INTOLERANCE,
    GESLIB_DEFAULT_SMOOTHING,
    GESLIB_DEFAULT_PATIENCE,
    GESLIB_STRIKEOUT_DISTANCE,
    GESLIB_CONTINUOUS_STRIKEOUT_DISTANCE,
    GESLIB_DEFAULT_COMPLETION,
    GESLIB_DEFAULT_PREDICT_PERCENT
};

/**
 * GestureLibrary stores and performs recognition on a collection of RecordGestures
 */
class GESTURETRACKERVR_API GestureLibrary
{
public:

    FGestureRecognitionParams Params;

    GestureLibrary(const FGestureRecognitionParams& InParams = DEFAULT_GESTURE_RECOGNITION_PARAMS);
    ~GestureLibrary();

    friend FArchive &operator<<(FArchive &archive, GestureLibrary &gl);

    // Save and load gesture library at path
    bool Save(const FString &path);
    bool Load(const FString &path);
    // Empties the gesture library, clearing all recorded gestures
    void Empty();

    // Get the number of stored gestures
    int Num() const;

    // Get an array of ids of the stored gestures
    TArray<int> GetIds() const;

    // Saves a gesture with id
    void Add(int id, const FString &name, const RecordGesture &inputGesture);

    // Indexing functions
    int GetIndexById(int id) const;
    int GetIndexByName(const FString &name) const;
    int GetAvailableId() const;

    // Gesture Getters
    RecordGesture Get(int index) const;
    FString Name(int index) const;
    int Id(int index) const;
    int Length(int index) const;
    float Resolution(int index) const;
    TArray<FVector> Path(int index) const;

    // The search index representing recognition progress along the path of the Gesture at index
    int TrackIndex(int index) const;
    
    // Search status getters for the Gesture at index
    bool Disqualified(int index) const;
    bool Finished(int index) const;
    bool Banned(int index) const;
    float PercentageComplete(int index) const;
   
    // The most likely tracked gesture
    int MostLikely() const;
    
    // Reset search for all gestures
    void ResetSearch();

    // Reset search for the gesture at index
    void ResetTracking(int index);

    // Set a set of ids to blacklist/whitelist during recognition tracking 
    void SetTrackIds(const TSet<int> trackIds, bool trackIdsIsBlacklist);

    /**
    * Performs one round of recognition tracking on all gestures
    *
    * @param rotVec Vector representing the rotation to track
    * @param maxTrackLength The maximum number of path points to progress recognition
    * @return index of a completed gesture or -1
    */
    int Search(const Gesture& TrackedGesture, int maxTrackLength);

    /**
    * Updates the recognition tracking progress for one gesture
    *
    * @param gestureIndex The index in the library of the gesture to be updated
    * @param rotVec Vector representing the rotation to track
    * @param maxTrackLength The maximum number of path points to progress recognition
    * @return Whether the gesture completed recognition
    */
    bool Track(int gestureIndex, const Gesture& TrackedGesture, float maxTrackLength);

    /**
    * Updates the recognition tracking progress for one gesture.
    * Is more aggressive in disqualifying gestures for continuous recognition
    */
    bool ContinuousTrack(int gestureIndex, const Gesture& TrackedGesture, float maxTrackLength);

    // Check if a gesture should be disqualified from search based on the tracked vector
    // Will give a strike if the tracked rotation vector has unacceptable similarity
    void CheckForDisqualification(int index, const Gesture& TrackedGesture);

private:

    struct Entry
    {
        int id;
        FString name;
        RecordGesture gesture;

        int trackIndex;
        int strikes;

        Entry() {}
        Entry(int id, const FString &name, const RecordGesture &gesture)
        {
            this->id = id;
            this->name = name;
            this->gesture = gesture;
            trackIndex = 1;
            strikes = 0;
        }

        friend FArchive &operator<<(FArchive &archive, Entry &entry) 
        {
            archive << entry.id << entry.name << entry.gesture;
            return archive;
        }
    };

    TArray<Entry> lib;

    // Blacklist/whitelist certain Gesture Ids for recognition
    TSet<int> trackIds;
    bool bTrackIdsIsBlacklist;

    // Track status updates for Gesture at index
    int ProgressAlongPath(int index);
    int RegressAlongPath(int index);
    float Strike(int index);
    void Disqualify(int index);
    void Finish(int index);
    void ResetTrackIndex(int index);
    void ResetStrikes(int index);

    /**
    * Determine whether the rotation vector is acceptably similar to the Gesture path at the current search  
    * index and any indices beyond within smoothing range for the Gesture at index. Returns the number of  
    * path points that tracking can progress along (0 if not similar and progress cannot be made).
    */
    int trackingVecAcceptable(int index, const FVector &rotVec, int smoothing) const;

    // True if the given rotation vector matches the last tracked vector
    bool trackingAtCorner(int index, const FVector &rotVec) const;

    // True if the tracking vector is unacceptably similar to the current search index of the Gesture at index
    bool trackingVecUnacceptable(int index, const FVector &rotVec) const;

    // Same as above but uses a precalculated similarity value
    bool trackingVecUnacceptable(float similarity) const;

    /*
    * Calculates a similarity value between -1 and 1 describing similarity of the tracked 
    * rotation vector to the portion of the Gesture path at index being recognized
    */
    float similarityToPath(int index, int pathIndex, const FVector &rotVec) const;
};

