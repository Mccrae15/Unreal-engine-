/*
Copyright (c) Meta Platforms, Inc. and affiliates.
All rights reserved.
This source code is licensed under the license found in the
LICENSE file in the root directory of this source tree.
*/

#pragma once
#include "IOculusXRPassthroughAssetsModule.h"

#define LOCTEXT_NAMESPACE "OculusXRPassthroughAssets"

//-------------------------------------------------------------------------------------------------
// FOculusXRPassthroughAssetsModule
//-------------------------------------------------------------------------------------------------

class FOculusXRPassthroughAssetsModule : public IOculusXRPassthroughAssetsModule
{
public:
	FOculusXRPassthroughAssetsModule();

	static inline FOculusXRPassthroughAssetsModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FOculusXRPassthroughAssetsModule>("OculusXRPassthroughAssets");
	}

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

#undef LOCTEXT_NAMESPACE
