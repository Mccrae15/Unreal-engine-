// Copyright 2016-2017 Hunter Delattre. All Rights Reserved.

//#pragma warning(disable: 4458)
#pragma once

#include "CoreMinimal.h"

/**
* A Gesture object is used to track the path through space that the gesture takes.
* This base Gesture class only stored the last two tracked points since they are the
* only ones necessary for recognition. See RecordGesture for complete path storage.
*/
class GESTURETRACKERVR_API Gesture
{
public:

    // Reset the gesture's path and assign a new startLocation and rotator.
    // Must be called before a Gesture is usable.
    virtual void Reset(const FVector &startLocation, const FRotator &rotator, float resolution);

    // Add a new vector the the gesture path. Should be in same coordinate space as the startLocation
    virtual bool AddToPath(const FVector &trackLocation);

    // The head of the tracked path, yaw rotation normalized to 0
    FVector PathHead() const;

    // A normal vector representing the rotation to the path head, yaw rotation normalized to 0
    FVector PathHeadAngle() const;

    // The distance from the last path head to the current path head in units
    float PathHeadLength() const;

    float Resolution() const;
    FRotator Rotator() const;

    // The startLocation of the gesture in world space
    FVector StartLocation() const;

    virtual float PathLength() const;

protected:
    // the length of a segment of the gesture in units
    float resolution;

    // The location in world space from which the current Gesture was started
    FVector startLocation;

    // A rotator describing the hand rotation when the Gesture started being tracked
    FRotator rotator;
    
    // Actual path head locations (yaw rotation not normalized). For internal use.
    FVector lastPathHead;
    FVector currentPathHead;
    
    // Unrotated (yaw rotation normalized) path head. For external use (comparing, recording gestures).
    FVector pathHeadLocation;

    // Unrotated normal vector pointing from the lastPathHead to the currentPathHead
    FVector pathHeadAngle;

    // Length in units of the last tracked segment (distance from current to last path head)
    float pathHeadLength;

    // Unrotate the given vector using this gesture's rotator (used to normalize vector rotations)
    FVector UnrotateVector(const FVector &rotationVector) const;

private:

    // The total length tracked by this gesture
    float pathTotalLength;

};
