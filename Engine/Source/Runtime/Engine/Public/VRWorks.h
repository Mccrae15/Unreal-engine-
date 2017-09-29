// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


struct FVRWorks
{
	static ENGINE_API bool IsFastGSNeeded();

	static ENGINE_API bool IsMultiResSupportEnabled();
	static ENGINE_API int GetMultiResRenderingLevel();

	static ENGINE_API bool IsSinglePassStereoSupportEnabled();
	static ENGINE_API bool IsSinglePassStereoRenderingEnabled();
	static ENGINE_API void DisableSinglePassStereoRendering();

	static ENGINE_API bool IsLensMatchedShadingSupportEnabled();
	static ENGINE_API int GetLensMatchedShadingLevel();
	static ENGINE_API float GetLensMatchedShadingUnwarpScale();
	static ENGINE_API bool IsOctagonOptimizationEnabled();

	static ENGINE_API bool IsVRSLIEnabled();
};