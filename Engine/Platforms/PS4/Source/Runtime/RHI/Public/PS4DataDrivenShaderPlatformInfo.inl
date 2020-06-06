// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


class FDataDrivenShaderPlatformInfo final : public FGenericDataDrivenShaderPlatformInfo
{
public:
	IMPLEMENT_DDPSPI_SETTING(GetIsLanguageSony,				true);
	IMPLEMENT_DDPSPI_SETTING(GetIsConsole,					true);
	IMPLEMENT_DDPSPI_SETTING(GetSupportsDrawIndirect,		true);

	IMPLEMENT_DDPSPI_SETTING(GetIsLanguageD3D,				false);
	IMPLEMENT_DDPSPI_SETTING(GetIsLanguageMetal,			false);
	IMPLEMENT_DDPSPI_SETTING(GetIsLanguageOpenGL,			false);
	IMPLEMENT_DDPSPI_SETTING(GetIsLanguageVulkan,			false);
	IMPLEMENT_DDPSPI_SETTING(GetIsLanguageNintendo,			false);
	IMPLEMENT_DDPSPI_SETTING(GetIsMobile,					false);
	IMPLEMENT_DDPSPI_SETTING(GetIsMetalMRT,					false);
	IMPLEMENT_DDPSPI_SETTING(GetIsPC,						false);
	IMPLEMENT_DDPSPI_SETTING(GetIsAndroidOpenGLES,			false);
	IMPLEMENT_DDPSPI_SETTING(GetSupportsMobileMultiView,	false);
	IMPLEMENT_DDPSPI_SETTING(GetTargetsTiledGPU,			false);
	IMPLEMENT_DDPSPI_SETTING(GetSupportsGPUScene,			true);

	IMPLEMENT_DDPSPI_SETTING_WITH_RETURN_TYPE(ERHIFeatureLevel::Type, GetMaxFeatureLevel, FStaticFeatureLevel(ERHIFeatureLevel::SM5));
};
