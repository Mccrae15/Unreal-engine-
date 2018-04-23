// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "GnmRHIPrivate.h"
#include "GrowableAllocator.h"

FGnmShaderResourceView::~FGnmShaderResourceView()
{
	SourceVertexBuffer = nullptr;
	SourceTexture = nullptr;
	SourceStructuredBuffer = nullptr;
}

FGnmUnorderedAccessView::FGnmUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBufferRHI, bool bInUseUAVCounter, bool bInAppendBuffer)
{	
	FGnmStructuredBuffer* StructuredBuffer = FGnmDynamicRHI::ResourceCast(StructuredBufferRHI);
	SourceStructuredBuffer = StructuredBuffer;
	DMAValueTarget = nullptr;

	// simply copy the buffer contents over
	FMemory::Memcpy(&Buffer, &StructuredBuffer->Buffer, sizeof(Buffer));	

	// todo, find out if we really need to keep two counters if these are both set.
	bUseUAVCounters = bInUseUAVCounter || bInAppendBuffer;
	
	if (bUseUAVCounters)
	{
		// assume 4 byte components
		NumComponents = StructuredBuffer->GetStride() / 4;
		DMAValueTargetBlock = FMemBlock::Allocate(4, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_UavCounters));
		DMAValueTarget = (int32*)DMAValueTargetBlock.GetPointer();
		*DMAValueTarget = 0;
	}
}

FGnmUnorderedAccessView::FGnmUnorderedAccessView(FTextureRHIParamRef TextureRHI, uint32 MipLevel)
{	
	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(TextureRHI);
	SourceTexture = (FRHITexture*)TextureRHI;

	// we will jsut use the source texture's Texture object later, no need to initialize anything new
	Gnm::DataFormat PlatformFormat = GGnmManager.GetDataFormat(SourceTexture->GetFormat());
	NumComponents = PlatformFormat.getNumComponents();
	SourceTextureMipLevel = MipLevel;
	DMAValueTarget = nullptr;
	bUseUAVCounters = false;
}

FGnmUnorderedAccessView::FGnmUnorderedAccessView(FVertexBufferRHIParamRef VertexBufferRHI, uint8 Format)
{
	FGnmVertexBuffer* VertexBuffer = FGnmDynamicRHI::ResourceCast(VertexBufferRHI);
	
	SourceVertexBuffer = VertexBuffer;

	// figure out the Gnm format identifier
	Gnm::DataFormat PlatformFormat = GGnmManager.GetDataFormat((EPixelFormat)Format);

	// get the size of an element of this size
	uint32 ElementSize = PlatformFormat.getBitsPerElement() / 8;

	// make sure the buffer is a multiple of this size
	check((VertexBuffer->GetSize() % ElementSize) == 0);
	Buffer.initAsDataBuffer(VertexBuffer->GetCurrentBuffer(), PlatformFormat, VertexBuffer->GetSize() / ElementSize);

	// get number of 4 byte components
	NumComponents = PlatformFormat.getNumComponents();
	DMAValueTarget = nullptr;
	bUseUAVCounters = false;
}

void FGnmUnorderedAccessView::UpdateBufferDescriptor()
{
	if (IsValidRef(SourceVertexBuffer))
	{
		Buffer.initAsDataBuffer(SourceVertexBuffer->GetCurrentBuffer(), Buffer.getDataFormat(), Buffer.getNumElements());
	}
}

FGnmUnorderedAccessView::~FGnmUnorderedAccessView()
{		
	//clearing the boundUAV when it goes out of scope is a key lifetime hint that tells us whether we need to maintain an append/consume counter
	//with expensive flushes for this object.  However, we can't reliably do this in parallel.  For now, we don't support append/consume buffers
	//with parallel rhi.
#if SUPPORTS_APPEND_CONSUME_BUFFERS
	GGnmManager.GetImmediateContext().ClearBoundUAV(this);
#endif

	if (DMAValueTarget)
	{
		FMemBlock::Free(DMAValueTargetBlock);
		DMAValueTarget = nullptr;
	}	
}

void FGnmUnorderedAccessView::Set(FGnmCommandListContext& GnmCommandContext, Gnm::ShaderStage Stage, uint32 ResourceIndex, bool bUploadValue)
{
	GnmContextType& Context = GnmCommandContext.GetContext();

	// set the UAV buffer into the given slot
	if (IsValidRef(SourceTexture))
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(SourceTexture);
		Gnm::Texture TextureCopy = *Surface.Texture;
		TextureCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.
		TextureCopy.setMipLevelRange(SourceTextureMipLevel, SourceTextureMipLevel);
		Context.setRwTextures(Stage, ResourceIndex, 1, &TextureCopy);
	}
	else 
	{
		// update the address in case it's dynamic
		UpdateBufferDescriptor();
		Buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.
		Context.setRwBuffers(Stage, ResourceIndex, 1, &Buffer);

#if SUPPORTS_APPEND_CONSUME_BUFFERS
		if (bUploadValue && bUseUAVCounters)
		{						
			check(DMAValueTarget != nullptr);
			Context.writeAppendConsumeCounters(0, ResourceIndex, 1, DMAValueTarget);
		}
#endif
	}
	TrackResource(sce::Shader::Binary::kInternalBufferTypeUav, ResourceIndex, Stage);
}

void FGnmUnorderedAccessView::Set(FGnmComputeCommandListContext& GnmComputeContext, uint32 ResourceIndex, bool bUploadValue)
{
	Gnmx::ComputeContext& Context = GnmComputeContext.GetContext();

	// set the UAV buffer into the given slot
	if (IsValidRef(SourceTexture))
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(SourceTexture);
		Gnm::Texture TextureCopy = *Surface.Texture;
		TextureCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.
		TextureCopy.setMipLevelRange(SourceTextureMipLevel, SourceTextureMipLevel);
		Context.setRwTextures(ResourceIndex, 1, &TextureCopy);
	}
	else
	{
		// update the address in case it's dynamic
		if (IsValidRef(SourceVertexBuffer))
		{
			Buffer.initAsDataBuffer(SourceVertexBuffer->GetCurrentBuffer(), Buffer.getDataFormat(), Buffer.getNumElements());
		}
		Buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.
		Context.setRwBuffers(ResourceIndex, 1, &Buffer);

#if SUPPORTS_APPEND_CONSUME_BUFFERS
		if (bUploadValue && bUseUAVCounters)
		{
			check(DMAValueTarget != nullptr);
			Context.writeAppendConsumeCounters(0, ResourceIndex, 1, DMAValueTarget);
		}
#endif
	}	
}

void FGnmUnorderedAccessView::SetResourceAccess(EResourceTransitionAccess InAccess)
{
	if (IsValidRef(SourceTexture))
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(SourceTexture);
		Surface.SetCurrentGPUAccess(InAccess);
	}
	else if (IsValidRef(SourceStructuredBuffer))
	{
		SourceStructuredBuffer->SetCurrentGPUAccess(InAccess);
	}
	else
	{
		check(IsValidRef(SourceVertexBuffer));
		SourceVertexBuffer->SetCurrentGPUAccess(InAccess);
	}
}

EResourceTransitionAccess FGnmUnorderedAccessView::GetResourceAccess() const
{
	if (IsValidRef(SourceTexture))
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(SourceTexture);
		return Surface.GetCurrentGPUAccess();
	}
	else if (IsValidRef(SourceStructuredBuffer))
	{
		return SourceStructuredBuffer->GetCurrentGPUAccess();
	}
	else
	{
		check(IsValidRef(SourceVertexBuffer));
		return SourceVertexBuffer->GetCurrentGPUAccess();
	}
	return EResourceTransitionAccess::EReadable;
}

void FGnmUnorderedAccessView::SetResourceDirty(bool bDirty, uint32 CurrentFrame)
{
	if (IsValidRef(SourceTexture))
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(SourceTexture);
		Surface.SetDirty(bDirty, CurrentFrame);
	}
	else if (IsValidRef(SourceStructuredBuffer))
	{
		SourceStructuredBuffer->SetDirty(bDirty, CurrentFrame);
	}
	else
	{
		check(IsValidRef(SourceVertexBuffer));
		SourceVertexBuffer->SetDirty(bDirty, CurrentFrame);
	}
}

bool FGnmUnorderedAccessView::IsResourceDirty() const
{
	if (IsValidRef(SourceTexture))
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(SourceTexture);
		return Surface.IsDirty();
	}
	else if (IsValidRef(SourceStructuredBuffer))
	{
		return SourceStructuredBuffer->IsDirty();
	}
	else
	{
		check(IsValidRef(SourceVertexBuffer));
		return SourceVertexBuffer->IsDirty();
	}
}

void FGnmUnorderedAccessView::SetAndClearCounter(FGnmCommandListContext& GnmCommandContext, Gnm::ShaderStage Stage, uint32 ResourceIndex, uint32 CounterVal)
{
	Set(GnmCommandContext, Stage, ResourceIndex, false);
	
#if SUPPORTS_APPEND_CONSUME_BUFFERS
	if (bUseUAVCounters)
	{	
		GnmContextType& Context = GnmCommandContext.GetContext();
		Context.clearAppendConsumeCounters(0, ResourceIndex, 1, CounterVal);		
	}
#endif
}

void FGnmUnorderedAccessView::StoreCurrentCounterValue(FGnmCommandListContext& GnmCommandContext, uint32 ResourceIndex)
{		
#if SUPPORTS_APPEND_CONSUME_BUFFERS
	if (bUseUAVCounters)
	{			
		// can only read the finalvalue after the shaders modifying the counter are done.
		GnmContextType& Context = GnmCommandContext.GetContext();
		check(DMAValueTarget != nullptr);
		Context.readAppendConsumeCounters(DMAValueTarget, 0, ResourceIndex, 1);		
	}
#endif
}

void FGnmShaderResourceView::SetResourceAccess(EResourceTransitionAccess InAccess)
{
	if (IsValidRef(SourceTexture))
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(SourceTexture);
		Surface.SetCurrentGPUAccess(InAccess);
	}	
	else if(IsValidRef(SourceVertexBuffer))
	{
		SourceVertexBuffer->SetCurrentGPUAccess(InAccess);
	}
	else if (IsValidRef(SourceStructuredBuffer))
	{
		SourceStructuredBuffer->SetCurrentGPUAccess(InAccess);
	}
}

EResourceTransitionAccess FGnmShaderResourceView::GetResourceAccess() const
{
	if (IsValidRef(SourceTexture))
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(SourceTexture);
		return Surface.GetCurrentGPUAccess();
	}	
	else if (IsValidRef(SourceVertexBuffer))
	{
		return SourceVertexBuffer->GetCurrentGPUAccess();
	}
	else if (IsValidRef(SourceStructuredBuffer))
	{
		return SourceStructuredBuffer->GetCurrentGPUAccess();
	}
	return EResourceTransitionAccess::EReadable;
}

uint32 FGnmShaderResourceView::GetLastFrameWritten()
{
	if (IsValidRef(SourceTexture))
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(SourceTexture);
		return Surface.GetLastFrameWritten();
	}
	else if (IsValidRef(SourceVertexBuffer))
	{
		return SourceVertexBuffer->GetLastFrameWritten();
	}
	else if (IsValidRef(SourceStructuredBuffer))
	{
		return SourceStructuredBuffer->GetLastFrameWritten();
	}

	return -1;
}

bool FGnmShaderResourceView::IsResourceDirty() const
{
	if (IsValidRef(SourceTexture))
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(SourceTexture);
		return Surface.IsDirty();
	}
	else if (IsValidRef(SourceVertexBuffer))
	{
		return SourceVertexBuffer->IsDirty();
	}
	else if (IsValidRef(SourceStructuredBuffer))
	{
		return SourceStructuredBuffer->IsDirty();
	}

	return false;
}

FUnorderedAccessViewRHIRef FGnmDynamicRHI::RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	// create the UAV buffer to point to the structured buffer's memory
	FGnmUnorderedAccessView* UAV = new FGnmUnorderedAccessView(StructuredBufferRHI, bUseUAVCounter, bAppendBuffer);	
	return UAV;
}

FUnorderedAccessViewRHIRef FGnmDynamicRHI::RHICreateUnorderedAccessView(FTextureRHIParamRef TextureRHI, uint32 MipLevel)
{
	// create the UAV buffer to point to the texture's memory
	FGnmUnorderedAccessView* UAV = new FGnmUnorderedAccessView(TextureRHI, MipLevel);
	return UAV;
}

FUnorderedAccessViewRHIRef FGnmDynamicRHI::RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBufferRHI, uint8 Format)
{	
	// create the UAV buffer to point to the vertex buffer's memory
	FGnmUnorderedAccessView* UAV = new FGnmUnorderedAccessView(VertexBufferRHI, Format);
	return UAV;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	FGnmStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;
	SRV->SourceStructuredBuffer = StructuredBuffer;

	// simply copy the buffer contents over
	FMemory::Memcpy(&SRV->Buffer, &StructuredBuffer->Buffer, sizeof(SRV->Buffer));
	return SRV;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint8 Format)
{
	FGnmVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;
	
	// calculate how many elements are in the buffer
	uint32 NumElements = VertexBuffer->GetSize() / Stride;

	SRV->SourceVertexBuffer = VertexBuffer;

	// initialize the buffer with the given format
	// note: we pass true to GetCurrentBuffer to make sure that memory is allocated for zero buffer resources
	SRV->Buffer.initAsDataBuffer(VertexBuffer->GetCurrentBuffer(), GGnmManager.GetDataFormat((EPixelFormat)Format), NumElements);

	// in case the stride != sizeof(Format)
	SRV->Buffer.setStride(Stride);

	return SRV;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FIndexBufferRHIParamRef BufferRHI)
{
	UE_LOG(LogRHI, Fatal, TEXT("PS4 RHI doesn't support RHICreateShaderResourceView with FIndexBufferRHIParamRef yet!"));
	
	return FShaderResourceViewRHIRef();
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;

	// Build our view of the texture
	SRV->SourceTexture = (FRHITexture*)Texture2DRHI;
	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture( SRV->SourceTexture );

	SRV->Texture = *Surface.Texture;
	SRV->Texture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
	SRV->Texture.setMipLevelRange(MipLevel, MipLevel);

	return SRV;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;

	Gnm::DataFormat PlatformFormat = GGnmManager.GetDataFormat((EPixelFormat)Format, Texture2DRHI->GetFlags() & TexCreate_SRGB);

	// Build our view of the texture
	SRV->SourceTexture = (FRHITexture*)Texture2DRHI;
	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture( SRV->SourceTexture );

	//HACK.  DX11 on PC currently uses a D24S8 depthbuffer and so needs an X24_G8 SRV to visualize stencil.
	//So take that as our cue to visualize stencil.  In the future, the platform independent code will have a real format
	//instead of PF_DepthStencil, so the cross-platform code could figure out the proper format to pass in for this.
	bool bStencil = Surface.DepthBuffer != nullptr && Format == PF_X24_G8;	
	if (bStencil)
	{
		void* StencilBaseAddress = Surface.DepthBuffer->getStencilReadAddress();
		
		GpuAddress::TilingParameters TilingParams;
		int32 InitStatus = TilingParams.initFromStencilSurface(Surface.DepthBuffer, 0);
		check(InitStatus == GpuAddress::kStatusSuccess);		

		SRV->Texture.initFromStencilTarget(Surface.DepthBuffer, Gnm::kTextureChannelTypeUInt, false);
		SRV->Texture.setBaseAddress(StencilBaseAddress);
	}
	else
	{
		SRV->Texture = *Surface.Texture;		
		SRV->Texture.setMipLevelRange( MipLevel, MipLevel );
	}
			
	SRV->Texture.setResourceMemoryType( Gnm::kResourceMemoryTypeRO );

	return SRV;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;

	// Build our view of the texture
	SRV->SourceTexture = (FRHITexture*)Texture3DRHI;
	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture( SRV->SourceTexture );

	SRV->Texture = *Surface.Texture;
	SRV->Texture.setResourceMemoryType( Gnm::kResourceMemoryTypeRO );
	SRV->Texture.setMipLevelRange( MipLevel, MipLevel );

	return SRV;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
{
	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;

	// Build our view of the texture
	SRV->SourceTexture = (FRHITexture*)Texture2DArrayRHI;
	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture( SRV->SourceTexture );

	SRV->Texture = *Surface.Texture;
	SRV->Texture.setResourceMemoryType( Gnm::kResourceMemoryTypeRO );
	SRV->Texture.setMipLevelRange( MipLevel, MipLevel );

	return SRV;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;

	// Build our view of the texture
	SRV->SourceTexture = (FRHITexture*)TextureCubeRHI;
	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture( SRV->SourceTexture );

	SRV->Texture = *Surface.Texture;
	SRV->Texture.setResourceMemoryType( Gnm::kResourceMemoryTypeRO );
	SRV->Texture.setMipLevelRange( MipLevel, MipLevel );

	return SRV;
}

void FGnmCommandListContext::RHIClearTinyUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values)
{
	FGnmUnorderedAccessView* UnorderedAccessView = ResourceCast(UnorderedAccessViewRHI);
	UnorderedAccessView->UpdateBufferDescriptor();

	if ((UnorderedAccessView->SourceVertexBuffer && UnorderedAccessView->SourceVertexBuffer->GetUsage() & BUF_DrawIndirect)
		|| (UnorderedAccessView->SourceStructuredBuffer && UnorderedAccessView->SourceStructuredBuffer->GetUsage() & BUF_DrawIndirect))
	{
		//this UAV is used for a DrawIndirect, so we must stall the pre-fetcher to make sure it doesn't cache bad values for the upcoming draw call.
		GetContext().stallCommandBufferParser();
	}

		if (UnorderedAccessView->NumComponents == 1)
		{
			GetContext().fillData(UnorderedAccessView->Buffer.getBaseAddress(), Values[0], UnorderedAccessView->Buffer.getSize(), Gnm::kDmaDataBlockingEnable);
		}
		else
		{
			// we can do a memset if all values are the same
			if (Values[0] == Values[1] && Values[1] == Values[2] && Values[2] == Values[3])
			{
				GetContext().fillData(UnorderedAccessView->Buffer.getBaseAddress(), Values[0], UnorderedAccessView->Buffer.getSize(), Gnm::kDmaDataBlockingEnable);
			}
			else
			{
				UE_LOG(LogPS4, Fatal, TEXT("For multi-component UAVs, the we can only ClearUAV to all the same values, wihtout a compute shader"));
			}
		}
	}

void FGnmDynamicRHI::RHIBindDebugLabelName(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const TCHAR* Name)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FGnmUnorderedAccessView* UnorderedAccessView = ResourceCast(UnorderedAccessViewRHI);
	sce::Gnm::registerResource(nullptr, GGnmManager.GetOwnerHandle(), UnorderedAccessView->Buffer.getBaseAddress(), UnorderedAccessView->Buffer.getSize(), TCHAR_TO_ANSI(Name), sce::Gnm::kResourceTypeBufferBaseAddress, 0);
#endif
}
