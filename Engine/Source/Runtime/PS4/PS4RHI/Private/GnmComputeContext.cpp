// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GnmComputeContext.cpp: Class to generate gnm async compute command buffers from RHI CommandLists
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "GnmComputeContext.h"
#include "GnmContextCommon.h"
#include "ShaderParameterUtils.h"
#include "SceneUtils.h"

#if !defined(PS4_SUPPORTS_PARALLEL_RHI_EXECUTE)
#error "PS4_SUPPORTS_PARALLEL_RHI_EXECUTE must be defined."
#endif

static int32 GPS4AsyncComputeBudgetMode = 1;
static FAutoConsoleVariableRef CVarPS4AsncComputeBudgetMode(
	TEXT("r.PS4.AsyncComputeBudgetMode"),
	GPS4AsyncComputeBudgetMode,
	TEXT("Defines the method for PS4 AsyncCompute budgeting..\n")
	TEXT(" 0: CU Masking\n")
	TEXT(" 1: Wave Limiting\n"),
	ECVF_Default
	);

void SGnmComputeSubmission::AddSubmissionToQueue(void* DCB, uint32 SizeBytes)
{
	//0 sized submissions will cause gnm errors.  This can happen legitimately if a PrepareForSubmit
	//happens and then the next command causes a reserve failure.
	if (SizeBytes > 0)
	{
		check(SubmissionCount < MaxNumStoredSubmissions);

		SubmissionAddrs[SubmissionCount] = (uint32*)DCB;
		SubmissionSizesBytes[SubmissionCount] = SizeBytes;
		check(SubmissionSizesBytes[SubmissionCount] <= Gnm::kIndirectBufferMaximumSizeInBytes);
		++SubmissionCount;
	}
}

bool SGnmComputeSubmission::AddSubmissionToQueue(const SGnmComputeSubmission& Other)
{
	const int32 NewTotal = SubmissionCount + Other.SubmissionCount;
	if (NewTotal <= MaxNumStoredSubmissions)
	{
		for (int32 i = 0, TargetIndex = SubmissionCount; i < Other.SubmissionCount; ++i, ++TargetIndex)
		{
			SubmissionAddrs[TargetIndex] = Other.SubmissionAddrs[i];
			SubmissionSizesBytes[TargetIndex] = Other.SubmissionSizesBytes[i];
		}
		SubmissionCount += Other.SubmissionCount;
		return true;
	}
	return false;
}

void SGnmComputeSubmission::Reset()
{
	SubmissionCount = 0;
}

FGnmComputeCommandListContext::FGnmComputeCommandListContext(bool bInIsImmediate)
	: DirtyUniformBuffers(0)
	, bAnySetUAVs(false)
	, bUpdateAnySetUAVs(false)
	, bIsImmediate(bInIsImmediate)
{
	for (int32 i = 0; i < FGnmContextCommon::MaxBoundUAVs; ++i)
	{
		BoundUAVs.Add(nullptr);
	}

	// how big should the buffer be for all shader types?
	uint32 Size = Align(MAX_GLOBAL_CONSTANT_BUFFER_SIZE, 16);		
	CSConstantBuffer = new FGnmConstantBuffer(Size);
}

FGnmComputeCommandListContext::~FGnmComputeCommandListContext()
{
	
}



bool FGnmComputeCommandListContext::HandleReserveFailed(Gnmx::ComputeContext* ComputeContext, Gnm::CommandBuffer* CommandBuffer, uint32_t SizeInDwords, void* UserData)
{
	FGnmComputeCommandListContext* CommandContext = (FGnmComputeCommandListContext*)UserData;
	check(&CommandContext->ComputeContext == ComputeContext);
	CommandContext->PrepareCurrentCommands();
	
	uint32 ResourceBufferSizeInBytes = CommandContext->ResourceBufferAllocator.GetChunkSize();
	void* ResourceBufferInGarlic = CommandContext->ResourceBufferAllocator.AllocateChunk();

	uint32 DCBChunkSizeInBytes = CommandContext->ACBAllocator.GetChunkSize();
	void* DCBBuffer = CommandContext->ACBAllocator.AllocateChunk();

	ComputeContext->m_currentDcbSubmissionStart = (uint32_t*)DCBBuffer;

	check(DCBChunkSizeInBytes <= (uint32_t)Gnm::kIndirectBufferMaximumSizeInBytes);

	ComputeContext->m_dcb.m_beginptr = (uint32_t*)DCBBuffer;
	ComputeContext->m_dcb.m_cmdptr = (uint32_t*)DCBBuffer;
	ComputeContext->m_dcb.m_endptr = (uint32_t*)DCBBuffer; 
	ComputeContext->m_dcb.m_bufferSizeInDwords = DCBChunkSizeInBytes / 4;

	ComputeContext->m_currentDcbSubmissionStart = ComputeContext->m_dcb.m_beginptr;
	ComputeContext->m_actualDcbEnd = (uint32_t*)DCBBuffer + (DCBChunkSizeInBytes / 4);
	
	/*
	const int32_t kResourceBufferCount = 1; // only one resource buffer per context
	uint32_t* resourceBuffer = (uint32_t*)ResourceBufferInGarlic;
	m_lwcue.init(&resourceBuffer, kResourceBufferCount, resourceBufferSizeInBytes / 4, globalInternalResourceTableAddr);
	m_lwcue.setDispatchCommandBuffer(&m_dcb);
	m_UsingLightweightConstantUpdateEngine = true;

	// Advance CB end pointers to the next (possibly artificial) boundary -- either current+(4MB-4), or the end of the actual buffer
	cmpc.m_dcb.m_endptr = std::min(cmpc.m_dcb.m_cmdptr + kIndirectBufferMaximumSizeInBytes / 4, (uint32_t*)cmpc.m_actualDcbEnd);
	*/

	return true;
}

void FGnmComputeCommandListContext::InitContextBuffers()
{
	checkf(ComputeSubmission.SubmissionCount == 0, TEXT("AsyncCompute work hasn't been submitted and will be lost."));
	ACBAllocator.Clear();
	ResourceBufferAllocator.Clear();
	TempFrameAllocator.Clear();
	ComputeSubmission.Reset();

	uint32 ResourceBufferSizeInBytes = ResourceBufferAllocator.GetChunkSize();
	void* ResourceBufferInGarlic = ResourceBufferAllocator.AllocateChunk();

	uint32 DCBChunkSizeInBytes = ACBAllocator.GetChunkSize();
	void* DCBBuffer = ACBAllocator.AllocateChunk();

	//init the actual LCUE with the initial size, and a special handler.
	//engine expected to allocate and set globaltables as required.

	// set current context	
	ComputeContext.init(DCBBuffer, DCBChunkSizeInBytes, ResourceBufferInGarlic, ResourceBufferSizeInBytes, nullptr);
	ComputeContext.m_cbFullCallback.m_func = FGnmComputeCommandListContext::HandleReserveFailed;
	ComputeContext.m_cbFullCallback.m_userData = this;	
}

void FGnmComputeCommandListContext::InitializeStateForFrameStart()
{	
	check(IsImmediate());
	InitContextBuffers();
	ComputeContext.initializeDefaultHardwareState();	

	ClearState();
}

void FGnmComputeCommandListContext::ClearState()
{
	CurrentComputeShader = nullptr;
	ClearAllBoundUAVs();
	AllocateGlobalResourceTable();
}

void FGnmComputeCommandListContext::ClearAllBoundUAVs()
{
	for (int i = 0; i < BoundUAVs.Num(); ++i)
	{
		BoundUAVs[i] = nullptr;
	}
}

void FGnmComputeCommandListContext::PrepareForDispatch()
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetCurrentComputeShader();
	FGnmComputeShader* ComputeShader = FGnmDynamicRHI::ResourceCast(ComputeShaderRHI);

	check(ComputeShader);
	if (ComputeShader->bShaderNeedsGlobalConstantBuffer)
	{
		CommitComputeConstants();
	}

	CommitComputeResourceTables(ComputeShader);
}

void FGnmComputeCommandListContext::CommitComputeConstants()
{
	CSConstantBuffer->CommitConstantsToDevice(*this, Gnm::kShaderStageCs, 0, true);
}

void FGnmComputeCommandListContext::CommitComputeResourceTables(FGnmComputeShader* ComputeShader)
{
	check(ComputeShader);	
	SetResourcesFromTables();
}

void* FGnmComputeCommandListContext::AllocateFromTempFrameBuffer(uint32 Size)
{
	//if the allocation is small enough use the local allocator that doesn't require a mutex
	if (Size <= TempFrameAllocator.GetChunkSize())
	{
		return TempFrameAllocator.Allocate(Size);
	}
	else
	{
		return GGnmManager.AllocateFromTempRingBuffer(Size);
	}
}

void FGnmComputeCommandListContext::SetResourcesFromTables()
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetCurrentComputeShader();
	FGnmComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	checkSlow(ComputeShader);

	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = ComputeShader->ShaderResourceTable.ResourceTableBits & DirtyUniformBuffers;
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits)& (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;
		auto* Buffer = (FGnmUniformBuffer*)BoundUniformBuffers[BufferIndex].GetReference();
		check(Buffer);
		check(BufferIndex < ComputeShader->ShaderResourceTable.ResourceTableLayoutHashes.Num());
		check(Buffer->GetLayout().GetHash() == ComputeShader->ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);

		// todo: could make this two pass: gather then set
		FGnmContextCommon::SetShaderResourcesFromBuffer_Surface(*this, Gnm::kShaderStageCs, Buffer, ComputeShader->ShaderResourceTable.TextureMap.GetData(), BufferIndex);
		FGnmContextCommon::SetShaderResourcesFromBuffer_SRV(*this, Gnm::kShaderStageCs, Buffer, ComputeShader->ShaderResourceTable.ShaderResourceViewMap.GetData(), BufferIndex);
		FGnmContextCommon::SetShaderResourcesFromBuffer_Sampler(*this, Gnm::kShaderStageCs, Buffer, ComputeShader->ShaderResourceTable.SamplerMap.GetData(), BufferIndex);
	}
	DirtyUniformBuffers = 0;
}

void FGnmComputeCommandListContext::SetTexture(FGnmSurface& Surface, uint32 TextureIndex, FName TextureName)
{
	check(IsInRenderingThread() || IsInRHIThread());
	if (!IsRunningRHIInSeparateThread())
	{		
		const EResourceTransitionAccess CurrentAccess = Surface.GetCurrentGPUAccess();
		const bool bAccessPass = CurrentAccess == EResourceTransitionAccess::EReadable || (CurrentAccess == EResourceTransitionAccess::ERWBarrier && !Surface.IsDirty()) || CurrentAccess == EResourceTransitionAccess::ERWSubResBarrier;	
		ensureMsgf(bAccessPass || Surface.GetLastFrameWritten() != GGnmManager.GetFrameCount(), TEXT("Attempting to set texture: %s that was not transitioned to readable"), *TextureName.ToString());
	}

	// set the texture into the texture register
	ComputeContext.setTextures(TextureIndex, 1, Surface.Texture);	
}

void FGnmComputeCommandListContext::SetTexture(FTextureRHIParamRef NewTextureRHI, uint32 TextureIndex)
{
	check(IsInRenderingThread() || IsInRHIThread());
	if (NewTextureRHI)
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(NewTextureRHI);

		//these tests can't currently be properly done with parallel rhi.
		if (!IsRunningRHIInSeparateThread())
		{			
			const EResourceTransitionAccess CurrentAccess = Surface.GetCurrentGPUAccess();
			const bool bAccessPass = CurrentAccess == EResourceTransitionAccess::EReadable || (CurrentAccess == EResourceTransitionAccess::ERWBarrier && !Surface.IsDirty()) || CurrentAccess == EResourceTransitionAccess::ERWSubResBarrier;
		
			ensureMsgf(!Surface.bNeedsFastClearResolve, TEXT("%s needs fast clear resolve. %p"), *NewTextureRHI->GetName().ToString(), Surface.Texture->getBaseAddress());
			ensureMsgf(bAccessPass || Surface.GetLastFrameWritten() != GGnmManager.GetFrameCount(), TEXT("ATtempting to set: %s that was not transitioned to readable"), *NewTextureRHI->GetName().ToString());
		}

		// set the texture into the texture register
		ComputeContext.setTextures(TextureIndex, 1, Surface.Texture);
	}
}

void FGnmComputeCommandListContext::SetSRV(FShaderResourceViewRHIParamRef SRVRHI, uint32 TextureIndex)
{
	FGnmShaderResourceView* SRV = ResourceCast(SRVRHI);
	if (SRV)
	{
		ensureMsgf(FGnmContextCommon::ValidateSRVForSet(SRV), TEXT("ValidateSRVForSet failed for %s."), SRV->SourceTexture ? *SRV->SourceTexture->GetName().ToString() : TEXT("None"));

		if (IsValidRef(SRV->SourceTexture))
		{
			ComputeContext.setTextures(TextureIndex, 1, &SRV->Texture);
		}
		else
		{
			// make sure the GPU address matches the source VB if there is one
			if (IsValidRef(SRV->SourceVertexBuffer))
			{
				void* BufferData = SRV->SourceVertexBuffer->GetCurrentBuffer();
#if PS4_USE_NEW_MULTIBUFFER == 0
				if (BufferData == nullptr)
				{
					UE_LOG(LogRHI, Warning, TEXT("Volatile Vertex buffer trying to use data before any data has been provided. Adding memory to avoid crash."));
					BufferData = SRV->SourceVertexBuffer->GetCurrentBuffer(true, true);
					FMemory::Memset(BufferData, 0, SRV->SourceVertexBuffer->GetSize());
				}
#endif

				SRV->Buffer.setBaseAddress(BufferData);
			}
			// set the texture into the texture register
			ComputeContext.setBuffers(TextureIndex, 1, &SRV->Buffer);
		}		
	}
}

void FGnmComputeCommandListContext::BindUAV(FGnmUnorderedAccessView* InUAV, int32 UAVIndex, int32 CounterValue, bool bOverrideCounter)
{
	FGnmUnorderedAccessView* BoundUAV = (FGnmUnorderedAccessView*)BoundUAVs[UAVIndex];
	bool bUpload = false;
	if (BoundUAV != InUAV)
	{
		bUpload = true;
	}
	BoundUAVs[UAVIndex] = InUAV;

	// always need to make the set call for LCUE, but don't always need to DMA the counter value.
	if (InUAV)
	{
		if (bOverrideCounter)
		{
#if SUPPORTS_APPEND_CONSUME_BUFFERS			
			InUAV->SetAndClearCounter(*this, UAVIndex, CounterValue);
#else
			ensureMsgf(0, TEXT("Append Consume unsupported"));
#endif
		}
		else
		{
			InUAV->Set(*this, UAVIndex, bUpload);
		}
	}
	bUpdateAnySetUAVs = true;
}

void FGnmComputeCommandListContext::RHIWaitComputeFence(FComputeFenceRHIParamRef InFenceRHI)
{
	FGnmComputeFence* ComputeFence = ResourceCast(InFenceRHI);
	SCOPED_RHI_DRAW_EVENTF(*this, RHIWaitComputeFence, TEXT("WaitForFence%s"), *InFenceRHI->GetName().ToString());
	ComputeFence->WaitFence(*this);	
}

void FGnmComputeCommandListContext::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	FGnmComputeShader* GnmComputeShader = ResourceCast(ComputeShaderRHI);

	//start setting up compute.
	CurrentComputeShader = ComputeShaderRHI;

	//UAVs need to be rebound when changing compute shaders just like when changing any other BoundShaderState.
	ClearAllBoundUAVs();

	ComputeContext.setCsShader(GnmComputeShader->Shader, &GnmComputeShader->ShaderOffsets);

	if (GnmComputeShader->Shader->m_common.m_scratchSizeInDWPerThread != 0)
	{
		uint32 Num1KbyteScratchChunksPerWave = (GnmComputeShader->Shader->m_common.m_scratchSizeInDWPerThread + 3) / 4;
		ComputeContext.setScratchSize(FGnmManager::ScratchBufferMaxNumWaves, Num1KbyteScratchChunksPerWave);
	}

	//ComputeContext.setAppendConsumeCounterRange(0x0, FGnmDynamicRHI::MaxBoundUAVs * 4);
}

void FGnmComputeCommandListContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
#if 0
	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.RegisterGPUWork(1);
	}
#endif

	FComputeShaderRHIParamRef ComputeShaderRHI = GetCurrentComputeShader();
	check(ComputeShaderRHI);

	PrepareForDispatch();

#if !NO_DRAW_CALLS
	PRE_DISPATCH;
	ComputeContext.dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	POST_DISPATCH;
#endif	

	//for append/consume which we don't support yet.
#if 0
	// compute shaders that write to UAVs must have the counters flushed regardless of the auto flag.
	StoreBoundUAVs(false, !bDoPostFlush);
#endif
}

void FGnmComputeCommandListContext::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
#if 0
	if (IsImmediate())
	{
		GGnmManager.GPUProfilingData.RegisterGPUWork(1);
	}
#endif

	FComputeShaderRHIParamRef ComputeShaderRHI = GetCurrentComputeShader();
	check(ComputeShaderRHI);

	FGnmComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	FGnmVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);
	
	PrepareForDispatch();	

	// dispatch a compute shader, where the xyz values are stored (UE4's FRHIDispatchIndirectParameters is the same as Gnm's DispatchIndirectArgs)	
	Gnm::DispatchIndirectArgs* ArgsLoc = (Gnm::DispatchIndirectArgs*)((uint8*)ArgumentBuffer->GetCurrentBuffer() + ArgumentOffset);

#if !NO_DRAW_CALLS
	PRE_DISPATCH;
	ComputeContext.dispatchIndirect(ArgsLoc);
	POST_DISPATCH;
#endif

#if 0
	// compute shaders that write to UAVs must have the counters flushed regardless of the auto flag.
	StoreBoundUAVs(false, !bDoPostFlush);
#endif
}

void FGnmComputeCommandListContext::RHISetAsyncComputeBudget(EAsyncComputeBudget Budget)
{
	//Gnm::kNumCusPerSe
	uint16 CUMask = 0xf;
	uint32 WavesPerSh = 0;
	uint32 ThreadGroupsPerCU = 0;
	uint32 LockThreshhold = 0;
	switch (Budget)
	{
		case EAsyncComputeBudget::ELeast_0:
			CUMask = (uint16)0x1;
			WavesPerSh = 36;
			break;
		case EAsyncComputeBudget::EGfxHeavy_1:
			CUMask = (uint16)0x7; //(1 << 0) | (1 << 1) | (1 << 2);
			WavesPerSh = 144;
			break;
		case EAsyncComputeBudget::EBalanced_2:
			CUMask = (uint16)0xF; // (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
			WavesPerSh = 216;
			break;
		case EAsyncComputeBudget::EComputeHeavy_3:
			CUMask = (uint16)0x3F; // (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5);
			WavesPerSh = 288;
			break;
		case EAsyncComputeBudget::EAll_4:			
		default:
			CUMask = (uint16)0xff;
			WavesPerSh = 0;
			break;
	}

	if (GPS4AsyncComputeBudgetMode == 1)
	{
		ComputeContext.m_dcb.setComputeShaderControl(WavesPerSh, ThreadGroupsPerCU, LockThreshhold);
	}
	else
	{
		if (sce::Gnm::getGpuMode() == sce::Gnm::kGpuModeBase)
		{
			ComputeContext.m_dcb.setComputeResourceManagementForBase(Gnm::kShaderEngine0, CUMask);
			ComputeContext.m_dcb.setComputeResourceManagementForBase(Gnm::kShaderEngine1, CUMask);
		}
		else
		{
			ComputeContext.m_dcb.setComputeResourceManagementForNeo(Gnm::kShaderEngine0, CUMask);
			ComputeContext.m_dcb.setComputeResourceManagementForNeo(Gnm::kShaderEngine1, CUMask);
			ComputeContext.m_dcb.setComputeResourceManagementForNeo(Gnm::kShaderEngine2, CUMask);
			ComputeContext.m_dcb.setComputeResourceManagementForNeo(Gnm::kShaderEngine3, CUMask);
		}
	}
}

void FGnmComputeCommandListContext::RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFenceRHI)
{
	const uint32 CurrentFrame = GGnmManager.GetFrameCount();
	SCOPED_RHI_DRAW_EVENTF(*this, RHITransitionResources, TEXT("TransitionTo%i: %i UAVs"), (int32)TransitionType, NumUAVs);
	for (int32 i = 0; i < NumUAVs; ++i)
	{
		if (InUAVs[i])
		{
			FGnmUnorderedAccessView* UAV = ResourceCast(InUAVs[i]);
			UAV->SetResourceAccess(TransitionType);

			if (TransitionType != EResourceTransitionAccess::ERWNoBarrier)
			{
				UAV->SetResourceDirty(false, CurrentFrame);
			}
		}
	}
	
	Gnm::CacheAction CacheAction = Gnm::kCacheActionWriteBackAndInvalidateL1andL2;

	volatile uint32_t* label = (volatile uint32_t*)ComputeContext.allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
	*label = 0x0;

	// NOTE: kEopCbDbReadsDone and kEopCsDone are two names for the same value, so this EOP event does cover both graphics and compute
	// use cases.
	ComputeContext.writeReleaseMemEvent(Gnm::kReleaseMemEventCsDone, Gnm::kEventWriteDestMemory, const_cast<uint32_t*>(label),
		Gnm::kEventWriteSource32BitsImmediate, 0x1, CacheAction, Gnm::kCachePolicyLru);	
	ComputeContext.waitOnAddress(const_cast<uint32_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
	

	FGnmComputeFence* WriteComputeFence = ResourceCast(WriteComputeFenceRHI);
	if (WriteComputeFence)
	{		
		WriteComputeFence->WriteFenceGPU(*this, false);
	}	
}

void FGnmComputeCommandListContext::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{	
	SetTexture(NewTextureRHI, TextureIndex);
}

void FGnmComputeCommandListContext::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FGnmSamplerState* NewState = ResourceCast(NewStateRHI);	
	ComputeContext.setSamplers(SamplerIndex, 1, &NewState->Sampler);
}

void FGnmComputeCommandListContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI)
{
	FGnmUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	check(UAVIndex < FGnmContextCommon::MaxBoundUAVs);

	if (UAV)
	{
		//check it's safe for r/w for this UAV
		const EResourceTransitionAccess CurrentUAVAccess = UAV->GetResourceAccess();
		const bool UAVDirty = UAV->IsResourceDirty();
		ensureMsgf(!UAVDirty || (CurrentUAVAccess == EResourceTransitionAccess::ERWNoBarrier), TEXT("UAV: %i is in unsafe state for GPU R/W: %s, Dirty: %i"), UAVIndex, *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)CurrentUAVAccess], (int32)UAVDirty);

		//UAVs always dirty themselves. If a shader wanted to just read, it should use an SRV.
		UAV->SetResourceDirty(true, GGnmManager.GetFrameCount());
	}

	BindUAV(UAV, UAVIndex, -1, false);
}

void FGnmComputeCommandListContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount)
{
	FGnmUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	check(UAVIndex < FGnmContextCommon::MaxBoundUAVs);

	if (UAV)
	{
		//check it's safe for r/w for this UAV
		const EResourceTransitionAccess CurrentUAVAccess = UAV->GetResourceAccess();
		const bool UAVDirty = UAV->IsResourceDirty();
		ensureMsgf(!UAVDirty || (CurrentUAVAccess == EResourceTransitionAccess::ERWNoBarrier), TEXT("UAV: %i is in unsafe state for GPU R/W: %s, Dirty: %i"), UAVIndex, *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)CurrentUAVAccess], (int32)UAVDirty);

		//UAVs always dirty themselves. If a shader wanted to just read, it should use an SRV.
		UAV->SetResourceDirty(true, GGnmManager.GetFrameCount());
	}

	// Store here and trigger a flush because counters might be getting cleared in sequence on the same UAV Index without a draw call or a dispatch to trigger the 
	// other automatic stores.  This problem will go away when we move to static GDS allocation for UAV groupings.
	//StoreBoundUAVs(true, false);
	BindUAV(UAV, UAVIndex, InitialCount, true);
}

void FGnmComputeCommandListContext::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	SetSRVForStage(SRVRHI, SamplerIndex, Gnm::kShaderStageCs);
}

void FGnmComputeCommandListContext::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FGnmUniformBuffer* Buffer = ResourceCast(BufferRHI);
	if (Buffer && Buffer->IsConstantBuffer())
	{
		Buffer->Set(*this, BufferIndex);
	}
	else
	{
		GGnmManager.DummyConstantBuffer->Set(*this, BufferIndex);
	}

	BoundUniformBuffers[BufferIndex] = BufferRHI;
	DirtyUniformBuffers |= (1 << BufferIndex);
}

void FGnmComputeCommandListContext::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	UpdateCSConstant(BufferIndex, NewValue, BaseIndex, NumBytes);
}

void FGnmComputeCommandListContext::RHIPushEvent(const TCHAR* Name, FColor Color)
{
	ComputeContext.pushMarker(TCHAR_TO_ANSI(Name), Color.DWColor());
}

void FGnmComputeCommandListContext::RHIPopEvent()
{
	ComputeContext.popMarker();
}

void FGnmComputeCommandListContext::RHISubmitCommandsHint()
{
	//break off a submit as long as we have any.
	SubmitCurrentCommands(1);
}

bool FGnmComputeCommandListContext::SubmitCurrentCommands(uint32 MinimumCommandByes)
{
	if (IsImmediate())
	{
		const int32 CommandBytes = (ComputeContext.m_dcb.m_cmdptr - ComputeContext.m_currentDcbSubmissionStart) * sizeof(uint32);
		if (CommandBytes >= MinimumCommandByes)
		{
			PrepareCurrentCommands();
			GGnmManager.AddComputeSubmission(ComputeSubmission);
			ComputeSubmission.Reset();
			return true;
		}
	}
	return false;
}

void FGnmComputeCommandListContext::PrepareCurrentCommands()
{
	//add the current submission to the list.
	ComputeContext.m_submissionRanges[ComputeContext.m_submissionCount].m_dcbStartDwordOffset = (uint32_t)(ComputeContext.m_currentDcbSubmissionStart - ComputeContext.m_dcb.m_beginptr);
	ComputeContext.m_submissionRanges[ComputeContext.m_submissionCount].m_dcbSizeInDwords = (uint32_t)(ComputeContext.m_dcb.m_cmdptr - ComputeContext.m_currentDcbSubmissionStart);
	ComputeContext.m_submissionCount++;
	check(ComputeContext.m_submissionCount <= Gnmx::ComputeContext::kMaxNumStoredSubmissions);

	//Gnmx stores the ranges as offsets from the start ptr which means we must store off actual addresses
	{
		const uint32* DCBSubmissionStart = ComputeContext.m_currentDcbSubmissionStart;
		const auto& SubmissionRanges = ComputeContext.m_submissionRanges;		
		for (int32 i = 0; i < ComputeContext.m_submissionCount; ++i)
		{
			void* SubmissionStart = (void*)&(DCBSubmissionStart[SubmissionRanges[i].m_dcbStartDwordOffset]);
			uint32 SubmissionSizeInBytes = SubmissionRanges[i].m_dcbSizeInDwords * sizeof(uint32);
			ComputeSubmission.AddSubmissionToQueue(SubmissionStart, SubmissionSizeInBytes);
		}
		ComputeContext.m_submissionCount = 0;
	}

	ComputeContext.m_currentDcbSubmissionStart = ComputeContext.m_dcb.m_cmdptr;
	ComputeContext.m_dcb.m_beginptr = ComputeContext.m_dcb.m_cmdptr;
}

void FGnmComputeCommandListContext::AllocateGlobalResourceTable()
{

	// Allocate the Global Resource table from the command buffer
	void* GlobalResourceTable = ComputeContext.allocateFromCommandBuffer(SCE_GNM_SHADER_GLOBAL_TABLE_SIZE, Gnm::kEmbeddedDataAlignment4);
	ComputeContext.m_lwcue.setGlobalResourceTableAddr(GlobalResourceTable);

	// Set initial values
	//GGnmManager.GraphicShaderScratchBuffer.Commit(*GnmContext);
	//GGnmManager.ComputeShaderScratchBuffer.Commit(*GnmContext);
}