// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScenePrivate.h"
#include "RenderGraph.h"
#include "SceneTextureParameters.h"

void BuildHZB(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef SceneDepth,
	FRDGTextureRef VisBufferTexture,
	const FIntRect ViewRect,
	const TCHAR* ClosestHZBName,
	FRDGTextureRef* OutClosestHZBTexture,
	const TCHAR* FurthestHZBName,
	FRDGTextureRef* OutFurthestHZBTexture,
	EPixelFormat Format = PF_R16F);

// Build only the furthest HZB
void BuildHZBFurthest(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef SceneDepth,
	FRDGTextureRef VisBufferTexture,
	const FIntRect ViewRect,
	const TCHAR* FurthestHZBName,
	FRDGTextureRef* OutFurthestHZBTexture,
	EPixelFormat Format = PF_R16F);
