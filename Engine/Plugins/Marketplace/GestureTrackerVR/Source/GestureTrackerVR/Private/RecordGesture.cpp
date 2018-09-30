// Copyright 2016-2017 Hunter Delattre. All Rights Reserved.

#include "RecordGesture.h"

RecordGesture::~RecordGesture()
{
    path.Empty();
}

FArchive &operator<<(FArchive &archive, RecordGesture &gesture)
{
    archive << gesture.resolution;

    int32 length;
    if (archive.IsLoading()) {
        archive << length;
        gesture.path.Init(FVector(), length);
    }
    else {
        length = gesture.Size();
        archive << length;
    }
    for (int32 ii = 0; ii < length; ii++) {
        archive << gesture.path[ii];
    }

    return archive;
}

void RecordGesture::Reset(const FVector &startLocation, const FRotator &rotator, float resolution)
{
    RecordGesture::Reset(startLocation, rotator, resolution, MAX_int32);
}

void RecordGesture::Reset(const FVector &startLocation, const FRotator &rotator, float resolution, int maxSize)
{
    Gesture::Reset(startLocation, rotator, resolution);

    path.Empty();
    this->maxSize = maxSize;
}

bool RecordGesture::AddToPath(const FVector &trackLocation)
{
    if (Size() > maxSize) {
        return false;
    }

    FVector segment = (trackLocation - startLocation) - currentPathHead;

    int segmentCount = segment.Size() / resolution;

    // The tracked location was not far enough from the path head to track (length < resolution)
    if (segmentCount < 1) {
        return false;
    }

    segment.Normalize();
    segment *= resolution;

    lastPathHead = currentPathHead;
    for (int ii = 0; ii < segmentCount && Size() < maxSize; ii++) {
        currentPathHead += segment;
        path.Add(UnrotateVector(currentPathHead));
    }

    pathHeadLocation = UnrotateVector(currentPathHead);
    pathHeadAngle = UnrotateVector(segment);
    pathHeadAngle.Normalize();
    pathHeadLength = 1;

    return true;
}

FVector RecordGesture::Get(int index) const
{
    return path[index];
}

FVector RecordGesture::PathAngleAtIndex(int pathIndex) const
{
    FVector retVal;

    if (pathIndex == 0) {
        retVal = path[pathIndex];
    }
    else {
        retVal = path[pathIndex] - path[pathIndex - 1];
    }
    retVal.Normalize();
    return retVal;
}

int RecordGesture::Size() const
{
    return path.Num();
}

TArray<FVector> RecordGesture::Path() const
{
    return path;
}

float RecordGesture::PathLength() const
{
    return path.Num() * resolution;
}