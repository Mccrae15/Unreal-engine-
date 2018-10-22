// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmState.cpp: Gnm state implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"

/**
 * Translate from UE4 enums to Gnm enums
 */
static Gnm::MipFilterMode TranslateMipFilterMode(ESamplerFilter Filter)
{
	switch (Filter)
	{
		case SF_Point:				return Gnm::kMipFilterModePoint;
		case SF_Bilinear:			return Gnm::kMipFilterModePoint;
		case SF_Trilinear:			return Gnm::kMipFilterModeLinear;
		case SF_AnisotropicPoint:	return Gnm::kMipFilterModePoint;
		case SF_AnisotropicLinear:	return Gnm::kMipFilterModeLinear;
		default:					return Gnm::kMipFilterModePoint;
	}
}

static Gnm::FilterMode TranslateFilterMode(ESamplerFilter Filter)
{
	switch (Filter)
	{
		case SF_Point:				return Gnm::kFilterModePoint;
		case SF_Bilinear:			return Gnm::kFilterModeBilinear;
		case SF_Trilinear:			return Gnm::kFilterModeBilinear;
		case SF_AnisotropicPoint:	return Gnm::kFilterModeAnisoBilinear;
		case SF_AnisotropicLinear:	return Gnm::kFilterModeAnisoBilinear;
		default:					return Gnm::kFilterModeBilinear;
	}
}

static Gnm::ZFilterMode TranslateZFilterMode(ESamplerFilter Filter)
{
	switch (Filter)
	{
		case SF_Point:				return Gnm::kZFilterModePoint;
		case SF_AnisotropicPoint:
		case SF_AnisotropicLinear:
		default:					return Gnm::kZFilterModeLinear;
	}
}

static Gnm::AnisotropyRatio TranslateAnisotropy( uint32 Anisotropy )
{
	return ( Gnm::AnisotropyRatio ) FMath::FloorLog2( Anisotropy );
}

static Gnm::PrimitiveSetupCullFaceMode TranslateCullMode(ERasterizerCullMode CullMode)
{
	switch (CullMode)
	{
		case CM_CCW:	return Gnm::kPrimitiveSetupCullFaceFront;
		case CM_CW:		return Gnm::kPrimitiveSetupCullFaceBack;
		default:		return Gnm::kPrimitiveSetupCullFaceNone;
	}
}

static Gnm::WrapMode TranslateWrapMode(ESamplerAddressMode AddressMode)
{
	switch (AddressMode)
	{
		case AM_Clamp:	return Gnm::kWrapModeClampLastTexel;
		case AM_Mirror: return Gnm::kWrapModeMirror;
		case AM_Border: return Gnm::kWrapModeClampBorder;
		default:		return Gnm::kWrapModeWrap;
	}
}

static Gnm::CompareFunc TranslateCompareFunction(ECompareFunction CompareFunction)
{
	switch(CompareFunction)
	{
		case CF_Less:			return Gnm::kCompareFuncLess;
		case CF_LessEqual:		return Gnm::kCompareFuncLessEqual;
		case CF_Greater:		return Gnm::kCompareFuncGreater;
		case CF_GreaterEqual:	return Gnm::kCompareFuncGreaterEqual;
		case CF_Equal:			return Gnm::kCompareFuncEqual;
		case CF_NotEqual:		return Gnm::kCompareFuncNotEqual;
		case CF_Never:			return Gnm::kCompareFuncNever;
		default:				return Gnm::kCompareFuncAlways;
	};
}

static Gnm::DepthCompare TranslateSamplerCompareFunction(ESamplerCompareFunction SamplerComparisonFunction)
{
	switch(SamplerComparisonFunction)
	{
		case SCF_Less:		return Gnm::kDepthCompareLess;
		case SCF_Never: 
		default:			return Gnm::kDepthCompareNever;
	};
}

static Gnm::StencilOp TranslateStencilOp(EStencilOp StencilOp)
{
	switch(StencilOp)
	{
		case SO_Zero:				return Gnm::kStencilOpZero;
		case SO_Replace:			return Gnm::kStencilOpReplaceTest;
		case SO_SaturatedIncrement:	return Gnm::kStencilOpAddClamp;
		case SO_SaturatedDecrement:	return Gnm::kStencilOpSubClamp;
		case SO_Invert:				return Gnm::kStencilOpInvert;
		case SO_Increment:			return Gnm::kStencilOpAddWrap;
		case SO_Decrement:			return Gnm::kStencilOpSubWrap;
		default:					return Gnm::kStencilOpKeep;
	};
}

static Gnm::PrimitiveSetupPolygonMode TranslateFillMode(ERasterizerFillMode FillMode)
{
	switch(FillMode)
	{
		case FM_Wireframe:	return Gnm::kPrimitiveSetupPolygonModeLine;
		case FM_Point:		return Gnm::kPrimitiveSetupPolygonModePoint;
		default:			return Gnm::kPrimitiveSetupPolygonModeFill;
	};
}

static Gnm::BlendFunc TranslateBlendOp(EBlendOperation BlendOp)
{
	switch(BlendOp)
	{
		case BO_Subtract:	return Gnm::kBlendFuncSubtract;
		case BO_Min:		return Gnm::kBlendFuncMin;
		case BO_Max:		return Gnm::kBlendFuncMax;
		default:			return Gnm::kBlendFuncAdd;
	};
}


static Gnm::BlendMultiplier TranslateBlendFactor(EBlendFactor BlendFactor)
{
	switch(BlendFactor)
	{
		case BF_One:					return Gnm::kBlendMultiplierOne;
		case BF_SourceColor:			return Gnm::kBlendMultiplierSrcColor;
		case BF_InverseSourceColor:		return Gnm::kBlendMultiplierOneMinusSrcColor;
		case BF_SourceAlpha:			return Gnm::kBlendMultiplierSrcAlpha;
		case BF_InverseSourceAlpha:		return Gnm::kBlendMultiplierOneMinusSrcAlpha;
		case BF_DestAlpha:				return Gnm::kBlendMultiplierDestAlpha;
		case BF_InverseDestAlpha:		return Gnm::kBlendMultiplierOneMinusDestAlpha;
		case BF_DestColor:				return Gnm::kBlendMultiplierDestColor;
		case BF_InverseDestColor:		return Gnm::kBlendMultiplierOneMinusDestColor;
		default:						return Gnm::kBlendMultiplierZero;
	};
}




/** 
 * Constructor
 */
FGnmSamplerState::FGnmSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	Sampler.init();

	// set the mip filtering
	Sampler.setMipFilterMode(TranslateMipFilterMode(Initializer.Filter));
	Gnm::FilterMode XYFilter = TranslateFilterMode(Initializer.Filter);
	Sampler.setXyFilterMode(XYFilter, XYFilter);
	Sampler.setZFilterMode(TranslateZFilterMode(Initializer.Filter));

	// set address mode
	Sampler.setWrapMode(
		TranslateWrapMode(Initializer.AddressU),
		TranslateWrapMode(Initializer.AddressV),
		TranslateWrapMode(Initializer.AddressW));
	
	// @todo gnm: Make sure this is putting it into proper number space
	Sampler.setLodBias(sce::Gnmx::convertF32ToS6_8(Initializer.MipBias), sce::Gnmx::convertF32ToS2_4(Initializer.MipBias));

	Sampler.setDepthCompareFunction(TranslateSamplerCompareFunction(Initializer.SamplerComparisonFunction));

/*
	// @todo gnm: Work on Border Color here. It's rather non-trivial unfortunately.
	if (Initializer.BorderColor == 0xFF000000)
	{
		Sampler.setBorderColor(Gnm::kBorderColorOpaqueBlack);
	}
	else if (Initializer.BorderColor == FLinearColor::White)
	{
		Sampler.setBorderColor(Gnm::kBorderColorOpaqueWhite);
	}
	else if (Initializer.BorderColor == FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
	{
		Sampler.setBorderColor(Gnm::kBorderColorTransparentBlack);
	}
	else if ()
	{
		Sampler.setBorderColor(Gnm::kBorderColorFromTable);
		Sampler.setBorderColorTableIndex(0);
		Sampler.setBorderColor(Gnm::kBorderColorOpaqueBlack);
	}
*/

	uint32 LocalMaxAnisotropy = ComputeAnisotropyRT(Initializer.MaxAnisotropy);
	Gnm::AnisotropyRatio MaxAnisotropyRatio = TranslateAnisotropy( LocalMaxAnisotropy );
	Sampler.setAnisotropyRatio( MaxAnisotropyRatio );
}

/**
 * Constructor
 */
FGnmRasterizerState::FGnmRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	Setup.init();

	// what to cull?
	Setup.setCullFace(TranslateCullMode(Initializer.CullMode));

	// set wireframe/solid
	Gnm::PrimitiveSetupPolygonMode PolygonMode = TranslateFillMode(Initializer.FillMode);
	Setup.setPolygonMode(PolygonMode, PolygonMode);

	// is polygon offset enabled?
	PolyOffset = Initializer.DepthBias;
	PolyScale = Initializer.SlopeScaleDepthBias * float((1<<24)-1);	// Warning: this assumes depth bits == 24, and won't be correct with 32 (This matches D3D & OpenGL behaviour!)
	bool bUsePolyOffset = (PolyOffset != 0.0f || PolyScale != 0.0f);
	Setup.setPolygonOffsetEnable(
		bUsePolyOffset ? Gnm::kPrimitiveSetupPolygonOffsetEnable : Gnm::kPrimitiveSetupPolygonOffsetDisable,
		bUsePolyOffset ? Gnm::kPrimitiveSetupPolygonOffsetEnable : Gnm::kPrimitiveSetupPolygonOffsetDisable);
}

/** 
 * Constructor
 */
FGnmDepthStencilState::FGnmDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	DepthStencilState.init();
	StencilOps.init();
	StencilControl.init();

	// set what is enabled
	bool bDepthTestEnabled = Initializer.DepthTest != CF_Always || Initializer.bEnableDepthWrite;
	bool bStencilTestEnabled = Initializer.bEnableFrontFaceStencil || Initializer.bEnableBackFaceStencil;
	DepthStencilState.setDepthEnable(bDepthTestEnabled);
	DepthStencilState.setStencilEnable(bStencilTestEnabled);
	DepthStencilState.setStencilFunction(TranslateCompareFunction(Initializer.FrontFaceStencilTest));

	// bEnableBackFaceStencil of false means to use the front face settings, not to disable back face stencil
	if (Initializer.bEnableBackFaceStencil)
	{
		DepthStencilState.setSeparateStencilEnable(true);
		DepthStencilState.setStencilFunctionBack(TranslateCompareFunction(Initializer.BackFaceStencilTest));
	}

	// set depth control
	Gnm::DepthControlZWrite ZWriteFlag = Initializer.bEnableDepthWrite ? Gnm::kDepthControlZWriteEnable : Gnm::kDepthControlZWriteDisable;
	DepthStencilState.setDepthControl(ZWriteFlag, TranslateCompareFunction(Initializer.DepthTest));	
	
	// set stencil operations
	StencilOps.setStencilOps(
		TranslateStencilOp(Initializer.FrontFaceStencilFailStencilOp),
		TranslateStencilOp(Initializer.FrontFacePassStencilOp),
		TranslateStencilOp(Initializer.FrontFaceDepthFailStencilOp));
	

	// bEnableBackFaceStencil of false means to use the front face settings, not to disable back face stencil
	if (Initializer.bEnableBackFaceStencil)
	{
		StencilOps.setStencilOpsBack(
			TranslateStencilOp(Initializer.BackFaceStencilFailStencilOp),
			TranslateStencilOp(Initializer.BackFacePassStencilOp),
			TranslateStencilOp(Initializer.BackFaceDepthFailStencilOp));
	}
	else
	{
		StencilOps.setStencilOpsBack(
			TranslateStencilOp(Initializer.FrontFaceStencilFailStencilOp),
			TranslateStencilOp(Initializer.FrontFacePassStencilOp),
			TranslateStencilOp(Initializer.FrontFaceDepthFailStencilOp));
	}

	///the only operations the RHI supports that uses opval are increments and decrements.
	StencilControl.m_opVal = 1;	

	// remember the masks
	StencilControl.m_mask = (uint8)Initializer.StencilReadMask;
	StencilControl.m_writeMask = (uint8)Initializer.StencilWriteMask;
}

bool FGnmDepthStencilState::GetZWriteEnabled() const
{
	// could break in an SDK change.  the DepthStencilControl is opaque.
	return (DepthStencilState.m_reg & 0x4) != 0;
}


/** 
 * Constructors
 */
FGnmBlendState::FGnmBlendState(const FBlendStateInitializerRHI& Initializer)
{
	// Gnm can only handle 8 RTs
	static_assert(MaxSimultaneousRenderTargets <= 8, "Too many MRTs.");

	// default to no color writing, each RT will enable 
	MRTColorWriteMask = 0;

	for (uint32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
	{
		// src and dest structs
		const FBlendStateInitializerRHI::FRenderTarget& InitState = Initializer.RenderTargets[Index];
		Gnm::BlendControl& State = RenderTargetStates[Index];
		State.init();

		bool bBlendEnabled = 
			InitState.ColorBlendOp != BO_Add || InitState.ColorDestBlend != BF_Zero || InitState.ColorSrcBlend != BF_One ||
			InitState.AlphaBlendOp != BO_Add || InitState.AlphaDestBlend != BF_Zero || InitState.AlphaSrcBlend != BF_One;

		// translate settings
		if (bBlendEnabled)
		{
			State.setBlendEnable(true);
			State.setSeparateAlphaEnable(true);
			State.setColorEquation(TranslateBlendFactor(InitState.ColorSrcBlend), TranslateBlendOp(InitState.ColorBlendOp), TranslateBlendFactor(InitState.ColorDestBlend));
			State.setAlphaEquation(TranslateBlendFactor(InitState.AlphaSrcBlend), TranslateBlendOp(InitState.AlphaBlendOp), TranslateBlendFactor(InitState.AlphaDestBlend));
		}

		// set the write masking for this RT
		MRTColorWriteMask |= (InitState.ColorWriteMask & 0xF) << (Index * 4);
	}
}



FSamplerStateRHIRef FGnmDynamicRHI::RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	// constructor does it all
	return new FGnmSamplerState(Initializer);
}

FRasterizerStateRHIRef FGnmDynamicRHI::RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
    return new FGnmRasterizerState(Initializer);
}

FDepthStencilStateRHIRef FGnmDynamicRHI::RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	// constructor does it all
	return new FGnmDepthStencilState(Initializer);
}


FBlendStateRHIRef FGnmDynamicRHI::RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	return new FGnmBlendState(Initializer);
}

FComputeFenceRHIRef FGnmDynamicRHI::RHICreateComputeFence(const FName& Name)
{
	uint64* LabelLoc = (uint64*)GGnmManager.AllocateGPULabelLocation();
	return new FGnmComputeFence(Name, LabelLoc);
}
