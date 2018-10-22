// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmVertexDeclaration.cpp: Gnm vertex declaration RHI implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"

FGnmVertexDeclaration::FGnmVertexDeclaration(const FVertexDeclarationElementList& InElements)
{
	Elements = InElements;

	// Build list of stream element strides
	uint16 UsedStreamsMask = 0;
	FMemory::Memzero(StreamStrides);

	for (int32 ElementIndex = 0; ElementIndex < InElements.Num(); ElementIndex++)
	{
		const FVertexElement& Element = InElements[ElementIndex];

		if ((UsedStreamsMask & 1 << Element.StreamIndex) != 0)
		{
			ensure(StreamStrides[Element.StreamIndex] == Element.Stride);
		}
		else
		{
			UsedStreamsMask = UsedStreamsMask | (1 << Element.StreamIndex);
			StreamStrides[Element.StreamIndex] = Element.Stride;
		}
	}
}

FVertexDeclarationRHIRef FGnmDynamicRHI::RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	return new FGnmVertexDeclaration(Elements);
}
