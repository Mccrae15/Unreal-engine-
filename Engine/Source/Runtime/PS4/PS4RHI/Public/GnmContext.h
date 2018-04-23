// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GnmContext.h: Class to generate gnm command buffers from RHI CommandLists
=============================================================================*/

#pragma once 

#include "GnmTempBlockAllocator.h"
#include "GnmResources.h"
#include "GnmContextCommon.h"

class FGnmCommandListContext : public IRHICommandContext
{
private:
	static const uint32 MaxLDSUsage = 64 * 1024;

public:		

	FGnmCommandListContext(bool bInIsImmediate);
	virtual ~FGnmCommandListContext();	

	/**
	 * Allocates space for DCB / Resource table out of per-frame allocators.
	 * Resets context with new buffers.
	 */
	void InitContextBuffers(bool bReuseDCBMem=false, bool bPreserveState=false);	

	/**
	* Clears out cached state.
	*/
	void ClearState();	

	/**
	* Initializes hardware to default state.  Necessary to restore to a known state after submitdone.
	* Should be called AFTER context is setup for rendering for the new frame
	*/
	void InitializeStateForFrameStart();

	/* Allocate a new resource table from the command buffer and initalize it */
	void AllocateGlobalResourceTable();

	bool IsImmediate()
	{
		return bIsImmediate;
	}

	/**
	* Retrieve currently set compute shader.
	*/
	const FComputeShaderRHIRef& GetCurrentComputeShader() const
	{
		return CurrentComputeShader;
	}

	/**
	* Remember current compute shader if any.
	*/
	void SetCurrentComputeShader(FComputeShaderRHIParamRef ComputeShader)
	{
		CurrentComputeShader = ComputeShader;
	}

	/**
	* Sets up the vertex shader mode for normal or geometry shaders
	*/
	void SetupVertexShaderMode(Gnmx::EsShader* ES, Gnmx::GsShader* GS);

	/** Cache the info for a stream - will be bound to Gnm next draw call */
	inline void SetPendingStream(uint32 InStreamIndex, void* InVertexBufferMem, uint32 InVertexBufferSize, uint32 InStride)
	{
		ensure(StreamStrides[InStreamIndex] == InStride);

		// remember the vertex buffer info
		PendingStreams[InStreamIndex].VertexBufferMem = InVertexBufferMem;
		PendingStreams[InStreamIndex].VertexBufferSize = InVertexBufferSize;
		PendingStreams[InStreamIndex].Stride = InStride;
		PendingStreams[InStreamIndex].NumElementsOrBufferSize = InStride ? (InVertexBufferSize / InStride) : InVertexBufferSize;

		// mark as dirty
		bPendingStreamsAreDirty = true;
	}

	/** Cache the info for a stream - will be bound to Gnm next draw call */
	void SetPendingVertexDeclaration(TRefCountPtr<FGnmVertexDeclaration> InVertexDeclaration);

	/** Bind the vertex buffer addresses to the GPU for all vertices, and update constant buffer */
	void PrepareForDrawCall(uint32 FirstInstance);
	void PrepareForDrawCallUP(uint32 NumVertices, void* ImmediateVertexMemory, uint32 ImmediateVertexStride);

	/** Bind shader resources for compute tasks late to support LCUE */
	void PrepareForDispatch();

	/**Uploads the default constant buffer to the compute shader stage */
	void CommitComputeConstants();

	/** Remember how many render targets are bound, so we only set state for those RTs */
	void SetNumRenderTargets(uint32 NumRenderTargets)
	{
		CurrentNumRenderTargets = NumRenderTargets;
	}
	uint32 GetNumRenderTargets()
	{
		return CurrentNumRenderTargets;
	}

	/** Set and cache the current render targets, for clearing */
	void SetCurrentRenderTarget(uint32 RTIndex, FTextureRHIParamRef RTTexture, bool CMASKRequired, uint32 MipIndex = 0, uint32 SliceIndex = -1);

	Gnm::RenderTarget* GetCurrentRenderTarget(uint32 RTIndex)
	{
		return &(CurrentRenderTargets[RTIndex]);
	}

	FTextureRHIParamRef GetCurrentRenderTargetTexture(uint32 RTIndex)
	{
		return CurrentRenderTargetTextures[RTIndex];
	}

	void SetCurrentDepthTarget(FTextureRHIParamRef DepthTexture, bool HTILERequired, bool bReadOnly);

	FTextureRHIParamRef GetCurrentDepthTarget()
	{
		return CurrentDepthTarget;
	}

	struct FRenderTargetViewInfo
	{
		uint32 MipIndex;
		uint32 ArraySliceIndex;
	};

	const FRenderTargetViewInfo& GetCurrentRenderTargetViewInfo(uint32 RTIndex)
	{
		return CurrentRenderTargetsViewInfo[RTIndex];
	}

	/**
	* Note that a render/depth target as resolved, which means we can now use it as a texture
	*/
	void SetTargetAsResolved(void* GPUAddr);

	/**
	* Set a texture into a slot for a stage, also verifies that it has been resolved if it's a render target texture
	*/
	void SetTextureForStage(FGnmSurface& Surface, uint32 TextureIndex, Gnm::ShaderStage Stage, FName TextureName);
	void SetTextureForStage(FTextureRHIParamRef NewTextureRHI, uint32 TextureIndex, Gnm::ShaderStage Stage);

	/**
	* Set a SRV into a slot for a stage, also verifies that it has been resolved if it's a render target texture
	*/
	void SetSRVForStage(FShaderResourceViewRHIParamRef SRV, uint32 TextureIndex, Gnm::ShaderStage Stage);	

	/**
	* Add an address of a UAV used by a pixel shader, as this will need to be flushed before running a compute shader
	*/
	void AddPixelShaderUAVAddress(void* UAVAddress, uint32 Size);

	/**
	* Add a global UAV to be set whenever the bound shader state changes.  Necessary for LCUE until RHI sets UAVs just like other Shader Resources (textures, samplers, etc).
	*/
	void AddDeferredPixelShaderUAV(int32 UAVIndex, FGnmUnorderedAccessView* UAV);

	/**
	* Bind UAV to the given slot and stage.  Manages Append/Consume/Structured buffer counter DMAs.
	*/
	inline void BindUAV(FGnmUnorderedAccessView* InUAV, int32 UAVIndex, Gnm::ShaderStage Stage)
	{
		BindUAV(InUAV, UAVIndex, Stage, -1, false);
	}

	/**
	* Bind UAV to the given slot and stage.  Manages Append/Consume/Structured buffer counter DMAs.  Uploads CounterValue to GPU
	*/
	inline void BindUAV(FGnmUnorderedAccessView* InUAV, int32 UAVIndex, Gnm::ShaderStage Stage, int32 CounterValue)
	{
		BindUAV(InUAV, UAVIndex, Stage, CounterValue, true);
	}

	/**
	* Unbinds the UAV from any stage.
	*/
	void ClearBoundUAV(FGnmUnorderedAccessView* InUAV);

	void ClearAllBoundUAVs();

	/**
	* Reads UAV counters back from GDS to system memory so they can be coherent with the UAVs if their desired location changes in GDS.
	* Optionally flushes graphics or compute before issuing DMA.
	*/
	void StoreBoundUAVs(bool bFlushGfx, bool bFlushCompute);

	/**
	* Sets all deferred pixel shader UAVs on the current graphics context
	*/
	void SetDeferredPixelShaderUAVs();

	/**
	* Clears the set of deferred UAVs to be set when shaders change.  Does not affect the UAVs added for address flushing.
	*/
	void ClearPixelShaderUAVs();

	/**
	* Flush anything needed before a compute shader
	*/
	void FlushBeforeComputeShader();

	/**
	* Flush anything needed after a compute shader
	*/
	void FlushAfterComputeShader();	

	/**
	* Set whether to auto flush after a compute shader
	*/
	void AutomaticCacheFlushAfterComputeShader(bool bEnable)
	{
		bAutoFlushAfterComputeShader = bEnable;
	}

	/**
	* Helper function to fill a generic memory buffer with a given 32 bit value. Uses compute
	*/
	void FillMemoryWithDword(FRHICommandList_RecursiveHazardous& RHICmdList, void* Address, uint32 NumDwords, uint32 Value);

	/**
	* Push not-yet-cleared CMASK bits to the render target so it can be read/displayed
	*/
	void EliminateFastClear(FTextureRHIParamRef Target);

	void FlushCBMetaData();

	/** 
	* Decompresses a RenderTarget's FMASK surface so that all pixels have valid FMASK data. This operation is necessary
	* before binding the FMASK surface as a texture.
	* note If CMASK is enabled for the provided target,
	* this operation will perform an implicit CMASK fast clear elimination pass, as well. .
	* The following render state is assumed to have been set up by the caller:
	* - Active shader stages [function assumes only VS/PS are enabled]
	* - Fast clear color for MRT slot 0 (if CMASK is enabled for the provided target).
	* The target must have a valid FMASK buffer.
	*/
	void DecompressFmaskSurface(FTextureRHIParamRef Target);

	/**
	* Push the HTile bits to the depth buffer so it can be read
	*/
	void DecompressDepthTarget(FTextureRHIParamRef DepthTexture);

	uint32 GetCurrentViewportWidth()
	{
		return CurrentWidth;
	}
	uint32 GetCurrentViewportHeight()
	{
		return CurrentHeight;
	}

	//restores the DCB to the set of cached state.  For functions that bypass the CUE and write directly to DCB.
	void RestoreCachedDCBState();

	/**
	* Set a constant into a constant buffer
	*/
	FORCEINLINE void UpdateVSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size);
	FORCEINLINE void UpdateHSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size);
	FORCEINLINE void UpdateDSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size);
	FORCEINLINE void UpdatePSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size);
	FORCEINLINE void UpdateGSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size);
	FORCEINLINE void UpdateCSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size);

	FORCEINLINE const FVector4& GetViewportBounds() const;

	FORCEINLINE void SetPrimitiveType(Gnm::PrimitiveType Type);

	FORCEINLINE void SetIndexSize(Gnm::IndexSize IndexSize);

	FORCEINLINE void SetIndexOffset(uint32 IndexOffset);

	FORCEINLINE void SetNumInstances(uint32 NumInstances);

	FORCEINLINE void SetPrimitiveSetup(Gnm::PrimitiveSetup PrimitiveSetup);

	FORCEINLINE void SetPolygonOffset(float PolyScale, float PolyOffset);

	FORCEINLINE bool SetScanModeControl(Gnm::ScanModeControlAa AAEnabled, Gnm::ScanModeControlViewportScissor ScissorEnabled);

	FORCEINLINE void SetAAEnabled(bool bEnabled);

	FORCEINLINE void SetViewportScissorRect(bool bEnabled, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY);
	FORCEINLINE void SetGenericScissorRect(bool bEnabled, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY);

	FORCEINLINE void SetViewportAndGuardBand(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ, float ZScale, float ZOffset);

	FORCEINLINE void SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ);
	
	FORCEINLINE void SetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 LeftMinY, uint32 RightMinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 LeftMaxY, uint32 RightMaxY, float MaxZ);

	/**
	* Helper function to set a full screen viewport and bypass the caching system
	*/
	FORCEINLINE void SetViewportUncached(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ, float ZScale, float ZOffset);

	FORCEINLINE void SetDepthStencilState(FGnmDepthStencilState* NewState, uint8 StencilRef);

	FORCEINLINE void SetStencilRef(uint8 StencilRef);

	FORCEINLINE void SetRenderTargetMask( uint32 RenderTargetMask );

	FORCEINLINE void SetBlendState(FGnmBlendState* NewState);

	FORCEINLINE void SetBlendColor(const FLinearColor& BlendColor);

	FORCEINLINE void EnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth);

	FORCEINLINE void SetDepthClearValue(float Depth);	

	template <class ShaderType>
	void SetResourcesFromTables(const ShaderType* RESTRICT Shader, Gnm::ShaderStage ShaderStage);

	void CommitGraphicsResourceTables();
	void CommitNonComputeShaderConstants();
	void CommitComputeResourceTables(FGnmComputeShader* ComputeShader);	

	/** Adds the current DCB to the async submission thread if there are enough commands to meet the minimum threshhold.  Only valid for the immediate context */
	bool SubmitCurrentCommands(uint32 MinimumCommandByes);
	void PushMarker(const char* DebugString);
	void PopMarker();
	
	FORCEINLINE void SetResource(Gnm::ShaderStage ShaderStage, uint32 BindIndex, FGnmShaderResourceView* RESTRICT Surface)
	{
		SetSRVForStage(Surface, BindIndex, ShaderStage);
	}
	
	FORCEINLINE void SetResource(Gnm::ShaderStage ShaderStage, uint32 BindIndex, FGnmSurface* RESTRICT Surface, FName ResourceName)
	{
		check(Surface->Texture);
		// todo: need to support SRV_Static for faster calls when possible
		//StateCache->SetShaderResourceView<Frequency>(SRV,BindIndex,FD3D11StateCache::SRV_Unknown);
		SetTextureForStage(*Surface, BindIndex, ShaderStage, ResourceName);
	}
	
	FORCEINLINE void SetResource(Gnm::ShaderStage ShaderStage, uint32 BindIndex, Gnm::Sampler* RESTRICT SamplerState)
	{
		GetContext().setSamplers(ShaderStage, BindIndex, 1, SamplerState);
	}

	/**
	* Pop all outstanding user push markers
	*/
	void PopAllMarkers()
	{
		while (MarkerStackLevel)
		{
			GnmContext->popMarker();
			MarkerStackLevel--;
		}
	}

	GnmContextType& GetContext()
	{
		return *GnmContext;
	}

	virtual void RHIWaitComputeFence(FComputeFenceRHIParamRef InFence) override final;
	virtual void RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader) final override;
	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override;
	virtual void RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIAutomaticCacheFlushAfterComputeShader(bool bEnable) final override;
	virtual void RHIFlushComputeShaderCache() final override;
	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) final override;
	virtual void RHIClearTinyUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values) final override;
	virtual void RHICopyToResolveTarget(FTextureRHIParamRef SourceTexture, FTextureRHIParamRef DestTexture, bool bKeepOriginalSurface, const FResolveParams& ResolveParams) final override;
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InRenderTargets, int32 NumTextures) final override;
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFence) final override;
	virtual void RHIBeginRenderQuery(FRenderQueryRHIParamRef RenderQuery) final override;
	virtual void RHIEndRenderQuery(FRenderQueryRHIParamRef RenderQuery) final override;
	virtual void RHIBeginOcclusionQueryBatch() final override;
	virtual void RHIEndOcclusionQueryBatch() final override;
	virtual void RHISubmitCommandsHint() final override;
	virtual void RHIBeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI) final override;
	virtual void RHIEndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync) final override;
	virtual void RHIBeginFrame() final override;
	virtual void RHIEndFrame() final override;
	virtual void RHIBeginScene() final override;
	virtual void RHIEndScene() final override;
	virtual void RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Offset) final override;
	virtual void RHISetRasterizerState(FRasterizerStateRHIParamRef NewState) final override;
	virtual void RHISetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ) final override;
	virtual void RHISetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 LeftMinY, uint32 RightMinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 LeftMaxY, uint32 RightMaxY, float MaxZ) final override;
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) final override;
	virtual void RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderState) final override;
	virtual void RHISetShaderTexture(FVertexShaderRHIParamRef VertexShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FComputeShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV) final override;
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount) final override;
	virtual void RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderParameter(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewState, uint32 StencilRef) final override;
	virtual void RHISetStencilRef(uint32 StencilRef) final override;
	virtual void RHISetBlendState(FBlendStateRHIParamRef NewState, const FLinearColor& BlendFactor) final override;
	virtual void RHISetBlendFactor(const FLinearColor& BlendFactor) final override;
	virtual void RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs) final override;
	virtual void RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo) final override;
	virtual void RHIBindClearMRTValues(bool bClearColor, bool bClearDepth, bool bClearStencil) final override;
	virtual void RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBuffer, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData) final override;
	virtual void RHIEndDrawPrimitiveUP() final override;
	virtual void RHIBeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData) final override;
	virtual void RHIEndDrawIndexedPrimitiveUP() final override;
	virtual void RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth) final override;
	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) final override;
	virtual void RHIPopEvent() final override;
	virtual void RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture) final override;

	void* AllocateFromTempFrameBuffer(uint32 Size);

	uint64* AllocateBeginCmdListTimestamp();
	uint64* AllocateEndCmdListTimestamp();

	uint64* GetBeginCmdListTimestamp()
	{
		return StartOfSubmissionTimestamp;
	}

	uint64* GetEndCmdListTimestamp()
	{
		return EndOfSubmissionTimestamp;
	}

	/**
	* Keeps a transient history of bound shader states so that they aren't destroyed/recreated all the time,
	* since creating one is a CPU speed hit
	*/
	FORCEINLINE void RememberBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderStateRHI)
	{
		BoundShaderStateHistory.Add(BoundShaderStateRHI);
	}

private:
	void RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);

	FORCEINLINE void SetupVertexBuffersUP(uint32 NumVertices, void* ImmediateVertexMemory, uint32 ImmediateVertexStride);
	FORCEINLINE void SetupVertexBuffers(uint32 FirstInstance);

	void SetRenderTargets_Impl(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs, bool bHWColorRequired, bool bHWDepthRequired);
	void BindClearMRTValues_Impl(bool bClearColor, bool bClearDepth, bool bClearStencil, bool bCheckErrors);

	/* Perform requested binding.  Override CounterValue is optional.  Performas all necessary DMAs */
	void BindUAV(FGnmUnorderedAccessView* InUAV, int32 UAVIndex, Gnm::ShaderStage Stage, int32 CounterValue, bool bOverrideCounter);

	/* Perform a fast CMASK/HTILE clear if possible.  Fall back to a slow immediate clear by shader if necessary. */
	void TryClearMRTByHardware(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);

	/* Perform a clear via shader.  Necessary for targets with no CMASK/HTILE or for partial clears. */
	void ClearMRTByShader(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil);

	void ClearColorTargetsByHardware(FRHICommandList_RecursiveHazardous& RHICmdList, int32 NumClearColors, const FLinearColor* ClearColorArray);
	void ClearDepthTargetByHardware(FRHICommandList_RecursiveHazardous& RHICmdList, float Depth);

	void ClearColorTargetsByShader(FRHICommandList_RecursiveHazardous& RHICmdList, int32 NumClearColors, const FLinearColor* ClearColorArray);
	void ClearDepthTargetByShader(FRHICommandList_RecursiveHazardous& RHICmdList, float Depth);

	void ClearStencilTarget(FRHICommandList_RecursiveHazardous& RHICmdList, uint32 Stencil);
	void ClearStencilTargetByGfxShader( uint32 Stencil );
	
	void SetupMSAARenderState(FGnmSurface* RT);

	TDCBAllocator					DCBAllocator;
	TLCUEResourceAllocator			ResourceBufferAllocator;
	TTempContextFrameGPUAllocator	TempFrameAllocator;

	bool bIsImmediate;
	bool bCurrentDepthTargetReadOnly;

	/** Last viewport set (used for compute shader clear) **/
	FVector4 CachedViewportBounds;
	FVector4 CachedStereoViewportBounds;
	float CachedViewportMinZ;
	float CachedViewportMaxZ;	
	
	uint16 StreamStrides[MaxVertexElementCount];

	/** The currently bound render targets */
	static_assert(MaxSimultaneousRenderTargets <= 8, "MaxSimultaneousRenderTargets is too big.");

	// separate list from the textures because we modify the Gnm::RenderTarget for slice/mip/cmask
	Gnm::RenderTarget CurrentRenderTargets[MaxSimultaneousRenderTargets];

	//Set of textures that were fast cleared, but not resolved yet.
	TSet<FTextureRHIParamRef> TargetsNeedingEliminateFastClear;
	
	Gnm::DepthRenderTarget CurrentDepthRenderTarget;
	FTextureRHIParamRef CurrentDepthTarget;
	FTextureRHIParamRef CurrentRenderTargetTextures[MaxSimultaneousRenderTargets];
	FRenderTargetViewInfo CurrentRenderTargetsViewInfo[MaxSimultaneousRenderTargets];

	/*
	 * Tracks the targets have been set but not resolved.  For PS4 this 'resolve' means
	 * 'this target may have been written to, before we can read from it the GPU must flush the proper caches'
	 * for depth buffers this may also include decompression for reading.
	 */
	TSet<void*> UnresolvedTargets;

	/** List of addresses that need the caches flushed */
	struct FPixelShaderUAVToFlushPair
	{
		void* Pointer;
		uint32 Size;
	};
	TArray<FPixelShaderUAVToFlushPair> PixelShaderUAVToFlush;

	struct FDeferredUAV
	{
		FDeferredUAV(int32 Index, FGnmUnorderedAccessView* InUAV) :
		UAVIndex(Index),
		UAV(InUAV)
		{}

		int32 UAVIndex;
		FGnmUnorderedAccessView* UAV;
	};

	/** List of UAVs which need setting for pixel shaders.  On PS4, UAVs act like any other shader resource, but D3D treats them like rendertargets so the RHI doesn't make SetUAV calls at the right time for PS4. */
	TArray<FDeferredUAV> PixelShaderUAVToSet;

	/** List of UAVs currently bound.  Required to properly manage DMAs for append/consume/structured buffer counters. */
	TArray<FGnmUnorderedAccessView*> BoundUAVs;
	bool bAnySetUAVs;
	bool bUpdateAnySetUAVs;

	GnmContextType* GnmContext;

	TRefCountPtr<FGnmBoundShaderState> CurrentBoundShaderState;

	/** The current vertex shader mode (vertex, export, local) */
	Gnm::ShaderStage CurrentVertexShaderStage;

	/** How many RTs are bound */
	uint32 CurrentNumRenderTargets;

	/** Current viewport size */
	uint32 CurrentWidth, CurrentHeight;	

	/** Cached state values used in combined API calls */
	Gnm::ScanModeControlAa CachedAAEnabled;
	Gnm::ScanModeControlViewportScissor CachedScissorEnabled;
	uint32 CachedViewportScissorMinX;
	uint32 CachedViewportScissorMinY;
	uint32 CachedViewportScissorMaxX;
	uint32 CachedViewportScissorMaxY;
	uint32 CachedGenericScissorMinX;
	uint32 CachedGenericScissorMinY;
	uint32 CachedGenericScissorMaxX;
	uint32 CachedGenericScissorMaxY;
	Gnm::DepthStencilControl CachedDepthStencilState;
	Gnm::StencilControl CachedStencilControl;
	Gnm::StencilOpControl CachedStencilOpControl;
	bool CachedDepthTestEnabled;
	bool CachedStencilTestEnabled;
	bool CachedDepthBoundsTestEnabled;
	float CachedDepthBoundsMin;
	float CachedDepthBoundsMax;
	Gnm::PrimitiveType CachedPrimitiveType;
	Gnm::IndexSize CachedIndexSize;
	Gnm::PrimitiveSetup CachedPrimitiveSetup;
	uint32 CachedIndexOffset;
	uint32 CachedNumInstances;
	float CachedPolyScale;
	float CachedPolyOffset;
	Gnm::ActiveShaderStages CachedActiveShaderStages;
	Gnm::BlendControl CachedBlendControlRT[MaxSimultaneousRenderTargets];
	FLinearColor CachedBlendColor;
	uint32 CachedRenderTargetMask;
	uint32 CachedRenderTargetColorWriteMask;
	uint32 CachedActiveRenderTargetMask;
	uint32 CachedDepthClearValue;
	Gnm::CbMode CachedCbMode;

	struct FEsGsVertexInfo
	{
		void Init()
		{
			ESVertexSizeInDword = 0;
			GSVertexSizeInDword[0] = 0; 
			GSVertexSizeInDword[1] = 0;
			GSVertexSizeInDword[2] = 0;
			GSVertexSizeInDword[3] = 0;
			GSOutputVertexCount = 0;
		}

		void Set( Gnmx::EsShader* ES, Gnmx::GsShader* GS )
		{
			ESVertexSizeInDword = ( ES->m_memExportVertexSizeInDWord );
			GSVertexSizeInDword[0] = ( GS->m_memExportVertexSizeInDWord[0] );
			GSVertexSizeInDword[1] = ( GS->m_memExportVertexSizeInDWord[1] );
			GSVertexSizeInDword[2] = ( GS->m_memExportVertexSizeInDWord[2] ); 
			GSVertexSizeInDword[3] = ( GS->m_memExportVertexSizeInDWord[3] );
			GSOutputVertexCount = ( GS->m_maxOutputVertexCount );
		}

		bool Equals( Gnmx::EsShader* ES, Gnmx::GsShader* GS )
		{
			return ( ES->m_memExportVertexSizeInDWord == ESVertexSizeInDword &&
					 GS->m_memExportVertexSizeInDWord[0] == GSVertexSizeInDword[0] &&
					 GS->m_memExportVertexSizeInDWord[1] == GSVertexSizeInDword[1] &&
					 GS->m_memExportVertexSizeInDWord[2] == GSVertexSizeInDword[2] &&
					 GS->m_memExportVertexSizeInDWord[3] == GSVertexSizeInDword[3] &&
					 GS->m_maxOutputVertexCount == GSOutputVertexCount );
		}

		uint32 ESVertexSizeInDword;
		uint32 GSVertexSizeInDword[4];
		uint32 GSOutputVertexCount;
	};

	FEsGsVertexInfo CachedEsGsVertexInfo;

	void* PendingDrawPrimitiveUPVertexData;
	uint32 PendingNumVertices;
	uint32 PendingVertexDataStride;

	void* PendingDrawPrimitiveUPIndexData;
	uint32 PendingPrimitiveType;
	uint32 PendingNumPrimitives;
	uint32 PendingMinVertexIndex;
	uint32 PendingIndexDataStride;

	/** Track the currently bound uniform buffers. */
	FUniformBufferRHIRef BoundUniformBuffers[SF_NumFrequencies][FGnmContextCommon::MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE];

	/** Bit array to track which uniform buffers have changed since the last draw call. */
	uint32 DirtyUniformBuffers[SF_NumFrequencies];

	FComputeShaderRHIRef CurrentComputeShader;

	struct FGnmPendingStream
	{
		/** GPU address of the vertex buffer */
		void* VertexBufferMem;

		/** Size of the vertex buffer */
		uint32 VertexBufferSize;

		/** Size of each vertex */
		uint32 Stride;

		/** NumElements for Stride > 0; for Stride == 0 contains the current vertex buffer size for validation
		*/
		uint32 NumElementsOrBufferSize;
	};

	/** All pending streams/vertex buffers */
	FGnmPendingStream PendingStreams[MAX_VERTEX_ATTRIBUTES];

	/** The next vertex declaration to use for the next draw call */
	TRefCountPtr<FGnmVertexDeclaration> PendingVertexDeclaration;

	/** Previously used value of 'FirstInstance' when drawing primitives.  Used to invalidate pending streams.*/
	uint32 CachedFirstInstance;

	/** Have there been any changes to the pending streams since last draw call? */
	bool bPendingStreamsAreDirty;

	/** When a new shader is set, we discard all old constants set for the previous shader. */
	bool bDiscardSharedConstants;

	/** The current level of the user push markers */
	// todo: mw add these up across all contexts on manager to do any necessary extra pops at EOF.
	int32 MarkerStackLevel;

	/** Whether to automatically flush caches after a compute shader completes */
	bool bAutoFlushAfterComputeShader;

	/** A set of constant buffer storage for all shader types */
	TRefCountPtr<FGnmConstantBuffer> VSConstantBuffer;
	TRefCountPtr<FGnmConstantBuffer> HSConstantBuffer;
	TRefCountPtr<FGnmConstantBuffer> DSConstantBuffer;
	TRefCountPtr<FGnmConstantBuffer> PSConstantBuffer;
	TRefCountPtr<FGnmConstantBuffer> GSConstantBuffer;
	TRefCountPtr<FGnmConstantBuffer> CSConstantBuffer;

	uint64* StartOfSubmissionTimestamp;
	uint64* EndOfSubmissionTimestamp;

	/** A history of the most recently used bound shader states, used to keep transient bound shader states from being recreated for each use. */
	TGlobalResource< TBoundShaderStateHistory<2500, false> > BoundShaderStateHistory;

public:
	template<typename TRHIType>
	static FORCEINLINE typename TGnmResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
	{
		return static_cast<typename TGnmResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}
};

#include "GnmContext.inl"