// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FStaticShaderPlatform
{
	inline FStaticShaderPlatform(const EShaderPlatform InPlatform)
	{
		checkSlow(SP_PS4 == InPlatform);
	}

	inline operator EShaderPlatform() const
	{
		return SP_PS4;
	}

	inline bool operator == (const EShaderPlatform Other) const
	{
		return Other == SP_PS4;
	}
	inline bool operator != (const EShaderPlatform Other) const
	{
		return Other != SP_PS4;
	}
};