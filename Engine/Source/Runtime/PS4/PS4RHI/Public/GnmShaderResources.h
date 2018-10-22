// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmShaderResources.h: GNM resource RHI definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderCore.h"

// Key used for determining whether shader code is packed or not.
const int32 PackedShaderKey = 'PSHA';

struct FGnmShaderResourceTable : public FBaseShaderResourceTable
{
	/** Mapping of bound Textures to their location in resource tables. */
	TArray<uint32> TextureMap;

	friend bool operator==(const FGnmShaderResourceTable& A, const FGnmShaderResourceTable& B)
	{
		bool bEqual = true;
		bEqual &= (A.ResourceTableBits == B.ResourceTableBits);
		bEqual &= (A.ShaderResourceViewMap.Num() == B.ShaderResourceViewMap.Num());
		bEqual &= (A.SamplerMap.Num() == B.SamplerMap.Num());
		bEqual &= (A.UnorderedAccessViewMap.Num() == B.UnorderedAccessViewMap.Num());
		bEqual &= (A.ResourceTableLayoutHashes.Num() == B.ResourceTableLayoutHashes.Num());
		if (!bEqual)
		{
			return false;
		}
		bEqual &= (FMemory::Memcmp(A.ShaderResourceViewMap.GetData(), B.ShaderResourceViewMap.GetData(), A.ShaderResourceViewMap.GetTypeSize()*A.ShaderResourceViewMap.Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.SamplerMap.GetData(), B.SamplerMap.GetData(), A.SamplerMap.GetTypeSize()*A.SamplerMap.Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.UnorderedAccessViewMap.GetData(), B.UnorderedAccessViewMap.GetData(), A.UnorderedAccessViewMap.GetTypeSize()*A.UnorderedAccessViewMap.Num()) == 0);
		bEqual &= (FMemory::Memcmp(A.ResourceTableLayoutHashes.GetData(), B.ResourceTableLayoutHashes.GetData(), A.ResourceTableLayoutHashes.GetTypeSize()*A.ResourceTableLayoutHashes.Num()) == 0);

		const FBaseShaderResourceTable& BaseA = A;
		const FBaseShaderResourceTable& BaseB = B;
		return bEqual && (FMemory::Memcmp(A.TextureMap.GetData(), B.TextureMap.GetData(), A.TextureMap.GetTypeSize()*A.TextureMap.Num()) == 0);
	}
};

inline FArchive& operator<<(FArchive& Ar, FGnmShaderResourceTable& SRT)
{
	FBaseShaderResourceTable& BaseSRT = SRT;
	Ar << BaseSRT;
	Ar << SRT.TextureMap;
	return Ar;
}

struct FGnmShaderAttributeInfo
{
	//e.g. kTypeFloat4
	TEnumAsByte<sce::Shader::Binary::PsslType> DataType;

	//e.g. kSemanticPosition
	TEnumAsByte<sce::Shader::Binary::PsslSemantic> PSSLSemantic;

	// Actual HW resource index
	//uint8 ResourceIndex;

	FName AttrName;
	FName SemanticName;

	friend FArchive& operator<<(FArchive& Ar, FGnmShaderAttributeInfo& Info)
	{
		// Changes to this serialization must change shader format version!
		Ar << Info.DataType;
		Ar << Info.PSSLSemantic;
		//Ar << Info.ResourceIndex;
		if (Ar.IsLoading())
		{
			FString Attr;
			FString Semantic;

			Ar << Attr;
			Ar << Semantic;

			Info.AttrName = Attr.Len() == 0 ? NAME_None : FName(*Attr);
			Info.SemanticName = Semantic.Len() == 0 ? NAME_None : FName(*Semantic);
		}
		else
		{
			FString Attr = Info.AttrName == NAME_None ? TEXT("") : Info.AttrName.ToString();
			Ar << Attr;

			FString Semantic = Info.SemanticName == NAME_None ? TEXT("") : Info.SemanticName.ToString();
			Ar << Semantic;
		}
		return Ar;
	}
};
