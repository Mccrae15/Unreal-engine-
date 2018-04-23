// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmRenderTarget.cpp: Gnm render target implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "ScreenRendering.h"
#include "UpdateTextureShaders.h"
#include "ShaderParameterUtils.h"

/**
* Copies the contents of the given surface to its resolve target texture.
* @param SourceSurface - surface with a resolve texture to copy to
* @param bKeepOriginalSurface - true if the original surface will still be used after this function so must remain valid
* @param ResolveParams - optional resolve params
*/
void FGnmCommandListContext::RHICopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, bool bKeepOriginalSurface, const FResolveParams& ResolveParams)
{
	if(!SourceTextureRHI || !DestTextureRHI)
	{
		// no need to do anything (silently ignored)
		return;
	}	

	RHITransitionResources(EResourceTransitionAccess::EReadable, &SourceTextureRHI, 1);


	FGnmSurface& SourceSurface = GetGnmSurfaceFromRHITexture(SourceTextureRHI);
	FGnmSurface& DestSurface = GetGnmSurfaceFromRHITexture(DestTextureRHI);

	void* SrcBaseAddress = SourceSurface.Texture->getBaseAddress();
	void* DestBaseAddress = DestSurface.Texture->getBaseAddress();

	bool bIsColorBuffer = SourceSurface.ColorBuffer != NULL;

	if (SrcBaseAddress != DestBaseAddress)
	{
		// If not resolving to the same address use a compute shader to
		// copy from the source texture to the destination texture

		// Format and sizes must match
		check(SourceSurface.Texture->getDataFormat().m_asInt == DestSurface.Texture->getDataFormat().m_asInt);
		check(SourceSurface.Texture->getWidth() == DestSurface.Texture->getWidth());
		check(SourceSurface.Texture->getHeight() == DestSurface.Texture->getHeight());

		// 3D textures not currently supported		
		check(SourceSurface.Texture->getTextureType() != Gnm::kTextureType3d);
		check(DestSurface.Texture->getTextureType() != Gnm::kTextureType3d);

		int32 DestBox[4];
		if (ResolveParams.Rect.IsValid())
		{
			DestBox[0] = ResolveParams.Rect.X1;
			DestBox[1] = ResolveParams.Rect.Y1;
			DestBox[2] = ResolveParams.Rect.X2;
			DestBox[3] = ResolveParams.Rect.Y2;
		}
		else
		{
			DestBox[0] = 0;
			DestBox[1] = 0;
			DestBox[2] = SourceSurface.Texture->getWidth();
			DestBox[3] = SourceSurface.Texture->getHeight();
		}

		DestSurface.BeginCopyToResolveTarget();

		Gnm::Texture SrcTextureCopy = *SourceSurface.Texture;
		Gnm::Texture DestTextureCopy = *DestSurface.Texture;
		uint32 SrcArrayIndex = ResolveParams.SourceArrayIndex;
		uint32 DestArrayIndex = ResolveParams.DestArrayIndex;

		if (SourceSurface.Texture->getTextureType() == Gnm::kTextureTypeCubemap)
		{
			// Treat cube textures as 2D texture arrays
			uint32 ArraySliceCount = SrcTextureCopy.getDepth() * CubeFace_MAX;
			ArraySliceCount = FMath::RoundUpToPowerOfTwo(ArraySliceCount);


			Gnm::TextureSpec TextureSpec;
			TextureSpec.init();
			TextureSpec.m_textureType = Gnm::kTextureType2dArray;
			TextureSpec.m_width = SrcTextureCopy.getWidth();
			TextureSpec.m_height = SrcTextureCopy.getHeight();
			TextureSpec.m_depth = 1;
			TextureSpec.m_pitch = 0;
			TextureSpec.m_numMipLevels = SrcTextureCopy.getLastMipLevel() + 1;
			TextureSpec.m_numSlices = ArraySliceCount;
			TextureSpec.m_format = SrcTextureCopy.getDataFormat();
			TextureSpec.m_tileModeHint = SrcTextureCopy.getTileMode();
			TextureSpec.m_minGpuMode = Gnm::getGpuMode();
			TextureSpec.m_numFragments = SrcTextureCopy.getNumFragments();

			SrcTextureCopy.init(&TextureSpec);

			SrcTextureCopy.setBaseAddress(SourceSurface.Texture->getBaseAddress());

			SrcArrayIndex *= CubeFace_MAX;
			SrcArrayIndex += GetGnmCubeFace(ResolveParams.CubeFace);
		}

		if (DestSurface.Texture->getTextureType() == Gnm::kTextureTypeCubemap)
		{
			// Treat cube textures as 2D texture arrays
			uint32 ArraySliceCount = DestTextureCopy.getDepth() * CubeFace_MAX;
			ArraySliceCount = FMath::RoundUpToPowerOfTwo(ArraySliceCount);

			Gnm::TextureSpec TextureSpec;
			TextureSpec.init();
			TextureSpec.m_textureType = Gnm::kTextureType2dArray;
			TextureSpec.m_width = DestTextureCopy.getWidth();
			TextureSpec.m_height = DestTextureCopy.getHeight();
			TextureSpec.m_depth = 1;
			TextureSpec.m_pitch = 0;
			TextureSpec.m_numMipLevels = DestTextureCopy.getLastMipLevel() + 1;
			TextureSpec.m_numSlices = ArraySliceCount;
			TextureSpec.m_format = DestTextureCopy.getDataFormat();
			TextureSpec.m_tileModeHint = DestTextureCopy.getTileMode();
			TextureSpec.m_minGpuMode = Gnm::getGpuMode();
			TextureSpec.m_numFragments = DestTextureCopy.getNumFragments();

			DestTextureCopy.init(&TextureSpec);

			DestTextureCopy.setBaseAddress(DestSurface.Texture->getBaseAddress());

			DestArrayIndex *= CubeFace_MAX;
			DestArrayIndex += GetGnmCubeFace(ResolveParams.CubeFace);
		}

		// Set mip level
		uint32 MipIndex = ResolveParams.MipIndex;
		SrcTextureCopy.setMipLevelRange(MipIndex, MipIndex);
		DestTextureCopy.setMipLevelRange(MipIndex, MipIndex);

		// Set cube face / array index
		SrcTextureCopy.setArrayView(SrcArrayIndex, SrcArrayIndex);
		DestTextureCopy.setArrayView(DestArrayIndex, DestArrayIndex);

		DestTextureCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.
		SrcTextureCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // The source texture is read-only, because we'll only ever read from it.

		TShaderMapRef<FCopyTexture2DCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();

		RHISetComputeShader(ShaderRHI);

		auto& Context = GetContext();

		Context.setTextures(Gnm::kShaderStageCs, ComputeShader->SrcTexture.GetBaseIndex(), 1, &SrcTextureCopy);
		Context.setRwTextures(Gnm::kShaderStageCs, ComputeShader->DestTexture.GetBaseIndex(), 1, &DestTextureCopy);
		SetShaderValueOnContext(*this, ShaderRHI, ComputeShader->DestPosSize, DestBox);

		uint32 Width = DestTextureCopy.getWidth();
		uint32 Height = DestTextureCopy.getHeight();
		Width = Width >> MipIndex;
		Height = Height >> MipIndex;

		PrepareForDispatch();

#if !NO_DRAW_CALLS
		PRE_DISPATCH;
		Context.dispatch((Width + 7) / 8, (Height + 7) / 8, 1);
		POST_DISPATCH;
#endif
		FlushAfterComputeShader();

		DestSurface.EndCopyToResolveTarget(*this);
	}
}

/** Helper for accessing R10G10B10A2 colors. */
struct FGnmR10G10B10A2
{
	uint32 R : 10;
	uint32 G : 10;
	uint32 B : 10;
	uint32 A : 2;
};

/** Helper for accessing R16G16 colors. */
struct FGnmRG16
{
	uint16 R;
	uint16 G;
};

/** Helper for accessing R16G16B16A16 colors. */
struct FGnmRGBA16
{
	uint16 R;
	uint16 G;
	uint16 B;
	uint16 A;
};

/**
* Helper for storing IEEE 32 bit float components
*/
struct FFloatIEEE
{
	union
	{
		struct
		{
			uint32	Mantissa : 23, Exponent : 8, Sign : 1;
		} Components;

		float	Float;
	};
};

/**
* Helper for storing 16 bit float components
*/
struct FGnmFloat16
{
	union
	{
		struct
		{
			uint16	Mantissa : 10, Exponent : 5, Sign : 1;
		} Components;

		uint16	Encoded;
	};

	/**
	* @return full 32 bit float from the 16 bit value
	*/
	operator float()
	{
		FFloatIEEE	Result;

		Result.Components.Sign = Components.Sign;
		Result.Components.Exponent = Components.Exponent - 15 + 127; // Stored exponents are biased by half their range.
		Result.Components.Mantissa = FMath::Min<uint32>(FMath::FloorToInt((float)Components.Mantissa / 1024.0f * 8388608.0f),(1 << 23) - 1);

		return Result.Float;
	}
};

/**
* Helper for storing kDataFormatR16G16B16A16Float components
*/
struct FGnmFloatR11G11B10
{
	// http://msdn.microsoft.com/En-US/library/bb173059(v=VS.85).aspx
	uint32 R_Mantissa : 6;
	uint32 R_Exponent : 5;
	uint32 G_Mantissa : 6;
	uint32 G_Exponent : 5;
	uint32 B_Mantissa : 5;
	uint32 B_Exponent : 5;

	/**
	* @return decompress into three 32 bit float
	*/
	operator FLinearColor()
	{
		FFloatIEEE	Result[3];

		Result[0].Components.Sign = 0;
		Result[0].Components.Exponent = R_Exponent - 15 + 127;
		Result[0].Components.Mantissa = FMath::Min<uint32>(FMath::FloorToInt((float)R_Mantissa / 32.0f * 8388608.0f),(1 << 23) - 1);
		Result[1].Components.Sign = 0;
		Result[1].Components.Exponent = G_Exponent - 15 + 127;
		Result[1].Components.Mantissa = FMath::Min<uint32>(FMath::FloorToInt((float)G_Mantissa / 64.0f * 8388608.0f),(1 << 23) - 1);
		Result[2].Components.Sign = 0;
		Result[2].Components.Exponent = B_Exponent - 15 + 127;
		Result[2].Components.Mantissa = FMath::Min<uint32>(FMath::FloorToInt((float)B_Mantissa / 64.0f * 8388608.0f),(1 << 23) - 1);

		return FLinearColor(Result[0].Float, Result[1].Float, Result[2].Float);
	}
};

//All of this stolen from the D3D11 files.  Probably needs to be made cross platform somehow.  Might be easier after EPixelFormat is changed to use real device formats.
static void ConvertRAWSurfaceDataToFColor(uint32 FormatAsInt, uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FColor* Out, FReadSurfaceDataFlags InFlags, bool bDepth)
{
	bool bLinearToGamma = InFlags.GetLinearToGamma();

	if (FormatAsInt == Gnm::kDataFormatR16Float.m_asInt)
	{

		// e.g. shadow maps
		for(uint32 Y = 0; Y < Height; Y++)
		{
			uint16* SrcPtr = (uint16*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;

			for(uint32 X = 0; X < Width; X++)
			{
				uint16 Value16 = *SrcPtr;
				float Value = Value16 / (float)(0xffff);

				*DestPtr = FLinearColor(Value, Value, Value).Quantize();
				++SrcPtr;
				++DestPtr;
			}
		}
	}	
	else if (FormatAsInt == Gnm::kDataFormatR8G8B8A8Sint.m_asInt)
	{
		// Read the data out of the buffer, converting it from ABGR to ARGB.
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;
			for(uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FColor(SrcPtr->B,SrcPtr->G,SrcPtr->R,SrcPtr->A);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if (FormatAsInt == Gnm::kDataFormatB8G8R8A8Unorm.m_asInt || FormatAsInt == Gnm::kDataFormatB8G8R8A8UnormSrgb.m_asInt)
	{
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;

			// Need to copy row wise since the Pitch might not match the Width.
			FMemory::Memcpy(DestPtr, SrcPtr, sizeof(FColor) * Width);
		}
	}
	else if (FormatAsInt == Gnm::kDataFormatR10G10B10A2Unorm.m_asInt)
	{
		// Read the data out of the buffer, converting it from R10G10B10A2 to FColor.
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FGnmR10G10B10A2* SrcPtr = (FGnmR10G10B10A2*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;
			for(uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FLinearColor(
					(float)SrcPtr->R / 1023.0f,
					(float)SrcPtr->G / 1023.0f,
					(float)SrcPtr->B / 1023.0f,
					(float)SrcPtr->A / 3.0f
					).Quantize();
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if (FormatAsInt == Gnm::kDataFormatR16G16B16A16Float.m_asInt)
	{
		FPlane	MinValue(0.0f,0.0f,0.0f,0.0f),
			MaxValue(1.0f,1.0f,1.0f,1.0f);

		check(sizeof(FGnmFloat16)==sizeof(uint16));

		for(uint32 Y = 0; Y < Height; Y++)
		{
			FGnmFloat16* SrcPtr = (FGnmFloat16*)(In + Y * SrcPitch);

			for(uint32 X = 0; X < Width; X++)
			{
				MinValue.X = FMath::Min<float>(SrcPtr[0],MinValue.X);
				MinValue.Y = FMath::Min<float>(SrcPtr[1],MinValue.Y);
				MinValue.Z = FMath::Min<float>(SrcPtr[2],MinValue.Z);
				MinValue.W = FMath::Min<float>(SrcPtr[3],MinValue.W);
				MaxValue.X = FMath::Max<float>(SrcPtr[0],MaxValue.X);
				MaxValue.Y = FMath::Max<float>(SrcPtr[1],MaxValue.Y);
				MaxValue.Z = FMath::Max<float>(SrcPtr[2],MaxValue.Z);
				MaxValue.W = FMath::Max<float>(SrcPtr[3],MaxValue.W);
				SrcPtr += 4;
			}
		}

		for(uint32 Y = 0; Y < Height; Y++)
		{
			FGnmFloat16* SrcPtr = (FGnmFloat16*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;

			for(uint32 X = 0; X < Width; X++)
			{
				FColor NormalizedColor =
					FLinearColor(
					(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
					(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
					(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
					(SrcPtr[3] - MinValue.W) / (MaxValue.W - MinValue.W)
					).ToFColor(bLinearToGamma);
				FMemory::Memcpy(DestPtr++,&NormalizedColor,sizeof(FColor));
				SrcPtr += 4;
			}
		}
	}	
	else if (FormatAsInt == Gnm::kDataFormatR11G11B10Float.m_asInt)
	{
		check(sizeof(FGnmFloatR11G11B10) == sizeof(uint32));

		for(uint32 Y = 0; Y < Height; Y++)
		{
			FGnmFloatR11G11B10* SrcPtr = (FGnmFloatR11G11B10*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;

			for(uint32 X = 0; X < Width; X++)
			{
				FLinearColor Value = *SrcPtr;

				FColor NormalizedColor = Value.ToFColor(bLinearToGamma);
				FMemory::Memcpy(DestPtr++, &NormalizedColor, sizeof(FColor));
				++SrcPtr;
			}
		}
	}	
	else if (FormatAsInt == Gnm::kDataFormatR32G32B32A32Float.m_asInt)
	{
		FPlane MinValue(0.0f,0.0f,0.0f,0.0f);
		FPlane MaxValue(1.0f,1.0f,1.0f,1.0f);

		for(uint32 Y = 0; Y < Height; Y++)
		{
			float* SrcPtr = (float*)(In + Y * SrcPitch);

			for(uint32 X = 0; X < Width; X++)
			{
				MinValue.X = FMath::Min<float>(SrcPtr[0],MinValue.X);
				MinValue.Y = FMath::Min<float>(SrcPtr[1],MinValue.Y);
				MinValue.Z = FMath::Min<float>(SrcPtr[2],MinValue.Z);
				MinValue.W = FMath::Min<float>(SrcPtr[3],MinValue.W);
				MaxValue.X = FMath::Max<float>(SrcPtr[0],MaxValue.X);
				MaxValue.Y = FMath::Max<float>(SrcPtr[1],MaxValue.Y);
				MaxValue.Z = FMath::Max<float>(SrcPtr[2],MaxValue.Z);
				MaxValue.W = FMath::Max<float>(SrcPtr[3],MaxValue.W);
				SrcPtr += 4;
			}
		}

		for(uint32 Y = 0; Y < Height; Y++)
		{
			float* SrcPtr = (float*)In;
			FColor* DestPtr = Out + Y * Width;

			for(uint32 X = 0; X < Width; X++)
			{
				FColor NormalizedColor =
					FLinearColor(
					(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
					(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
					(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
					(SrcPtr[3] - MinValue.W) / (MaxValue.W - MinValue.W)
					).ToFColor(bLinearToGamma);
				FMemory::Memcpy(DestPtr++,&NormalizedColor,sizeof(FColor));
				SrcPtr += 4;
			}
		}
	}	
	else if (FormatAsInt == Gnm::kDataFormatR32Float.m_asInt)
	{				
		for(uint32 Y = 0; Y < Height; Y++)
		{
			float* SrcPtr = (float *)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;

			for(uint32 X = 0; X < Width; X++)
			{
				float DeviceVal = (*SrcPtr);
				float LinearValue = DeviceVal;						
				if (bDepth)
				{
					LinearValue = FMath::Min(InFlags.ComputeNormalizedDepth(DeviceVal), 1.0f);
				}
				else
				{
					LinearValue = FMath::Clamp(DeviceVal * 255.0f, 0.0f, 255.0f);
				}
				FColor NormalizedColor = FLinearColor(LinearValue, LinearValue, LinearValue, 0).ToFColor(bLinearToGamma);
				FMemory::Memcpy(DestPtr++, &NormalizedColor, sizeof(FColor));
				SrcPtr+=1;
			}
		}
	}	
	else if (FormatAsInt == Gnm::kDataFormatR16G16B16A16Unorm.m_asInt)
	{
		// Read the data out of the buffer, converting it to FColor.
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FGnmRGBA16* SrcPtr = (FGnmRGBA16*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;
			for(uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FLinearColor(
					(float)SrcPtr->R / 65535.0f,
					(float)SrcPtr->G / 65535.0f,
					(float)SrcPtr->B / 65535.0f,
					(float)SrcPtr->A / 65535.0f
					).Quantize();
				++SrcPtr;
				++DestPtr;
			}
		}
	}	
	else if (FormatAsInt == Gnm::kDataFormatR16G16Unorm.m_asInt)
	{
		// Read the data out of the buffer, converting it to FColor.
		for(uint32 Y = 0; Y < Height; Y++)
		{
			FGnmRG16* SrcPtr = (FGnmRG16*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;
			for(uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FLinearColor(
					(float)SrcPtr->R / 65535.0f,
					(float)SrcPtr->G / 65535.0f,
					0).Quantize();
				++SrcPtr;
				++DestPtr;
			}
		}
	}	
	else if (FormatAsInt == Gnm::kDataFormatR8Unorm.m_asInt)
	{
		// Read the data out of the buffer, repeating the values as greyscale.
		for(uint32 Y = 0; Y < Height; Y++)
		{
			uint8* SrcPtr = (uint8*)(In + Y * SrcPitch);			
			FColor* DestPtr = Out + Y * Width;
			for(uint32 X = 0; X < Width; X++)
			{
				uint8 SrcVal = *SrcPtr;
				*DestPtr = FColor(SrcVal, SrcVal, SrcVal, SrcVal);
				++SrcPtr;
				++DestPtr;
			}
		}
	}	
	else
	{
		check(0);		
	}
}

void FGnmDynamicRHI::ReadSurfaceDataNoMSAARaw(FTextureRHIParamRef TextureRHI,FIntRect Rect,TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags, uint32* OutFormatAsInt, uint32* OutPitch)
{
	check(OutFormatAsInt != nullptr);
	check(OutPitch != nullptr);

	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(TextureRHI);

	// @todo ps4: Handle MSAA? (see D3D if so)
	const uint32 SizeX = Rect.Width();
	const uint32 SizeY = Rect.Height();

	GpuAddress::TilingParameters TilingParams;
	const uint8 MipLevelToRead = InFlags.GetMip();

	uint64_t OutSize = 0;
	Gnm::AlignmentType OutAlign = 0;
	void* BaseAddress = nullptr;

	uint32 InputFormatAsInt = Surface.Texture->getDataFormat().m_asInt;
	bool bIsDepthStencil = Surface.DepthBuffer != nullptr;

	// currently only support writing out stencil for depth surfaces.
	bool bIsSupportedDepthFormat = bIsDepthStencil && InFlags.GetOutputStencil() && Surface.DepthBuffer->getStencilFormat() == Gnm::kStencil8 && Surface.DepthBuffer->getStencilReadAddress() != nullptr;
	
	uint32 BytesPerTexel = 1;
	uint32 SrcPitch = 1;
	uint32 FormatAsInt = Gnm::kDataFormatInvalid.m_asInt;

	if (bIsDepthStencil)
	{	
		// de-tile the texture
		if (InFlags.GetOutputStencil())
		{
			//initing the TilingParams directly from the stencil target doesn't seem to work right.
			// @todo ps4: Handle cubemaps?
			bool bIsCubemap = false;
			Gnm::Texture HackTexture;
			HackTexture.initFromStencilTarget(Surface.DepthBuffer, Gnm::kTextureChannelTypeUInt, bIsCubemap);
			
			TilingParams.initFromTexture(&HackTexture, 0, 0);
			BaseAddress = Surface.DepthBuffer->getStencilReadAddress();		

			///we're only dealing with 8bit stencil surfaces at the moment.
			BytesPerTexel = 1;
			SrcPitch = SizeX * BytesPerTexel;
			FormatAsInt = Gnm::kDataFormatR8Unorm.m_asInt;
		}
		else
		{
			TilingParams.initFromDepthSurface(Surface.DepthBuffer, 0);
			BaseAddress = Surface.DepthBuffer->getZReadAddress();		

			switch(Surface.DepthBuffer->getZFormat())
			{
				case Gnm::kZFormat16:
					BytesPerTexel = 2;
					FormatAsInt = Gnm::kDataFormatR16Uint.m_asInt;
					break;
				case Gnm::kZFormat32Float:
					BytesPerTexel = 4;
					FormatAsInt = Gnm::kDataFormatR32Float.m_asInt;
					break;
				default:
					check(0);
					BytesPerTexel = 1;
					break;
			}
			SrcPitch = SizeX * BytesPerTexel;
		}		
	}
	else
	{	
		// de-tile the texture
		TilingParams.initFromTexture(Surface.Texture, MipLevelToRead, 0);		
		BaseAddress = Surface.Texture->getBaseAddress();

		//Gnm::Texture doesn't have 'init from other texture with another mip', so we have to go through Gnm::RenderTarget
		Gnm::RenderTarget HackRenderTarget;
		int32 InitStatus = HackRenderTarget.initFromTexture(Surface.Texture, MipLevelToRead);
		check(InitStatus == 0);

		Gnm::Texture HackTexture;
		HackTexture.initFromRenderTarget(&HackRenderTarget, false);
		
		BaseAddress = (uint8*)HackTexture.getBaseAddress();

		// get source info
		BytesPerTexel = HackTexture.getDataFormat().getBitsPerElement() / 8;
		SrcPitch = SizeX * BytesPerTexel;
		FormatAsInt	= HackTexture.getDataFormat().m_asInt;
	}

	// get the size for an untiled version of the mip		
	GpuAddress::computeUntiledSurfaceSize(&OutSize, &OutAlign, &TilingParams);

	// tile the mip right into gpu memory	
	OutData.Empty();
	OutData.AddUninitialized(OutSize);

	GpuAddress::detileSurface(OutData.GetData(), BaseAddress, &TilingParams);

	*OutPitch = SrcPitch;
	*OutFormatAsInt = FormatAsInt;
}

void FGnmDynamicRHI::RHIReadSurfaceData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	GGnmManager.WaitForGPUIdleNoReset();

	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(TextureRHI);

	// verify the input image format (but don't crash)
	uint32 InputFormatAsInt = Surface.Texture->getDataFormat().m_asInt;
	bool bIsDepthStencil = Surface.DepthBuffer != nullptr;

	// currently only support writing out stencil for depth surfaces.
	bool bIsSupportedDepthFormat = bIsDepthStencil && InFlags.GetOutputStencil() && Surface.DepthBuffer->getStencilFormat() == Gnm::kStencil8 && Surface.DepthBuffer->getStencilReadAddress() != nullptr;

	if (InputFormatAsInt != Gnm::kDataFormatB8G8R8A8Unorm.m_asInt && InputFormatAsInt != Gnm::kDataFormatR32Float.m_asInt && !bIsSupportedDepthFormat)
	{
		UE_LOG(LogPS4, Log, TEXT("Trying to read non-8888 surface, which is not supported yet. Add support in FGnmDynamicRHI::RHIReadSurfaceData. Will output all red image."));
	}

	TArray<uint8> OutDataRaw;
	uint32 FormatAsInt;
	uint32 SrcPitch;
	ReadSurfaceDataNoMSAARaw(TextureRHI, Rect, OutDataRaw, InFlags, &FormatAsInt, &SrcPitch);
	

	const uint32 SizeX = Rect.Width();
	const uint32 SizeY = Rect.Height();

	// allocate output space
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY);
	ConvertRAWSurfaceDataToFColor(FormatAsInt, SizeX, SizeY, OutDataRaw.GetData(), SrcPitch, OutData.GetData(), InFlags, bIsDepthStencil);		
}

void FGnmDynamicRHI::RHIMapStagingSurface(FTextureRHIParamRef TextureRHI,void*& OutData,int32& OutWidth,int32& OutHeight)
{
	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(TextureRHI);

	// textures with CPU readback will have a label that is set to 1 while a CopyToResolveTarget is inflight, so we check that now to make sure 
	// it's been resolved to
	Surface.BlockUntilCopyToResolveTargetComplete();

	OutWidth = Surface.Texture->getWidth();
	OutHeight = Surface.Texture->getHeight();
	OutData = Surface.Texture->getBaseAddress();
}

void FGnmDynamicRHI::RHIUnmapStagingSurface(FTextureRHIParamRef TextureRHI)
{
}

void FGnmDynamicRHI::RHIReadSurfaceFloatData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex)
{
	GGnmManager.WaitForGPUIdleNoReset();

	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(TextureRHI);

	// verify the input image format (but don't crash)
	uint32 InputFormatAsInt = Surface.Texture->getDataFormat().m_asInt;

	if (InputFormatAsInt != Gnm::kDataFormatR16G16B16A16Float.m_asInt)
	{
		UE_LOG(LogPS4, Log, TEXT("Trying to read non-FloatRGBA surface."));
	}

	void* SrcAddress = Surface.Texture->getBaseAddress();

	if( Surface.Texture->getLastMipLevel() > 0 || TextureRHI->GetTextureCube() || TextureRHI->GetTexture2DArray() )
	{
		if( TextureRHI->GetTextureCube() )
		{
			// adjust index to account for cubemaps as texture arrays
			ArrayIndex *= CubeFace_MAX;
			ArrayIndex += GetGnmCubeFace(CubeFace);
		}

		uint64_t SrcMipOffset;
		uint64_t SrcSize;
		GpuAddress::computeTextureSurfaceOffsetAndSize(&SrcMipOffset, &SrcSize, Surface.Texture, MipIndex, ArrayIndex);
		SrcAddress = (void*)((PTRINT)(SrcAddress) + SrcMipOffset);
	}


	// de-tile the texture
	GpuAddress::TilingParameters TilingParams;
	TilingParams.initFromTexture(Surface.Texture, MipIndex, ArrayIndex);		

	// get the size for an untiled version of the mip
	uint64_t OutSize = 0;
	Gnm::AlignmentType OutAlign = 0;
	GpuAddress::computeUntiledSurfaceSize(&OutSize, &OutAlign, &TilingParams);

	TArray<uint8> OutDataRaw;
	OutDataRaw.Empty();
	OutDataRaw.AddUninitialized(OutSize);

	GpuAddress::detileSurface(OutDataRaw.GetData(), SrcAddress, &TilingParams);


	// allocate output space
	const uint32 SizeX = Rect.Width();
	const uint32 SizeY = Rect.Height();
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY);

	uint32 BytesPerTexel =  Surface.Texture->getDataFormat().getBitsPerElement() / 8;
	uint32 SrcPitch = SizeX * BytesPerTexel;

	for(int32 Y = Rect.Min.Y; Y < Rect.Max.Y; Y++)
	{
		FFloat16Color* SrcPtr = (FFloat16Color*)((uint8*)OutDataRaw.GetData() + (Y - Rect.Min.Y) * SrcPitch);
		int32 Index = (Y - Rect.Min.Y) * SizeX;
		check(Index + ((int32)SizeX - 1) < OutData.Num());
		FFloat16Color* DestColor = &OutData[Index];
		FFloat16* DestPtr = (FFloat16*)(DestColor);
		FMemory::Memcpy(DestPtr,SrcPtr,SizeX * sizeof(FFloat16) * 4);
	}
}

void FGnmDynamicRHI::RHIRead3DSurfaceFloatData(FTextureRHIParamRef TextureRHI,FIntRect InRect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
{
	checkf(0, TEXT("FGnmDynamicRHI::RHIRead3DSurfaceFloatData: Implement me!"));
}

FTexture2DRHIRef FGnmDynamicRHI::RHIGetFMaskTexture(FTextureRHIParamRef SourceTextureRHI)
{
	FGnmSurface* Surface = &GetGnmSurfaceFromRHITexture(SourceTextureRHI);

	Gnm::RenderTarget* RenderTarget = Surface->ColorBuffer;

	Gnm::Texture FMaskTexture;

	FMaskTexture.initAsFmask(RenderTarget);
	FMaskTexture.setBaseAddress(RenderTarget->getFmaskAddress());
	FMaskTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

	return new FGnmTexture2D(FMaskTexture, EResourceTransitionAccess::EReadable, false);
}
