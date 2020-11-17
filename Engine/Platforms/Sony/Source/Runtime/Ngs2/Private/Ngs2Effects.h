// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "AudioEffect.h"
#include "Ngs2Device.h"

/** 
 * Ngs2 effects manager
 */
class FNgs2EffectsManager : public FAudioEffectsManager
{
public:
	FNgs2EffectsManager(class FNgs2Device* InDevice, int32 NumChannels);
	~FNgs2EffectsManager();

	/** 
	 * Calls the platform specific code to set the parameters that define reverb
	 */
	virtual void SetReverbEffectParameters(const FAudioEffectParameters& InEffectParameters) override;

	/** 
	 * Calls the platform specific code to set the parameters that define EQ
	 */
	virtual void SetEQEffectParameters(const FAudioEffectParameters& InEffectParameters) override;

	/** 
	 * Calls the platform specific code to set the parameters that define a radio effect.
	 * 
	 * @param	RadioEffectParameters	The new parameters for the radio distortion effect. 
	 */
	virtual void SetRadioEffectParameters(const FAudioEffectParameters& InEffectParameters) override;

private:

	/** Cached typecasted pointer */
	class FNgs2Device* Ngs2Device;

	/** The reverb rack for reverb effects */
	SceNgs2Handle ReverbRack;

	/** The reverb voice from the rack */
	SceNgs2Handle ReverbVoice;

	/** The EQ rack for reverb effects */
	SceNgs2Handle EQRack;

	/** The EQ voice from the rack */
	SceNgs2Handle EQVoice;

	friend class FNgs2Device;
	friend class FNgs2SoundSource;
};
