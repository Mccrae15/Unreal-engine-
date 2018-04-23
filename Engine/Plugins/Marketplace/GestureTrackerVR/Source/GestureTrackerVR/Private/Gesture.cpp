// Copyright 2016-2017 Hunter Delattre. All Rights Reserved.

#include "Gesture.h"

void Gesture::Reset(const FVector &startLocation, const FRotator &rotator, float resolution)
{
    this->currentPathHead = FVector(0, 0, 0);
    this->startLocation = startLocation;
    this->rotator = rotator;
    this->resolution = resolution;

    this->pathHeadAngle = FVector(0, 0, 0);
    this->pathHeadLocation = FVector(0, 0, 0);

    this->pathTotalLength = 0;
}


bool Gesture::AddToPath(const FVector &trackLocation)
{
    FVector newSegment = (trackLocation - startLocation) - currentPathHead;

    int segmentCount = newSegment.Size() / resolution;

    if (segmentCount < 1) {
        // The tracked location was not far enough from the path head to track (length < resolution)
        return false;
    }

    // the length of any new path segment is a multiple of resolution
    pathHeadLength = segmentCount * resolution;
    pathTotalLength += pathHeadLength;
    newSegment.Normalize();

    lastPathHead = currentPathHead;
    currentPathHead = lastPathHead + (newSegment * pathHeadLength);

    pathHeadLocation = UnrotateVector(currentPathHead);
    pathHeadAngle = UnrotateVector(currentPathHead - lastPathHead);
    pathHeadAngle.Normalize();

    return true;
}

float Gesture::Resolution() const
{
    return resolution;
}

FVector Gesture::PathHead() const
{
    return pathHeadLocation;
}

FVector Gesture::PathHeadAngle() const
{
    return pathHeadAngle;
}

float Gesture::PathHeadLength() const
{
    return pathHeadLength;
}

FRotator Gesture::Rotator() const
{
    return rotator;
}

FVector Gesture::UnrotateVector(const FVector &vector) const
{
    return rotator.UnrotateVector(vector);
}

float Gesture::PathLength() const
{
    return pathTotalLength;
}

FVector Gesture::StartLocation() const
{
    return startLocation;
}
