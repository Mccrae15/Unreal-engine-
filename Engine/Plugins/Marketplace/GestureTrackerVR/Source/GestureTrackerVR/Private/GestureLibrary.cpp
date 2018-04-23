// Copyright 2016-2017 Hunter Delattre. All Rights Reserved.

#include "GestureLibrary.h"
#include "GestureTracker.h"
#include "FileHelper.h"
#include "Paths.h"
#include "BufferArchive.h"
#include "ArrayReader.h"

const int START_TRACK_INDEX = 1;
const float START_DRAW_DISTANCE = 20;

GestureLibrary::GestureLibrary(const FGestureRecognitionParams& InParams)
    : Params(InParams)
{
    lib.Reserve(GESLIB_RESERVED_MEM);
}

GestureLibrary::~GestureLibrary()
{
    lib.Empty();
}

FArchive &operator<<(FArchive &archive, GestureLibrary &gl)
{
    int32 length;
    if (archive.IsLoading()) {
        archive << length;
        gl.lib.Init(GestureLibrary::Entry(), length);
    }
    else {
        length = gl.lib.Num();
        archive << length;
    }
    for (int32 ii = 0; ii < length; ii++) {
        archive << gl.lib[ii];
    }

    return archive;
}


// Entry access functions

void GestureLibrary::Add(int id, const FString &name, const RecordGesture &inputGesture)
{
    int idIndex = GetIndexById(id);
    if (idIndex < 0) {
        lib.Add(Entry(id, name, inputGesture));
    }
    else {
        lib[idIndex].name = name;
        lib[idIndex].gesture = inputGesture;
    }
}

int GestureLibrary::GetAvailableId() const
{
    int newId = 0;
    for (int ii = 0; ii < lib.Num(); ii++) {
        if (newId <= Id(ii)) {
            newId = Id(ii) + 1;
        }
    }
    return newId;
}



int GestureLibrary::Num() const
{
    return lib.Num();
}

TArray<int> GestureLibrary::GetIds() const
{
    TArray<int> retVal;

    for (int ii = 0; ii < lib.Num(); ii++) {
        retVal.Add(lib[ii].id);
    }

    return retVal;
}


RecordGesture GestureLibrary::Get(int index) const
{
    return lib[index].gesture;
}

int GestureLibrary::Id(int index) const
{
    return lib[index].id;
}

FString GestureLibrary::Name(int index) const
{
    return lib[index].name;
}

int GestureLibrary::GetIndexById(int id) const
{
    for (int ii = 0; ii < lib.Num(); ii++) {
        if (id == Id(ii)) {
            return ii;
        }
    }
    return INDEX_NONE;
}

int GestureLibrary::GetIndexByName(const FString &name) const
{
    for (int ii = 0; ii < lib.Num(); ii++) {
        if (name.Equals(Name(ii))) {
            return ii;
        }
    }
    return INDEX_NONE;
}

int GestureLibrary::Length(int index) const
{
    return lib[index].gesture.Size();
}

TArray<FVector> GestureLibrary::Path(int index) const
{
    return lib[index].gesture.Path();
}


float GestureLibrary::Resolution(int index) const
{
    return lib[index].gesture.Resolution();
}


bool GestureLibrary::Disqualified(int index) const
{
    return lib[index].trackIndex < START_TRACK_INDEX;
}

bool GestureLibrary::Finished(int index) const
{
    return TrackIndex(index) >= (Length(index) - 1);
}

bool GestureLibrary::Banned(int index) const
{
    return (bTrackIdsIsBlacklist == trackIds.Contains(Id(index)));
}

float GestureLibrary::PercentageComplete(int index) const
{
    const int trackProgress = TrackIndex(index) - START_TRACK_INDEX;
    const int trackLength = Length(index) - 1;
    if (trackProgress <= 0) {
        return 0.f;
    }
    else if (trackProgress >= trackLength) {
        return 1.f;
    }
    else {
        return ((float)trackProgress) / ((float)trackLength);
    }
}







// Gesture management

bool GestureLibrary::Save(const FString &path)
{
    FString fullPath = FPaths::ProjectContentDir() + path;

    FBufferArchive archive;
    archive << *this;
    bool successSave = FFileHelper::SaveArrayToFile(archive, *fullPath);
    bool successClose = archive.Close();

    return successSave && successClose;
}

bool GestureLibrary::Load(const FString &path)
{
    FString fullPath = FPaths::ProjectContentDir() + path;

    // check file exists
    if (!FPaths::FileExists(fullPath)) {
        return false;
    }
    // try reading file
    FArrayReader reader;
    bool successLoad = FFileHelper::LoadFileToArray(reader, *fullPath);
    if (successLoad) {
        reader << *this;
    }
    bool successClose = reader.Close();

    ResetSearch();

    return successLoad && successClose;
}

void GestureLibrary::Empty()
{
    lib.Empty();
}






// Search functions

int GestureLibrary::TrackIndex(int index) const
{
    return lib[index].trackIndex;
}

int GestureLibrary::ProgressAlongPath(int index)
{
    return lib[index].trackIndex++;
}

int GestureLibrary::RegressAlongPath(int index)
{
    return lib[index].trackIndex--;
}

void GestureLibrary::Disqualify(int index)
{
    lib[index].trackIndex = INDEX_NONE;
}

void GestureLibrary::Finish(int index)
{
    lib[index].trackIndex = Length(index) - 1;
}

float GestureLibrary::Strike(int index)
{
    return (++lib[index].strikes) * Resolution(index);
}

void GestureLibrary::ResetTracking(int index)
{
    ResetTrackIndex(index);
    ResetStrikes(index);
}

void GestureLibrary::ResetTrackIndex(int index)
{
    lib[index].trackIndex = START_TRACK_INDEX;
}

void GestureLibrary::SetTrackIds(const TSet<int> trackIds, bool trackIdsIsBlacklist)
{
    this->trackIds = trackIds;
    this->bTrackIdsIsBlacklist = trackIdsIsBlacklist;
}

void GestureLibrary::ResetStrikes(int index)
{
    lib[index].strikes = 0;
}

int GestureLibrary::MostLikely() const
{
    int mostLikelyIndex = INDEX_NONE;
    float maxCompletion = Params.predictPercentage;
    for (int ii = 0; ii < lib.Num(); ii++) {
        if (Disqualified(ii)) {
            continue;
        }
        else if (Finished(ii)) {
            if (mostLikelyIndex == INDEX_NONE) {
                mostLikelyIndex = ii;
            }
            else {
                continue;
            }
        }
        else if (TrackIndex(ii) * Resolution(ii) > START_DRAW_DISTANCE) {
            float completion = ((float)TrackIndex(ii) / (float)Length(ii));
            if (completion > maxCompletion) {
                mostLikelyIndex = ii;
                maxCompletion = completion;
            }
        }
    }
    return mostLikelyIndex;
}

void GestureLibrary::ResetSearch()
{
    for (int ii = 0; ii < lib.Num(); ii++) {
        ResetTracking(ii);
    }
}




int GestureLibrary::Search(const Gesture& TrackedGesture, int maxTrackLength)
{
    int recognizedGestureIndex = INDEX_NONE;

    for (int ii = 0; ii < lib.Num(); ii++) {
        // skip gestures that have failed
        if (!Banned(ii) && !Disqualified(ii)) {
            if (Finished(ii)) {
                // It's possible that we detected a gesture that will not end up being the actual final
                // gesture the player is performing. If a gesture has been completed we should still
                // check to make sure it shouldn't be disqualified
                CheckForDisqualification(ii, TrackedGesture);
            }
            else if (Track(ii, TrackedGesture, maxTrackLength)) {
                recognizedGestureIndex = ii;
            }
        }
    }
    return recognizedGestureIndex;
}



bool GestureLibrary::Track(int gestureIndex, const Gesture& TrackedGesture, float maxTrackLength)
{
    const float refGestureResolution = Resolution(gestureIndex);
    const int maxTrackSegments = FMath::Max(1, (int)(maxTrackLength / refGestureResolution));
    const int smoothing = FMath::RoundToInt(Params.smoothing / refGestureResolution);

    const FVector& TrackedRotationVector = TrackedGesture.PathHeadAngle();

    // update tracking along path for the given gesture
    for (int trackedSegments = 0; trackedSegments < maxTrackSegments && TrackIndex(gestureIndex) >= START_TRACK_INDEX && !Finished(gestureIndex);) {
        // Progress while tracking vec is close enough to reference vec
        int progress = trackingVecAcceptable(gestureIndex, TrackedRotationVector, smoothing);
        if (progress > 0) {
            ProgressAlongPath(gestureIndex);
            trackedSegments++;
            ResetStrikes(gestureIndex);
        }
        else if (TrackedGesture.PathLength() >= Params.patience && trackingVecUnacceptable(gestureIndex, TrackedRotationVector)) {
            // Earn a strike if tracking vec is too different from reference vec, striking out leads to disqual
            if (!trackingAtCorner(gestureIndex, TrackedRotationVector) && Strike(gestureIndex) >= Params.strikeoutDistance) {
                Disqualify(gestureIndex);
            }
            return false;
        }
        else {
            // nothing to do if angle was between acceptable and unacceptable
            return false;
        }
    }
    // check if path finished tracking
    if (TrackIndex(gestureIndex) >= ((float)(Length(gestureIndex) - 1) * Params.completionPercentage)) {
        return true;
    }
    return false;
}

bool GestureLibrary::ContinuousTrack(int gestureIndex, const Gesture& TrackedGesture, float maxTrackLength)
{
    const float refGestureResolution = Resolution(gestureIndex);
    const int maxTrackSegments = FMath::Max(1, (int) (maxTrackLength / refGestureResolution));
    const int smoothing = FMath::RoundToInt(Params.smoothing / refGestureResolution);

    const FVector& TrackedRotationVector = TrackedGesture.PathHeadAngle();

    // update tracking along path for the given gesture
    for (int trackedSegments = 0; trackedSegments < maxTrackSegments && TrackIndex(gestureIndex) >= START_TRACK_INDEX && !Finished(gestureIndex);) {
        // Progress while tracking vec is close enough to reference vec
        int progress = trackingVecAcceptable(gestureIndex, TrackedRotationVector, smoothing);
        if (progress > 0) {
            ProgressAlongPath(gestureIndex);
            trackedSegments++;
            ResetStrikes(gestureIndex);
        }
        // tracking angle was less than acceptable
        else {
            // Gestures that fail when they're just starting should be immediately disqualed so tracking can be reset by the next tick
            if (TrackIndex(gestureIndex) == START_TRACK_INDEX) {
                Disqualify(gestureIndex);
                return false;
            }

            // Earn a strike if tracking vec is too different from reference vec, striking out leads to disqual
            if (trackingVecUnacceptable(gestureIndex, TrackedRotationVector)) {
                if (!trackingAtCorner(gestureIndex, TrackedRotationVector) && Strike(gestureIndex) >= Params.continuousStrikeoutDistance) {
                    Disqualify(gestureIndex);
                }
            }
            return false;
        }
    }
    // check if path finished tracking
    if (TrackIndex(gestureIndex) >= ((float)(Length(gestureIndex) - 1) * Params.completionPercentage)) {
        Finish(gestureIndex);
        return true;
    }
    return false;
}

/**
* Tallies a single strike for an unacceptable angle
* To be used on finished gestures to make sure they haven't been abandoned by later gesture movements
*/
void GestureLibrary::CheckForDisqualification(int index, const Gesture& TrackedGesture)
{
    float similarity = similarityToPath(index, TrackIndex(index), TrackedGesture.PathHeadAngle());
    if (trackingVecUnacceptable(similarity)) {
        if (Strike(index)) {
            Disqualify(index);
        }
    }
}



int GestureLibrary::trackingVecAcceptable(int index, const FVector &rotVec, int smoothing) const
{
    const int startTrackIndex = TrackIndex(index);
    for (int ii = 0; ii <= smoothing; ii++) {
        const int currentTrackIndex = startTrackIndex + ii;
        if (currentTrackIndex < Length(index)) {
            if (similarityToPath(index, currentTrackIndex, rotVec) >= Params.acceptableSimilarity) {
                return ii + 1;
            }
        }
    }
    return 0;
}

bool GestureLibrary::trackingAtCorner(int index, const FVector &rotVec) const
{
    return similarityToPath(index, TrackIndex(index) - 1, rotVec) >= Params.acceptableSimilarity;
}

bool GestureLibrary::trackingVecUnacceptable(int index, const FVector &rotVec) const
{
    return similarityToPath(index, TrackIndex(index), rotVec) <= Params.unacceptableSimilarity;
}

bool GestureLibrary::trackingVecUnacceptable(float similarity) const
{
    return similarity <= Params.unacceptableSimilarity;
}

float GestureLibrary::similarityToPath(int index, int pathIndex, const FVector &rotVec) const
{
    FVector pathVec = Get(index).PathAngleAtIndex(pathIndex);
    return FVector::DotProduct(pathVec, rotVec);
}
