// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * A platform specific implementation of FNativeWindow.
 * Native windows provide platform-specific backing for and are always owned by an SWindow.
 */
class APPLICATIONCORE_API FPS4Window
	: public FGenericWindow
{
public:

	FPS4Window()
	{
		FDisplayMetrics Metrics;
		FDisplayMetrics::RebuildDisplayMetrics(Metrics);
		// Always 16:9 on PS4 so this is fairly simple.
		DPIScaleFactor = (float)Metrics.PrimaryDisplayHeight / 1080.0f;
	}

protected:

	// FGenericWindow interface

	virtual EWindowMode::Type GetWindowMode() const override
	{
		return EWindowMode::Fullscreen;
	}

	virtual float GetDPIScaleFactor() const override
	{
		return DPIScaleFactor;
	}

	/**
	* Ratio of pixels to SlateUnits in this window.
	* E.g. DPIScale of 2.0 means there is a 2x2 pixel square for every 1x1 SlateUnit.
	*/
	float DPIScaleFactor;
};
