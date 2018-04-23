// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/**
* Set a constant into a constant buffer
*/
FORCEINLINE void FGnmCommandListContext::UpdateVSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size)
{
	check(BufferIndex == 0);
	VSConstantBuffer->UpdateConstant((const uint8*)NewValue, (uint16)BaseIndex, (uint16)Size);
}
FORCEINLINE void FGnmCommandListContext::UpdateHSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size)
{
	check(BufferIndex == 0);
	HSConstantBuffer->UpdateConstant((const uint8*)NewValue, (uint16)BaseIndex, (uint16)Size);
}
FORCEINLINE void FGnmCommandListContext::UpdateDSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size)
{
	check(BufferIndex == 0);
	DSConstantBuffer->UpdateConstant((const uint8*)NewValue, (uint16)BaseIndex, (uint16)Size);
}
FORCEINLINE void FGnmCommandListContext::UpdatePSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size)
{
	check(BufferIndex == 0);
	PSConstantBuffer->UpdateConstant((const uint8*)NewValue, (uint16)BaseIndex, (uint16)Size);
}
FORCEINLINE void FGnmCommandListContext::UpdateGSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size)
{
	check(BufferIndex == 0);
	GSConstantBuffer->UpdateConstant((const uint8*)NewValue, (uint16)BaseIndex, (uint16)Size);
}
FORCEINLINE void FGnmCommandListContext::UpdateCSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size)
{
	check(BufferIndex == 0);
	CSConstantBuffer->UpdateConstant((const uint8*)NewValue, (uint16)BaseIndex, (uint16)Size);
}

FORCEINLINE const FVector4& FGnmCommandListContext::GetViewportBounds() const
{
	return CachedViewportBounds;
}

FORCEINLINE void FGnmCommandListContext::SetPrimitiveType(Gnm::PrimitiveType Type)
{
	if (Type != CachedPrimitiveType)
	{
		GnmContext->setPrimitiveType(Type);
		CachedPrimitiveType = Type;
	}
}

FORCEINLINE void FGnmCommandListContext::SetIndexSize(Gnm::IndexSize IndexSize)
{
	if (IndexSize != CachedIndexSize)
	{
		GnmContext->setIndexSize(IndexSize);
		CachedIndexSize = IndexSize;
	}
}

FORCEINLINE void FGnmCommandListContext::SetPrimitiveSetup(Gnm::PrimitiveSetup PrimitiveSetup)
{
	if (PrimitiveSetup.m_reg != CachedPrimitiveSetup.m_reg)
	{
		GnmContext->setPrimitiveSetup(PrimitiveSetup);
		CachedPrimitiveSetup = PrimitiveSetup;
	}
}

FORCEINLINE void FGnmCommandListContext::SetIndexOffset( uint32 IndexOffset )
{
	if( IndexOffset != CachedIndexOffset )
	{
		GnmContext->setIndexOffset( IndexOffset );
		CachedIndexOffset = IndexOffset;
	}
}

FORCEINLINE void FGnmCommandListContext::SetNumInstances(uint32 NumInstances)
{
	if (NumInstances != CachedNumInstances)
	{
		GnmContext->setNumInstances(NumInstances);
		CachedNumInstances = NumInstances;
	}
}

FORCEINLINE void FGnmCommandListContext::SetPolygonOffset(float PolyScale, float PolyOffset)
{
	if (PolyScale != CachedPolyScale || PolyOffset != CachedPolyOffset)
	{
		GnmContext->setPolygonOffsetFront(PolyScale, PolyOffset);
		GnmContext->setPolygonOffsetBack(PolyScale, PolyOffset);
		CachedPolyScale = PolyScale;
		CachedPolyOffset = PolyOffset;
	}
}

FORCEINLINE bool FGnmCommandListContext::SetScanModeControl(Gnm::ScanModeControlAa AAEnabled, Gnm::ScanModeControlViewportScissor ScissorEnabled)
{
	if (AAEnabled != CachedAAEnabled || ScissorEnabled != CachedScissorEnabled)
	{
		GnmContext->setScanModeControl(AAEnabled, ScissorEnabled);
		CachedAAEnabled = AAEnabled;
		CachedScissorEnabled = ScissorEnabled;
		return true;
	}

	return false;
}

FORCEINLINE void FGnmCommandListContext::SetAAEnabled(bool bEnabled)
{
	SetScanModeControl(
		bEnabled ? Gnm::kScanModeControlAaEnable : Gnm::kScanModeControlAaDisable,
		CachedScissorEnabled);
}

FORCEINLINE void FGnmCommandListContext::SetViewportScissorRect(bool bEnabled, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	// Only need to test cached scissor bounds if we had scissor enabled
	bool bTestScissorBounds = CachedScissorEnabled && bEnabled;
	SetScanModeControl(
		CachedAAEnabled,
		bEnabled ? Gnm::kScanModeControlViewportScissorEnable : Gnm::kScanModeControlViewportScissorDisable);
	if (bEnabled)
	{
		if (!bTestScissorBounds || (CachedViewportScissorMinX != MinX || CachedViewportScissorMinY != MinY || CachedViewportScissorMaxX != MaxX || CachedViewportScissorMaxY != MaxY))
		{
			// Double check here since the hardware will happily overwrite memory if the scissor is not correct			
			check(MinX >= 0 && MinX >= 0);
			MaxX = FMath::Min(MaxX, (uint32)CachedViewportBounds.Z);
			MaxY = FMath::Min(MaxY, (uint32)CachedViewportBounds.W);
			GnmContext->setViewportScissor(0, MinX, MinY, MaxX, MaxY, Gnm::kWindowOffsetDisable);
			CachedViewportScissorMinX = MinX;
			CachedViewportScissorMinY = MinY;
			CachedViewportScissorMaxX = MaxX;
			CachedViewportScissorMaxY = MaxY;
		}
	}
}

FORCEINLINE void FGnmCommandListContext::SetGenericScissorRect(bool bEnabled, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	if (!bEnabled)
	{
		if (CurrentRenderTargetTextures[0])
		{
			Gnm::RenderTarget* ColorBuffer = &CurrentRenderTargets[0];
			MaxX = ColorBuffer->getWidth();
			MaxY = ColorBuffer->getHeight();
		}
		else if (CurrentDepthTarget)
		{
			Gnm::DepthRenderTarget* DepthTarget = &CurrentDepthRenderTarget;

			MaxX = DepthTarget->getWidth();
			MaxY = DepthTarget->getHeight();
		}
		MinX = 0;
 		MinY = 0;
	}

	// check against cached version because setting scissor will roll hardware context.
	if ((CachedGenericScissorMinX != MinX || CachedGenericScissorMinY != MinY || CachedGenericScissorMaxX != MaxX || CachedGenericScissorMaxY != MaxY))
	{		
		GnmContext->setGenericScissor(MinX, MinY, MaxX, MaxY, Gnm::kWindowOffsetDisable);
		CachedGenericScissorMinX = MinX;
		CachedGenericScissorMinY = MinY;
		CachedGenericScissorMaxX = MaxX;
		CachedGenericScissorMaxY = MaxY;
	}	
}

static FORCEINLINE void SetupGuardBands(const FVector& Scale, const FVector& Bias, GnmContextType* const GnmContext)
{
	// Set the guard band offset so that the guard band is centered around the viewport region.
	// 10xx limits hardware offset to multiples of 16 pixels
	// Primitive filtering further restricts this offset to a multiple of 64 pixels.
	int32 HWOffsetX = SCE_GNM_MIN(508, (int32)floor(Bias[0] / 16.0f + 0.5f)) & ~0x3;
	int32 HWOffsetY = SCE_GNM_MIN(508, (int32)floor(Bias[1] / 16.0f + 0.5f)) & ~0x3;
	GnmContext->setHardwareScreenOffset(HWOffsetX, HWOffsetY);

	// Set the guard band clip distance to the maximum possible values by calculating the minimum distance
	// from the closest viewport edge to the edge of the hardware's coordinate space
	float HWMin = -(float)((1 << 23) - 0) / (float)(1 << 8);
	float HWMax = (float)((1 << 23) - 1) / (float)(1 << 8);
	float GuardBandMaxX = SCE_GNM_MIN(HWMax - fabsf(Scale[0]) - Bias[0] + HWOffsetX * 16, -fabsf(Scale[0]) + Bias[0] - HWOffsetX * 16 - HWMin);
	float GuardBandMaxY = SCE_GNM_MIN(HWMax - fabsf(Scale[1]) - Bias[1] + HWOffsetY * 16, -fabsf(Scale[1]) + Bias[1] - HWOffsetY * 16 - HWMin);
	float GuardBandHorizontalClipAdjust = GuardBandMaxX / fabsf(Scale[0]);
	float GuardBandVerticalClipAdjust = GuardBandMaxY / fabsf(Scale[1]);

	GnmContext->setGuardBands(GuardBandHorizontalClipAdjust, GuardBandVerticalClipAdjust, 1.0f, 1.0f);
}

FORCEINLINE void FGnmCommandListContext::SetViewportAndGuardBand(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ, float ZScale, float ZOffset)
{
	const FVector Scale((MaxX - MinX) * 0.5f, (MaxY - MinY) * -0.5f, ZScale);
	const FVector Bias(MinX + (MaxX - MinX) * 0.5f, MinY + (MaxY - MinY) * 0.5f, ZOffset);

	GnmContext->setViewport(0, MinZ, MaxZ, &Scale.X, &Bias.X);

	SetupGuardBands(Scale, Bias, GnmContext);
}

FORCEINLINE void FGnmCommandListContext::SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
{
	FVector4 NewViewportBounds(MinX, MinY, MaxX, MaxY);
	if (CachedViewportBounds != NewViewportBounds || MinZ != CachedViewportMinZ || MaxZ != CachedViewportMaxZ)
	{
		SetViewportAndGuardBand(MinX, MinY, MinZ, MaxX, MaxY, MaxZ, 1.0f, 0.0f);

		CurrentWidth = MaxX - MinX;
		CurrentHeight = MaxY - MinY;
		CachedViewportBounds = NewViewportBounds;
		CachedStereoViewportBounds.Set(0, 0, 0, 0);
		CachedViewportMinZ = MinZ;
		CachedViewportMaxZ = MaxZ;
	}

	SetViewportScissorRect(true, MinX, MinY, MaxX, MaxY);
}

FORCEINLINE void FGnmCommandListContext::SetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 LeftMinY, uint32 RightMinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 LeftMaxY, uint32 RightMaxY, float MaxZ)
{
	// Set guard band to encompassing bounds
	const float GuardMinY = LeftMinY < RightMinY ? LeftMinY : RightMinY;
	const float GuardMaxY = LeftMaxY > RightMaxY ? LeftMaxY : RightMaxY;

	const FVector4 NewViewportBounds(LeftMinX, GuardMinY, RightMaxX, GuardMaxY);
	if (CachedStereoViewportBounds == NewViewportBounds)
	{
		return;
	}

	CachedStereoViewportBounds = NewViewportBounds;
	CachedViewportBounds = NewViewportBounds;
	CachedViewportMinZ = MinZ;
	CachedViewportMaxZ = MaxZ;

	const float ZScale = 1.0f;
	const float ZOffset = 0.0f;

	const FVector LeftScale((LeftMaxX - LeftMinX) * 0.5f, (LeftMaxY - LeftMinY) * -0.5f, ZScale);
	const FVector LeftBias(LeftMinX + (LeftMaxX - LeftMinX) * 0.5f, LeftMinY + (LeftMaxY - LeftMinY) * 0.5f, ZOffset);

	const FVector RightScale((RightMaxX - RightMinX) * 0.5f, (RightMaxY - RightMinY) * -0.5f, ZScale);
	const FVector RightBias(RightMinX + (RightMaxX - RightMinX) * 0.5f, RightMinY + (RightMaxY - RightMinY) * 0.5f, ZOffset);

	const FVector GuardScale((RightMaxX - LeftMinX) * 0.5f, (GuardMaxY - GuardMinY) * -0.5f, ZScale);
	const FVector GuardBias(LeftMinX + (RightMaxX - LeftMinX) * 0.5f, GuardMinY + (GuardMaxY - GuardMinY) * 0.5f, ZOffset);

	GnmContext->setViewport(0, MinZ, MaxZ, &LeftScale.X, &LeftBias.X);
	GnmContext->setViewport(1, MinZ, MaxZ, &RightScale.X, &RightBias.X);

	SetupGuardBands(GuardScale, GuardBias, GnmContext);

	GnmContext->setViewportScissor(0, LeftMinX, LeftMinY, LeftMaxX, LeftMaxY, Gnm::kWindowOffsetDisable);
	GnmContext->setViewportScissor(1, RightMinX, RightMinY, RightMaxX, RightMaxY, Gnm::kWindowOffsetDisable);
}

/**
* Helper function to set a full screen viewport and bypass the caching system
*/
FORCEINLINE void FGnmCommandListContext::SetViewportUncached(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ, float ZScale, float ZOffset)
{
	SetViewportAndGuardBand(MinX, MinY, MinZ, MaxX, MaxY, MaxZ, ZScale, ZOffset);
	GnmContext->setViewportScissor(0, MinX, MinY, MaxX, MaxY, Gnm::kWindowOffsetDisable);
}

FORCEINLINE void FGnmCommandListContext::SetStencilRef(uint8 StencilRef)
{
	static_assert(sizeof(CachedStencilControl) == sizeof(uint32), "Type punning StencilControl will fail");
	Gnm::StencilControl NewStencilControl = CachedStencilControl;
	NewStencilControl.m_testVal = StencilRef; // This must be patched on the stack to prevent interactions between different threads.
	if (*(uint32*)&CachedStencilControl != *(uint32*)&NewStencilControl)
	{
		CachedStencilControl = NewStencilControl;
		GnmContext->setStencil(CachedStencilControl);
	}
}

FORCEINLINE void FGnmCommandListContext::SetDepthStencilState(FGnmDepthStencilState* NewState, uint8 StencilRef)
{
	check(!bCurrentDepthTargetReadOnly || !NewState->GetZWriteEnabled());

	// set current depth/stencil state
	if (CachedDepthStencilState.m_reg != NewState->DepthStencilState.m_reg)
	{
		// Also set the current depth bounds test state
		Gnm::DepthStencilControl DepthStencilState = NewState->DepthStencilState;
		DepthStencilState.setDepthBoundsEnable(CachedDepthBoundsTestEnabled);
		GnmContext->setDepthStencilControl(DepthStencilState);
		CachedDepthStencilState = NewState->DepthStencilState;
	}

	static_assert(sizeof(CachedStencilControl) == sizeof(uint32), "Type punning StencilControl will fail");
	Gnm::StencilControl NewStencilControl = NewState->StencilControl;
	NewStencilControl.m_testVal = StencilRef; // This must be patched on the stack to prevent interactions between different threads.
	if (*(uint32*)&CachedStencilControl != *(uint32*)&NewStencilControl)
	{
		CachedStencilControl = NewStencilControl;
		GnmContext->setStencil(CachedStencilControl);
	}

	if (CachedStencilOpControl.m_reg != NewState->StencilOps.m_reg)
	{
		CachedStencilOpControl = NewState->StencilOps;
		GnmContext->setStencilOpControl(CachedStencilOpControl);
	}
}

FORCEINLINE void FGnmCommandListContext::SetRenderTargetMask( uint32 RenderTargetMask )
{
	if( CachedRenderTargetMask != RenderTargetMask )
	{
		GnmContext->setRenderTargetMask(RenderTargetMask);
		CachedRenderTargetMask = RenderTargetMask;
	}
}

FORCEINLINE void FGnmCommandListContext::SetBlendState(FGnmBlendState* NewState)
{
	// set blend state for all RTs
	for (uint32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
	{
		if (CachedBlendControlRT[Index].m_reg != NewState->RenderTargetStates[Index].m_reg)
		{
			GnmContext->setBlendControl(Index, NewState->RenderTargetStates[Index]);
			CachedBlendControlRT[Index].m_reg = NewState->RenderTargetStates[Index].m_reg;
		}
	}

	// set color write masking for all RTs
	if (CachedRenderTargetColorWriteMask != NewState->MRTColorWriteMask)
	{
		CachedRenderTargetColorWriteMask = NewState->MRTColorWriteMask;
		SetRenderTargetMask( CachedRenderTargetColorWriteMask & CachedActiveRenderTargetMask );
	}
}

FORCEINLINE void FGnmCommandListContext::SetBlendColor(const FLinearColor& BlendColor)
{
	if (CachedBlendColor != BlendColor)
	{
		GnmContext->setBlendColor(BlendColor.R, BlendColor.G, BlendColor.B, BlendColor.A);
		CachedBlendColor = BlendColor;
	}
}

FORCEINLINE void FGnmCommandListContext::EnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
{
	Gnm::DepthStencilControl DepthStencilState = CachedDepthStencilState;
	DepthStencilState.setDepthBoundsEnable(bEnable);
	if (DepthStencilState.m_reg != CachedDepthStencilState.m_reg)
	{
		GnmContext->setDepthStencilControl(DepthStencilState);
		CachedDepthBoundsTestEnabled = bEnable;
	}

	if (CachedDepthBoundsMin != MinDepth || CachedDepthBoundsMax != MaxDepth)
	{
		GnmContext->setDepthBoundsRange(MinDepth, MaxDepth);
		CachedDepthBoundsMin = MinDepth;
		CachedDepthBoundsMax = MaxDepth;
	}
}

FORCEINLINE void FGnmCommandListContext::SetDepthClearValue(float Depth)
{
	if (Depth != CachedDepthClearValue)
	{
		CachedDepthClearValue = Depth;
		GnmContext->setDepthClearValue(Depth);
	}
}
