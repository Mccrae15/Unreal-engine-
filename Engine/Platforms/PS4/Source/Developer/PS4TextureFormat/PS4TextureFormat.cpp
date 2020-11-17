// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "PixelFormat.h"
#include "Interfaces/ITextureFormat.h"
#include "Interfaces/ITextureFormatModule.h"
#include "HAL/IConsoleManager.h"
#include "TextureCompressorModule.h"
#include "ImageCore.h"

#ifdef __clang__
#define __int128_t __sce_int128_t
#endif

#pragma pack(push,8)
#include <gpu_address.h>
#include <gnm/texture.h>
#pragma pack(pop)

#ifdef __clang__
#undef __int128_t
#endif

using namespace sce;

DEFINE_LOG_CATEGORY_STATIC(LogPS4TextureFormat, Log, All);

/**
 * DXT texture format handler, that also tiles for PS4
 */
class FPS4TextureFormat : public ITextureFormat
{
public:
	FPS4TextureFormat()
		: DXTFormat(*FModuleManager::LoadModuleChecked<ITextureFormatModule>(TEXT("TextureFormatDXT")).GetTextureFormat())
		, UncompressedFormat(*FModuleManager::LoadModuleChecked<ITextureFormatModule>(TEXT("TextureFormatUncompressed")).GetTextureFormat())
		, IntelISPCTexCompFormat(*FModuleManager::LoadModuleChecked<ITextureFormatModule>(TEXT("TextureFormatIntelISPCTexComp")).GetTextureFormat())
	{
		// cache the normal names from the bases
		BaseUncompressedFormats.Empty();
		UncompressedFormat.GetSupportedFormats(BaseUncompressedFormats);

		BaseCompressedFormats.Empty();
		DXTFormat.GetSupportedFormats(BaseCompressedFormats);

		BaseISPCFormats.Empty();
		IntelISPCTexCompFormat.GetSupportedFormats( BaseISPCFormats );
	}

	virtual FTextureFormatCompressorCaps GetFormatCapabilities() const override
	{
		return FTextureFormatCompressorCaps(); // Default capabilities.
	}

private:
	virtual uint16 GetVersion(FName Format, const struct FTextureBuildSettings* BuildSettings) const override
	{
		// include the other formats version numbers in mine so that if they change, my data will become invalid, since this class depends on that data
		return DXTFormat.GetVersion(Format, BuildSettings) + UncompressedFormat.GetVersion(Format, BuildSettings) + IntelISPCTexCompFormat.GetVersion(Format, BuildSettings) + 7;
	}

	virtual bool AllowParallelBuild() const override
	{
		return true;
	}

	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override
	{
		// prepend PS4_ (see PS4TargetPlatform.cpp, GetTextureFormats()) to the normal formats
		for (int32 Index = 0; Index < BaseUncompressedFormats.Num(); Index++)
		{
			FName PS4Format(*(FString(TEXT("PS4_")) + BaseUncompressedFormats[Index].ToString()));
			OutFormats.Add(PS4Format);
		}

		for (int32 Index = 0; Index < BaseCompressedFormats.Num(); Index++)
		{
			FName PS4Format(*(FString(TEXT("PS4_")) + BaseCompressedFormats[Index].ToString()));
			OutFormats.Add(PS4Format);
		}

		for (int32 Index = 0; Index < BaseISPCFormats.Num(); Index++)
		{
			FName PS4Format( *(FString( TEXT( "PS4_" ) ) + BaseISPCFormats[Index].ToString()) );
			OutFormats.Add( PS4Format );
		}
	}

	/**
	 * @return the Gnm format that matches the RawImageFormat
	 */
	Gnm::DataFormat TranslateDataFormat(FName Format, bool bImageHasAlphaChannel) const
	{
		if (Format == TEXT("DXT1") || (Format == TEXT("AutoDXT") && !bImageHasAlphaChannel))
		{
			return Gnm::kDataFormatBc1Unorm;
		}
		else if (Format == TEXT("DXT1"))
		{
			return Gnm::kDataFormatBc2Unorm;
		}
		else if (Format == TEXT("DXT5") || Format == TEXT("DXT5n") || (Format == TEXT("AutoDXT") && bImageHasAlphaChannel))
		{
			return Gnm::kDataFormatBc3Unorm;
		}
		else if (Format == TEXT("BC5"))
		{
			return Gnm::kDataFormatBc5Unorm;
		}
		else if (Format == TEXT("RGBA8"))
		{
			return Gnm::kDataFormatR8G8B8A8Unorm;
		}
		else if (Format == TEXT("BGRA8") || Format == TEXT("XGXR8"))
		{
			return Gnm::kDataFormatB8G8R8A8Unorm;
		}
		else if (Format == TEXT("G8"))
		{
			return Gnm::kDataFormatR8Unorm;
		}
		else if (Format == TEXT("G16"))
		{
			return Gnm::kDataFormatR16Unorm;
		}
		else if (Format == TEXT("VU8"))
		{
			return Gnm::kDataFormatR8G8Unorm;
		}
		else if (Format == TEXT("RGBA16F"))
		{
			return Gnm::kDataFormatR16G16B16A16Float;
		}
		else if( Format == TEXT( "BC4" ) )
		{
			return Gnm::kDataFormatBc4Unorm;
		}
		else if( Format == TEXT( "BC6H" ) )
		{
			return Gnm::kDataFormatBc6Unorm;
		}
		else if( Format == TEXT( "BC7" ) )
		{
			return Gnm::kDataFormatBc7Unorm;
		}
		else if (Format == TEXT("R8_UINT"))
		{
			return Gnm::kDataFormatR8Uint;
		}

		// make sure we aren't using a new format
		UE_LOG(LogPS4TextureFormat, Fatal, TEXT("An invalid format (%s) was passed in to TranslateDataFormat. Was a format added without fixing this function?"), *Format.ToString());
		return Gnm::kDataFormatInvalid;
	}

	virtual bool CompressImage(
		const FImage& InImage,
		const struct FTextureBuildSettings& BuildSettings,
		bool bImageHasAlphaChannel,
		FCompressedImage2D& OutCompressedImage
		) const override
	{
		// store the image data before any tiling is done (OutCompressedImage will contain the tiled data)
		FCompressedImage2D PreTiledImage;

		// get the base format name WITHOUT the PS4_, to figure out which base to compress it with
		FName BaseFormatName(*BuildSettings.TextureFormatName.ToString().Replace(TEXT("PS4_"), TEXT("")));

		// copy the build settings, but replace the format name with the non-PS4 format
		FTextureBuildSettings BaseSettings = BuildSettings;
		BaseSettings.TextureFormatName = BaseFormatName;

		bool bCompressionSucceeded = false;
		if (BaseUncompressedFormats.Contains(BaseFormatName))
		{
			// use the base uncompressed class to convert the texture if it's an uncompressed format
			bCompressionSucceeded = UncompressedFormat.CompressImage(InImage, BaseSettings, bImageHasAlphaChannel, PreTiledImage);
		}
		else if (BaseCompressedFormats.Contains(BaseFormatName))
		{
			// use the base DXT class to convert the texture if it's a compressed format
			bCompressionSucceeded = DXTFormat.CompressImage(InImage, BaseSettings, bImageHasAlphaChannel, PreTiledImage);
		}
		else if( BaseISPCFormats.Contains( BaseFormatName ) )
		{
			// use the base DXT class to convert the texture if it's a compressed format
			bCompressionSucceeded = IntelISPCTexCompFormat.CompressImage( InImage, BaseSettings, bImageHasAlphaChannel, PreTiledImage );
		}
		else
		{
			UE_LOG(LogPS4TextureFormat, Fatal, TEXT("Unhandled texture format '%s' passed to FPS4TextureFormat::CompressImage"), *BuildSettings.TextureFormatName.ToString());
		}

		if (bCompressionSucceeded)
		{
			// tile the resulting image (this is the entire reason for this whole class)
			
			// first, we translate unreal format into platform format
			Gnm::DataFormat PlatformFormat = TranslateDataFormat(BaseFormatName, bImageHasAlphaChannel);

			uint32 Mip0Width = BuildSettings.TopMipSize.X;
			uint32 Mip0Height = BuildSettings.TopMipSize.Y;
			uint32 Mip0Depth = BuildSettings.VolumeSizeZ;
			uint32 Mip0ArraySlices = BuildSettings.ArraySlices;

			// 3D textures are treated as a blob, not by number of slices (this is for texture arrays and cube maps)
			int NumSlices = 1;

			if (BuildSettings.bTextureArray)
			{
				NumSlices = Mip0ArraySlices;
			}
			else if (BuildSettings.bCubemap) 
			{
				NumSlices = 6;
			}
			
			// then, we make a Texture object
			Gnm::Texture Texture;

			Gnm::TextureSpec TextureSpec;
			TextureSpec.init();
			TextureSpec.m_width = InImage.SizeX;
			TextureSpec.m_height = InImage.SizeY;
			TextureSpec.m_depth = 1;
			TextureSpec.m_pitch = 0;
			TextureSpec.m_numMipLevels = 1;
			TextureSpec.m_numSlices = NumSlices;
			TextureSpec.m_format = PlatformFormat;
			TextureSpec.m_minGpuMode = Gnm::kGpuModeBase;
			TextureSpec.m_numFragments = Gnm::kNumFragments1;

			if (BuildSettings.bCubemap)
			{
				// make a cube map texture 
				GpuAddress::computeSurfaceTileMode( Gnm::kGpuModeBase, &TextureSpec.m_tileModeHint, GpuAddress::kSurfaceTypeTextureCubemap, PlatformFormat, 1 );
				TextureSpec.m_textureType = Gnm::kTextureTypeCubemap;
			}
			else if (BuildSettings.bTextureArray)
			{
				// Make a texture array.
				GpuAddress::computeSurfaceTileMode(Gnm::kGpuModeBase, &TextureSpec.m_tileModeHint, GpuAddress::kSurfaceTypeTextureFlat, PlatformFormat, 1);
				TextureSpec.m_textureType = Gnm::kTextureType2dArray;
			}
			else if (InImage.NumSlices == 1)
			{
				// make a 2D texture
				GpuAddress::computeSurfaceTileMode( Gnm::kGpuModeBase, &TextureSpec.m_tileModeHint, GpuAddress::kSurfaceTypeTextureFlat, PlatformFormat, 1 );
				TextureSpec.m_textureType = Gnm::kTextureType2d;
			}
			else
			{
				// make a 3D texture
				GpuAddress::computeSurfaceTileMode( Gnm::kGpuModeBase, &TextureSpec.m_tileModeHint, GpuAddress::kSurfaceTypeTextureVolume, PlatformFormat, 1 );
				TextureSpec.m_textureType = Gnm::kTextureType3d;
				TextureSpec.m_depth = InImage.NumSlices;
			}

			bool bStatus = Texture.init(&TextureSpec) == SCE_GNM_OK;
			checkf(bStatus, TEXT("Could not init gmn compressed texture."));

			// ask addrlib for tiled mip size
			uint64 MipOffset;
			uint64 MipSize;
			GpuAddress::computeTextureSurfaceOffsetAndSize(&MipOffset, &MipSize, &Texture, 0, 0);

			// allocate space for tiled texture
			OutCompressedImage.RawData.AddZeroed(MipSize * NumSlices);

			// UE_LOG(LogPS4TextureFormat, Display, TEXT("Texture, format %s, went from %d to %d bytes. %x x %x x %x"), *BaseSettings.TextureFormatName.ToString(), PreTiledImage.RawData.Num(), OutCompressedImage.RawData.Num(), InImage.SizeX, InImage.SizeY, InImage.NumSlices);

			uint64 SourceMipSize = PreTiledImage.RawData.Num() / NumSlices;

			for (int32 SliceIndex = 0; SliceIndex < NumSlices; SliceIndex++)
			{
				// get offset to face
				GpuAddress::computeTextureSurfaceOffsetAndSize(&MipOffset, &MipSize, &Texture, 0, SliceIndex);

				// set up tiling struct
				GpuAddress::TilingParameters TilingParams;
				TilingParams.initFromTexture(&Texture, 0, SliceIndex);

				// tile the memory
				GpuAddress::tileSurface(OutCompressedImage.RawData.GetData() + MipOffset,
					PreTiledImage.RawData.GetData() + (SliceIndex * SourceMipSize),
					&TilingParams);
			}

			// copy the basic params
			OutCompressedImage.PixelFormat = PreTiledImage.PixelFormat;
			OutCompressedImage.SizeX = PreTiledImage.SizeX;
			OutCompressedImage.SizeY = PreTiledImage.SizeY;
			OutCompressedImage.SizeZ = 1;
			if (BuildSettings.bVolume || BuildSettings.bTextureArray) { OutCompressedImage.SizeZ = PreTiledImage.SizeZ; }
		}

		return bCompressionSucceeded;
	}

private:
	/** Cache the base format modules */
	const ITextureFormat& DXTFormat;
	const ITextureFormat& UncompressedFormat;
	const ITextureFormat& IntelISPCTexCompFormat;

	/** Cache the format names for the non-PS4 versions (static so that const*/
	TArray<FName> BaseUncompressedFormats;
	TArray<FName> BaseCompressedFormats;
	TArray<FName> BaseISPCFormats;
};

/**
 * Module for PS4 texture tiling.
 */
static ITextureFormat* Singleton = NULL;

class FPS4TextureFormatModule : public ITextureFormatModule
{
public:
	virtual ~FPS4TextureFormatModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual ITextureFormat* GetTextureFormat()
	{
		if (!Singleton)
		{
			// load the delay-loaded DLLs
			FString SDKDir = FPlatformMisc::GetEnvironmentVariable(TEXT("SCE_ORBIS_SDK_DIR"));

			// the Sce DLLs have interdependencies, so they assume they will find each other in the path,
			// so normal delay loading isn't good enough - we need to set a directory to look in
			FString BinariesDir = FString::Printf(TEXT("%s\\host_tools\\bin"), *SDKDir);
			FPlatformProcess::PushDllDirectory(*BinariesDir);

			void* Handle = FPlatformProcess::GetDllHandle(*FString::Printf(TEXT("%s\\host_tools\\bin\\libSceGnm.dll"), *SDKDir));
			if (Handle)
			{
				Handle = FPlatformProcess::GetDllHandle(*FString::Printf(TEXT("%s\\host_tools\\bin\\libSceGpuAddress.dll"), *SDKDir));
			}

			// and go back to the previous
			FPlatformProcess::PopDllDirectory(*BinariesDir);

			// return NULL if the DLLs failed to load
			if (Handle != NULL)
			{
				Singleton = new FPS4TextureFormat();
			}
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE(FPS4TextureFormatModule, PS4TextureFormat);
