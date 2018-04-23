// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GnmComputeContext.h: Class to generate gnm async compute command buffers from RHI CommandLists
=============================================================================*/

#pragma once 

#include "GnmTempBlockAllocator.h"
#include "GnmResources.h"
#include "GnmContextCommon.h"

struct SGnmComputeSubmission
{
	SGnmComputeSubmission()
	{
		Reset();
	}

	void AddSubmissionToQueue(void* DCB, uint32 SizeBytes);
	bool AddSubmissionToQueue(const SGnmComputeSubmission& Other);
	void Reset();

	// Maximum number of submissions that can be recorded.
	static const uint32 MaxNumStoredSubmissions = Gnmx::ComputeContext::kMaxNumStoredSubmissions;

	// Stores the range of each previously-constructed submission (not including the one currently under construction).	
	uint32	SubmissionSizesBytes[MaxNumStoredSubmissions];
	void*	SubmissionAddrs[MaxNumStoredSubmissions];

	// The current number of stored submissions in SubmissionRanges
	uint32 SubmissionCount;
};

class FGnmComputeCommandListContext : public IRHIComputeContext
{
public:

	FGnmComputeCommandListContext(bool bInIsImmediate);
	~FGnmComputeCommandListContext();

	bool IsImmediate() const
	{
		return bIsImmediate;
	}

	/**
	* Allocates space for ACB / Resource table out of per-frame allocators.
	* Resets context with new buffers.
	*/
	void InitContextBuffers();	

	void InitializeStateForFrameStart();
	void ClearState();

	Gnmx::ComputeContext& GetContext()
	{
		return ComputeContext;
	}

	void SetTextureForStage(FGnmSurface& Surface, uint32 TextureIndex, Gnm::ShaderStage Stage, FName TextureName);
	void SetTextureForStage(FTextureRHIParamRef NewTextureRHI, uint32 TextureIndex, Gnm::ShaderStage Stage);	
	void SetSRVForStage(FShaderResourceViewRHIParamRef SRV, uint32 TextureIndex, Gnm::ShaderStage Stage);

	void* AllocateFromTempFrameBuffer(uint32 Size);

	//IRHIComputeContext
	virtual void RHIWaitComputeFence(FComputeFenceRHIParamRef InFence) override final;
	virtual void RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader) override final;
	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) override final;
	virtual void RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) override final;	
	virtual void RHISetAsyncComputeBudget(EAsyncComputeBudget Budget) override final;
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFence) override final;
	virtual void RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) override final;
	virtual void RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) override final;
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV) override final;
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount) override final;
	virtual void RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) override final;
	virtual void RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) override final;
	virtual void RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) override final;
	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) override final;
	virtual void RHIPopEvent() override final;
	virtual void RHISubmitCommandsHint() override final;
	
	FComputeShaderRHIParamRef GetCurrentComputeShader()
	{
		 return CurrentComputeShader;
	}

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
		ComputeContext.setSamplers(BindIndex, 1, SamplerState);
	}

private:

	void PrepareCurrentCommands();

	/** Adds the current DCB to the async submission thread if there are enough commands to meet the minimum threshhold.  Only valid for the immediate compute context */
	bool SubmitCurrentCommands(uint32 MinimumCommandByes);
	void AllocateGlobalResourceTable();

	void SetTexture(FGnmSurface& Surface, uint32 TextureIndex, FName TextureName);
	void SetTexture(FTextureRHIParamRef NewTextureRHI, uint32 TextureIndex);
	void SetSRV(FShaderResourceViewRHIParamRef SRV, uint32 TextureIndex);
	void BindUAV(FGnmUnorderedAccessView* InUAV, int32 UAVIndex, int32 CounterValue, bool bOverrideCounter);
	FORCEINLINE void UpdateCSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size);

	void ClearAllBoundUAVs();
	void PrepareForDispatch();
	void CommitComputeConstants();
	void CommitComputeResourceTables(FGnmComputeShader* ComputeShader);
	void SetResourcesFromTables();

	static bool HandleReserveFailed(Gnmx::ComputeContext* ComputeContext, Gnm::CommandBuffer* CommandBuffer, uint32_t SizeInDwords, void* UserData);

	Gnmx::ComputeContext ComputeContext;
	SGnmComputeSubmission ComputeSubmission;

	TDCBAllocator					ACBAllocator;
	TLCUEResourceAllocator			ResourceBufferAllocator;
	TTempContextFrameGPUAllocator	TempFrameAllocator;

	FComputeShaderRHIParamRef CurrentComputeShader;
	TRefCountPtr<FGnmConstantBuffer> CSConstantBuffer;

	/** Track the currently bound uniform buffers. */
	FUniformBufferRHIRef BoundUniformBuffers[FGnmContextCommon::MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE];

	/** Bit array to track which uniform buffers have changed since the last dispatch. */
	uint32 DirtyUniformBuffers;

	/** List of UAVs currently bound.  Required to properly manage DMAs for append/consume/structured buffer counters. */
	TArray<FGnmUnorderedAccessView*> BoundUAVs;

	template<typename TRHIType>
	static FORCEINLINE typename TGnmResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
	{
		return static_cast<typename TGnmResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}
	
	bool bAnySetUAVs;
	bool bUpdateAnySetUAVs;
	bool bIsImmediate;
};

#include "GnmComputeContext.inl"