/*
Copyright (c) Meta Platforms, Inc. and affiliates.
All rights reserved.
This source code is licensed under the license found in the
LICENSE file in the root directory of this source tree.
*/

#include "OculusXRPassthroughAssetsModule.h"

#include "AssetToolsModule.h"
#include "OculusXRPassthroughColorLutAsset.h"

#define LOCTEXT_NAMESPACE "OculusXRPassthroughAssets"

//-------------------------------------------------------------------------------------------------
// FOculusXRPassthroughAssetsModule
//-------------------------------------------------------------------------------------------------

FOculusXRPassthroughAssetsModule::FOculusXRPassthroughAssetsModule()
{
}

void FOculusXRPassthroughAssetsModule::StartupModule()
{ 
	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_OculusXRPassthroughColorLut));
}

void FOculusXRPassthroughAssetsModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FOculusXRPassthroughAssetsModule, OculusXRPassthroughAssets)

#undef LOCTEXT_NAMESPACE
