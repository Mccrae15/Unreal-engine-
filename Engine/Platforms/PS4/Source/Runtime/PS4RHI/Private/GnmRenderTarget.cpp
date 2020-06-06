// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmRenderTarget.cpp: Gnm render target implementation.
=============================================================================*/

#include "GnmRenderTarget.h"
#include "GnmRHIPrivate.h"
#include "ScreenRendering.h"
#include "UpdateTextureShaders.h"
#include "ShaderParameterUtils.h"
#include "Math/PackedVector.h"


namespace CopyTextureImpl
{
	/** Structure that describes how we alias textures for copying. */
	struct FTextureAliasInfo
	{
		sce::Gnm::DataFormat DataFormat = sce::Gnm::kDataFormatInvalid;
		uint32 NumComponents = 1;
		uint32 SizeDivisor = 1;
	};

	/** Get the FTextureAliasInfo for a given texture. */
	bool GetTextureAliasInfo(FGnmSurface const& InSurface, FTextureAliasInfo& OutInfo)
	{
		sce::Gnm::SurfaceFormat SurfaceFormat = InSurface.Texture->getDataFormat().getSurfaceFormat();

		switch (SurfaceFormat)
		{
		case sce::Gnm::kSurfaceFormat8:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR8Uint, 1, 1 }; break;
		case sce::Gnm::kSurfaceFormat16:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR16Uint, 1, 1 }; break;
		case sce::Gnm::kSurfaceFormat32:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR32Uint, 1, 1 }; break;
		case sce::Gnm::kSurfaceFormatBc1:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR32G32Uint, 2, 4 }; break;
		case sce::Gnm::kSurfaceFormatBc3:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR32G32B32A32Uint, 4, 4 }; break;
		case sce::Gnm::kSurfaceFormatBc4:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR32G32Uint, 2, 4 }; break;
		case sce::Gnm::kSurfaceFormatBc5:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR32G32B32A32Uint, 4, 4 }; break;
		case sce::Gnm::kSurfaceFormat8_8:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR8G8Uint, 2, 1 }; break;
		case sce::Gnm::kSurfaceFormat8_8_8_8:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR8G8B8A8Uint, 4, 1 }; break;
		case sce::Gnm::kSurfaceFormat2_10_10_10:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR10G10B10A2Uint, 4, 1 }; break;
		case sce::Gnm::kSurfaceFormat10_11_11:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR11G11B10Float, 3, 1 }; break;
		case sce::Gnm::kSurfaceFormat16_16_16_16:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR16G16B16A16Uint, 4, 1 }; break;
		case sce::Gnm::kSurfaceFormat32_32:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR32G32Uint, 2, 1 }; break;
		case sce::Gnm::kSurfaceFormat32_32_32_32:  OutInfo = FTextureAliasInfo{ sce::Gnm::kDataFormatR32G32B32A32Uint, 4, 1 }; break;
		default:
			// If we don't find a match we need to add a new surface format in the list above
			checkf(false, TEXT("No alias format yet defined for %d. Add if required."), (int32)SurfaceFormat);
			return false;
		}

		return true;
	}

	//** If InOutTexture is a cube map then convert to a texture array. Also convert InOutArrayIndex to map into the new texture array. */
	void AliasCubemapAsTextureArray(Gnm::Texture& InOutTexture, int32& InOutArrayIndex, ECubeFace InCubeFace)
	{
		if (InOutTexture.getTextureType() != Gnm::kTextureTypeCubemap)
		{
			return;
		}

		void* BaseAddress = InOutTexture.getBaseAddress();
			
		uint32 ArraySliceCount = InOutTexture.getDepth() * CubeFace_MAX;
		ArraySliceCount = FMath::RoundUpToPowerOfTwo(ArraySliceCount);

		Gnm::TextureSpec TextureSpec;
		TextureSpec.init();
		TextureSpec.m_textureType = Gnm::kTextureType2dArray;
		TextureSpec.m_width = InOutTexture.getWidth();
		TextureSpec.m_height = InOutTexture.getHeight();
		TextureSpec.m_depth = 1;
		TextureSpec.m_pitch = 0;
		TextureSpec.m_numMipLevels = InOutTexture.getLastMipLevel() + 1;
		TextureSpec.m_numSlices = ArraySliceCount;
		TextureSpec.m_format = InOutTexture.getDataFormat();
		TextureSpec.m_tileModeHint = InOutTexture.getTileMode();
		TextureSpec.m_minGpuMode = Gnm::getGpuMode();
		TextureSpec.m_numFragments = InOutTexture.getNumFragments();
			
		InOutTexture.init(&TextureSpec);
		InOutTexture.setBaseAddress(BaseAddress);

		InOutArrayIndex *= CubeFace_MAX;
		InOutArrayIndex += GetGnmCubeFace(InCubeFace);
	}

	/** Shader binding for the TCopyTexture2DCS shader used by CopyToResolveTarget. */
	template<uint32 NumComponents, typename ComponentType>
	void BindCopy2DShader(FGnmCommandListContext& CmdContext, const Gnm::Texture& SrcTexture, const Gnm::Texture& DstTexture, FIntPoint SrcPos, FIntVector4 DestPosSize)
	{
		TShaderMapRef< TCopyTexture2DCS<NumComponents, ComponentType> > ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();

		CmdContext.RHISetComputeShader(ShaderRHI);

		SetShaderValueOnContext(CmdContext, ShaderRHI, ComputeShader->DestPosSizeParameter, DestPosSize);
		SetShaderValueOnContext(CmdContext, ShaderRHI, ComputeShader->SrcPosParameter, SrcPos);

		GnmContextType& GnmContext = CmdContext.GetContext();
		GnmContext.setTextures(Gnm::kShaderStageCs, ComputeShader->SrcTexture.GetBaseIndex(), 1, &SrcTexture);
		GnmContext.setRwTextures(Gnm::kShaderStageCs, ComputeShader->DestTexture.GetBaseIndex(), 1, &DstTexture);
	}

	/** Helper function for copy between 2D texture sub-resources. */
	void CopyTexture2D(
		FGnmCommandListContext& CmdContext, 
		FGnmSurface const& SourceSurface,
		int32 SrcArrayIndex, ECubeFace SrcCubeIndex, int32 SrcMip,
		FIntPoint SrcPos,
		FGnmSurface const& DestSurface,
		int32 DestArrayIndex, ECubeFace DestCubeIndex, int32 DestMip,
		FIntPoint DestPos,
		FIntPoint Size)
	{
		if (SourceSurface.Texture->getTextureType() == Gnm::kTextureType3d || DestSurface.Texture->getTextureType() == Gnm::kTextureType3d)
		{
			checkf(false, TEXT("3D textures not supported"));
			return;
		}

		// We always try to alias as UINTx for the copy
		// For copying between some formats (such as BC compressed) we need to support aliasing with a different texture size
		FTextureAliasInfo SrcAliasInfo, DestAliasInfo;
		GetTextureAliasInfo(SourceSurface, SrcAliasInfo);
		GetTextureAliasInfo(DestSurface, DestAliasInfo);
		check(SrcAliasInfo.NumComponents == DestAliasInfo.NumComponents);
		check(SrcAliasInfo.DataFormat.getBufferChannelType() == DestAliasInfo.DataFormat.getBufferChannelType());

		// Adjust copy regions and sizes for the aliased textures
		check(SrcPos[0] % SrcAliasInfo.SizeDivisor == 0 && SrcPos[1] % SrcAliasInfo.SizeDivisor == 0);
		SrcPos[0] /= SrcAliasInfo.SizeDivisor; SrcPos[1] /= SrcAliasInfo.SizeDivisor;
		check(DestPos[0] % DestAliasInfo.SizeDivisor == 0 && DestPos[1] % DestAliasInfo.SizeDivisor == 0);
		DestPos[0] /= DestAliasInfo.SizeDivisor; DestPos[1] /= DestAliasInfo.SizeDivisor;
		check(Size[0] % SrcAliasInfo.SizeDivisor == 0 && Size[1] % SrcAliasInfo.SizeDivisor == 0);
		Size[0] /= SrcAliasInfo.SizeDivisor; Size[1] /= SrcAliasInfo.SizeDivisor;

		// Create aliased texture objects
		Gnm::Texture SrcTextureCopy = *SourceSurface.Texture;
		Gnm::Texture DestTextureCopy = *DestSurface.Texture;

		// Adjust format and sizes in the alias textures
		SrcTextureCopy.setDataFormat(SrcAliasInfo.DataFormat);
		SrcTextureCopy.setWidthMinus1(((SrcTextureCopy.getWidth() + SrcAliasInfo.SizeDivisor - 1) / SrcAliasInfo.SizeDivisor) - 1);
		SrcTextureCopy.setHeightMinus1(((SrcTextureCopy.getHeight() + SrcAliasInfo.SizeDivisor - 1) / SrcAliasInfo.SizeDivisor) - 1);
		SrcTextureCopy.setPitchMinus1(((SrcTextureCopy.getPitch() + SrcAliasInfo.SizeDivisor - 1) / SrcAliasInfo.SizeDivisor) - 1);
		DestTextureCopy.setDataFormat(DestAliasInfo.DataFormat);
		DestTextureCopy.setWidthMinus1(((DestTextureCopy.getWidth() + DestAliasInfo.SizeDivisor - 1) / DestAliasInfo.SizeDivisor) - 1);
		DestTextureCopy.setHeightMinus1(((DestTextureCopy.getHeight() + DestAliasInfo.SizeDivisor - 1) / DestAliasInfo.SizeDivisor) - 1);
		DestTextureCopy.setPitchMinus1(((DestTextureCopy.getPitch() + DestAliasInfo.SizeDivisor - 1) / DestAliasInfo.SizeDivisor) - 1);

		// Alias cube maps to texture arrays
		AliasCubemapAsTextureArray(SrcTextureCopy, SrcArrayIndex, SrcCubeIndex);
		SrcTextureCopy.setArrayView(SrcArrayIndex, SrcArrayIndex);
		SrcTextureCopy.setMipLevelRange(SrcMip, SrcMip);
		AliasCubemapAsTextureArray(DestTextureCopy, DestArrayIndex, DestCubeIndex);
		DestTextureCopy.setArrayView(DestArrayIndex, DestArrayIndex);
		DestTextureCopy.setMipLevelRange(DestMip, DestMip);

		// The destination texture is GPU-coherent, because we will write to it
		DestTextureCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); 
		// The source texture is read-only, because we'll only ever read from it
		SrcTextureCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

		// Set the compute shader
		switch (SrcAliasInfo.DataFormat.getBufferChannelType())
		{
		case Gnm::kBufferChannelTypeUInt:
			switch (SrcAliasInfo.NumComponents)
			{
			case 1u: BindCopy2DShader<1u, uint32>(CmdContext, SrcTextureCopy, DestTextureCopy, SrcPos, FIntVector4(DestPos.X, DestPos.Y, Size.X, Size.Y)); break;
			case 2u: BindCopy2DShader<2u, uint32>(CmdContext, SrcTextureCopy, DestTextureCopy, SrcPos, FIntVector4(DestPos.X, DestPos.Y, Size.X, Size.Y)); break;
			case 4u: BindCopy2DShader<4u, uint32>(CmdContext, SrcTextureCopy, DestTextureCopy, SrcPos, FIntVector4(DestPos.X, DestPos.Y, Size.X, Size.Y)); break;
			default: check(false);
			}
			break;
		case Gnm::kBufferChannelTypeFloat:
			switch (SrcAliasInfo.NumComponents)
			{
			case 3u: BindCopy2DShader<3u, float>(CmdContext, SrcTextureCopy, DestTextureCopy, SrcPos, FIntVector4(DestPos.X, DestPos.Y, Size.X, Size.Y)); break;
			default: check(false);
			}
			break;
		default: check(false);
		}

		// Dispatch. Notice that we leave transitions and flushes to the calling functions.
		CmdContext.PrepareForDispatch();

#if !NO_DRAW_CALLS
		PRE_DISPATCH;
		const uint32 Width = Size[0];
		const uint32 Height = Size[1];
		CmdContext.GetContext().dispatch((Width + 7) / 8, (Height + 7) / 8, 1);
		POST_DISPATCH;
#endif
	}
}

void FGnmCommandListContext::RHICopyToResolveTarget(FRHITexture* SourceTextureRHI, FRHITexture* DestTextureRHI, const FResolveParams& ResolveParams)
{
	if(SourceTextureRHI == nullptr || DestTextureRHI == nullptr)
	{
		return;
	}	
	
	RHITransitionResources(EResourceTransitionAccess::EReadable, &SourceTextureRHI, 1);

	FGnmSurface& SourceSurface = GetGnmSurfaceFromRHITexture(SourceTextureRHI);
	FGnmSurface& DestSurface = GetGnmSurfaceFromRHITexture(DestTextureRHI);

	if (SourceSurface.Texture->getBaseAddress() == DestSurface.Texture->getBaseAddress())
	{
		return;
	}

	const FIntPoint SrcPos = ResolveParams.Rect.IsValid() ? FIntPoint(ResolveParams.Rect.X1, ResolveParams.Rect.Y1) : FIntPoint::ZeroValue;
	const FIntPoint DestPos = ResolveParams.DestRect.IsValid() ? FIntPoint(ResolveParams.DestRect.X1, ResolveParams.DestRect.Y1) : FIntPoint::ZeroValue;
	const FIntVector SrcSize = SourceTextureRHI->GetSizeXYZ();
	const FIntPoint Size = ResolveParams.Rect.IsValid() ? FIntPoint(ResolveParams.Rect.X2 - ResolveParams.Rect.X1, ResolveParams.Rect.Y2 - ResolveParams.Rect.Y1) : FIntPoint(SrcSize.X, SrcSize.Y);

	DestSurface.BeginCopyToResolveTarget();
	{
		CopyTextureImpl::CopyTexture2D(
			*this,
			SourceSurface, ResolveParams.SourceArrayIndex, ResolveParams.CubeFace, ResolveParams.MipIndex, SrcPos,
			DestSurface, ResolveParams.DestArrayIndex, ResolveParams.CubeFace, ResolveParams.MipIndex, DestPos,
			Size);
	}
	FlushAfterComputeShader();
	DestSurface.EndCopyToResolveTarget(*this);
}

void FGnmCommandListContext::RHICopyTexture(FRHITexture* SourceTextureRHI, FRHITexture* DestTextureRHI, const FRHICopyTextureInfo& CopyInfo)
{
	if (SourceTextureRHI == nullptr || DestTextureRHI == nullptr)
	{
		return;
	}

	RHITransitionResources(EResourceTransitionAccess::EReadable, &SourceTextureRHI, 1);

	FGnmSurface& SourceSurface = GetGnmSurfaceFromRHITexture(SourceTextureRHI);
	FGnmSurface& DestSurface = GetGnmSurfaceFromRHITexture(DestTextureRHI);

	const FIntPoint SrcPos = FIntPoint(CopyInfo.SourcePosition.X, CopyInfo.SourcePosition.Y);
	const FIntPoint DestPos = FIntPoint(CopyInfo.DestPosition.X, CopyInfo.DestPosition.Y);
	const FIntVector SrcSize = SourceTextureRHI->GetSizeXYZ();
	const FIntPoint Size = CopyInfo.Size.X <= 0 || CopyInfo.Size.Y <=0 ? FIntPoint(SrcSize.X, SrcSize.Y) : FIntPoint(CopyInfo.Size.X, CopyInfo.Size.Y);
	const bool bIsCube = SourceTextureRHI->GetTextureCube() != nullptr;
	const bool bAllCubeFaces = bIsCube && (CopyInfo.NumSlices % 6) == 0;
	const int32 NumArraySlices = bAllCubeFaces ? CopyInfo.NumSlices / 6 : CopyInfo.NumSlices;
	const int32 NumFaces = bAllCubeFaces ? 6 : 1;

	// Copy sub-resources in individual dispatches
	//todo: Copy multiple array slices and mips in a single dispatch to improve performance
	//todo: The generic TCopyTexture2DCS shader could be tuned for GCN (lane swizzling for tile coherency etc.)
	DestSurface.BeginCopyToResolveTarget();
	{
		for (int32 ArrayIndex = 0; ArrayIndex < NumArraySlices; ++ArrayIndex)
		{
			const int32 SourceArrayIndex = CopyInfo.SourceSliceIndex + ArrayIndex;
			const int32 DestArrayIndex = CopyInfo.DestSliceIndex + ArrayIndex;
			for (int32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
			{
				CopyTextureImpl::CopyTexture2D(
					*this,
					SourceSurface, SourceArrayIndex, (ECubeFace)FaceIndex, CopyInfo.SourceMipIndex, SrcPos,
					DestSurface, DestArrayIndex, (ECubeFace)FaceIndex, CopyInfo.DestMipIndex, DestPos,
					Size);
			}
		}
	}
	FlushAfterComputeShader();
	DestSurface.EndCopyToResolveTarget(*this);
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

		check(sizeof(FFloat16)==sizeof(uint16));

		for(uint32 Y = 0; Y < Height; Y++)
		{
			FFloat16* SrcPtr = (FFloat16*)(In + Y * SrcPitch);

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
			FFloat16* SrcPtr = (FFloat16*)(In + Y * SrcPitch);
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
		check(sizeof(FFloat3Packed) == sizeof(uint32));

		for(uint32 Y = 0; Y < Height; Y++)
		{
			FFloat3Packed* SrcPtr = (FFloat3Packed*)(In + Y * SrcPitch);
			FColor* DestPtr = Out + Y * Width;

			for(uint32 X = 0; X < Width; X++)
			{
				FLinearColor Value = (*SrcPtr).ToLinearColor();

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

void FGnmDynamicRHI::ReadSurfaceDataNoMSAARaw(FRHITexture* TextureRHI,FIntRect Rect,TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags, uint32* OutFormatAsInt, uint32* OutPitch)
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

void FGnmDynamicRHI::RHIReadSurfaceData(FRHITexture* TextureRHI, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
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
		UE_LOG(LogSony, Log, TEXT("Trying to read non-8888 surface, which is not supported yet. Add support in FGnmDynamicRHI::RHIReadSurfaceData. Will output all red image."));
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

void FGnmDynamicRHI::RHIMapStagingSurface(FRHITexture* TextureRHI, FRHIGPUFence* FenceRHI, void*& OutData, int32& OutWidth, int32& OutHeight, uint32 GPUIndex)
{
	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(TextureRHI);

	// textures with CPU readback will have a label that is set to 1 while a CopyToResolveTarget is inflight, so we check that now to make sure 
	// it's been resolved to
	Surface.BlockUntilCopyToResolveTargetComplete();

	OutWidth = Surface.Texture->getPitch();
	OutHeight = Surface.Texture->getHeight();
	OutData = Surface.Texture->getBaseAddress();
}

void FGnmDynamicRHI::RHIUnmapStagingSurface(FRHITexture* TextureRHI, uint32 GPUIndex)
{
}

void FGnmDynamicRHI::RHIReadSurfaceFloatData(FRHITexture* TextureRHI, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex)
{
	GGnmManager.WaitForGPUIdleNoReset();

	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(TextureRHI);

	// verify the input image format (but don't crash)
	uint32 InputFormatAsInt = Surface.Texture->getDataFormat().m_asInt;

	if (InputFormatAsInt != Gnm::kDataFormatR16G16B16A16Float.m_asInt)
	{
		UE_LOG(LogSony, Log, TEXT("Trying to read non-FloatRGBA surface."));
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

void FGnmDynamicRHI::RHIRead3DSurfaceFloatData(FRHITexture* TextureRHI,FIntRect InRect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
{
	checkf(0, TEXT("FGnmDynamicRHI::RHIRead3DSurfaceFloatData: Implement me!"));
}


FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceViewHTile(FRHITexture2D* RenderTarget)
{
	FGnmTexture2D* Texture = ResourceCast(RenderTarget);
	const FGnmSurface& Surface = Texture->Surface; 

	Gnm::DepthRenderTarget* DepthTarget = Surface.DepthBuffer;
	check(DepthTarget);

	void* HTileAddress = DepthTarget->getHtileAddress();
	check(DepthTarget);

	const uint32 NumElements = DepthTarget->getHtileSliceSizeInBytes() >> 2;

	// Build an R32 read-only buffer around the HTILE data.
	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;
	SRV->Buffer.initAsRegularBuffer(HTileAddress, sizeof(uint32), NumElements);
	SRV->Buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
	check(SRV->Buffer.isBuffer());

	return SRV;
}

FUnorderedAccessViewRHIRef FGnmDynamicRHI::RHICreateUnorderedAccessViewStencil(FRHITexture2D* DepthTarget, int32 MipLevel)
{
	FGnmSurface* Surface = &GetGnmSurfaceFromRHITexture(DepthTarget);
	check(Surface->DepthBuffer);

	// create the UAV buffer to point to the stencil memory
	FGnmUnorderedAccessView* UAV = new FGnmUnorderedAccessView;
	UAV->Texture.initFromStencilTarget(Surface->DepthBuffer, Gnm::kTextureChannelTypeUInt, false);
	UAV->Texture.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
	UAV->SourceTexture = DepthTarget;
	check(UAV->Texture.isTexture());

	return UAV;
}

FUnorderedAccessViewRHIRef FGnmDynamicRHI::RHICreateUnorderedAccessViewHTile(FRHITexture2D* RenderTarget)
{
	FGnmTexture2D* Texture = ResourceCast(RenderTarget);
	const FGnmSurface& Surface = Texture->Surface;

	Gnm::DepthRenderTarget* DepthTarget = Surface.DepthBuffer;
	check(DepthTarget);

	void* HTileAddress = DepthTarget->getHtileAddress();
	check(DepthTarget);

	const uint32 NumElements = DepthTarget->getHtileSliceSizeInBytes() >> 2;

	// Build an R32 read-write buffer around the HTILE data.
	FGnmUnorderedAccessView* UAV = new FGnmUnorderedAccessView;
	UAV->Buffer.initAsRegularBuffer(HTileAddress, sizeof(uint32), NumElements);
	UAV->Buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
	check(UAV->Buffer.isBuffer());

	return UAV;
}
