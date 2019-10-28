// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "GnmRHIPrivate.h"
#include "GrowableAllocator.h"

FGnmShaderResourceView::~FGnmShaderResourceView()
{
	Clear();
}

FGnmUnorderedAccessView::FGnmUnorderedAccessView(FRHIStructuredBuffer* StructuredBufferRHI, bool bInUseUAVCounter, bool bInAppendBuffer)
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

FGnmUnorderedAccessView::FGnmUnorderedAccessView(FRHITexture* TextureRHI, uint32 MipLevel)
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

FGnmUnorderedAccessView::FGnmUnorderedAccessView(FRHIVertexBuffer* VertexBufferRHI, uint8 Format)
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

FGnmUnorderedAccessView::FGnmUnorderedAccessView(FRHIIndexBuffer* IndexBufferRHI, uint8 Format)
{
	FGnmIndexBuffer* IndexBuffer = FGnmDynamicRHI::ResourceCast(IndexBufferRHI);

	SourceIndexBuffer = IndexBuffer;

	// figure out the Gnm format identifier
	Gnm::DataFormat PlatformFormat = GGnmManager.GetDataFormat((EPixelFormat)Format);

	// get the size of an element of this size
	uint32 ElementSize = PlatformFormat.getBitsPerElement() / 8;

	// make sure the buffer is a multiple of this size
	check((IndexBuffer->GetSize() % ElementSize) == 0);
	Buffer.initAsDataBuffer(IndexBuffer->GetCurrentBuffer(), PlatformFormat, IndexBuffer->GetSize() / ElementSize);

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
	else if (IsValidRef(SourceIndexBuffer))
	{
		Buffer.initAsDataBuffer(SourceIndexBuffer->GetCurrentBuffer(), Buffer.getDataFormat(), Buffer.getNumElements());
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
		else if (IsValidRef(SourceIndexBuffer))
		{
			Buffer.initAsDataBuffer(SourceIndexBuffer->GetCurrentBuffer(), Buffer.getDataFormat(), Buffer.getNumElements());
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
	else if (IsValidRef(SourceVertexBuffer))
	{
		SourceVertexBuffer->SetCurrentGPUAccess(InAccess);
	}
	else
	{
		check(IsValidRef(SourceIndexBuffer));
		SourceIndexBuffer->SetCurrentGPUAccess(InAccess);
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
	else if (IsValidRef(SourceVertexBuffer))
	{
		return SourceVertexBuffer->GetCurrentGPUAccess();
	}
	else
	{
		check(IsValidRef(SourceIndexBuffer));
		return SourceIndexBuffer->GetCurrentGPUAccess();
	}
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
	else if (IsValidRef(SourceVertexBuffer))
	{
		SourceVertexBuffer->SetDirty(bDirty, CurrentFrame);
	}
	else
	{
		check(IsValidRef(SourceIndexBuffer));
		SourceIndexBuffer->SetDirty(bDirty, CurrentFrame);
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
	else if (IsValidRef(SourceVertexBuffer))
	{
		return SourceVertexBuffer->IsDirty();
	}
	else
	{
		check(IsValidRef(SourceIndexBuffer));
		return SourceIndexBuffer->IsDirty();
	}
}

uint32 FGnmUnorderedAccessView::GetLastFrameWritten() const
{
	if (IsValidRef(SourceTexture))
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(SourceTexture);
		return Surface.GetLastFrameWritten();
	}
	else if (IsValidRef(SourceStructuredBuffer))
	{
		return SourceStructuredBuffer->GetLastFrameWritten();
	}
	else if (IsValidRef(SourceVertexBuffer))
	{
		return SourceVertexBuffer->GetLastFrameWritten();
	}
	else
	{
		check(IsValidRef(SourceIndexBuffer));
		return SourceIndexBuffer->GetLastFrameWritten();
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
	else if(IsValidRef(SourceMultiBufferRHI))
	{
		SourceMultiBufferResource->SetCurrentGPUAccess(InAccess);
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
	else if (IsValidRef(SourceMultiBufferRHI))
	{
		return SourceMultiBufferResource->GetCurrentGPUAccess();
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
	else if (IsValidRef(SourceMultiBufferRHI))
	{
		return SourceMultiBufferResource->GetLastFrameWritten();
	}
	else if (IsValidRef(SourceStructuredBuffer))
	{
		return SourceStructuredBuffer->GetLastFrameWritten();
	}

	return -1;
}

void FGnmShaderResourceView::Clear()
{
	SourceMultiBufferRHI = nullptr;
	SourceMultiBufferResource = nullptr;
	SourceTexture = nullptr;
	SourceStructuredBuffer = nullptr;
}

bool FGnmShaderResourceView::IsResourceDirty() const
{
	if (IsValidRef(SourceTexture))
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(SourceTexture);
		return Surface.IsDirty();
	}
	else if (IsValidRef(SourceMultiBufferRHI))
	{
		return SourceMultiBufferResource->IsDirty();
	}
	else if (IsValidRef(SourceStructuredBuffer))
	{
		return SourceStructuredBuffer->IsDirty();
	}

	return false;
}

FUnorderedAccessViewRHIRef FGnmDynamicRHI::RHICreateUnorderedAccessView(FRHIStructuredBuffer* StructuredBufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	// create the UAV buffer to point to the structured buffer's memory
	FGnmUnorderedAccessView* UAV = new FGnmUnorderedAccessView(StructuredBufferRHI, bUseUAVCounter, bAppendBuffer);	
	return UAV;
}

FUnorderedAccessViewRHIRef FGnmDynamicRHI::RHICreateUnorderedAccessView(FRHITexture* TextureRHI, uint32 MipLevel)
{
	// create the UAV buffer to point to the texture's memory
	FGnmUnorderedAccessView* UAV = new FGnmUnorderedAccessView(TextureRHI, MipLevel);
	return UAV;
}

FUnorderedAccessViewRHIRef FGnmDynamicRHI::RHICreateUnorderedAccessView(FRHIVertexBuffer* VertexBufferRHI, uint8 Format)
{	
	// create the UAV buffer to point to the vertex buffer's memory
	FGnmUnorderedAccessView* UAV = new FGnmUnorderedAccessView(VertexBufferRHI, Format);
	return UAV;
}

FUnorderedAccessViewRHIRef FGnmDynamicRHI::RHICreateUnorderedAccessView(FRHIIndexBuffer* IndexBufferRHI, uint8 Format)
{
	// create the UAV buffer to point to the index buffer's memory
	FGnmUnorderedAccessView* UAV = new FGnmUnorderedAccessView(IndexBufferRHI, Format);
	return UAV;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FRHIStructuredBuffer* StructuredBufferRHI)
{
	FGnmStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;
	SRV->SourceStructuredBuffer = StructuredBuffer;

	// simply copy the buffer contents over
	FMemory::Memcpy(&SRV->Buffer, &StructuredBuffer->Buffer, sizeof(SRV->Buffer));
	return SRV;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FRHIVertexBuffer* VertexBufferRHI, uint32 Stride, uint8 Format)
{
	FGnmVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;
	
	if (VertexBuffer)
	{
		// calculate how many elements are in the buffer
		uint32 NumElements = VertexBuffer->GetSize() / Stride;

		SRV->SourceMultiBufferRHI = VertexBuffer;
		SRV->SourceMultiBufferResource = VertexBuffer;

		// initialize the buffer with the given format
		// note: we pass true to GetCurrentBuffer to make sure that memory is allocated for zero buffer resources
		SRV->Buffer.initAsDataBuffer(VertexBuffer->GetCurrentBuffer(), GGnmManager.GetDataFormat((EPixelFormat)Format), NumElements);

		// in case the stride != sizeof(Format)
		SRV->Buffer.setStride(Stride);
	}
	return SRV;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FRHIIndexBuffer* IndexBufferRHI)
{
	FGnmIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;

	if (IndexBuffer)
	{
		uint32 Stride = IndexBuffer->IndexSize == Gnm::kIndexSize16 ? 2 : 4;
		EPixelFormat Format = IndexBuffer->IndexSize == Gnm::kIndexSize16 ? PF_R16_UINT : PF_R32_UINT;

		// calculate how many elements are in the buffer
		uint32 NumElements = IndexBuffer->GetSize() / Stride;

		SRV->SourceMultiBufferRHI = IndexBuffer;
		SRV->SourceMultiBufferResource = IndexBuffer;

		// initialize the buffer with the given format
		// note: we pass true to GetCurrentBuffer to make sure that memory is allocated for zero buffer resources
		SRV->Buffer.initAsDataBuffer(IndexBuffer->GetCurrentBuffer(), GGnmManager.GetDataFormat((EPixelFormat)Format), NumElements);

		// in case the stride != sizeof(Format)
		SRV->Buffer.setStride(Stride);
	}
	return SRV;
}

void FGnmDynamicRHI::RHIUpdateShaderResourceView(FRHIShaderResourceView* SRV, FRHIVertexBuffer* VertexBuffer, uint32 Stride, uint8 Format)
{
	check(SRV);
	FGnmShaderResourceView* SRVGnm = ResourceCast(SRV);
	SRVGnm->Clear();
	if (VertexBuffer)
	{
		FGnmVertexBuffer* VBGnm = ResourceCast(VertexBuffer);
		const uint32 NumElements = VBGnm->GetSize() / Stride;
		SRVGnm->SourceMultiBufferRHI = VBGnm;
		SRVGnm->SourceMultiBufferResource = VBGnm;
		SRVGnm->Buffer.initAsDataBuffer(VBGnm->GetCurrentBuffer(), GGnmManager.GetDataFormat((EPixelFormat)Format), NumElements);
		SRVGnm->Buffer.setStride(Stride);
	}
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FRHITexture* Texture, const FRHITextureSRVCreateInfo& CreateInfo)
{
	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;

	// Build our view of the texture
	SRV->SourceTexture = Texture;
	const FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(Texture);

	const bool bStencil = Surface.DepthBuffer != nullptr && CreateInfo.Format == PF_X24_G8;
	if (bStencil)
	{
		//HACK.  DX11 on PC currently uses a D24S8 depthbuffer and so needs an X24_G8 SRV to visualize stencil.
		//So take that as our cue to visualize stencil.  In the future, the platform independent code will have a real format
		//instead of PF_DepthStencil, so the cross-platform code could figure out the proper format to pass in for this.
		void* StencilBaseAddress = Surface.DepthBuffer->getStencilReadAddress();

		GpuAddress::TilingParameters TilingParams;
		const int32 InitStatus = TilingParams.initFromStencilSurface(Surface.DepthBuffer, 0);
		check(InitStatus == GpuAddress::kStatusSuccess);

		SRV->Texture.initFromStencilTarget(Surface.DepthBuffer, Gnm::kTextureChannelTypeUInt, false);
		SRV->Texture.setBaseAddress(StencilBaseAddress);
	}
	else
	{
		const bool bBaseSRGB = (Texture->GetFlags() & TexCreate_SRGB) != 0;
		const bool bSRGB = (CreateInfo.SRGBOverride == SRGBO_ForceEnable) || (CreateInfo.SRGBOverride == SRGBO_Default && bBaseSRGB);
		const EPixelFormat Format = (CreateInfo.Format != PF_Unknown) ? (EPixelFormat)CreateInfo.Format : Texture->GetFormat();
		Gnm::DataFormat PlatformFormat = GGnmManager.GetDataFormat(Format, bSRGB);

		SRV->Texture = *Surface.Texture;
		SRV->Texture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
		SRV->Texture.setMipLevelRange(CreateInfo.MipLevel, CreateInfo.MipLevel + CreateInfo.NumMipLevels - 1u);
		SRV->Texture.setDataFormat(PlatformFormat);
	}

	return SRV;
}

void FGnmCommandListContext::RHIClearTinyUAV(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const uint32* Values)
{
	FGnmUnorderedAccessView* UnorderedAccessView = ResourceCast(UnorderedAccessViewRHI);
	UnorderedAccessView->UpdateBufferDescriptor();

	if ((UnorderedAccessView->SourceVertexBuffer && UnorderedAccessView->SourceVertexBuffer->GetUsage() & BUF_DrawIndirect)
		|| (UnorderedAccessView->SourceIndexBuffer && UnorderedAccessView->SourceIndexBuffer->GetUsage() & BUF_DrawIndirect)
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

void FGnmDynamicRHI::RHIBindDebugLabelName(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const TCHAR* Name)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FGnmUnorderedAccessView* UnorderedAccessView = ResourceCast(UnorderedAccessViewRHI);
	sce::Gnm::registerResource(nullptr, GGnmManager.GetOwnerHandle(), UnorderedAccessView->Buffer.getBaseAddress(), UnorderedAccessView->Buffer.getSize(), TCHAR_TO_ANSI(Name), sce::Gnm::kResourceTypeBufferBaseAddress, 0);
#endif
}
