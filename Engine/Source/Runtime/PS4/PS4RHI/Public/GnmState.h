// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmState.h: Gnm state definitions.
=============================================================================*/

#pragma once

#include "RHI.h"
#include "RHIResources.h"

class FGnmSamplerState : public FRHISamplerState
{
public:
	
	/** Sampler state object */
	Gnm::Sampler Sampler;

	/** 
	 * Constructor
	 */
	FGnmSamplerState(const FSamplerStateInitializerRHI& Initializer);
};

class FGnmRasterizerState : public FRHIRasterizerState
{
public:

	/** Structure that contains most rasterization state */
	Gnm::PrimitiveSetup Setup;

	/** Polygon offset values */
	float PolyOffset;
	float PolyScale;

	/** MSAA control */
	bool bAllowMSAA;

	/**
	 * Constructor
	 */
	FGnmRasterizerState(const FRasterizerStateInitializerRHI& Initializer);
};

class FGnmDepthStencilState : public FRHIDepthStencilState
{
public:

	/** Cached depth stencil state structure */
	Gnm::DepthStencilControl DepthStencilState;

	/** Operations to perform for front and back */
	Gnm::StencilOpControl StencilOps;	

	/** Stencil mask values */
	Gnm::StencilControl StencilControl;

	/** 
	 * Constructor
	 */
	FGnmDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer);

	bool GetZWriteEnabled() const;
};

class FGnmBlendState : public FRHIBlendState
{
public:
	/** Blend state for each possible render target (mirrors the array in FBlendStateInitializerRHI) */
	TStaticArray<Gnm::BlendControl, MaxSimultaneousRenderTargets> RenderTargetStates;
	
	/** Color write mask for all RTs in one value */
	uint32 MRTColorWriteMask;

	/** 
	 * Constructor
	 */
	FGnmBlendState(const FBlendStateInitializerRHI& Initializer);
};
