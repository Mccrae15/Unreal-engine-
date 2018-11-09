// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaOverlaySample.h"
#include "Internationalization/Text.h"
#include "Math/Vector2D.h"
#include "MediaSampleQueue.h"
#include "Misc/Timespan.h"

#include <sceavplayer.h>


/**
 * Implements an overlay text sample for PS4Media.
 */
class FPS4MediaOverlaySample
	: public IMediaOverlaySample
{
public:

	/** Default constructor. */
	FPS4MediaOverlaySample()
		: Position(FVector2D::ZeroVector)
		, Time(FTimespan::Zero())
	{ }

	/** Virtual destructor. */
	virtual ~FPS4MediaOverlaySample() { }

public:

	/**
	 * Initialize the sample.
	 *
	 * @param FrameInfo Timed text frame information.
	 * @return true on success, false otherwise.
	 */
	bool Initialize(const SceAvPlayerFrameInfo& FrameInfo)
	{
		const SceAvPlayerTimedText& Details = FrameInfo.details.subs;
		const FString TimedText = FString(Details.textSize, StringCast<TCHAR>((char*)FrameInfo.pData).Get());

		Position = FVector2D(FrameInfo.details.subs.position.left, FrameInfo.details.subs.position.top);
		Text = FText::FromString(TimedText);
		Time = FTimespan::FromMilliseconds(FrameInfo.timeStamp);

		return true;
	}

public:

	//~ IMediaOverlaySample interface

	virtual FTimespan GetDuration() const override
	{
		return FTimespan::MaxValue(); // overlay sink will be flushed with each time text
	}

	virtual TOptional<FVector2D> GetPosition() const override
	{
		return Position;
	}

	virtual FText GetText() const override
	{
		return Text;
	}

	virtual FTimespan GetTime() const override
	{
		return Time;
	}

	virtual EMediaOverlaySampleType GetType() const override
	{
		return EMediaOverlaySampleType::Caption;
	}

private:

	/** The position at which to display the overlay text. */
	FVector2D Position;

	/** The overlay text. */
	FText Text;

	/** The play time for which the sample was generated. */
	FTimespan Time;
};


/** Implements a queue for PS4 text overlay samples. */
class FPS4MediaOverlaySampleQueue : public TMediaSampleQueue<FPS4MediaOverlaySample> { };
