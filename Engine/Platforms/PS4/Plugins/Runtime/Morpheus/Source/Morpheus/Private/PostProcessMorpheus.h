// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenPass.h"

struct FHMDDistortionInputs;

FScreenPassTexture AddMorpheusDistortionPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FHMDDistortionInputs& Inputs);
