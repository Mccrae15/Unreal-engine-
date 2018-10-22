// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmViewport.h: Gnm viewport RHI definitions.
=============================================================================*/

#pragma once

class FGnmViewport : public FRHIViewport
{
public:

	FGnmViewport(void* WindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen);
	~FGnmViewport();

	virtual void SetCustomPresent(FRHICustomPresent* InCustomPresent) override;
	void Resize(uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen);

	// Accessors.
	uint32 GetSizeX() const { return SizeX; }
	uint32 GetSizeY() const { return SizeY; }

private:
	uint32 SizeX;
	uint32 SizeY;
	bool bIsFullscreen;
	bool bIsValid;
};

template<>
struct TGnmResourceTraits<FRHIViewport>
{
	typedef FGnmViewport TConcreteType;
};

