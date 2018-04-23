// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmVertexBuffer.cpp: Gnm texture RHI implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "UpdateTextureShaders.h"
#include "ShaderParameterUtils.h"
#include <gnm/measuredrawcommandbuffer.h>
#include "GPUDefragAllocator.h"
#include "RenderUtils.h"

TAutoConsoleVariable<int32> CVarPS4UseLegacyTexturePool(
	TEXT("r.PS4UseLegacyTexturePool"),
	1,
	TEXT("Switches between the old and new behaviour of the texture pool on PS4.\n")
	TEXT(" - 0: Texture pool size is fixed (new).\n")
	TEXT(" - 1: Texture pool size is dynamic, driven by the GPU defrag allocator (legacy, default).\n")
	TEXT("The new behaviour also changes which resources are counted towards the texture streaming budget.\n")
	TEXT("When enabling the new behaviour, r.Streaming.PoolSize will need to be re-adjusted.\n")
	TEXT("See PS4 release notes for UE 4.18."),
	ECVF_ReadOnly
);

/** Texture reference class. */
class FGnmTextureReference : public FRHITextureReference
{
public:
	explicit FGnmTextureReference(FLastRenderTimeContainer* InLastRenderTime)
		: FRHITextureReference(InLastRenderTime)
	{}

	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}

	void SetReferencedTexture(FRHITexture* InTexture)
	{
		FRHITextureReference::SetReferencedTexture(InTexture);
	}

	virtual void* GetNativeResource() const override final
	{
		FRHITexture* RefTexture = static_cast<const FGnmTextureReference*>(this)->GetReferencedTexture();
		if (RefTexture)
		{
			return static_cast<const FGnmTextureReference*>(this)->GetReferencedTexture()->GetNativeResource();
		}
		else
		{
			return (void *)GWhiteTexture->TextureRHI->GetNativeResource();
		}
	}

};

static bool ShouldCountAsTextureMemory(const FGnmSurface& Surface)
{
	return (Surface.ColorBuffer == nullptr && Surface.DepthBuffer == nullptr);	
}

static TStatId GetTextureMemoryStatEnum(const FGnmSurface& Surface, bool bIsCube, bool b3D)
{
#if STATS
	if(ShouldCountAsTextureMemory(Surface))
	{		
		// normal texture
		if(bIsCube)
		{
			return GET_STATID(STAT_TextureMemoryCube);
		}
		else if(b3D)
		{
			return GET_STATID(STAT_TextureMemory3D);
		}
		else
		{
			return GET_STATID(STAT_TextureMemory2D);
		}
	}
	else
	{
		// render target
		if(bIsCube)
		{
			return GET_STATID(STAT_RenderTargetMemoryCube);
		}
		else if(b3D)
		{
			return GET_STATID(STAT_RenderTargetMemory3D);
		}
		else
		{
			return GET_STATID(STAT_RenderTargetMemory2D);
		}
	}
#endif
	return TStatId();
}

// Note: This function can be called from many different threads
// @param TextureSize >0 to allocate, <0 to deallocate
// @param b3D true:3D, false:2D or cube map
void UpdateGnmTextureStats(const FGnmSurface& Surface, int64 TextureSize, bool bIsCube, bool b3D, bool bStreamable)
{
	if(TextureSize == 0)
	{
		return;

	}

	bool bUseLegacyTexturePool = CVarPS4UseLegacyTexturePool.GetValueOnAnyThread() != 0;

	int64 AlignedSize = (TextureSize > 0) ? Align(TextureSize, 1024) / 1024 : -(Align(-TextureSize, 1024) / 1024);
	if (bUseLegacyTexturePool ? ShouldCountAsTextureMemory(Surface) : bStreamable)
	{
		FPlatformAtomics::InterlockedAdd(&GCurrentTextureMemorySize, AlignedSize);
	}
	else
	{
		FPlatformAtomics::InterlockedAdd(&GCurrentRendertargetMemorySize, AlignedSize);
	}

	INC_MEMORY_STAT_BY_FName(GetTextureMemoryStatEnum(Surface, bIsCube, b3D).GetName(), TextureSize);

	if(TextureSize > 0)
	{
		INC_DWORD_STAT(STAT_GnmTexturesAllocated);
	}
	else
	{
		INC_DWORD_STAT(STAT_GnmTexturesReleased);
	}
}

/**
 * Calculate the tiling mode based on creation flags and resource type (2D/3D/etc)
 */
static Gnm::TileMode DetermineTileMode(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 Flags, ERHIResourceType ResourceType, Gnm::DataFormat DataFormat, EPixelFormat UnrealFormat, uint32 NumSamples = 1)
{
	const bool bWritable = (Flags & TexCreate_RenderTargetable) || (Flags & TexCreate_UAV);

	Gnm::TileMode TileMode;
	int32 Status = sce::GpuAddress::kStatusSuccess;
	if (Flags & TexCreate_Presentable)
	{
		if (DataFormat.m_asInt == Gnm::kDataFormatL8Unorm.m_asInt)
		{
			// aux output uses this format, and should be linear aligned.
			TileMode = Gnm::kTileModeDisplay_LinearAligned;
			Status = sce::GpuAddress::kStatusSuccess;
		}
		else
		{
		// presentable must always use the displayable surface type
		Status = GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &TileMode, GpuAddress::kSurfaceTypeColorTargetDisplayable, DataFormat, 1);
		}

		// back buffer MUST be this format
		check(DataFormat.m_asInt == Gnm::kDataFormatB8G8R8A8Unorm.m_asInt ||					// normal output
			DataFormat.m_asInt == GPixelFormats[GRHIHDRDisplayOutputFormat].PlatformFormat ||	// hdr output
			DataFormat.m_asInt == Gnm::kDataFormatL8Unorm.m_asInt);								// aux output
		checkf(ResourceType == RRT_Texture2D, TEXT("Only plain 2D textures may be TexCreate_Presentable"));
	}
	else if (Flags & TexCreate_NoTiling)
	{
		// force no tiling if desired
		TileMode = Gnm::kTileModeDisplay_LinearAligned;
		Status = sce::GpuAddress::kStatusSuccess;
	}
	else if (Flags & (TexCreate_DepthStencilTargetable | TexCreate_DepthStencilResolveTarget))
	{
		// depth render target (stencil is a separate buffer, so we just tile the depth target)
		Status = GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &TileMode, GpuAddress::kSurfaceTypeDepthOnlyTarget, DataFormat, NumSamples);

		if( TileMode == Gnm::kTileModeDepth_2dThin_256 && Gnm::getGpuMode() == Gnm::kGpuModeNeo )
		{
			// Temp hack for broken stencil textures on Neo
	 		TileMode = Gnm::kTileModeDepth_2dThin_64;
	 	}
	}
	else if (ResourceType == RRT_TextureCube)
	{
		const GpuAddress::SurfaceType SurfaceType = bWritable ? GpuAddress::kSurfaceTypeRwTextureCubemap : GpuAddress::kSurfaceTypeTextureCubemap;
		// handle special tiling for cubemaps
		Status = GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &TileMode, SurfaceType, DataFormat, 1);
	}
	else if (ResourceType == RRT_Texture3D)
	{
		const GpuAddress::SurfaceType SurfaceType = bWritable ? GpuAddress::kSurfaceTypeRwTextureVolume : GpuAddress::kSurfaceTypeTextureVolume;
		// handle special tiling for volume textures
		Status = GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &TileMode, SurfaceType, DataFormat, 1);

		if (bWritable && TileMode == Gnm::kTileModeThick_1dThick && (Flags & TexCreate_ReduceMemoryWithTilingMode))
		{
			// Workaround for volume texture tiling modes (kTileModeThick_1dThick and kTileModeThick_2dThick) bloating 128^3 textures by 4x
			// This decreases texture cache performance
			TileMode = Gnm::kTileModeThin_1dThin;
		}
	}
	else if (Flags & (TexCreate_RenderTargetable | TexCreate_ResolveTargetable))
	{
		// normal render targets use the ColorTarget surface type
		Status = GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &TileMode, GpuAddress::kSurfaceTypeColorTarget, DataFormat, NumSamples);
	}
	else
	{
		// anything else is a plain texture (or array of plain textures)
		Status = GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &TileMode, GpuAddress::kSurfaceTypeTextureFlat, DataFormat, 1);
	}

	check(Status == sce::GpuAddress::kStatusSuccess);
	return TileMode;
}


FGnmTexture2D::FGnmTexture2D(const Gnm::Texture& GnmTexture, EResourceTransitionAccess InGPUAccess, bool bCreateRenderTarget)
: FRHITexture2D(GnmTexture.getWidth(), GnmTexture.getHeight(), GnmTexture.getLastMipLevel() + 1, 1, PF_Unknown, TexCreate_ShaderResource, FClearValueBinding::None)
, Surface(RRT_Texture2D, GnmTexture, InGPUAccess, bCreateRenderTarget)
{
	//bool SRGB = GnmTexture.getDataFormat()..m_bits.m_channelType == Gnm::kTextureChannelTypeSrgb;
	
}

//allocate rendertargets in the defrag allocator. without PS4_DEFRAG_RENDERTARGETS they will be 'locked' and not move. things will move around them.
#define PS4_RENDERTARGETS_IN_DEFRAGHEAP (1 && USE_DEFRAG_ALLOCATOR)

//if enabled, actually try to move the rendertargets around.
#define PS4_DEFRAG_RENDERTARGETS (0 && USE_DEFRAG_ALLOCATOR)

FGnmSurface::FGnmSurface(ERHIResourceType ResourceType, const Gnm::Texture& GnmTexture, EResourceTransitionAccess InGPUAccess, bool bCreateRenderTarget)
	: Texture(nullptr)
	, ColorBuffer(nullptr)
	, DepthBuffer(nullptr)
	, RHITexture(nullptr)
	, NumSlices(0)
	, bNeedsFastClearResolve(false)
	, bNeedsToTileOnUnlock(false)
	, bSkipBlockOnUnlock(false)
	, bStreamable(false)
	, CacheToFlushOnResolve(0)
	, UntiledLockedMemory(nullptr)
	, LastUsedRenderTargetAddr(nullptr)
	, LastFrameFastCleared(-1)
	, CopyToResolveTargetLabel(nullptr)
	, FrameSubmitted(-1)
	, bDefraggable(false)
#if VALIDATE_MEMORY_PROTECTION
	, bNeedsGPUWrite(InGPUAccess == EResourceTransitionAccess::EWritable)
#endif
{
	check(ResourceType == RRT_Texture2D);
	Texture = new Gnm::Texture;
	*Texture = GnmTexture;

	NumSamples = Gnm::kNumSamples1;

	if (bCreateRenderTarget)
	{
		ColorBuffer = new Gnm::RenderTarget();
		ColorBuffer->initFromTexture(Texture, 0);
	}
}

FGnmSurface::FGnmSurface(ERHIResourceType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 InNumSamples, uint32 Flags, const FClearValueBinding& ClearValue, FResourceBulkDataInterface* BulkData, FRHITexture* InRHITexture)
	: Texture(nullptr)
	, ColorBuffer(nullptr)
	, DepthBuffer(nullptr)
	, RHITexture(nullptr)
	, NumSlices(SizeZ * ArraySize)
	, bNeedsFastClearResolve(false)
	, bSkipBlockOnUnlock(false)
	, bStreamable(!!(Flags & TexCreate_Streamable))
	, CacheToFlushOnResolve(0)
	, UntiledLockedMemory(NULL)
	, LastUsedRenderTargetAddr(nullptr)
	, LastFrameFastCleared(-1)
	, CopyToResolveTargetLabel(nullptr)
	, FrameSubmitted(-1)
	, bDefraggable(false)
	, bSkipEliminate(false)
#if VALIDATE_MEMORY_PROTECTION
	, bNeedsGPUWrite(false)
#endif
{
	check(bArray == true || ArraySize == 1);
	RHITexture = InRHITexture;

	bSkipEliminate = (Flags & TexCreate_NoFastClearFinalize) != 0;

	// no need to tile textures that were processed offline (via GnmTextureFormat.cpp)
	bNeedsToTileOnUnlock = (Flags & TexCreate_OfflineProcessed) == 0;

	// texture native format
	Gnm::DataFormat PlatformFormat = GGnmManager.GetDataFormat(Format, Flags & TexCreate_SRGB);

	NumSamples = Gnm::kNumSamples1;
	Gnm::NumFragments NumFragments = Gnm::kNumFragments1;

	switch (InNumSamples)
	{
	case 1:
		NumSamples = Gnm::kNumSamples1;
		NumFragments = Gnm::kNumFragments1;
		break;
	case 2:
		NumSamples = Gnm::kNumSamples2;
		NumFragments = Gnm::kNumFragments2;
		break;
	case 4:
		NumSamples = Gnm::kNumSamples4;
		NumFragments = Gnm::kNumFragments4;
		break;
	case 8:
		NumSamples = Gnm::kNumSamples8;
		NumFragments = Gnm::kNumFragments8;
		break;
	default:
		check(0);
		break;
	}

	// color and depth buffer pointers - will only be non-NULL for targetable textures
	ColorBuffer = NULL;
	DepthBuffer = NULL;

#if VALIDATE_MEMORY_PROTECTION
	//bNeedsGPUWrite = (Flags & (TexCreate_UAV | TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable)) != 0;
	//for the moment all textures need GPU write because of how texture streaming moves data around.  could validate more if this was not true, or handled elsewhere.
	bNeedsGPUWrite = true;
#endif

	if (Flags & TexCreate_CPUReadback)
	{
		CopyToResolveTargetLabel = (uint64*)FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Label)).GetPointer();
		*CopyToResolveTargetLabel = (uint64)ECopyToResolveState::Valid;
		FrameSubmitted = 0;

		// Force texture to be linear so that the CPU can read from it
		Flags |= TexCreate_NoTiling;
	}
	else
	{
		CopyToResolveTargetLabel = NULL;
	}

	// figure out Gnm tile mode 
	Gnm::TileMode SurfaceTileMode = DetermineTileMode(SizeX, SizeY, SizeZ, Flags, ResourceType, PlatformFormat, Format, InNumSamples);

	// If the surface tile mode is linear, there is no need to tile on unlock.
	if (SurfaceTileMode == sce::Gnm::kTileModeDisplay_LinearAligned)
	{
		bNeedsToTileOnUnlock = false;
	}

	// init the texture based on type
	Texture = new Gnm::Texture;
	Gnm::TextureSpec TextureSpec;
	TextureSpec.init();

	TextureSpec.m_width = SizeX;
	TextureSpec.m_height = SizeY;
	TextureSpec.m_depth = 1;
	TextureSpec.m_pitch = 0;
	TextureSpec.m_numMipLevels = NumMips;
	TextureSpec.m_numSlices = 1;
	TextureSpec.m_format = PlatformFormat;
	TextureSpec.m_tileModeHint = SurfaceTileMode;
	TextureSpec.m_minGpuMode = Gnm::getGpuMode();
	TextureSpec.m_numFragments = NumFragments;

	if (ResourceType == RRT_TextureCube)
	{
		TextureSpec.m_textureType = Gnm::kTextureTypeCubemap;
		if (bArray)
		{
			TextureSpec.m_numSlices = ArraySize;		
		}

		Flags |= TexCreate_NoFastClear;
	}
	else if (ResourceType == RRT_Texture2DArray)
	{
		check(bArray);
		TextureSpec.m_textureType = Gnm::kTextureType2dArray;
		TextureSpec.m_numSlices = ArraySize;

		Flags |= TexCreate_NoFastClear;
	}
	else if (ResourceType == RRT_Texture3D)
	{
		TextureSpec.m_textureType = Gnm::kTextureType3d;
		TextureSpec.m_depth = SizeZ;

		Flags |= TexCreate_NoFastClear;
	}
	else
	{
		if (NumFragments > Gnm::kNumFragments1)
		{
			TextureSpec.m_textureType = Gnm::kTextureType2dMsaa;
		}
		else
		{
			TextureSpec.m_textureType = Gnm::kTextureType2d;
		}
	}

	int32 Ret = Texture->init(&TextureSpec);
	check(Ret == SCE_OK);

	AlignedSize = Texture->getSizeAlign();

	EGnmMemType GnmMemType = EGnmMemType::GnmMem_GPU;
	if( Flags & TexCreate_Presentable )
	{
		if (Format == PF_L8)
		{
			// Aux surface must be 16kb aligned
			AlignedSize.m_align = 16 * 1024;
		}
		else
		{
		// presentable surfaces must be aligned to 64KiB.
			AlignedSize.m_align = FMath::Max(AlignedSize.m_align, 0x10000u);
	}

#if USE_NEW_PS4_MEMORY_SYSTEM
		GnmMemType = EGnmMemType::GnmMem_FrameBuffer;
#endif
	}

	bool bAllowFastClears = (Flags & TexCreate_NoFastClear) == 0 && (ClearValue.ColorBinding != EClearBinding::ENoneBound);
	
#if USE_DEFRAG_ALLOCATOR
	const bool bAllowDefrag = (Flags & TexCreate_DisableAutoDefrag) == 0;
	bDefraggable = bAllowDefrag && (SurfaceTileMode != sce::Gnm::kTileModeDisplay_LinearAligned);
#endif

	const bool bDepthTarget = ((Flags & (TexCreate_DepthStencilTargetable | TexCreate_DepthStencilResolveTarget)) != 0);
	const bool bRenderTarget = !bDepthTarget && ((Flags & (TexCreate_RenderTargetable | TexCreate_ResolveTargetable)) != 0);

#if !PS4_RENDERTARGETS_IN_DEFRAGHEAP
	bDefraggable &= (!bRenderTarget);
	bDefraggable &= (!bDepthTarget);
#endif	

#if PS4_DEFRAG_RENDERTARGETS
	const bool bUnlockOnExit = true;
#else
	const bool bUnlockOnExit = !(bRenderTarget || bDepthTarget);
#endif	

	// is this a render target texture?
	if (bRenderTarget)
	{
		if( Flags & TexCreate_Presentable )
		{
			if (Format == PF_L8)
			{
				// Aux surface must be 16kb aligned
				AlignedSize.m_align = 16 * 1024;
			}
			else
			{
			// presentable surfaces must be aligned to 64KiB.
				AlignedSize.m_align = FMath::Max(AlignedSize.m_align, 0x10000u);
			}
		}

		Gnm::RenderTargetSpec RenderTargetSpec;
		RenderTargetSpec.init();
		RenderTargetSpec.m_width = TextureSpec.m_width;
		RenderTargetSpec.m_height = TextureSpec.m_height;
		RenderTargetSpec.m_numSlices = NumSlices;
		RenderTargetSpec.m_pitch = TextureSpec.m_pitch;		
		RenderTargetSpec.m_colorFormat = TextureSpec.m_format;
		RenderTargetSpec.m_colorTileModeHint = TextureSpec.m_tileModeHint;
		RenderTargetSpec.m_numSamples = NumSamples;
		RenderTargetSpec.m_numFragments = NumFragments;

		//RenderTargets always start writable.
		SetCurrentGPUAccess(EResourceTransitionAccess::EWritable);

		bool bNeedsCMask = USE_CMASK && bAllowFastClears;

		bool bIsMultisampled = (NumSamples != Gnm::kNumSamples1) || (NumFragments != Gnm::kNumFragments1);
		
		if ((PlatformFormat.getBitsPerElement() == 128) // CMask not supported for 128bpp
			|| (SurfaceTileMode == sce::Gnm::kTileModeDisplay_LinearAligned) 
			|| (SurfaceTileMode >= sce::Gnm::kTileModeThick_1dThick)) // VolumeTextures do not support CMASK
		{
			bNeedsCMask = false;
		}

		// allocate the object
		ColorBuffer = new Gnm::RenderTarget;

		if (bNeedsCMask || bIsMultisampled)
		{			
			RenderTargetSpec.m_flags.enableCmaskFastClear = 1;
			RenderTargetSpec.m_flags.enableFmaskCompression = 1;
			ColorBuffer->init(&RenderTargetSpec);

			Gnm::SizeAlign RTSize = ColorBuffer->getColorSizeAlign();

			AlignedSize.m_size = FMath::Max( AlignedSize.m_size, RTSize.m_size );

			CMaskAlignedSize = ColorBuffer->getCmaskSizeAlign();
			// Need to allocate BufferMem and CMaskMem as one allocation to work around a CMask corruption issue
			uint32 BufferMasksSize = AlignedSize.m_size + AlignedSize.m_align;
			BufferMasksSize += CMaskAlignedSize.m_size + CMaskAlignedSize.m_align;


			if (bIsMultisampled)
			{
				FMaskAlignedSize = ColorBuffer->getFmaskSizeAlign();
				BufferMasksSize += FMaskAlignedSize.m_size + FMaskAlignedSize.m_align;
			}

#if USE_DEFRAG_ALLOCATOR && PS4_RENDERTARGETS_IN_DEFRAGHEAP
			TryAllocateDefraggable(BufferMasksSize, AlignedSize.m_align, GET_STATID(STAT_Garlic_RenderTarget));
#endif
			if (!bDefraggable)
			{
				BufferMasksMem = FMemBlock::Allocate(BufferMasksSize, AlignedSize.m_align, GnmMemType, GET_STATID(STAT_Garlic_RenderTarget));
			}

 			DEC_MEMORY_STAT_BY(STAT_Garlic_RenderTarget, CMaskAlignedSize.m_size);
			INC_MEMORY_STAT_BY(STAT_Garlic_CMask, CMaskAlignedSize.m_size);

			BufferMem.OverridePointer(BufferMasksMem.GetPointer());
			BufferMem.OverrideSize( AlignedSize.m_size );

			CMaskMem.OverridePointer( Align<uint8*>((uint8*)BufferMem.GetPointer() + AlignedSize.m_size, CMaskAlignedSize.m_align ) );
			CMaskMem.OverrideSize( CMaskAlignedSize.m_size );

			// make sure we start with a clean cmask
			FMemory::Memset(CMaskMem.GetPointer(), 0, CMaskMem.GetSize());

			check( (uint8*)CMaskMem.GetPointer() + CMaskMem.GetSize() <= (uint8*)BufferMasksMem.GetPointer() + BufferMasksMem.GetSize() );

			BufferMem.OverridePointer(BufferMasksMem.GetPointer());

			uint32 ConvertedClearColor[2];
			ConvertColorToCMASKBits( RenderTargetSpec.m_colorFormat, ClearValue.GetClearColor(), ConvertedClearColor );
			ColorBuffer->setCmaskClearColor( ConvertedClearColor[0], ConvertedClearColor[1] );

			if (bIsMultisampled)
			{
				FMaskMem.OverridePointer(Align<uint8*>((uint8*)CMaskMem.GetPointer() + CMaskAlignedSize.m_size, FMaskAlignedSize.m_align));
				FMaskMem.OverrideSize(FMaskAlignedSize.m_size);

				// make sure we start with a clean fmask
				FMemory::Memset(FMaskMem.GetPointer(), 0, FMaskMem.GetSize());

				check((uint8*)FMaskMem.GetPointer() + FMaskMem.GetSize() <= (uint8*)BufferMasksMem.GetPointer() + BufferMasksMem.GetSize());
			}
			else
			{
				FMaskMem.OverridePointer(BufferMem.GetPointer());
				FMaskMem.OverrideSize( FMaskAlignedSize.m_size );
			}
		}
		else
		{
			ColorBuffer->init(&RenderTargetSpec);

			Gnm::SizeAlign RTSize = ColorBuffer->getColorSizeAlign();

			AlignedSize.m_size = FMath::Max( AlignedSize.m_size, RTSize.m_size );

#if USE_DEFRAG_ALLOCATOR && PS4_RENDERTARGETS_IN_DEFRAGHEAP
			TryAllocateDefraggable(AlignedSize.m_size, AlignedSize.m_align, GET_STATID(STAT_Garlic_RenderTarget));
#endif
			if (!bDefraggable)
			{
				BufferMasksMem = FMemBlock::Allocate(AlignedSize.m_size, AlignedSize.m_align, GnmMemType, GET_STATID(STAT_Garlic_RenderTarget));
			}
			BufferMem.OverridePointer(BufferMasksMem.GetPointer() );
			BufferMem.OverrideSize(BufferMasksMem.GetSize() );
			check(((uint64)BufferMem.GetPointer() & (AlignedSize.m_align - 1)) == 0);

			FMaskMem.OverridePointer(BufferMem.GetPointer());
			FMaskMem.OverrideSize( BufferMasksMem.GetSize() );
			//CMaskMem.OverridePointer(BufferMem.GetPointer());
			//CMaskMem.OverrideSize(BufferMasksMem.GetSize());
		}

		// set the texture address
		Texture->setBaseAddress(BufferMem.GetPointer());

		// set the ColorBuffer to point to the same address as the texture
		// (at least in 0.820, the fmask address must be set to the cmask address for MRT CMASK to work properly)
		ColorBuffer->setAddresses(BufferMem.GetPointer(), CMaskMem.GetPointer(), FMaskMem.GetPointer());
		ColorBuffer->setCmaskFastClearEnable(bIsMultisampled);
		ColorBuffer->setFmaskCompressionEnable(bIsMultisampled);
 		ColorBuffer->setFmaskTileMode(ColorBuffer->getTileMode());
 		ColorBuffer->setFmaskSliceNumTilesMinus1(ColorBuffer->getSliceSizeDiv64Minus1());	
	}
	else
	{
		if (bDepthTarget)
		{
			// optional stencil
			bool bNeedsStencil = Format == PF_DepthStencil;
			bool bNeedsHTile = USE_HTILE && bAllowFastClears;

			Gnm::StencilFormat StencilFormat = bNeedsStencil ? Gnm::kStencil8 : Gnm::kStencilInvalid;
			Gnm::ZFormat ZFormat = PlatformFormat.getZFormat();

			Gnm::DepthRenderTargetSpec DepthTargetSpec;
			DepthTargetSpec.init();
			DepthTargetSpec.m_width = TextureSpec.m_width;
			DepthTargetSpec.m_height = TextureSpec.m_height;
			DepthTargetSpec.m_numSlices = NumSlices;
			DepthTargetSpec.m_pitch = TextureSpec.m_pitch;	
			DepthTargetSpec.m_zFormat = ZFormat;
			DepthTargetSpec.m_stencilFormat = StencilFormat;
			DepthTargetSpec.m_tileModeHint = TextureSpec.m_tileModeHint;
			DepthTargetSpec.m_numFragments = NumFragments;
			DepthTargetSpec.m_flags.enableHtileAcceleration = bNeedsHTile ? 1 : 0;			

			// allocate the object
			DepthBuffer = new Gnm::DepthRenderTarget;

			// initialize the object
			Ret = DepthBuffer->init(&DepthTargetSpec);
			check(Ret == SCE_OK);

			Texture->setPitchMinus1( DepthBuffer->getPitch() - 1 );

			Gnm::SizeAlign DepthSize = DepthBuffer->getZSizeAlign();

			AlignedSize.m_size = FMath::Max( AlignedSize.m_size, DepthSize.m_size );

#if USE_DEFRAG_ALLOCATOR
			TryAllocateDefraggable(AlignedSize.m_size, AlignedSize.m_align, bDepthTarget ? GET_STATID(STAT_Garlic_DepthRenderTarget) : GET_STATID(STAT_Garlic_Texture));
#endif

			if (!bDefraggable)
			{
				BufferMasksMem = FMemBlock::Allocate(AlignedSize.m_size, AlignedSize.m_align, GnmMemType,
					bDepthTarget ? GET_STATID(STAT_Garlic_DepthRenderTarget) : GET_STATID(STAT_Garlic_Texture));
			}

			BufferMem.OverridePointer(BufferMasksMem.GetPointer());
			BufferMem.OverrideSize(BufferMasksMem.GetSize());
			check(((uint64)BufferMem.GetPointer() & (AlignedSize.m_align - 1)) == 0);

			// allocate stencil memory if needed
			if (bNeedsStencil)
			{
				Gnm::SizeAlign StencilAlignedSize = DepthBuffer->getStencilSizeAlign();
				StencilMem = FMemBlock::Allocate(StencilAlignedSize.m_size, StencilAlignedSize.m_align, GnmMemType, GET_STATID(STAT_Garlic_StencilBuffer));
			}
	
			// hook up pointers
			DepthBuffer->setAddresses(BufferMem.GetPointer(), StencilMem.GetPointer());

			// allocate and set HTile memory
			if (bNeedsHTile)
			{
				Gnm::SizeAlign HTileAlignedSize = DepthBuffer->getHtileSizeAlign();
				HTileMem = FMemBlock::Allocate(HTileAlignedSize.m_size, HTileAlignedSize.m_align, GnmMemType, GET_STATID(STAT_Garlic_HTile));

				// make sure we start with a clean htile
				FMemory::Memset(HTileMem.GetPointer(), 0, HTileMem.GetSize());

				DepthBuffer->setHtileAddress(HTileMem.GetPointer());				
				DepthBuffer->setHtileStencilDisable(true);
			}

			//htile acceleration is critical as it controls HiZ.  Unlike CMASK which is only used for clears,
			//we can't afford to not be using HTILE acceleration in almost any situation
			DepthBuffer->setHtileAccelerationEnable(bNeedsHTile);

			check( DepthBuffer->getZSizeAlign().m_size >= Texture->getSizeAlign().m_size );

		}
		else
		{
#if USE_DEFRAG_ALLOCATOR
			TryAllocateDefraggable(AlignedSize.m_size, AlignedSize.m_align, bDepthTarget ? GET_STATID(STAT_Garlic_DepthRenderTarget) : GET_STATID(STAT_Garlic_Texture));
#endif

			if (!bDefraggable)
			{
				BufferMasksMem = FMemBlock::Allocate(AlignedSize.m_size, AlignedSize.m_align, GnmMemType,
					bDepthTarget ? GET_STATID(STAT_Garlic_DepthRenderTarget) : GET_STATID(STAT_Garlic_Texture));
			}

			BufferMem.OverridePointer(BufferMasksMem.GetPointer());
			BufferMem.OverrideSize(BufferMasksMem.GetSize());
			check(((uint64)BufferMem.GetPointer() & (AlignedSize.m_align - 1)) == 0);

			Texture->setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
		}

		// set the texture address
		Texture->setBaseAddress(BufferMem.GetPointer());

	}

	// upload existing bulkdata
	// @todo gnm: This assumes that the offline cooker did any needed padding needed between slices, etc
	// 3D textures do seem to work, cube/array untested
	if (BulkData)
	{
		LoadPixelDataFromMemory(BulkData->GetResourceBulkData(), BulkData->GetResourceBulkDataSize());

		// bulk data can be unloaded now
		BulkData->Discard();
	}
	
	Gnm::TextureType TextureType = Texture->getTextureType();
	UpdateGnmTextureStats(*this, GetMemorySize() + AlignedSize.m_align, TextureType == Gnm::kTextureTypeCubemap, TextureType == Gnm::kTextureType3d, bStreamable);

	//put new code above here so accesses to the base address remain safe
#if USE_DEFRAG_ALLOCATOR
	if (bDefraggable)
	{
		check(RHITexture);
		GGnmManager.DefragAllocator.RegisterDefraggable(this);

		if (bUnlockOnExit)
		{
#if !PS4_DEFRAG_RENDERTARGETS
			check(!(ColorBuffer || DepthBuffer));
#endif
			GGnmManager.DefragAllocator.Unlock(DefragAddress);
		}
	}
#endif
}

bool FGnmSurface::TryAllocateDefraggable(uint32 AllocationSize, uint32 Alignment, TStatId StatId)
{
#if USE_DEFRAG_ALLOCATOR
	if (bDefraggable)
	{
		FPS4GPUDefragAllocator& DefragAllocator = GGnmManager.DefragAllocator;
		const int32 DefragAllocationAlign = DefragAllocator.GetAllocationAlignment();

		int32 ExtraAlignSize = Alignment <= DefragAllocationAlign ? 0 : Alignment;
		void* DefraggableData = GGnmManager.DefragAllocator.AllocateLocked(AllocationSize + ExtraAlignSize, DefragAllocationAlign, StatId, true);
		if (DefraggableData)
		{
			DefragAddress = DefraggableData;
			BufferMasksMem.OverridePointer(Align(DefraggableData, Alignment));
			BufferMasksMem.OverrideSize(AllocationSize);
			check(((uint8*)BufferMasksMem.GetPointer() + AllocationSize) <= ((uint8*)DefraggableData + AllocationSize + ExtraAlignSize));
		}
		else
		{
			UE_LOG(LogPS4, Warning, TEXT("Tried to load texture into defrag pool but not enough space.  Falling back to normal allocator."));
			DefragAddress = nullptr;
			bDefraggable = false;
		}
	}
#else
	bDefraggable = false;
#endif
	return bDefraggable;
}


FGnmSurface::~FGnmSurface()
{
#if USE_DEFRAG_ALLOCATOR
	FPS4GPUDefragAllocator& DefragAllocator = GGnmManager.DefragAllocator;	
	FScopedGPUDefragLock DefragAllocatorLock(DefragAllocator);	

	const void* OrigBaseAddress = BufferMasksMem.GetPointer();
	if (bDefraggable)
	{
		if (RHITexture)
		{
			DefragAllocator.UnregisterDefraggable(this);
		}

		//if rendertargets are in the defrag heap, but not defraggable that means we left them locked.
		//do a final unlock to keep the counts right.
#if !PS4_DEFRAG_RENDERTARGETS
		if (ColorBuffer || DepthBuffer)
		{
			DefragAllocator.Unlock(DefragAddress);
		}
#endif
		DefragAllocator.Free(DefragAddress);
	}
	else
#endif
	{
		// To work around a bug with CMask corruption the Buffer and CMask are allocated in one block
		// and stored in BufferMasksMem so BufferMem, CMaskMem, and FMaskMem are only used to store information 
		// about the allocation and don't actually need freeing
		FMemBlock::Free(BufferMasksMem);
	}	

	INC_MEMORY_STAT_BY(STAT_Garlic_RenderTarget, CMaskMem.GetSize());
	DEC_MEMORY_STAT_BY(STAT_Garlic_CMask, CMaskMem.GetSize());

	Gnm::TextureType TextureType = Texture->getTextureType();
	UpdateGnmTextureStats(*this, -(int64)((GetMemorySize() + AlignedSize.m_align)), TextureType == Gnm::kTextureTypeCubemap, TextureType == Gnm::kTextureType3d, bStreamable);

	/*
	FMemBlock::Free(BufferMem);
	FMemBlock::Free(CMaskMem);
	*/
	FMemBlock::Free(StencilMem);
#if USE_HTILE
	FMemBlock::Free(HTileMem);
#endif

	delete ColorBuffer;
	ColorBuffer = nullptr;

	delete DepthBuffer;
	DepthBuffer = nullptr;

	delete Texture;
	Texture = nullptr;

#if USE_DEFRAG_ALLOCATOR
	checkf(!bDefraggable || (OrigBaseAddress == BufferMasksMem.GetPointer()), TEXT("Texture was relocated during free!"));
#endif
}


void* FGnmSurface::Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
#if USE_DEFRAG_ALLOCATOR
	//protect against the defragger potentially changing the base address on us while locking down the texture.
	FScopedGPUDefragLock DefragLock(GGnmManager.DefragAllocator);

	//if the texture is defraggable we must wait for any outstanding GPU memory moves and prevent further GPU adjustment
	//until the CPU is done with the texture.
	if (bDefraggable)
	{
		GGnmManager.DefragAllocator.Lock(DefragAddress);
	}
#endif

	// ask addrlib for mip size
	uint64_t MipOffset;
	uint64_t MipSize;
	GpuAddress::computeTextureSurfaceOffsetAndSize(&MipOffset, &MipSize, Texture, MipIndex, ArrayIndex);

	// set up tiling struct (it doesn't matter which face we pass in to the initFromTexture, and 0.630 will break with a 1x1x1 non-zero face)
	GpuAddress::TilingParameters TilingParams;
	TilingParams.initFromTexture(Texture, MipIndex, 0);
	int32 BytesPerElement = Texture->getDataFormat().getBitsPerElement() / 8;
	
	// allocate CPU memory
	if (bNeedsToTileOnUnlock)
	{	
		// get the size for an untiled version of the mip
		uint64_t OutSize;
		Gnm::AlignmentType OutAlign;
		GpuAddress::computeUntiledSurfaceSize(&OutSize, &OutAlign, &TilingParams);

		check(UntiledLockedMemory == nullptr);

		// allocate space to load the untiled into
		UntiledLockedMemory = FMemory::Malloc(OutSize);

		// for runtime textures, the stride is often needed
		// compute stride from the width from the tilingparams, padding not necessary as this memory will be tiled on unlock
		// via the same tilingparams.
		DestStride = TilingParams.m_linearWidth * BytesPerElement;
		check(DestStride * TilingParams.m_linearHeight == OutSize);
	}
	else if (TilingParams.m_tileMode == Gnm::kTileModeDisplay_LinearAligned)
	{
		// Use gpu_addr to come up with actual legal/padded surface parameters
		GpuAddress::SurfaceInfo SurfInfoOut = {0};
		int32_t Status = GpuAddress::computeSurfaceInfo(&SurfInfoOut, &TilingParams);
		check(Status == GpuAddress::kStatusSuccess);

		DestStride = SurfInfoOut.m_pitch * BytesPerElement;
		
		int32 TiledHeight = SurfInfoOut.m_height;		
		ensureMsgf(DestStride * TiledHeight == SurfInfoOut.m_surfaceSize, TEXT("DestStride: %i, TiledHeight: %i for Mip: %i of Texture: %i x %i, BPE: %i doesn't match OutSize: %i"), DestStride, TiledHeight, MipIndex, Texture->getWidth(), Texture->getHeight(), BytesPerElement, SurfInfoOut.m_surfaceSize);
	}
	else
	{
		// don't need it for textures that were processed offline (the loading code will get confused if the stride
		// doesn't match what it thinks the stride should be, and do one line at a time, which will never work 
		// for offline tiled textures)
		DestStride = 0;
	}

	return bNeedsToTileOnUnlock ? UntiledLockedMemory : (uint8*)Texture->getBaseAddress() + MipOffset;
}

void FGnmSurface::Unlock(uint32 MipIndex, uint32 ArrayIndex)
{
	if (bNeedsToTileOnUnlock)
	{
		// set up tiling struct (it doesn't matter which face we pass in to the initFromTexture, and 0.630 will break with a 1x1x1 non-zero face)
		GpuAddress::TilingParameters TilingParams;
		TilingParams.initFromTexture(Texture, MipIndex, 0);

		// ask addrlib for mip size and offset
		uint64_t MipOffset;
		uint64_t MipSize;
		GpuAddress::computeTextureSurfaceOffsetAndSize(&MipOffset, &MipSize, Texture, MipIndex, ArrayIndex);

		// tile the mip right into gpu memory
		GpuAddress::tileSurface((uint8*)Texture->getBaseAddress() + MipOffset, UntiledLockedMemory, &TilingParams);

		// free temp untiled memory
		FMemory::Free(UntiledLockedMemory);
		UntiledLockedMemory = nullptr;
	}

#if USE_DEFRAG_ALLOCATOR
	if (bDefraggable)
	{
		GGnmManager.DefragAllocator.Unlock(DefragAddress);
	}
#endif
}

void* FGnmSurface::LockDeferred(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
	check(MipIndex == 0);
	check(ArrayIndex == 0);
	check(Texture);
	check(Texture->getLastMipLevel() == 0);
	check(ColorBuffer == nullptr);
	check(DepthBuffer == nullptr);
	check(UntiledLockedMemory == nullptr);


	// ask addrlib for mip size
	uint64_t MipOffset;
	uint64_t MipSize;
	GpuAddress::computeTextureSurfaceOffsetAndSize(&MipOffset, &MipSize, Texture, MipIndex, ArrayIndex);

	// set up tiling struct (it doesn't matter which face we pass in to the initFromTexture, and 0.630 will break with a 1x1x1 non-zero face)
	GpuAddress::TilingParameters TilingParams;
	TilingParams.initFromTexture(Texture, MipIndex, 0);
	int32 BytesPerElement = Texture->getDataFormat().getBitsPerElement() / 8;

	uint64_t OutSize = 0;

	// allocate CPU memory
	if (bNeedsToTileOnUnlock)
	{
		// get the size for an untiled version of the mip		
		Gnm::AlignmentType OutAlign;
		GpuAddress::computeUntiledSurfaceSize(&OutSize, &OutAlign, &TilingParams);

		// for runtime textures, the stride is often needed
		// compute stride from the width from the tilingparams, padding not necessary as this memory will be tiled on unlock
		// via the same tilingparams.
		DestStride = TilingParams.m_linearWidth * BytesPerElement;
		check(DestStride * TilingParams.m_linearHeight == OutSize);
	}
	else if (TilingParams.m_tileMode == Gnm::kTileModeDisplay_LinearAligned)
	{
		// Use gpu_addr to come up with actual legal/padded surface parameters
		GpuAddress::SurfaceInfo SurfInfoOut = { 0 };
		int32_t Status = GpuAddress::computeSurfaceInfo(&SurfInfoOut, &TilingParams);
		check(Status == GpuAddress::kStatusSuccess);

		DestStride = SurfInfoOut.m_pitch * BytesPerElement;

		int32 TiledHeight = SurfInfoOut.m_height;
		ensureMsgf(DestStride * TiledHeight == SurfInfoOut.m_surfaceSize, TEXT("DestStride: %i, TiledHeight: %i for Mip: %i of Texture: %i x %i, BPE: %i doesn't match OutSize: %i"), DestStride, TiledHeight, MipIndex, Texture->getWidth(), Texture->getHeight(), BytesPerElement, SurfInfoOut.m_surfaceSize);

		OutSize = DestStride * TiledHeight;
	}
	else
	{		
		// don't need it for textures that were processed offline (the loading code will get confused if the stride
		// doesn't match what it thinks the stride should be, and do one line at a time, which will never work 
		// for offline tiled textures)
		DestStride = 0;
		checkf(false, TEXT("Can't deferred lock offline processed textures."));
	}

	check(DestStride * TilingParams.m_linearHeight == OutSize);

	// allocate space to load the untiled into
	void* WriteOnlyTextureMem = FMemory::Malloc(OutSize);
	return WriteOnlyTextureMem;
}

void FGnmSurface::UnlockDeferred(void* Data, uint32 MipIndex, uint32 ArrayIndex)
{
	LLM_SCOPE(ColorBuffer ? ELLMTag::RenderTargets : ELLMTag::Textures);

	check(Data != nullptr);
	check(MipIndex == 0);
	check(ArrayIndex == 0);
	check(Texture);
	check(Texture->getLastMipLevel() == 0);
	check(ColorBuffer == nullptr);
	check(DepthBuffer == nullptr);
	check(UntiledLockedMemory == nullptr);

#if USE_DEFRAG_ALLOCATOR
	FScopedGPUDefragLock DefragLock(GGnmManager.DefragAllocator);
	//always free the original data.  Frees are delayed a frame internally.
	if (bDefraggable)
	{

		//if rendertargets are in the defrag heap, but not defraggable that means we left them locked.
		//do a final unlock to keep the counts right.
#if !PS4_DEFRAG_RENDERTARGETS
		if (ColorBuffer || DepthBuffer)
		{
			GGnmManager.DefragAllocator.Unlock(DefragAddress);
		}
#endif

		GGnmManager.DefragAllocator.Free(DefragAddress);
		GGnmManager.DefragAllocator.UnregisterDefraggable(this);
	}
	else
#endif
	{
		FMemBlock::Free(BufferMasksMem);
	}

	if (bDefraggable)
	{
		TryAllocateDefraggable(AlignedSize.m_size, AlignedSize.m_align, GET_STATID(STAT_Garlic_Texture));
	}

	if (!bDefraggable)
	{
		BufferMasksMem = FMemBlock::Allocate(AlignedSize.m_size, AlignedSize.m_align, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_Texture));
	}
	BufferMem.OverridePointer(BufferMasksMem.GetPointer());
	// set the texture address
	Texture->setBaseAddress(BufferMem.GetPointer());

	//if we need to tile on unlock, then we have to allocate some target mem and do the tiling.
	if (bNeedsToTileOnUnlock)
	{		
		
		check(((uint64)BufferMem.GetPointer() & (AlignedSize.m_align - 1)) == 0);		
		
		GpuAddress::TilingParameters TilingParams;
		TilingParams.initFromTexture(Texture, MipIndex, 0);

		// ask addrlib for mip size and offset
		uint64_t MipOffset;
		uint64_t MipSize;
		GpuAddress::computeTextureSurfaceOffsetAndSize(&MipOffset, &MipSize, Texture, MipIndex, ArrayIndex);

		// tile the mip right into gpu memory
		GpuAddress::tileSurface((uint8*)Texture->getBaseAddress() + MipOffset, Data, &TilingParams);		
	}
	//have to copy in linear case also to GPU memory.
	else
	{
		check(Texture->getTileMode() == Gnm::kTileModeDisplay_LinearAligned);	
		FMemory::Memcpy(BufferMasksMem.GetPointer(), Data, AlignedSize.m_size);
	}

	FMemory::Free(Data);

#if USE_DEFRAG_ALLOCATOR
	if (bDefraggable)
	{
		GGnmManager.DefragAllocator.RegisterDefraggable(this);
		GGnmManager.DefragAllocator.Unlock(DefragAddress);
	}
#endif
}

#include "ScopedTimers.h"
double PS4LoadTilingTime = 0.0;
void FGnmSurface::LoadPixelDataFromMemory(const void* PixelData, uint32 PixelDataSize)
{
	if (bNeedsToTileOnUnlock)
	{
		FScopedDurationTimer Timer(PS4LoadTilingTime);
		for (uint32 SliceIndex = Texture->getBaseArraySliceIndex(); SliceIndex <= Texture->getLastArraySliceIndex(); SliceIndex++)
		{
			for (uint32 MipIndex = Texture->getBaseMipLevel(); MipIndex <= Texture->getLastMipLevel(); MipIndex++)
			{
				// set up tiling struct (it doesn't matter which face we pass in to the initFromTexture, and 0.630 will break with a 1x1x1 non-zero face)
				GpuAddress::TilingParameters TilingParams;
				TilingParams.initFromTexture(Texture, MipIndex, 0);

				// ask addrlib for mip size and offset
				uint64_t MipOffset;
				uint64_t MipSize;
				GpuAddress::computeTextureSurfaceOffsetAndSize(&MipOffset, &MipSize, Texture, MipIndex, SliceIndex);

				// tile the mip right into gpu memory
				GpuAddress::tileSurface((uint8*)Texture->getBaseAddress() + MipOffset, (uint8*)PixelData + MipOffset, &TilingParams);
			}
		}
	}
	else
	{
		FMemory::Memcpy(Texture->getBaseAddress(), PixelData, PixelDataSize);
	}
}

/**
 * Set the surface as having a CopyToResolveTarget in flight
 */
void FGnmSurface::BeginCopyToResolveTarget()
{
	if( CopyToResolveTargetLabel != NULL )
	{
		// Set the label to the "pending" state until the copy is complete by the GPU
		*CopyToResolveTargetLabel = (uint64)ECopyToResolveState::Pending;
		FrameSubmitted = GGnmManager.GetFrameCount();
	}
}

/**
 * Set the surface as having its CopyToResolveTarget completed by the GPU
 */
void FGnmSurface::EndCopyToResolveTarget(FGnmCommandListContext& GnmCommandContext)
{
	if( CopyToResolveTargetLabel != NULL )
	{
		// The GPU will write the "valid" state to the label when the copy is complete
	    const uint64 ValidState = (uint64)ECopyToResolveState::Valid;
		GnmCommandContext.GetContext().writeDataInline((void*)CopyToResolveTargetLabel, &ValidState, 2, Gnm::kWriteDataConfirmDisable);
	}
}

/**
 * If there's a labeled CopyToResolveTarget in flight, this will block until it's done, otherwise, return immediately
 */
void FGnmSurface::BlockUntilCopyToResolveTargetComplete()
{
	if (CopyToResolveTargetLabel != NULL)
	{
		if( GGnmManager.GetFrameCount() == FrameSubmitted )
		{
			// Trying to get the result before the command buffer has been submitted to the GPU
			return;
		}

		// block until the GPU sets the label to "valid" state indicating that the copy has completed
		while (*CopyToResolveTargetLabel != (uint64)ECopyToResolveState::Valid)
		{
			FPlatformProcess::Sleep(0);
		}
	}
}

void* FGnmSurface::GetBaseAddress()
{
	return DefragAddress;
}

void FGnmSurface::UpdateBaseAddress(void* NewBaseAddress)
{
	check(NewBaseAddress);
	check(ColorBuffer == nullptr);
	check(DepthBuffer == nullptr);
	check(FGPUDefragAllocator::IsAligned(NewBaseAddress, AlignedSize.m_align));

	DefragAddress = NewBaseAddress;
	Texture->setBaseAddress(NewBaseAddress);
	
	BufferMasksMem.OverridePointer(NewBaseAddress);
	BufferMem.OverridePointer(NewBaseAddress);

	// if CMask or FMask are unused the offsets added here will be zero, so we will just alias BufferMem.
	CMaskMem.OverridePointer(Align<uint8*>((uint8*)BufferMem.GetPointer() + AlignedSize.m_size, CMaskAlignedSize.m_align));
	CMaskMem.OverrideSize(CMaskAlignedSize.m_size);

	FMaskMem.OverridePointer(Align<uint8*>((uint8*)CMaskMem.GetPointer() + CMaskAlignedSize.m_size, FMaskAlignedSize.m_align));
	FMaskMem.OverrideSize(FMaskAlignedSize.m_size);
}


/**
 * Helper class for packing 32 bit floats into GPU compatible small float formats
 */
template<uint32 NumMantissaBits>
class TGPUFloatPacker
{
public:
	union
	{
		struct
		{
			uint16	Mantissa : NumMantissaBits;
			uint16	Exponent : 5;
			uint16	Pad : 16 - 5 - NumMantissaBits;
		} Components;

		uint16	Encoded;
	};

	uint16 Encode( float FP32Value )
	{
		FFloat32 FP32(FP32Value);

		Components.Pad = 0;

		// Check for zero, denormal or too small value.
		if ( FP32.Components.Exponent <= 112 )			// Too small exponent? (0+127-15)
		{
			// Set to 0.
			Components.Exponent = 0;
			Components.Mantissa = 0;
		}
		// Check for INF or NaN, or too high value
		else if ( FP32.Components.Exponent >= 143 )		// Too large exponent? (31+127-15)
		{
			// Set to 65000.0 (max value)
			Components.Exponent = 30;
			Components.Mantissa = ( 1 << NumMantissaBits ) - 1;
		}
		// Handle normal number.
		else
		{
			Components.Exponent = int32(FP32.Components.Exponent) - 127 + 15;
			Components.Mantissa = uint16(FP32.Components.Mantissa >>  (23 - NumMantissaBits) );
		}

		return Encoded;
	}
};

/**
* Converts a color to the bits the FastClear will write to the render target (needs to be in target format)
*/
void FGnmSurface::ConvertColorToCMASKBits(sce::Gnm::DataFormat DataFormat, const FLinearColor& Color, uint32* Bits)
{
	float Swizzle[8] = { 0.0f, 1.0f, 0.0f, 0.0f, Color.R, Color.G, Color.B, Color.A };
	float XVal = Swizzle[ DataFormat.getChannel( 0 ) ];
	float YVal = Swizzle[ DataFormat.getChannel( 1 ) ];
	float ZVal = Swizzle[ DataFormat.getChannel( 2 ) ];
	float WVal = Swizzle[ DataFormat.getChannel( 3 ) ];

	sce::Gnm::RenderTargetFormat RenderTargetFormat = DataFormat.getRenderTargetFormat();
	switch( RenderTargetFormat )
	{
		case sce::Gnm::kRenderTargetFormat8:
		{
			uint8 R = (uint8)FMath::Clamp( XVal * 255, 0.0f, 255.0f );
			Bits[0] = R;
			Bits[1] = 0;
			break;
		}
		case sce::Gnm::kRenderTargetFormat8_8:
		{
			uint8 R = (uint8)FMath::Clamp( XVal * 255, 0.0f, 255.0f );
			uint8 G = (uint8)FMath::Clamp( YVal * 255, 0.0f, 255.0f );
			Bits[0] = (G << 8) | R;
			break;
		}
		case sce::Gnm::kRenderTargetFormat16:
		{
			uint16 R = FFloat16( Color.R ).Encoded;
			Bits[0] = R;
			Bits[1] = 0;
			break;
		}
		case sce::Gnm::kRenderTargetFormat16_16:
		{
			uint16 R = FFloat16( XVal ).Encoded;
			uint16 G = FFloat16( YVal ).Encoded;
			Bits[0] = G << 16 | R;
			Bits[1] = 0;
			break;
		}
		case sce::Gnm::kRenderTargetFormat10_11_11:
		{
			TGPUFloatPacker<6> Float11Packer;
			TGPUFloatPacker<5> Float10Packer;
			uint16 R = Float11Packer.Encode( XVal );
			uint16 G = Float11Packer.Encode( YVal );
			uint16 B = Float10Packer.Encode( ZVal );
			Bits[0] = (B << 22) | (G << 11) | R;
			Bits[1] = 0;
			break;
		}
		case sce::Gnm::kRenderTargetFormat2_10_10_10:
		{
			uint16 R = (uint8)FMath::Clamp( XVal * 1023, 0.0f, 1023.0f );
			uint16 G = (uint8)FMath::Clamp( YVal * 1023, 0.0f, 1023.0f );
			uint16 B = (uint8)FMath::Clamp( ZVal * 1023, 0.0f, 1023.0f );
			uint16 A = (uint8)FMath::Clamp( WVal * 3, 0.0f, 3.0f );
			Bits[0] = (A << 30) | (B << 20) | (G << 10) | R;
			Bits[1] = 0;
			break;
		}
		case sce::Gnm::kRenderTargetFormat8_8_8_8:
		{
			uint8 R = (uint8)FMath::Clamp( XVal * 255, 0.0f, 255.0f );
			uint8 G = (uint8)FMath::Clamp( YVal * 255, 0.0f, 255.0f );
			uint8 B = (uint8)FMath::Clamp( ZVal * 255, 0.0f, 255.0f );
			uint8 A = (uint8)FMath::Clamp( WVal * 255, 0.0f, 255.0f );
			Bits[0] = (A << 24) | (B << 16) | (G << 8) | R;
			Bits[1] = 0;
			break;
		}
		case sce::Gnm::kRenderTargetFormat16_16_16_16:
		{
			// 16 bits per channel
			uint16 R = FFloat16( XVal ).Encoded;
			uint16 G = FFloat16( YVal ).Encoded;
			uint16 B = FFloat16( ZVal ).Encoded;
			uint16 A = FFloat16( WVal ).Encoded;
			Bits[0] = G << 16 | R;
			Bits[1] = A << 16 | B;
			break;
		}
		case sce::Gnm::kRenderTargetFormat32_32_32_32:
		{
			// 128 bit targets are not supported
			check( false );
			break;
		}
		default:
		{
			// Not handled...
			check( false );
		}
	}
}


/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

uint64 FGnmDynamicRHI::RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{	
	// dummy texture to help compute the size.
	Gnm::Texture Texture;

	// texture native format
	Gnm::DataFormat PlatformFormat = GGnmManager.GetDataFormat((EPixelFormat)Format, Flags & TexCreate_SRGB);

	// @todo handle MSAA
	Gnm::NumSamples NumSamplesPlatform = Gnm::kNumSamples1;
	Gnm::NumFragments NumFragmentsPlatform = Gnm::kNumFragments1;

	switch (NumSamples)
	{
	case 1:
		NumSamplesPlatform = Gnm::kNumSamples1;
		NumFragmentsPlatform = Gnm::kNumFragments1;
		break;
	case 2:
		NumSamplesPlatform = Gnm::kNumSamples2;
		NumFragmentsPlatform = Gnm::kNumFragments2;
		break;
	case 4:
		NumSamplesPlatform = Gnm::kNumSamples4;
		NumFragmentsPlatform = Gnm::kNumFragments4;
		break;
	case 8:
		NumSamplesPlatform = Gnm::kNumSamples8;
		NumFragmentsPlatform = Gnm::kNumFragments8;
		break;
	default:
		check(0);
		break;
	}

	// figure out Gnm tile mode 
	Gnm::TileMode SurfaceTileMode = DetermineTileMode(SizeX, SizeY, 1, Flags, RRT_Texture2D, PlatformFormat, (EPixelFormat)Format);

	// compute size the hardware needs for this texture.
	Gnm::TextureSpec TextureSpec;
	TextureSpec.init();
	TextureSpec.m_textureType = Gnm::kTextureType2d;
	TextureSpec.m_width = SizeX;
	TextureSpec.m_height = SizeY;
	TextureSpec.m_depth = 1;
	TextureSpec.m_pitch = 0;
	TextureSpec.m_numMipLevels = NumMips;

	TextureSpec.m_format = PlatformFormat;
	TextureSpec.m_tileModeHint = SurfaceTileMode;
	TextureSpec.m_minGpuMode = Gnm::getGpuMode();
	TextureSpec.m_numFragments = NumFragmentsPlatform;

	Texture.init(&TextureSpec);
	Gnm::SizeAlign AlignedSize = Texture.getSizeAlign();
	
	// @todo handle CMASK ?
	OutAlign = AlignedSize.m_align;
	return AlignedSize.m_size;
}

uint64 FGnmDynamicRHI::RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	// dummy texture to help compute the size.
	Gnm::Texture Texture;

	// texture native format
	Gnm::DataFormat PlatformFormat = GGnmManager.GetDataFormat((EPixelFormat)Format, Flags & TexCreate_SRGB);	

	// figure out Gnm tile mode 
	Gnm::TileMode SurfaceTileMode = DetermineTileMode(SizeX, SizeY, SizeZ, Flags, RRT_Texture2D, PlatformFormat, (EPixelFormat)Format);

	// compute size the hardware needs for this texture.
	Gnm::TextureSpec TextureSpec;
	TextureSpec.init();
	TextureSpec.m_textureType = Gnm::kTextureType3d;
	TextureSpec.m_width = SizeX;
	TextureSpec.m_height = SizeY;
	TextureSpec.m_depth = SizeZ;
	TextureSpec.m_pitch = 0;
	TextureSpec.m_numMipLevels = NumMips;

	TextureSpec.m_format = PlatformFormat;
	TextureSpec.m_tileModeHint = SurfaceTileMode;
	TextureSpec.m_minGpuMode = Gnm::getGpuMode();
	TextureSpec.m_numFragments = Gnm::kNumFragments1;
	Texture.init(&TextureSpec);
	Gnm::SizeAlign AlignedSize = Texture.getSizeAlign();

	// @todo handle CMASK ?
	OutAlign = AlignedSize.m_align;
	return AlignedSize.m_size;
}


uint64 FGnmDynamicRHI::RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags,	uint32& OutAlign)
{
	// dummy texture to help compute the size.
	Gnm::Texture Texture;

	// texture native format
	Gnm::DataFormat PlatformFormat = GGnmManager.GetDataFormat((EPixelFormat)Format, Flags & TexCreate_SRGB);	

	// figure out Gnm tile mode 
	Gnm::TileMode SurfaceTileMode = DetermineTileMode(Size, Size, 1, Flags, RRT_Texture2D, PlatformFormat, (EPixelFormat)Format);

	// compute size the hardware needs for this texture.
	Gnm::TextureSpec TextureSpec;
	TextureSpec.init();

	TextureSpec.m_textureType = Gnm::kTextureTypeCubemap;
	TextureSpec.m_width = Size;
	TextureSpec.m_height = Size;
	TextureSpec.m_depth = 1;
	TextureSpec.m_pitch = 0;
	TextureSpec.m_numMipLevels = NumMips;

	TextureSpec.m_format = PlatformFormat;
	TextureSpec.m_tileModeHint = SurfaceTileMode;
	TextureSpec.m_minGpuMode = Gnm::getGpuMode();
	TextureSpec.m_numFragments = Gnm::kNumFragments1;
	Texture.init(&TextureSpec);

	Gnm::SizeAlign AlignedSize = Texture.getSizeAlign();


	// @todo handle CMASK ?
	OutAlign = AlignedSize.m_align;
	return AlignedSize.m_size;
}

/**
 * Retrieves texture memory stats.
 *
 * @return bool indicating that out variables were left unchanged.
 */
void FGnmDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
#if USE_NEW_PS4_MEMORY_SYSTEM == 0

	extern uint64 GGarlicHeapSize;

	OutStats.DedicatedVideoMemory = GGarlicHeapSize;
	OutStats.DedicatedSystemMemory = 0;
	OutStats.SharedSystemMemory = 0;
	OutStats.TotalGraphicsMemory = GGarlicHeapSize ? GGarlicHeapSize : -1;

#else

	// TODO: Fix these stats
	OutStats.DedicatedVideoMemory = 0;
	OutStats.DedicatedSystemMemory = 0;
	OutStats.SharedSystemMemory = 0;
	OutStats.TotalGraphicsMemory = -1;

#endif

#if USE_DEFRAG_ALLOCATOR && 0
	bool bUseLegacyTexturePool = CVarPS4UseLegacyTexturePool.GetValueOnAnyThread() != 0;
	if (bUseLegacyTexturePool)
	{
		int64 Slack = 80 * 1024 * 1024;

#if VALIDATE_MEMORY_PROTECTION || MULTIBUFFER_CACHED_LOCK_DETECTION
		Slack = 200 * 1024 * 1024;
#endif

		int64 UsedSize;
		int64 AvailableSize;
		int64 PendingAdjustment;
		int64 PaddingWaste;
		GGnmManager.DefragAllocator.GetMemoryStats(UsedSize, AvailableSize, PendingAdjustment, PaddingWaste);

		static const auto CVarStreamingTexturePoolSize = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Streaming.PoolSize"));
		int64 CVARTexturePoolSize = (int64)CVarStreamingTexturePoolSize->GetValueOnAnyThread() * 1024 * 1024;

		static const auto CVarDefragPoolSize = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PS4DefragPoolSize"));
		int64 CVARDefragPoolSize = (int64)CVarDefragPoolSize->GetInt() * 1024 * 1024;

		OutStats.AllocatedMemorySize = UsedSize;//int64(GCurrentTextureMemorySize) * 1024;
		OutStats.LargestContiguousAllocation = GGnmManager.DefragAllocator.GetLargestAvailableAllocation();
		OutStats.TexturePoolSize = FMath::Min3(CVARTexturePoolSize, CVARDefragPoolSize, GGnmManager.DefragAllocator.GetTotalSize() - Slack); //GTexturePoolSize;
		OutStats.PendingMemoryAdjustment = 0;
	}
	else
#endif
	{
		OutStats.AllocatedMemorySize = int64(GCurrentTextureMemorySize) * 1024;
		OutStats.TexturePoolSize = GTexturePoolSize;
		OutStats.PendingMemoryAdjustment = 0;
	}
}

/**
	* Fills a texture with to visualize the texture pool memory.
	*
	* @param	TextureData		Start address
	* @param	SizeX			Number of pixels along X
	* @param	SizeY			Number of pixels along Y
	* @param	Pitch			Number of bytes between each row
	* @param	PixelSize		Number of bytes each pixel represents
	*
	* @return true if successful, false otherwise
	*/
bool FGnmDynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	return false;
}

uint32 FGnmDynamicRHI::RHIComputeMemorySize(FTextureRHIParamRef TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}

	return GetGnmSurfaceFromRHITexture(TextureRHI).GetMemorySize();
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

/**
* Creates a 2D RHI texture resource
* @param SizeX - width of the texture to create
* @param SizeY - height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
FTexture2DRHIRef FGnmDynamicRHI::RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags,FRHIResourceCreateInfo& Info)
{
	return new FGnmTexture2D((EPixelFormat)Format, SizeX, SizeY, NumMips, NumSamples, Flags, Info.BulkData, Info.ClearValueBinding);
}

FStructuredBufferRHIRef FGnmDynamicRHI::RHICreateRTWriteMaskBuffer(FTexture2DRHIParamRef RenderTarget)
{
	FGnmTexture2D* Texture = ResourceCast(RenderTarget);
	const FGnmSurface& Surface = Texture->Surface;

	FRHIStructuredBuffer *CMaskBuffer = nullptr;
	if (Surface.GetCMaskMem().GetPointer())
	{
		CMaskBuffer = new FGnmStructuredBuffer(1, Surface.GetCMaskMem().GetSize(), (void*)Surface.GetCMaskMem().GetPointer(), 0);
	}

	return CMaskBuffer;
}

FTexture2DRHIRef FGnmDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 Flags,void** InitialMipData,uint32 NumInitialMips)
{
	check(0);
	return FTexture2DRHIRef();
}

void FGnmDynamicRHI::RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D,FTexture2DRHIParamRef SrcTexture2D)
{
	check(0);
}

FTexture2DArrayRHIRef FGnmDynamicRHI::RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FGnmTexture2DArray((EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

FTexture3DRHIRef FGnmDynamicRHI::RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FGnmTexture3D((EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

void FGnmDynamicRHI::RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
}

void FGnmDynamicRHI::RHIGenerateMips(FTextureRHIParamRef SourceSurfaceRHI)
{
	UE_LOG(LogPS4, Fatal, TEXT("RHIGenerateMips is not supported"));
}

static void DoAsyncReallocateTexture2D(FGnmTexture2D* OldTexture, FGnmTexture2D* NewTexture, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandGnmAsyncReallocateTexture2D_Execute);
	FGnmCommandListContext& CmdContext = GGnmManager.GetImmediateContext();
	check(CmdContext.IsImmediate());

	// figure out what mips to copy from/to
	const uint32 NumSharedMips = FMath::Min(OldTexture->GetNumMips(), NewTexture->GetNumMips());
	const uint32 SourceFirstMip = OldTexture->GetNumMips() - NumSharedMips;
	const uint32 DestFirstMip = NewTexture->GetNumMips() - NumSharedMips;

	// get mip offsets of source and dest
	uint64_t SourceMipOffset, DestMipOffset;
	uint64_t MipSize;
	GpuAddress::computeTextureSurfaceOffsetAndSize(&SourceMipOffset, &MipSize, OldTexture->Surface.Texture, SourceFirstMip, Gnm::kCubemapFaceNone);
	const uint8* SourceMipLoc = (uint8*)OldTexture->Surface.Texture->getBaseAddress() + SourceMipOffset;

	GpuAddress::computeTextureSurfaceOffsetAndSize(&DestMipOffset, &MipSize, NewTexture->Surface.Texture, DestFirstMip, Gnm::kCubemapFaceNone);
	uint8* DestMipLoc = (uint8*)NewTexture->Surface.Texture->getBaseAddress() + DestMipOffset;

	// start DMAing old mips to new
	uint64 CopyAmount = FMath::Min(OldTexture->Surface.GetMemorySize(false) - SourceMipOffset, NewTexture->Surface.GetMemorySize(false) - DestMipOffset);
	// @todo gnm: If we make this BlockingDisable, will we need to add logic to FinalizeAsyncReallocateTexture2D?

#if PS4_ENABLE_COMMANDBUFFER_RESERVE
	sce::Gnm::MeasureDrawCommandBuffer MeasureDCB;
	MeasureDCB.resetBuffer();
	Gnmx::copyData(&MeasureDCB, DestMipLoc, SourceMipLoc, CopyAmount, Gnm::kDmaDataBlockingEnable);
	uint32 ReserveSizeInBytes = MeasureDCB.getSizeInBytes();
	CmdContext.GetContext().Reserve(ReserveSizeInBytes);
#endif

	CmdContext.GetContext().copyData(DestMipLoc, SourceMipLoc, CopyAmount, Gnm::kDmaDataBlockingEnable);
	CmdContext.GetContext().CommandBufferReserve();

	// request is now complete
	RequestStatus->Decrement();

	// the next unlock for this texture can't block the GPU (it's during runtime)
	NewTexture->Surface.bSkipBlockOnUnlock = true;
}

struct FRHICommandGnmAsyncReallocateTexture2D final : public FRHICommand<FRHICommandGnmAsyncReallocateTexture2D>
{
	FGnmTexture2D* OldTexture;
	FGnmTexture2D* NewTexture;
	FComputeFenceRHIRef ComputeFence;
	int32 NewMipCount;
	int32 NewSizeX;
	int32 NewSizeY;
	FThreadSafeCounter* RequestStatus;

	FORCEINLINE_DEBUGGABLE FRHICommandGnmAsyncReallocateTexture2D(FGnmTexture2D* InOldTexture, FGnmTexture2D* InNewTexture, FComputeFenceRHIParamRef InComputeFence, int32 InNewMipCount, int32 InNewSizeX, int32 InNewSizeY, FThreadSafeCounter* InRequestStatus)
		: OldTexture(InOldTexture)
		, NewTexture(InNewTexture)
		, ComputeFence(InComputeFence)
		, NewMipCount(InNewMipCount)
		, NewSizeX(InNewSizeX)
		, NewSizeY(InNewSizeY)
		, RequestStatus(InRequestStatus)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		DoAsyncReallocateTexture2D(OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
		if (ComputeFence.GetReference())
		{
			FGnmCommandListContext& CmdContext = GGnmManager.GetImmediateContext();
			check(CmdContext.IsImmediate());
			CmdContext.RHITransitionResources(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToGfx, nullptr, 0, ComputeFence.GetReference());
		}
	}
};

static int32 GPS4FlushForAsyncReallocateTexture2D = 0;
static FAutoConsoleVariableRef CVarPS4FlushForAsyncReallocateTexture2D(
	TEXT("r.PS4FlushForAsyncReallocateTexture2D"),
	GPS4FlushForAsyncReallocateTexture2D,
	TEXT("RHIAsyncReallocateTexture2D flushes"),
	ECVF_Default
	);

static const FName PS4CopyMipFenceName(TEXT("CopyMipData"));
FTexture2DRHIRef FGnmDynamicRHI::AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	if (GPS4FlushForAsyncReallocateTexture2D || RHICmdList.Bypass())
	{
		return FDynamicRHI::AsyncReallocateTexture2D_RenderThread(RHICmdList, OldTextureRHI, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	}
	FGnmTexture2D* OldTexture = ResourceCast(OldTextureRHI);
	FGnmTexture2D* NewTexture = new FGnmTexture2D(OldTexture->GetFormat(), NewSizeX, NewSizeY, NewMipCount, 1, OldTexture->GetFlags(), NULL, OldTextureRHI->GetClearBinding());

	FComputeFenceRHIRef MipCopyFence = nullptr;

#if USE_DEFRAG_ALLOCATOR	
	{
		FScopedGPUDefragLock DefragLock(GGnmManager.DefragAllocator);

		const bool bOldDefraggable = OldTexture->Surface.IsDefraggable();
		const bool bNewDefraggable = NewTexture->Surface.IsDefraggable();
		const bool bEitherDefraggable = bOldDefraggable || bNewDefraggable;

		//we're going to use the GPU to move memory around, so we lock these allocations if they are defraggable to avoid defragging messing up our move.
		
		if (bEitherDefraggable)
		{
			MipCopyFence = RHICreateComputeFence(PS4CopyMipFenceName);
		}

		if (bNewDefraggable)
		{
			GGnmManager.DefragAllocator.LockWithFence(NewTexture->Surface.Texture->getBaseAddress(), MipCopyFence);
		}
		if (bOldDefraggable)
		{
			GGnmManager.DefragAllocator.LockWithFence(OldTexture->Surface.Texture->getBaseAddress(), MipCopyFence);
		}
	}
#endif

	new (RHICmdList.AllocCommand<FRHICommandGnmAsyncReallocateTexture2D>()) FRHICommandGnmAsyncReallocateTexture2D(OldTexture, NewTexture, MipCopyFence, NewMipCount, NewSizeX, NewSizeY, RequestStatus);

	return NewTexture;
}



FTexture2DRHIRef FGnmDynamicRHI::RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	// note this is only called if GPS4FlushForAsyncReallocateTexture2D is true
	FGnmTexture2D* OldTexture = ResourceCast(OldTextureRHI);
	FGnmTexture2D* NewTexture = new FGnmTexture2D(OldTexture->GetFormat(), NewSizeX, NewSizeY, NewMipCount, 1, OldTexture->GetFlags(), NULL, OldTextureRHI->GetClearBinding());

	

	{
		FComputeFenceRHIRef MipCopyFence = nullptr;
#if USE_DEFRAG_ALLOCATOR
		FScopedGPUDefragLock DefragLock(GGnmManager.DefragAllocator);
		const bool bOldDefraggable = OldTexture->Surface.IsDefraggable();
		const bool bNewDefraggable = NewTexture->Surface.IsDefraggable();
		const bool bEitherDefraggable = bOldDefraggable || bNewDefraggable;
		
		if (bEitherDefraggable)
		{
			MipCopyFence = RHICreateComputeFence(PS4CopyMipFenceName);
		}

		if (bNewDefraggable)
		{
			GGnmManager.DefragAllocator.LockWithFence(NewTexture->Surface.Texture->getBaseAddress(), MipCopyFence);
		}
		if (bOldDefraggable)
		{
			GGnmManager.DefragAllocator.LockWithFence(OldTexture->Surface.Texture->getBaseAddress(), MipCopyFence);
		}
#endif

		DoAsyncReallocateTexture2D(OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);

		if (MipCopyFence)
		{
			FGnmCommandListContext& CmdContext = GGnmManager.GetImmediateContext();
			check(CmdContext.IsImmediate());
			CmdContext.RHITransitionResources(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToGfx, nullptr, 0, MipCopyFence);
		}
	}

	
	return NewTexture;
}

ETextureReallocationStatus FGnmDynamicRHI::RHIFinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	FGnmTexture2D* NewTexture = ResourceCast(Texture2D);	
	return TexRealloc_Succeeded;
}

ETextureReallocationStatus FGnmDynamicRHI::RHICancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	FGnmTexture2D* NewTexture = ResourceCast(Texture2D);	
	return TexRealloc_Succeeded;
}


void* FGnmDynamicRHI::RHILockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FGnmTexture2D* Texture = ResourceCast(TextureRHI);
	return Texture->Surface.Lock(MipIndex, 0, LockMode, DestStride);
}

void FGnmDynamicRHI::RHIUnlockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,bool bLockWithinMiptail)
{
	FGnmTexture2D* Texture = ResourceCast(TextureRHI);
	Texture->Surface.Unlock(MipIndex, 0);
}

void* FGnmDynamicRHI::RHILockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FGnmTexture2DArray* Texture = ResourceCast(TextureRHI);
	return Texture->Surface.Lock(MipIndex, TextureIndex, LockMode, DestStride);
}

void FGnmDynamicRHI::RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FGnmTexture2DArray* Texture = ResourceCast(TextureRHI);
	Texture->Surface.Unlock(MipIndex, TextureIndex);
}

void AdjustTextureUpdateSrcFormat(const Gnm::DataFormat& DestDataFormat, Gnm::DataFormat& OutSrcDataFormat, int32& OutNumSrcElements)
{	
	switch (OutSrcDataFormat.m_bits.m_surfaceFormat)
	{
	case Gnm::kBufferFormat8:
	case Gnm::kBufferFormat8_8:
	case Gnm::kBufferFormat8_8_8_8:
		OutSrcDataFormat.m_bits.m_surfaceFormat = Gnm::kBufferFormat8;
		OutNumSrcElements *= DestDataFormat.getNumComponents();
		break;
	case Gnm::kBufferFormat16:
	case Gnm::kBufferFormat16_16:
	case Gnm::kBufferFormat16_16_16_16:
		OutSrcDataFormat.m_bits.m_surfaceFormat = Gnm::kBufferFormat16;
		OutNumSrcElements *= DestDataFormat.getNumComponents();
		break;
	case Gnm::kBufferFormat32:
	case Gnm::kBufferFormat32_32:
	case Gnm::kBufferFormat32_32_32:
	case Gnm::kBufferFormat32_32_32_32:
		OutSrcDataFormat.m_bits.m_surfaceFormat = Gnm::kBufferFormat32;
		OutNumSrcElements *= DestDataFormat.getNumComponents();
		break;
	case Gnm::kBufferFormat10_11_11:
	case Gnm::kBufferFormat11_11_10:
	case Gnm::kBufferFormat10_10_10_2:
	case Gnm::kBufferFormat2_10_10_10:
	default:
		UE_LOG(LogPS4, Warning, TEXT("Unsupported texture update format: %i"), OutSrcDataFormat.m_bits.m_surfaceFormat);
		break;
	}
}

void FGnmDynamicRHI::RHIUpdateTexture2D(FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{	
	check(IsInRenderingThread() || IsInRHIThread());

	FGnmCommandListContext& CmdContext = GGnmManager.GetImmediateContext();	
	CmdContext.PushMarker("UpdateTexture2D");
	FGnmTexture2D* Texture = ResourceCast(TextureRHI);
	const FGnmSurface& Surface = Texture->Surface;

	//protect against the defragger potentially changing the base address on us while locking down the texture.
	//FScopedGPUDefragLock can't cover any scope that will add dcb commands or we might deadlock with a master reserve failure.
	FComputeFenceRHIRef TextureUpdateFence = nullptr;
	{
#if USE_DEFRAG_ALLOCATOR
		FScopedGPUDefragLock DefragLock(GGnmManager.DefragAllocator);
		if (Surface.IsDefraggable())
		{
			static const FName UpdateTexture2DFenceName(TEXT("UpdateTexture2DFence"));
			TextureUpdateFence = RHICreateComputeFence(UpdateTexture2DFenceName);
			GGnmManager.DefragAllocator.LockWithFence(Surface.Texture->getBaseAddress(), TextureUpdateFence);
		}
#endif
	}

	Gnm::DataFormat DestDataFormat = Surface.Texture->getDataFormat();

	//always treat as uint for buffer->texture ops in the shader.  The source texture could be single component float, half floats, srgb, whatever
	//but the compute shader just needs to transfer the bytes without interpreting.
	DestDataFormat.m_bits.m_channelType = Gnm::kTextureChannelTypeUInt;
	DestDataFormat.m_bits.m_channelX = Gnm::kTextureChannelX;
	DestDataFormat.m_bits.m_channelY = Gnm::kTextureChannelY;
	DestDataFormat.m_bits.m_channelZ = Gnm::kTextureChannelZ;
	DestDataFormat.m_bits.m_channelW = Gnm::kTextureChannelW;

	uint32 BytesPerTexel = DestDataFormat.getBitsPerElement() / 8;

	// Allocate some space for GPU to read from (needs to be GPU accessible, unlike SourceData)	
	uint32 DataSize = UpdateRegion.Width * UpdateRegion.Height * BytesPerTexel;
	void* TempBuffer = GGnmManager.AllocateFromTempRingBuffer(DataSize);

	FMemory::Memcpy(TempBuffer, SourceData, DataSize);

	// Setup the compute shader
	auto& Context = CmdContext.GetContext();

	TShaderMapRef<FUpdateTexture2DSubresouceCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();

	CmdContext.RHISetComputeShader(ShaderRHI);

	uint32 DestPosSize[4] = {UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.Width, UpdateRegion.Height};
	
	//shader users a Buffer<uint> to support textures of 1-4 components.  Pitch must be adjusted by # of entries per pixel.
	uint32 SrcPitchSrcComponents[2] = { UpdateRegion.Width * DestDataFormat.getNumComponents(), DestDataFormat.getNumComponents() };

	SetShaderValueOnContext(CmdContext, ShaderRHI, ComputeShader->DestPosSizeParameter, DestPosSize);
	SetShaderValueOnContext(CmdContext, ShaderRHI, ComputeShader->SrcPitchParameter, SrcPitchSrcComponents);


	// Set source buffer to use the same format as the destination
	Gnm::Buffer SourceBuffer;
	Gnm::DataFormat SrcDataFormat = DestDataFormat;

	int32 NumSrcElements = UpdateRegion.Width * UpdateRegion.Height;
	AdjustTextureUpdateSrcFormat(DestDataFormat, SrcDataFormat, NumSrcElements);

	SourceBuffer.initAsDataBuffer(TempBuffer, SrcDataFormat, NumSrcElements);
	SourceBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
	Context.setBuffers(Gnm::kShaderStageCs, ComputeShader->SrcBuffer.GetBaseIndex(), 1, &SourceBuffer);

	// Set destination texture
	DestDataFormat.m_bits.m_channelX = Gnm::kBufferChannelX;
	DestDataFormat.m_bits.m_channelY = Gnm::kBufferChannelY;
	DestDataFormat.m_bits.m_channelZ = Gnm::kBufferChannelZ;
	DestDataFormat.m_bits.m_channelW = Gnm::kBufferChannelW;
	Gnm::Texture DestTextureCopy = *Texture->Surface.Texture;
	DestTextureCopy.setDataFormat( DestDataFormat );
	DestTextureCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.
	Context.setRwTextures(Gnm::kShaderStageCs, ComputeShader->DestTexture.GetBaseIndex(), 1, &DestTextureCopy);

	CmdContext.RHIDispatchComputeShader((UpdateRegion.Width + 7) / 8, (UpdateRegion.Height + 7) / 8, 1);	

	if (TextureUpdateFence)
	{
		CmdContext.RHITransitionResources(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToGfx, nullptr, 0, TextureUpdateFence);
	}
	CmdContext.PopMarker();
}

void FGnmDynamicRHI::RHIUpdateTexture3D_Internal(FTexture3DRHIParamRef TextureRHI, uint32 MipIndex, const FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, void* SourceDataGarlic)
{
	FGnmCommandListContext& CmdContext = GGnmManager.GetImmediateContext();
	CmdContext.PushMarker("UpdateTexture3D");
	const FGnmTexture3D* Texture = ResourceCast(TextureRHI);
	const FGnmSurface& Surface = Texture->Surface;

	Gnm::DataFormat DestDataFormat = Surface.Texture->getDataFormat();

	//always treat as uint for buffer->texture ops in the shader.  The source texture could be single component float, half floats, srgb, whatever
	//but the compute shader just needs to transfer the bytes without interpreting.
	DestDataFormat.m_bits.m_channelType = Gnm::kTextureChannelTypeUInt;
	uint32 BytesPerTexel = DestDataFormat.getBitsPerElement() / 8;

	//protect against the defragger potentially changing the base address on us while locking down the texture.
	//FScopedGPUDefragLock can't cover any scope that will add dcb commands or we might deadlock with a master reserve failure.
	FComputeFenceRHIRef TextureUpdateFence = nullptr;
	{
#if USE_DEFRAG_ALLOCATOR
		FScopedGPUDefragLock DefragLock(GGnmManager.DefragAllocator);
		if (Surface.IsDefraggable())
		{
			static const FName UpdateTexture3DFenceName(TEXT("UpdateTexture3DFence"));
			TextureUpdateFence = RHICreateComputeFence(UpdateTexture3DFenceName);
			GGnmManager.DefragAllocator.LockWithFence(Surface.Texture->getBaseAddress(), TextureUpdateFence);
		}
#endif
	}

	// Setup the compute shader
	auto& Context = CmdContext.GetContext();

	TShaderMapRef<FUpdateTexture3DSubresouceCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();

	CmdContext.RHISetComputeShader(ShaderRHI);

	uint32 DestPos[4] = { (uint32)UpdateRegion.DestX, (uint32)UpdateRegion.DestY, (uint32)UpdateRegion.DestZ, 0 };
	uint32 DestSize[4] = { (uint32)UpdateRegion.Width, (uint32)UpdateRegion.Height, (uint32)UpdateRegion.Depth, 0 };

	SetShaderValueOnContext(CmdContext, ShaderRHI, ComputeShader->DestPosParameter, DestPos);
	SetShaderValueOnContext(CmdContext, ShaderRHI, ComputeShader->DestSizeParameter, DestSize);

	checkf(SourceRowPitch % BytesPerTexel == 0 && SourceDepthPitch % BytesPerTexel == 0,
		TEXT("Padding needs to be in multiple of texels. SourceRowPitch=%d, SourceDepthPitch=%d, BytesPerTexel=%d"),
		SourceRowPitch,
		SourceDepthPitch,
		BytesPerTexel);

	//shader users a Buffer<uint> to support textures of 1-4 components.  Pitch must be adjusted by # of entries per pixel.
	uint32 SrcPitchSrcComponents[2] =
	{
		SourceRowPitch == 0 ? UpdateRegion.Width * DestDataFormat.getNumComponents() : SourceRowPitch / BytesPerTexel * DestDataFormat.getNumComponents(),
		DestDataFormat.getNumComponents()
	};

	SetShaderValueOnContext(CmdContext, ShaderRHI, ComputeShader->SrcPitchParameter, SrcPitchSrcComponents);
	SetShaderValueOnContext(CmdContext, ShaderRHI, ComputeShader->SrcDepthPitchParameter,
		SourceDepthPitch == 0 ?
		SrcPitchSrcComponents[0] * UpdateRegion.Height :
		SourceDepthPitch / BytesPerTexel * DestDataFormat.getNumComponents());

	// Set source buffer to use the same format as the destination
	Gnm::Buffer SourceBuffer;
	Gnm::DataFormat SrcDataFormat = DestDataFormat;

	int32 NumSrcElements = SourceDepthPitch == 0 ?
		UpdateRegion.Width * UpdateRegion.Height * UpdateRegion.Depth :
		SourceDepthPitch * UpdateRegion.Depth / BytesPerTexel;
	AdjustTextureUpdateSrcFormat(DestDataFormat, SrcDataFormat, NumSrcElements);

	SourceBuffer.initAsDataBuffer(SourceDataGarlic, SrcDataFormat, NumSrcElements);
	SourceBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
	Context.setBuffers(Gnm::kShaderStageCs, ComputeShader->SrcBuffer.GetBaseIndex(), 1, &SourceBuffer);

	// Set destination texture
	DestDataFormat.m_bits.m_channelX = Gnm::kBufferChannelX;
	DestDataFormat.m_bits.m_channelY = Gnm::kBufferChannelY;
	DestDataFormat.m_bits.m_channelZ = Gnm::kBufferChannelZ;
	DestDataFormat.m_bits.m_channelW = Gnm::kBufferChannelW;
	Gnm::Texture DestTextureCopy = *Texture->Surface.Texture;
	DestTextureCopy.setDataFormat(DestDataFormat);
	DestTextureCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.
	Context.setRwTextures(Gnm::kShaderStageCs, ComputeShader->DestTexture3D.GetBaseIndex(), 1, &DestTextureCopy);

	CmdContext.RHIDispatchComputeShader((UpdateRegion.Width + 7) / 8, (UpdateRegion.Height + 7) / 8, UpdateRegion.Depth);
	if (TextureUpdateFence)
	{
		CmdContext.RHITransitionResources(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToGfx, nullptr, 0, TextureUpdateFence);
	}
	CmdContext.PopMarker();
}

void FGnmDynamicRHI::RHIUpdateTexture3D(FTexture3DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion3D& UpdateRegion,uint32 SourceRowPitch,uint32 SourceDepthPitch,const uint8* SourceData)
{
	const FGnmTexture3D* Texture = ResourceCast(TextureRHI);
	const FGnmSurface& Surface = Texture->Surface;

	Gnm::DataFormat DestDataFormat = Surface.Texture->getDataFormat();
	
	//always treat as uint for buffer->texture ops in the shader.  The source texture could be single component float, half floats, srgb, whatever
	//but the compute shader just needs to transfer the bytes without interpreting.
	DestDataFormat.m_bits.m_channelType = Gnm::kTextureChannelTypeUInt;

	// allocate some space to DMA from (needs to be GPU accessible, unlike SourceData)	
	uint32 DataSize = SourceDepthPitch == 0 ?
		UpdateRegion.Width * UpdateRegion.Height * UpdateRegion.Depth * DestDataFormat.getBitsPerElement() / 8 :
		SourceDepthPitch * UpdateRegion.Depth;
	void* TempBuffer = GGnmManager.AllocateFromTempRingBuffer(DataSize, FGnmManager::ETempRingBufferAllocateMode::AllowFailure);
	
	FMemBlock MemBlock;
	if (!TempBuffer)
	{
		// If we can't allocate from the temporary store, fall back to main heap.
		MemBlock = FMemBlock::Allocate(DataSize, DEFAULT_VIDEO_ALIGNMENT, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_Texture));
		TempBuffer = MemBlock.GetPointer();
	}

	// move data over
	FMemory::Memcpy(TempBuffer, SourceData, DataSize);

	RHIUpdateTexture3D_Internal(TextureRHI, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, TempBuffer);

	if (MemBlock.GetPointer())
	{
		// Delayed free (puts the block on the queue to be freed on the next frame)
		FMemBlock::Free(MemBlock);
	}
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FGnmDynamicRHI::RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FGnmTextureCube((EPixelFormat)Format, Size, false, 1, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

FTextureCubeRHIRef FGnmDynamicRHI::RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FGnmTextureCube((EPixelFormat)Format, Size, true, ArraySize, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

void* FGnmDynamicRHI::RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FGnmTextureCube* TextureCube = ResourceCast(TextureCubeRHI);
	uint32 GnmFace = GetGnmCubeFace((ECubeFace)FaceIndex);
	return TextureCube->Surface.Lock(MipIndex, FaceIndex + 6 * ArrayIndex, LockMode, DestStride);
}

void FGnmDynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FGnmTextureCube* TextureCube = ResourceCast(TextureCubeRHI);
	uint32 GnmFace = GetGnmCubeFace((ECubeFace)FaceIndex);
	TextureCube->Surface.Unlock(MipIndex, FaceIndex + ArrayIndex * 6);
}

void FGnmDynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
	//todo: require names at texture creation time.
	FName DebugName(Name);
	TextureRHI->SetName(DebugName);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(TextureRHI);
	sce::Gnm::registerResource(nullptr, GGnmManager.GetOwnerHandle(), Surface.Texture->getBaseAddress(), Surface.GetMemorySize(false), TCHAR_TO_ANSI(Name), sce::Gnm::kResourceTypeTextureBaseAddress, 0);

	if (Surface.ColorBuffer)
	{
		sce::Gnm::registerResource(nullptr, GGnmManager.GetOwnerHandle(), Surface.ColorBuffer->getBaseAddress(), Surface.GetMemorySize(false), TCHAR_TO_ANSI(Name), sce::Gnm::kResourceTypeRenderTargetBaseAddress, 0);

		if (Surface.ColorBuffer->getCmaskFastClearEnable())
		{
			sce::Gnm::registerResource(nullptr, GGnmManager.GetOwnerHandle(), Surface.ColorBuffer->getCmaskAddress(), Surface.ColorBuffer->getCmaskSliceSizeInBytes(), TCHAR_TO_ANSI(Name), sce::Gnm::kResourceTypeRenderTargetCMaskAddress, 0);
		}
	}
	if (Surface.DepthBuffer)
	{
		sce::Gnm::registerResource(nullptr, GGnmManager.GetOwnerHandle(), Surface.DepthBuffer->getZWriteAddress(), Surface.GetMemorySize(false), TCHAR_TO_ANSI(Name), sce::Gnm::kResourceTypeDepthRenderTargetBaseAddress, 0);

		if (Surface.DepthBuffer->getStencilSliceSizeInBytes() > 0)
		{
			sce::Gnm::registerResource(nullptr, GGnmManager.GetOwnerHandle(), Surface.DepthBuffer->getStencilWriteAddress(), Surface.DepthBuffer->getStencilSliceSizeInBytes(), TCHAR_TO_ANSI(Name), sce::Gnm::kResourceTypeDepthRenderTargetStencilAddress, 0);
		}
	}
#endif
}

void FGnmDynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	// TODO: Implement for PS4
	UE_LOG(LogPS4, Fatal, TEXT("RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip) needs to be implemented"));
}

void FGnmDynamicRHI::RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	// TODO: Implement for PS4
	UE_LOG(LogPS4, Fatal, TEXT("RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip) needs to be implemented"));
}

FTextureReferenceRHIRef FGnmDynamicRHI::RHICreateTextureReference(FLastRenderTimeContainer* LastRenderTime)
{
	return new FGnmTextureReference(LastRenderTime);
}

void FGnmCommandListContext::RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRefRHI, FTextureRHIParamRef NewTextureRHI)
{
	//check(IsImmediate());  // doing this in a parallel translate would likely be bad
	// Updating texture references is disallowed while the RHI could be caching them in referenced resource tables.
	check(GGnmManager.GetResourceTableFrameCounter() == INDEX_NONE);

	FGnmTextureReference* TextureRef = (FGnmTextureReference*)TextureRefRHI;
	if (TextureRef)
	{
		TextureRef->SetReferencedTexture(NewTextureRHI);
	}
}
