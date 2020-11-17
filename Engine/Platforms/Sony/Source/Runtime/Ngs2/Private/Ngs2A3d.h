/* SCEA CONFIDENTIAL
 $PSLibId$
 * Copyright (C) 2012 Sony Computer Entertainment of America Inc.
 * All Rights Reserved.
 */
#pragma once

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <ngs2.h>
#include <libsysmodule.h>
#include <stdlib.h>
#include <stdio.h>
//#include <sampleutil.h>	
#include <audio3d.h>
#include <kernel.h>

// default libaudio3d support to on.  Licensees can fall back to pure NGS2 if necessary
// via this define.
#ifndef A3D
#define A3D 0
#endif

#ifndef USING_A3D
#define USING_A3D 0
#endif

#define PI2						(3.1415926535897932384626433832795f)

#if A3D
/**
 * Class to manage system level libAudio3d operations. 
 */
class Audio3dService
{
public:
	Audio3dService(void) :
		bIsInitialized(false)
	{
	}

	virtual ~Audio3dService(void)
	{
		Terminate();
	}
	
	void Init();	
	void Terminate(void);	

private:
	bool bIsInitialized;
};

class Audio3dUtils
{
public:

	/**
	* Structures and classes to implement custom Ngs2 sampler rack.
	*/
	struct A3dModuleParam
	{
		uint32_t Priority;				// Priority
		float X, Y, Z;					// Position
		float Spread;					// Spread
		float Gain;						// Gain
	};

	struct A3dModuleWork
	{
		SceAudio3dObjectId objectId;
	};

	static int32_t A3dModuleSetup(SceNgs2UserFx2SetupContext* Context);
	static int32_t A3dModuleProcess(SceNgs2UserFx2ProcessContext* Context);
};
#endif