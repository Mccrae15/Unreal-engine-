// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FStaticFeatureLevel
{
	FStaticFeatureLevel(const ERHIFeatureLevel::Type InFeatureLevel)
	{
		checkSlow(InFeatureLevel == ERHIFeatureLevel::SM5);
	}

	FStaticFeatureLevel(const TEnumAsByte<ERHIFeatureLevel::Type> InFeatureLevel) 
	{
		checkSlow(InFeatureLevel.GetValue() == ERHIFeatureLevel::SM5);
	}

	inline operator ERHIFeatureLevel::Type() const
	{
		return ERHIFeatureLevel::SM5;
	}

	inline bool operator == (const ERHIFeatureLevel::Type Other) const
	{
		return Other == ERHIFeatureLevel::SM5;
	}

	inline bool operator != (const ERHIFeatureLevel::Type Other) const
	{
		return Other != ERHIFeatureLevel::SM5;
	}

	inline bool operator <= (const ERHIFeatureLevel::Type Other) const
	{
		return ERHIFeatureLevel::SM5 <= Other;
	}

	inline bool operator < (const ERHIFeatureLevel::Type Other) const
	{
		return ERHIFeatureLevel::SM5 < Other;
	}

	inline bool operator >= (const ERHIFeatureLevel::Type Other) const
	{
		return ERHIFeatureLevel::SM5 >= Other;
	}

	inline bool operator > (const ERHIFeatureLevel::Type Other) const
	{
		return ERHIFeatureLevel::SM5 > Other;
	}
};