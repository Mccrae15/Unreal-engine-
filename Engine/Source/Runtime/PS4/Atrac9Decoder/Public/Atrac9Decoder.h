// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioDecompress.h"



/**
* FAtrac9DecoderModule
*/

class FAtrac9DecoderModule : public IModuleInterface
{
	//--------------------------------------------------------------------------
	// Module functionality
	//--------------------------------------------------------------------------
public:
	FAtrac9DecoderModule();
	virtual ~FAtrac9DecoderModule();

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline FAtrac9DecoderModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FAtrac9DecoderModule >("Atrac9Decoder");
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("Atrac9Decoder");
	}

	static ICompressedAudioInfo* CreateCompressedAudioInfo();

	/** Initialize the Ajm library and codecs */
	static bool InitializeAjm();

	/** Shutdown the Ajm library and codecs */
	static bool ShutdownAjm();

	/** Returns the Ajm system context */
	static uint32 GetCodecSystemContextId();

private:

	/** Will test if Ajm is OK to allocate our theoretical max, if not we are in trouble. */
	static bool SanityCheckMemoryUsage();
};