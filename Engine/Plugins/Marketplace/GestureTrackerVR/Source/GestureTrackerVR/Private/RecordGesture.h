// Copyright 2016-2017 Hunter Delattre. All Rights Reserved.

//#pragma warning(disable: 4458)
#pragma once

#include "CoreMinimal.h"
#include "Gesture.h"

/**
* A RecordGesture remembers the entire path of a Gesture and 
* can be stored in an archive for saving/loading.
*/
class GESTURETRACKERVR_API RecordGesture : public Gesture
{
public:

    virtual ~RecordGesture();

    // Used for storing a gesture in an archive
    friend FArchive &operator<<(FArchive &archive, RecordGesture &gesture);

    virtual void Reset(const FVector &startLocation, const FRotator &rotator, float resolution);
    virtual void Reset(const FVector &startLocation, const FRotator &rotator, float resolution, int maxSize);

    // Add a new vector the the path. Should be in same coordinate space as the startLocation
    virtual bool AddToPath(const FVector &trackLocation);

    FVector PathAngleAtIndex(int pathIndex) const;

    FVector Get(int index) const;
    int Size() const;
    TArray<FVector> Path() const;

    virtual float PathLength() const;

protected:
    TArray<FVector> path;

    int maxSize;
};
