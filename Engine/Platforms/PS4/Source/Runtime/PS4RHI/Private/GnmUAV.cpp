// Copyright Epic Games, Inc. All Rights Reserved.


#include "GnmRHIPrivate.h"
#include "GrowableAllocator.h"
#include "RenderUtils.h"
#include "ClearReplacementShaders.h"

FGnmShaderResourceView::~FGnmShaderResourceView()
{
	Clear();
}

FGnmUnorderedAccessView::FGnmUnorderedAccessView()
	: NumComponents(0)
	, DMAValueTarget(nullptr)
	, bUseUAVCounters(false)
{
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

	// Unless we encounter sRGB, we will just use the source texture's Texture object later, no need to initialize everything now
	Gnm::DataFormat PlatformFormat = GGnmManager.GetDataFormat(SourceTexture->GetFormat());
	NumComponents = PlatformFormat.getNumComponents();
	
	// Copy the texture contents over and modify
	FMemory::Memcpy(&Texture, Surface.Texture, sizeof(Texture));
	Texture.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.
	Texture.setMipLevelRange(MipLevel, MipLevel);

	if (Texture.getTextureChannelType() == sce::Gnm::kTextureChannelTypeSrgb)
	{
		// To make PS4's behavior more cross-platform compatible (e.g. with DX12) we force sRGB to UNorm for UAVs (if just set as-is it behaves much like SNorm)
		Texture.setDataFormat(PlatformFormat);
	}

	DMAValueTarget = nullptr;
	bUseUAVCounters = false;
}

FGnmUnorderedAccessView::FGnmUnorderedAccessView(FRHITexture* TextureRHI, uint32 MipLevel, uint8 Format)
{
	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(TextureRHI);
	SourceTexture = (FRHITexture*)TextureRHI;

	Gnm::DataFormat PlatformFormat = GGnmManager.GetDataFormat((EPixelFormat)Format);
	NumComponents = PlatformFormat.getNumComponents();

	// Copy the texture contents over and modify
	FMemory::Memcpy(&Texture, Surface.Texture, sizeof(Texture));
	Texture.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.
	Texture.setMipLevelRange(MipLevel, MipLevel);
	Texture.setDataFormat(PlatformFormat);

	// Adjust size for format aliasing cases if required
	uint32 WidthMultiplier = 1;
	uint32 WidthDivisor = 1;
	uint32 HeightMultiplier = 1;
	uint32 HeightDivisor = 1;
	if (Surface.Texture->getDataFormat().isBlockCompressedFormat() != PlatformFormat.isBlockCompressedFormat())
	{
		WidthMultiplier = HeightMultiplier = PlatformFormat.isBlockCompressedFormat() ? 4 : 1;
		WidthDivisor = HeightDivisor = Surface.Texture->getDataFormat().isBlockCompressedFormat() ? 4 : 1; 
	}
	if (Surface.Texture->getDataFormat().getTotalBitsPerElement() != PlatformFormat.getTotalBitsPerElement())
	{
		WidthMultiplier *= Surface.Texture->getDataFormat().getBitsPerElement();
		WidthDivisor *= PlatformFormat.getBitsPerElement();
	}
	Texture.setWidthMinus1(Surface.Texture->getWidth() * WidthMultiplier / WidthDivisor - 1);
	Texture.setHeightMinus1(Surface.Texture->getHeight() * HeightMultiplier / HeightDivisor - 1);
	Texture.setPitchMinus1(Surface.Texture->getPitch() * WidthMultiplier / WidthDivisor - 1);

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
	check(IsInRenderingThread());
	Buffer.initAsDataBuffer(VertexBuffer->GetCurrentBuffer(true), PlatformFormat, VertexBuffer->GetSize() / ElementSize);

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
	check(IsInRenderingThread());
	Buffer.initAsDataBuffer(IndexBuffer->GetCurrentBuffer(true), PlatformFormat, IndexBuffer->GetSize() / ElementSize);

	// get number of 4 byte components
	NumComponents = PlatformFormat.getNumComponents();
	DMAValueTarget = nullptr;
	bUseUAVCounters = false;
}

void FGnmUnorderedAccessView::UpdateBufferDescriptor()
{
	if (IsValidRef(SourceVertexBuffer))
	{
		Buffer.initAsDataBuffer(SourceVertexBuffer->GetCurrentBuffer(false), Buffer.getDataFormat(), Buffer.getNumElements());
	}
	else if (IsValidRef(SourceIndexBuffer))
	{
		Buffer.initAsDataBuffer(SourceIndexBuffer->GetCurrentBuffer(false), Buffer.getDataFormat(), Buffer.getNumElements());
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

	// set the UAV into the given slot
	if (IsValidRef(SourceTexture))
	{
		Context.setRwTextures(Stage, ResourceIndex, 1, &Texture);
	}
	else 
	{
		// copy and modify buffer object before setting on context
		Gnm::Buffer BufferCopy = Buffer;
		if (IsValidRef(SourceVertexBuffer))
		{
			BufferCopy.initAsDataBuffer(SourceVertexBuffer->GetCurrentBuffer(false), Buffer.getDataFormat(), Buffer.getNumElements());
		}
		else if (IsValidRef(SourceIndexBuffer))
		{
			BufferCopy.initAsDataBuffer(SourceIndexBuffer->GetCurrentBuffer(false), Buffer.getDataFormat(), Buffer.getNumElements());
		}
		BufferCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.
		Context.setRwBuffers(Stage, ResourceIndex, 1, &BufferCopy);

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
		Context.setRwTextures(ResourceIndex, 1, &Texture);
	}
	else
	{
		// update the address in case it's dynamic
		if (IsValidRef(SourceVertexBuffer))
		{
			Buffer.initAsDataBuffer(SourceVertexBuffer->GetCurrentBuffer(false), Buffer.getDataFormat(), Buffer.getNumElements());
		}
		else if (IsValidRef(SourceIndexBuffer))
		{
			Buffer.initAsDataBuffer(SourceIndexBuffer->GetCurrentBuffer(false), Buffer.getDataFormat(), Buffer.getNumElements());
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

FUnorderedAccessViewRHIRef FGnmDynamicRHI::RHICreateUnorderedAccessView(FRHITexture* TextureRHI, uint32 MipLevel, uint8 Format)
{
	// create the UAV buffer to point to the texture's memory
	FGnmUnorderedAccessView* UAV = new FGnmUnorderedAccessView(TextureRHI, MipLevel, Format);
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
	return FGnmDynamicRHI::RHICreateShaderResourceView(FShaderResourceViewInitializer(StructuredBufferRHI));
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FRHIVertexBuffer* VertexBufferRHI, uint32 Stride, uint8 Format)
{
	ensureMsgf(Stride == GPixelFormats[Format].BlockBytes, TEXT("provided stride: %i was not consitent with Pixelformat: %s"), Stride, GPixelFormats[Format].Name);
	return FGnmDynamicRHI::RHICreateShaderResourceView(FShaderResourceViewInitializer(VertexBufferRHI, EPixelFormat(Format)));
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(const FShaderResourceViewInitializer& Initializer)
{
	switch (Initializer.GetType())
	{
		case FShaderResourceViewInitializer::EType::VertexBufferSRV:
		{
			FShaderResourceViewInitializer::FVertexBufferShaderResourceViewInitializer Desc = Initializer.AsVertexBufferSRV();

			FGnmVertexBuffer* VertexBuffer = ResourceCast(Desc.VertexBuffer);

			FGnmShaderResourceView* SRV = new FGnmShaderResourceView;

			if (VertexBuffer)
			{
				uint32 Stride = GPixelFormats[Desc.Format].BlockBytes;
				// calculate how many elements are in the buffer
				uint32 NumElements = VertexBuffer->GetSize() / Stride;

				SRV->SourceMultiBufferRHI = VertexBuffer;
				SRV->SourceMultiBufferResource = VertexBuffer;

				// initialize the buffer with the given format
				// note: we pass true to GetCurrentBuffer to make sure that memory is allocated for zero buffer resources
				check(IsInRenderingThread());
				
				uint32 StartOffsetBytes = FMath::Min(Desc.StartOffsetBytes, NumElements * Stride);
				SRV->BufferOffset = StartOffsetBytes;

				uint8* BufferStart = (uint8*)(VertexBuffer->GetCurrentBuffer(true)) + StartOffsetBytes;
				SRV->Buffer.initAsDataBuffer(BufferStart, GGnmManager.GetDataFormat((EPixelFormat)Desc.Format), FMath::Min(Desc.NumElements, NumElements - (StartOffsetBytes / Stride)));

				// in case the stride != sizeof(Format)
				SRV->Buffer.setStride(Stride);
			}
			return SRV;
		}

		case FShaderResourceViewInitializer::EType::StructuredBufferSRV:
		{
			FShaderResourceViewInitializer::FStructuredBufferShaderResourceViewInitializer Desc = Initializer.AsStructuredBufferSRV();

			FGnmStructuredBuffer* StructuredBuffer = ResourceCast(Desc.StructuredBuffer);

			FGnmShaderResourceView* SRV = new FGnmShaderResourceView;
			SRV->SourceStructuredBuffer = StructuredBuffer;

			// simply copy the buffer contents over
			SRV->Buffer = StructuredBuffer->Buffer;
			uint32 Stride = SRV->Buffer.getStride();
			uint32 NumElements = StructuredBuffer->Buffer.getNumElements();
			uint32 StartOffsetBytes = FMath::Min(Desc.StartOffsetBytes, NumElements * Stride);
			SRV->BufferOffset = StartOffsetBytes;

			SRV->Buffer.setBaseAddress((char*)(SRV->Buffer.getBaseAddress()) + StartOffsetBytes);
			SRV->Buffer.setNumElements(FMath::Min(Desc.NumElements, NumElements - (StartOffsetBytes / Stride)));
			return SRV;
		}

		case FShaderResourceViewInitializer::EType::IndexBufferSRV:
		{
			FShaderResourceViewInitializer::FIndexBufferShaderResourceViewInitializer Desc = Initializer.AsIndexBufferSRV();

			FGnmIndexBuffer* IndexBuffer = ResourceCast(Desc.IndexBuffer);

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
				check(IsInRenderingThread());
					
				uint32 StartOffsetBytes = FMath::Min(Desc.StartOffsetBytes, NumElements * Stride);
				SRV->BufferOffset = StartOffsetBytes;

				SRV->Buffer.initAsDataBuffer((char*)(IndexBuffer->GetCurrentBuffer(true)) + StartOffsetBytes, GGnmManager.GetDataFormat((EPixelFormat)Format), FMath::Min(Desc.NumElements, NumElements - (StartOffsetBytes / Stride)));
				
				// in case the stride != sizeof(Format)
				SRV->Buffer.setStride(Stride);
			}
			return SRV;
		}
	}

	checkNoEntry();
	return nullptr;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceView(FRHIIndexBuffer* IndexBufferRHI)
{
	return FGnmDynamicRHI::RHICreateShaderResourceView(FShaderResourceViewInitializer(IndexBufferRHI));
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
		SRVGnm->Buffer.initAsDataBuffer(VBGnm->GetCurrentBuffer(false), GGnmManager.GetDataFormat((EPixelFormat)Format), NumElements);
		SRVGnm->Buffer.setStride(Stride);
	}
}

void FGnmDynamicRHI::RHIUpdateShaderResourceView(FRHIShaderResourceView* SRV, FRHIIndexBuffer* IndexBuffer)
{
	check(SRV);
	FGnmShaderResourceView* SRVGnm = ResourceCast(SRV);
	SRVGnm->Clear();
	if (IndexBuffer)
	{
		FGnmIndexBuffer* IBGnm = ResourceCast(IndexBuffer);
		const uint32 Stride = IBGnm->IndexSize == Gnm::kIndexSize16 ? 2 : 4;
		const EPixelFormat Format = IBGnm->IndexSize == Gnm::kIndexSize16 ? PF_R16_UINT : PF_R32_UINT;
		const uint32 NumElements = IBGnm->GetSize() / Stride;
		SRVGnm->SourceMultiBufferRHI = IBGnm;
		SRVGnm->SourceMultiBufferResource = IBGnm;
		SRVGnm->Buffer.initAsDataBuffer(IBGnm->GetCurrentBuffer(false), GGnmManager.GetDataFormat((EPixelFormat)Format), NumElements);
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
		const EGnmSrgbMode SrgbMode = (CreateInfo.SRGBOverride == SRGBO_ForceDisable) ? EGnmSrgbMode::Disable : 
			((!!(Texture->GetFlags() & TexCreate_SRGB)) ? EGnmSrgbMode::Enable : EGnmSrgbMode::Default);
		const EPixelFormat Format = (CreateInfo.Format != PF_Unknown) ? (EPixelFormat)CreateInfo.Format : Texture->GetFormat();
		Gnm::DataFormat PlatformFormat = GGnmManager.GetDataFormat(Format, SrgbMode);

		SRV->Texture = *Surface.Texture;
		SRV->Texture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
		SRV->Texture.setMipLevelRange(CreateInfo.MipLevel, CreateInfo.MipLevel + CreateInfo.NumMipLevels - 1u);
		SRV->Texture.setDataFormat(PlatformFormat);
	}

	return SRV;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceViewWriteMask(FRHITexture2D* RenderTarget)
{
	FGnmTexture2D* Texture = ResourceCast(RenderTarget);
	const FGnmSurface& Surface = Texture->Surface;

	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;

	// Build an R8 buffer around the cmask data.
	Gnm::DataFormat BufferFormat = Gnm::DataFormat::build(Gnm::kBufferFormat8, Gnm::kBufferChannelTypeUInt);
	check(BufferFormat.supportsBuffer());
	SRV->Buffer.initAsDataBuffer((void*)Surface.GetCMaskMem().GetPointer(), Gnm::kDataFormatR8Uint, Surface.GetCMaskMem().GetSize());
	SRV->Buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
	check(SRV->Buffer.isBuffer());

	return SRV;
}

FShaderResourceViewRHIRef FGnmDynamicRHI::RHICreateShaderResourceViewFMask(FRHITexture2D* RenderTarget)
{
	FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(RenderTarget);

	if (Surface.Texture->getNumFragments() == Gnm::kNumFragments1)
		return nullptr;

	FGnmShaderResourceView* SRV = new FGnmShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)RenderTarget;
	SRV->Texture.initAsFmask(Surface.ColorBuffer);
	SRV->Texture.setBaseAddress(Surface.ColorBuffer->getFmaskAddress());
	SRV->Texture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

	return SRV;
}

void FGnmCommandListContext::ClearUAV(TRHICommandList_RecursiveHazardous<FGnmCommandListContext>& RHICmdList, FGnmUnorderedAccessView* UnorderedAccessView, const void* ClearValues, bool bFloat)
{
	UnorderedAccessView->UpdateBufferDescriptor();

	if (UnorderedAccessView->SourceVertexBuffer || UnorderedAccessView->SourceIndexBuffer)
	{
		// Vertex / Index buffer
		Gnm::Buffer& Buffer = UnorderedAccessView->Buffer;

		EClearReplacementValueType ValueType = EClearReplacementValueType::Float;
		switch (Buffer.getDataFormat().getBufferChannelType())
		{
		case Gnm::kBufferChannelTypeUInt: ValueType = EClearReplacementValueType::Uint32; break;
		case Gnm::kBufferChannelTypeSInt: ValueType = EClearReplacementValueType::Int32; break;
		}

		ensureMsgf(bFloat == (ValueType == EClearReplacementValueType::Float), TEXT("Attempt to clear a UAV using the wrong RHIClearUAV function. Float vs Integer mismatch."));

		ClearUAVShader_T<EClearReplacementResourceType::Buffer, 4, false>(RHICmdList, UnorderedAccessView, Buffer.getNumElements(), 1, 1, ClearValues, EClearReplacementValueType::Int32);
	}
	else if (UnorderedAccessView->SourceStructuredBuffer)
	{
		// Structured Buffer
		Gnm::Buffer& Buffer = UnorderedAccessView->Buffer;

		uint32 TotalNumBytes = Buffer.getSize();

		// The buffer must be an exact multiple of sizeof(uint32).
		check((TotalNumBytes % sizeof(uint32)) == 0);
		uint32 NumElements = TotalNumBytes / sizeof(uint32);

		// Always treat the clear value data as uint32. The callback lambda sets the buffer format to k32UInt,
		// so that we memset the structured buffer without any format conversion done on the clear value.
		ClearUAVShader_T<EClearReplacementResourceType::Buffer, EClearReplacementValueType::Uint32, 1, false>(RHICmdList, UnorderedAccessView, NumElements, 1, 1, *reinterpret_cast<const uint32(*)[1]>(ClearValues),
			[&RHICmdList, &Buffer, NumElements, UnorderedAccessView](FRHIComputeShader* ShaderRHI, const FShaderResourceParameter& Param, bool bSet)
		{
			if (bSet)
			{
				RHICmdList.SetUAVParameter(ShaderRHI, Param.GetBaseIndex(), UnorderedAccessView);

				RHICmdList.RunOnContext([&Buffer, Index = Param.GetBaseIndex(), NumElements](auto& Context)
				{
					Gnm::Buffer BufferCopy = Buffer;
					BufferCopy.setStride(sizeof(uint32));
					BufferCopy.setNumElements(NumElements);
					BufferCopy.setFormat(Gnm::DataFormat::build(Gnm::kBufferFormat32, Gnm::kBufferChannelTypeUInt));
					BufferCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);

					Context.GetContext().setRwBuffers(Gnm::kShaderStageCs, Index, 1, &BufferCopy);
				});
			}
		});
	}
	else if (UnorderedAccessView->SourceTexture)
	{
		// Texture
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(UnorderedAccessView->SourceTexture);
		Gnm::Texture& GnmTexture = *Surface.Texture;
		
		EClearReplacementValueType ValueType = EClearReplacementValueType::Float;
		switch (GnmTexture.getTextureChannelType())
	{
		case Gnm::kTextureChannelTypeUBInt:
		case Gnm::kTextureChannelTypeUInt:
			ValueType = EClearReplacementValueType::Uint32;
			break;

		case Gnm::kTextureChannelTypeSInt:
			ValueType = EClearReplacementValueType::Int32;
			break;
	}

		ensureMsgf(bFloat == (ValueType == EClearReplacementValueType::Float), TEXT("Attempt to clear a UAV using the wrong RHIClearUAV function. Float vs Integer mismatch."));

		uint32 MipWidth = GnmTexture.getWidth() >> GnmTexture.getBaseMipLevel();
		uint32 MipHeight = GnmTexture.getHeight() >> GnmTexture.getBaseMipLevel();
		uint32 NumSlices = (GnmTexture.getLastArraySliceIndex() - GnmTexture.getBaseArraySliceIndex()) + 1;

		switch (GnmTexture.getTextureType())
		{
		default:
			checkNoEntry();
			break;

		case Gnm::kTextureType3d:      ClearUAVShader_T<EClearReplacementResourceType::Texture3D,      4, false>(RHICmdList, UnorderedAccessView, MipWidth, MipHeight, NumSlices, ClearValues, ValueType); break;
		case Gnm::kTextureType2d:      ClearUAVShader_T<EClearReplacementResourceType::Texture2D,      4, false>(RHICmdList, UnorderedAccessView, MipWidth, MipHeight, NumSlices, ClearValues, ValueType); break;
		case Gnm::kTextureType2dArray: ClearUAVShader_T<EClearReplacementResourceType::Texture2DArray, 4, false>(RHICmdList, UnorderedAccessView, MipWidth, MipHeight, NumSlices, ClearValues, ValueType); break;
		}
		}
		else
		{
		checkNoEntry(); // Unknown UAV type
	}
}

void FGnmCommandListContext::RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4& Values)
			{
	TRHICommandList_RecursiveHazardous<FGnmCommandListContext> RHICmdList(this);
	ClearUAV(RHICmdList, ResourceCast(UnorderedAccessViewRHI), &Values, true);
			}

void FGnmCommandListContext::RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values)
			{
	TRHICommandList_RecursiveHazardous<FGnmCommandListContext> RHICmdList(this);
	ClearUAV(RHICmdList, ResourceCast(UnorderedAccessViewRHI), &Values, false);
			}

void FGnmComputeCommandListContext::RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4& Values)
{
	checkNoEntry(); // @todo gnm: implement
		}

void FGnmComputeCommandListContext::RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values)
{
	checkNoEntry(); // @todo gnm: implement
	}

void FGnmDynamicRHI::RHIBindDebugLabelName(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const TCHAR* Name)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FGnmUnorderedAccessView* UnorderedAccessView = ResourceCast(UnorderedAccessViewRHI);
	sce::Gnm::registerResource(nullptr, GGnmManager.GetOwnerHandle(), UnorderedAccessView->Buffer.getBaseAddress(), UnorderedAccessView->Buffer.getSize(), TCHAR_TO_ANSI(Name), sce::Gnm::kResourceTypeBufferBaseAddress, 0);
#endif
}
