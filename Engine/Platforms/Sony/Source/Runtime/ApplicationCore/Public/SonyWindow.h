// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * A platform specific implementation of FNativeWindow.
 * Native windows provide platform-specific backing for and are always owned by an SWindow.
 */
class APPLICATIONCORE_API FSonyWindow
	: public FGenericWindow
{
	mutable int32 VideoOutBusType;

public:

	FSonyWindow()
	{
		FDisplayMetrics Metrics;
		FDisplayMetrics::RebuildDisplayMetrics(Metrics);
		// Always 16:9 on Sony so this is fairly simple.
		DPIScaleFactor = (float)Metrics.PrimaryDisplayHeight / 1080.0f;
	}

	inline void SetVideoOutBusType(int32 BusType) { VideoOutBusType = BusType; }

protected:

	// FGenericWindow interface
	virtual void* GetOSWindowHandle() const override
	{
		return &VideoOutBusType;
	}

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
