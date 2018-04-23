// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * A platform specific implementation of FNativeWindow.
 * Native windows provide platform-specific backing for and are always owned by an SWindow.
 */
class APPLICATIONCORE_API FPS4Window
	: public FGenericWindow
{
protected:

	// FGenericWindow interface

	virtual EWindowMode::Type GetWindowMode() const override
	{
		return EWindowMode::Fullscreen;
	}
};
