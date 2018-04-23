// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmResources.h: Gnm resource RHI definitions.
=============================================================================*/

#pragma once

#include "GPUDefragAllocator.h"

// Set this to 1 to check for volatile/singleframe buffers being used over multiple frames.  only valid without RHIThread because
// the cached framenumbers could be off with it on.
#define ENABLE_BUFFER_INTEGRITY_CHECK 0

#define ENABLE_OPTIONAL_SHADER_INPUT_OUTPUT_INFO	0

#define PS4_USE_NEW_MULTIBUFFER (USE_NEW_PS4_MEMORY_SYSTEM && 1)

#include "BoundShaderStateCache.h"
#include "GnmShaderResources.h"

class FGnmCommandListContext;
class FGnmComputeCommandListContext;

class FGnmDefraggable
{
public:
	FGnmDefraggable(){}
	
	virtual bool CanRelocate() const = 0;

#if VALIDATE_MEMORY_PROTECTION
	virtual bool NeedsGPUWrite() const
	{
		return false;
	}
#endif

	/**
	 * Update texture to point to new location when defragging
	 */
	virtual void UpdateBaseAddress(void* NewBaseAddress) = 0;
	virtual void* GetBaseAddress() = 0;
};

class FGnmBaseShaderResource
{
public:
	FGnmBaseShaderResource()
		: CurrentGPUAccess(EResourceTransitionAccess::EReadable)
		, LastFrameWritten(-1)
		, bDirty(false)
	{}

	void SetCurrentGPUAccess(EResourceTransitionAccess Access)
	{
		if (Access == EResourceTransitionAccess::EReadable)
		{
			bDirty = false;
		}		
		CurrentGPUAccess = Access;
	}

	EResourceTransitionAccess GetCurrentGPUAccess() const
	{
		return CurrentGPUAccess;
	}

	uint32 GetLastFrameWritten() const
	{
		return LastFrameWritten;
	}

	void SetDirty(bool bInDirty, uint32 CurrentFrame)
	{
		bDirty = bInDirty;
		if (bDirty)
		{
			LastFrameWritten = CurrentFrame;
		}
		ensureMsgf(IsRunningRHIInSeparateThread() || !(CurrentGPUAccess == EResourceTransitionAccess::EReadable && bDirty), TEXT("ShaderResource is dirty, but set to Readable."));
	}

	bool IsDirty() const
	{
		return bDirty;
	}

private:

	/** Whether the current resource is logically GPU readable or writable.  Mostly for validation for newer RHI's*/
	EResourceTransitionAccess CurrentGPUAccess;

	/** Most recent frame this resource was written to. */
	uint32 LastFrameWritten;

	/** Resource has been written to without a subsequent read barrier.  Mostly for UAVs */
	bool bDirty;
};

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FGnmVertexDeclaration : public FRHIVertexDeclaration
{
public:

	/** Cached element info array (offset, stream index, etc) */
	FVertexDeclarationElementList Elements;

	uint16 StreamStrides[MaxVertexElementCount];

	/** Initialization constructor. */
	FGnmVertexDeclaration(const FVertexDeclarationElementList& InElements);
};

template<typename BaseResourceType, typename ShaderType>
class TGnmBaseShader : public BaseResourceType, public IRefCountedObject
{
public:
	/** This is the bytecode as well as the semantic info loaded from the .sb file */
	Shader::Binary::Program Program;

#if ENABLE_OPTIONAL_SHADER_INPUT_OUTPUT_INFO
	TArray<FGnmShaderAttributeInfo> DebugInputAttributes;
	TArray<FGnmShaderAttributeInfo> DebugOutputAttributes;
#endif

	// LCUE
	LCUE::InputResourceOffsets	ShaderOffsets;

	/** Copy of the microcode as the caller might free it up from us */
	TArray<uint8> Microcode;

	/** The shader object */
	// @todo gnm: The samples never free these objects, it's not clear how to free the memory, or do they just point in to ShaderMemory?
	ShaderType* Shader;

	/** Memory for the shader */
	FMemBlock ShaderMemory;

	FGnmShaderResourceTable ShaderResourceTable;

	/** Whether this shader uses the global constant buffer */
	bool bShaderNeedsGlobalConstantBuffer;

	/** Initialization constructor. */
	TGnmBaseShader();

	/** Initialization constructor. */
	void Init(const void* InCodePtr, uint32 InCodeSize, bool bInShaderNeedsGlobalConstantBuffer);

	/** Destructor */
	virtual ~TGnmBaseShader();

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

	typedef TGnmBaseShader<BaseResourceType, IRefCountedObject> TParent;
};

typedef TGnmBaseShader<FRHIPixelShader, Gnmx::PsShader> FGnmPixelShader;
typedef TGnmBaseShader<FRHIHullShader, Gnmx::HsShader> FGnmHullShader;
typedef TGnmBaseShader<FRHIDomainShader, Gnmx::VsShader> FGnmDomainShader;
typedef TGnmBaseShader<FRHIComputeShader, Gnmx::CsShader> FGnmComputeShader;


/**
 * Vertex shader needs some extra members
 */
class FGnmVertexShader : public TGnmBaseShader<FRHIVertexShader, Gnmx::VsShader>
{
public:
	/** Vertex the export shader version of vertex shader, this will be set instead of the base member */
	Gnmx::EsShader* ExportShader;

	LCUE::InputResourceOffsets	ExportShaderOffsets;

	/** Which stage this shader is for (vertex, export, or eventually local) */
	Gnm::ShaderStage ShaderStage;

	/** Constructor/destructor */
	FGnmVertexShader();
};

/**
 * Geometry shader needs some extra members
 */
class FGnmGeometryShader : public TGnmBaseShader<FRHIGeometryShader, Gnmx::GsShader>
{
public:
	/** Memory for the vertex shader portion of the geometry shader */
	FMemBlock VertexShaderMemory;

	/** Constructor/destructor */
	FGnmGeometryShader() {}
	virtual ~FGnmGeometryShader();
};

/**
 * Combined shader state and vertex definition for rendering geometry. 
 * Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
 */
class FGnmBoundShaderState : public FRHIBoundShaderState
{
public:

#if PS4_SUPPORTS_PARALLEL_RHI_EXECUTE
	FCachedBoundShaderStateLink_Threadsafe CacheLink;
#else
	FCachedBoundShaderStateLink CacheLink;
#endif

	uint16 StreamStrides[MaxVertexElementCount];

	/** For shaders that need a fetch shader, this is the address of it */
	FMemBlock FetchShader;

	/** LCUE needs a patched InputResourceOffsetTable for the vertex shader that is unique to each semantic remap table */
	 LCUE::InputResourceOffsets RemappedVSInputResourceOffsets;	 
	 TArray<int32> VSMappingTable;


	/** This is a bit of the fetch shader that needs to be pulled out, so it doesn't get modified at usage time (not thread safe otherwise) */
	uint32 FetchShaderModifier;

	/** Cached vertex structure */
	TRefCountPtr<FGnmVertexDeclaration> VertexDeclaration;

	/** The number of 1KB chunks per wave for scratch mem (set when the shaders are set) */
	uint32 Num1KbyteScratchChunksPerWave;

	/** Whether each shader type uses the global constant buffer */
	bool bShaderNeedsGlobalConstantBuffer[SF_NumFrequencies];

	/** Cached shaders */
	TRefCountPtr<FGnmVertexShader> VertexShader;
	TRefCountPtr<FGnmPixelShader> PixelShader;
	TRefCountPtr<FGnmHullShader> HullShader;
	TRefCountPtr<FGnmDomainShader> DomainShader;
	TRefCountPtr<FGnmGeometryShader> GeometryShader;

	/** Initialization constructor. */
	FGnmBoundShaderState(
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI);

	/**
	 *Destructor
	 */
	~FGnmBoundShaderState();
};


#define MULTIBUFFER_OVERRUN_DETECTION 0

//multibuffer cached lock detection code is not compatible with gpudefrag's VALIDATE_MEMORY_PROTECTION as they will fight over 
//memory protection privileges.
#define MULTIBUFFER_CACHED_LOCK_DETECTION 0 //(MULTIBUFFER_OVERRUN_DETECTION && !VALIDATE_MEMORY_PROTECTION)

#if PS4_USE_NEW_MULTIBUFFER

/**
 * This class manages most buffer resource types. They can be locked/unlocked arbitrarily.
 * Extra memory is allocated dynamically, and deferred-deleted to ensure the GPU is done with it.
 */
class FGnmMultiBufferResource : public FGnmBaseShaderResource
{
public:
	/** Constructor */
	FGnmMultiBufferResource(uint32 Size, uint32 Usage, TStatId StatGPU, TStatId StatCPU);

	/** Destructor */
	virtual ~FGnmMultiBufferResource();

	/**
	 * Gets the current buffer pointer for the RT or RHIT.
	 */
	void* GetCurrentBuffer();

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(FRHICommandListImmediate& RHICmdList, EResourceLockMode LockMode, uint32 Size, uint32 Offset);

	/**
	 * Commits the last prepared buffer as the current buffer to use on the GPU.
	 * Any existing GPU buffer is placed in a deferred delete queue, so it is
	 * freed once the GPU has finished using it.
	 */
	void Unlock(FRHICommandListImmediate& RHICmdList);

private:
	FGnmMultiBufferResource(const FGnmMultiBufferResource&) = delete;
	FGnmMultiBufferResource(FGnmMultiBufferResource&&) = delete;
	FGnmMultiBufferResource& operator=(const FGnmMultiBufferResource&) = delete;
	FGnmMultiBufferResource& operator=(FGnmMultiBufferResource&&) = delete;

	const uint32 CreatedSize;
	const EGnmMemType MemoryType;
	const TStatId StatId;

	FMemBlock CurrentBufferRT;
	FMemBlock CurrentBufferRHIT;

	bool bLocked;
	bool bFirstLock;
};

#else

/**
 * This class helps manage resource types that may need multiple buffers
 * to handle dynamic types (like a dynamic vertex buffer for particles)
 * It may only have one buffer, but it can support more than one as well
 */
class FGnmMultiBufferResource : public FGnmBaseShaderResource
{
public:
	/** Constructor */
	FGnmMultiBufferResource(uint32 Size, uint32 Usage, TStatId StatGPU, TStatId StatCPU);

	/** Destructor */
	virtual ~FGnmMultiBufferResource();

	/**
	 * Gets the current buffer, and potentially increments the current buffer
	 * so that locking (with write access) will not overwrite the buffer(s)
	 * currently used by the GPU
	 *
	 * @param bNeedWriteAcccess If true, will return the next buffer, or a fresh one depending on usage type
	 * @param bool bUpdatePointer If true, CurrentBuffer or memblock pointer will be updated.
	 * @param OverrideSizeForZeroBuffer If this is non-zero, then zero buffer resources will allocate this much space instead of the originally requested amount
	 *
	 * @param The memory for the buffer
	 */
	void* GetCurrentBuffer(bool bNeedWriteAccess = false, bool bUpdatePointer = false, bool bDefragLock = false);

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(EResourceLockMode LockMode, uint32 Size, uint32 Offset);

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void Unlock();

	/** Prepare a CPU accessible buffer for without modifying the resource object */
	void* LockDeferredUpdate(EResourceLockMode LockMode, uint32 Size, uint32 Offset);

	/** Modify the buffer to point to the updated data */
	void UnlockDeferredUpdate(void* BufferData);

	uint32 GetBufferSize() const
	{
		return Buffers[0].GetMemBlock().GetSize();
	}	

private:

	FGnmMultiBufferResource(const FGnmMultiBufferResource& OtherBuffer)
	{
		check(false);
	}

	class FMultiBufferInternalDefrag : public FGnmDefraggable
	{
	public:

		FMultiBufferInternalDefrag(FGnmMultiBufferResource* InOwner, int32 InBufferIndex, bool InMayNeedGPUWrite);

		~FMultiBufferInternalDefrag()
		{
			Free();
		}

		virtual bool CanRelocate() const override;
		virtual void* GetBaseAddress() override;
		virtual void UpdateBaseAddress(void* NewBaseAddress) override;

#if VALIDATE_MEMORY_PROTECTION
		virtual bool NeedsGPUWrite() const override
		{
			return bMayNeedGPUWrite;
		}		
#endif

		void Allocate(uint32 Size, EGnmMemType MemoryType, bool bAllowDefrag, TStatId Stat);
		void Free();

		void* Lock();
		void Unlock();

		const FMemBlock& GetMemBlock() const
		{
#if MULTIBUFFER_OVERRUN_DETECTION
			CheckCanary();
#endif
			return MemBlock;
		}

		FMemBlock& GetMemBlock()
		{
#if MULTIBUFFER_OVERRUN_DETECTION
			CheckCanary();
#endif
			return MemBlock;
		}

		FGnmMultiBufferResource* Owner;
		
		int32 BufferIndex;
		bool bDefraggable;

	private:

#if MULTIBUFFER_CACHED_LOCK_DETECTION
		void ProtectFromCPUAccess();
		void AllowCPUAccess();
#endif

#if MULTIBUFFER_OVERRUN_DETECTION
		void CheckCanary(bool bForce = false) const;
#endif

		FMemBlock MemBlock;

#if MULTIBUFFER_OVERRUN_DETECTION
		int32 LockCount;
		bool bLocallyOwned;
		static const int32 CanaryValue = 0xDEADBEEF;
#endif

#if VALIDATE_MEMORY_PROTECTION
		bool bMayNeedGPUWrite;
#endif

		FMultiBufferInternalDefrag(const FGnmMultiBufferResource& Other)
		{
			check(false);
		}
	};

	/** Array of defrag handlers for defraggable buffers */
	TArray<FMultiBufferInternalDefrag, TInlineAllocator<1>> Buffers;

	/** The current buffer for reading. Locking will increment this */
	uint16 CurrentBuffer;

	bool bMultiBufferDefraggable;
	bool bVolatile;

#if ENABLE_BUFFER_INTEGRITY_CHECK
	/** The frame the buffer was allocated on */
	uint32 Buffer0AllocationFrame;
#endif	
};

#endif // PS4_USE_NEW_MULTIBUFFER == 0

/** Texture/RT wrapper. */
class FGnmSurface : public FGnmBaseShaderResource, public FGnmDefraggable
{
public:

	/** The texture object */
	Gnm::Texture* Texture;

	/** The render target object for color buffers */
	Gnm::RenderTarget* ColorBuffer;

	/** The render target object for depth buffers */
	Gnm::DepthRenderTarget* DepthBuffer;

	FRHITexture* RHITexture;

	/** The number of slices the render target has */
	uint32 NumSlices;

	Gnm::NumSamples NumSamples;

	/** Was this surface fast cleared in a way that needs to be resolved later?  Only valid in single-threaded mode */
	bool bNeedsFastClearResolve;

	/** We need to tile generated textures during unlock */
	bool bNeedsToTileOnUnlock;

	/** If this is true, the next unlock won't block the CPU until the GPU is idle */
	bool bSkipBlockOnUnlock;

	/** True if this texture is a streamable 2D texture, that should count towards the texture streaming budget. */
	bool bStreamable;

	/** Which cache will this surface need to flush? (Cb0, Cb1, Db, etc) */
	int32 CacheToFlushOnResolve;

	/** The memory for raw untiled memory, that will be tiled into GPU accessible memory */
	void* UntiledLockedMemory;

	/** The address that the render target was bound to (can vary for rendering to a mip) */
	void* LastUsedRenderTargetAddr;	

	uint32 LastFrameFastCleared;

	/** 
	 * Constructor that will create Texture and Color/DepthBuffers as needed
	 */
	FGnmSurface(ERHIResourceType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 Flags, const FClearValueBinding& ClearValue, FResourceBulkDataInterface* BulkData, FRHITexture* InRHITexture);

	/**
	 * Constructor to alias an existing texture.  Nothing is allocated.
	 */
	FGnmSurface(ERHIResourceType ResourceType, const Gnm::Texture& GnmTexture, EResourceTransitionAccess InGPUAccess, bool bCreateRenderTarget = true);

	/**
	 * Destructor
	 */
	~FGnmSurface();


	/**
	 * @return the amount of memory allocated for this texture
	 */
	int32 GetMemorySize(bool bAllMemory=true) const
	{
		return BufferMem.GetSize() + (bAllMemory ? (StencilMem.GetSize() + HTileMem.GetSize() + CMaskMem.GetSize()) : 0);
	}

	uint32 GetMemoryAlign() const
	{
		return AlignedSize.m_align;
	}

	/**
	 * Locks one of the texture's mip-maps.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 * @return A pointer to the specified texture data.
	 */
	void* Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride);

	/** Unlocks a previously locked mip-map.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 */
	void Unlock(uint32 MipIndex, uint32 ArrayIndex);

	/*
	 *  Allocates duplicate memory for the texture to be filled in by the caller.  On Unlockdeferred the Surface's base address will be swapped
	 *  and the old memory deleted in a deferred fashion.  Only supports RLM_WriteOnly as the memory returned will be uninitialized
	 *  Only supports textures with a single mip.
	 */
	void* LockDeferred(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride);

	/*
	*  Swaps surface baseaddress with incoming Data.  Data should have come from a call to LockDeferred
	*  Old memory will be deleted in a deferred fashion.
	*/
	void UnlockDeferred(void* Data, uint32 MipIndex, uint32 ArrayIndex);


	/**
	 * Loads pixel data from memory into texture. This assumes all padding and formatting of the data is already correct.
	 */
	void LoadPixelDataFromMemory(const void* PixelData, uint32 PixelDataSize);

	/**
	 * Set the surface as having a CopyToResolveTarget in flight
	 */
	void BeginCopyToResolveTarget();

	/**
	 * Set the surface as having its CopyToResolveTarget completed by the GPU
	 */
	void EndCopyToResolveTarget(FGnmCommandListContext& GnmCommandContext);

	/**
	 * If there's a labeled CopyToResolveTarget in flight, this will block until it's done, otherwise, return immediately
	 */
	void BlockUntilCopyToResolveTargetComplete();

	void SetFastCleared(uint32 CurrentFrame)
	{
		LastFrameFastCleared = CurrentFrame;
	}

	uint32 GetFastCleared()
	{
		return LastFrameFastCleared;
	}

	bool IsDefraggable() const
	{
		return bDefraggable;
	}

	virtual void* GetBaseAddress() override;
	virtual void UpdateBaseAddress(void* NewBaseAddress) override;
	virtual bool CanRelocate() const override
	{
		return bDefraggable;
	}

#if VALIDATE_MEMORY_PROTECTION
	virtual bool NeedsGPUWrite() const override
	{
		return bNeedsGPUWrite;
	}
#endif

	const FMemBlock &GetCMaskMem()	const	{ return CMaskMem; }
	const bool GetSkipEliminate()	const	{ return bSkipEliminate; }
protected:

	bool TryAllocateDefraggable(uint32 AllocationSize, uint32 Alignment, TStatId StatId);
	static void ConvertColorToCMASKBits(sce::Gnm::DataFormat DataFormat, const FLinearColor& Color, uint32* Bits);

	enum class ECopyToResolveState
	{
		Valid = 0,
		Pending = 1
	};

	/** Various memory blocks */
	FMemBlock BufferMasksMem;	// To work around a bug with CMask corruption the Buffer, CMask, and FMask are allocated in one block
	FMemBlock BufferMem;
	FMemBlock StencilMem;
	FMemBlock HTileMem;
	FMemBlock CMaskMem;
	FMemBlock FMaskMem;
	void* DefragAddress;

	/** Amount of memory (with alignment) allocated by this texture */
	Gnm::SizeAlign AlignedSize;
	Gnm::SizeAlign CMaskAlignedSize;
	Gnm::SizeAlign FMaskAlignedSize;

	/** For textures created with TexCreate_CPUReadback, this label is used to determine when a RHICopyToResolveTarget has completed so the CPU can read it */
	volatile uint64* CopyToResolveTargetLabel;

	// The frame the copy to resolve was put into a command buffer. */
	uint32 FrameSubmitted;

	bool bDefraggable;
	bool bSkipEliminate;

#if VALIDATE_MEMORY_PROTECTION
	bool bNeedsGPUWrite;
#endif

	friend class FGnmManager;
};

class FGnmTexture2D : public FRHITexture2D
{
public:
	/** The surface info */
	FGnmSurface Surface;

	// Constructor, just calls base and Surface constructor
	FGnmTexture2D(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITexture2D(SizeX, SizeY, NumMips, NumSamples, Format, Flags, InClearValue)
		, Surface(RRT_Texture2D, Format, SizeX, SizeY, 1, /*bArray=*/ false, 1, NumMips, NumSamples, Flags, InClearValue, BulkData, this)
	{		
	}

	//constructor to create an RHI texture from a GnmTexture managed by a separate library (like AvPlayer).
	FGnmTexture2D(const Gnm::Texture& GnmTexture, EResourceTransitionAccess InGPUAccess, bool bCreateRenderTarget = true);

	virtual void* GetNativeResource() const override final
	{
		return (void *)&Surface;
	}
};

class FGnmTexture2DArray : public FRHITexture2DArray
{
public:
	/** The surface info */
	FGnmSurface Surface;

	// Constructor, just calls base and Surface constructor
	FGnmTexture2DArray(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITexture2DArray(SizeX, SizeY, ArraySize, NumMips, Format, Flags, InClearValue)
		, Surface(RRT_Texture2DArray, Format, SizeX, SizeY, 1, /*bArray=*/ true, ArraySize, NumMips, 1, Flags, InClearValue, BulkData, this)
	{
	}

	virtual void* GetNativeResource() const override final
	{
		return (void *)&Surface;
	}
};

class FGnmTexture3D : public FRHITexture3D
{
public:
	/** The surface info */
	FGnmSurface Surface;

	// Constructor, just calls base and Surface constructor
	FGnmTexture3D(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITexture3D(SizeX, SizeY, SizeZ, NumMips, Format, Flags, InClearValue)
		, Surface(RRT_Texture3D, Format, SizeX, SizeY, SizeZ, /*bArray=*/ false, 1, NumMips, 1, Flags, InClearValue, BulkData, this)
	{
	}

	virtual void* GetNativeResource() const override final
	{
		return (void *)&Surface;
	}
};

class FGnmTextureCube : public FRHITextureCube
{
public:
	/** The surface info */
	FGnmSurface Surface;

	// Constructor, just calls base and Surface constructor
	FGnmTextureCube(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITextureCube(Size, NumMips, Format, Flags, InClearValue)
		, Surface(RRT_TextureCube, Format, Size, Size, 6, bArray, ArraySize, NumMips, 1, Flags, InClearValue, BulkData, this)
	{
	}

	virtual void* GetNativeResource() const override final
	{
		return (void *)&Surface;
	}
};

/** Given a pointer to a RHI texture that was created by the Gnm RHI, returns a pointer to the FGnmTextureBase it encapsulates. */
FORCEINLINE_DEBUGGABLE FGnmSurface& GetGnmSurfaceFromRHITexture(FRHITexture* Texture)
{
	FGnmSurface* Result = (FGnmSurface*)Texture->GetNativeResource();
	check(Result);
	return *Result;
}


/** Gnm occlusion query */
class FGnmRenderQuery : public FRHIRenderQuery
{
public:

	/** Container for the Results pointer */
	FMemBlock ResultsMemory;

	/** Query results object that can be written to by GPU, read from CPU */
	Gnm::OcclusionQueryResults* OcclusionQueryResults;

	uint64* TimeStampResults;

	/** The cached query result. */
	uint64 Result;

	/** true if the query's result is cached. */
	bool bResultIsCached : 1;

	/* true if the query has been put into a command buffer. */
	bool bSubmitted : 1;

	/* frame the query was put into a command buffer. */
	uint32 FrameSubmitted;

	// todo: memory optimize
	ERenderQueryType QueryType;

	/** Initialization constructor. */
	FGnmRenderQuery(ERenderQueryType InQueryType);

	~FGnmRenderQuery();

	/**
	 * Kick off an occlusion test 
	 */
	void Begin(FGnmCommandListContext& GnmCommandContext);

	/**
	 * Finish up an occlusion test 
	 */
	void End(FGnmCommandListContext& GnmCommandContext);

	bool BlockUntilComplete(bool& bSuccess);
};

/** Index buffer resource class that stores stride information. */
class FGnmIndexBuffer : public FGnmMultiBufferResource, public FRHIIndexBuffer
{
public:

	/** Size of each index */
	Gnm::IndexSize IndexSize;

	/** Constructor */
	FGnmIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage);
};


/** Vertex buffer resource class that stores usage type. */
class FGnmVertexBuffer : public FGnmMultiBufferResource, public FRHIVertexBuffer
{
public:

	/** Constructor */
	FGnmVertexBuffer(uint32 InSize, uint32 InUsage);
};

class FGnmUniformBuffer : public FRHIUniformBuffer
{
public:
	/** Resource table containing RHI references. */
	TArray<TRefCountPtr<FRHIResource> > ResourceTable;

	// Constructor
	FGnmUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& InLayout, EUniformBufferUsage Usage);

	// Destructor 
	~FGnmUniformBuffer();

	/**
	 * Push the uniforms to the GPU 
	 * @param Stage The shader stage (Vs, Ps, etc) to set the buffer for
	 * @param BufferIndex Which buffer to send to
	 */
	void Set(FGnmCommandListContext& CommandListContext, Gnm::ShaderStage Stage, uint32 BufferIndex);
	void Set(FGnmComputeCommandListContext& CommandListContext, uint32 BufferIndex);

	bool IsConstantBuffer() const
	{
		return ConstantBufferSize > 0;
	}

private:
	// for storage (multi use) buffers, this is the permanent storage that needs to be freed on destruction
	FMemBlock UniformBufferMemory;

	// the Gnm buffer descriptor
	Gnm::Buffer Buffer;

	uint32 ConstantBufferSize;

#if ENABLE_BUFFER_INTEGRITY_CHECK
	/** The frame the buffer was allocated on */
	uint32 AllocationFrame;
#endif
};


class FGnmStructuredBuffer : public FRHIStructuredBuffer, public FGnmBaseShaderResource
{
public:
	// Constructor
	explicit FGnmStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 InUsage);

	// This constructor taskes a pointer directly; it never makes a copy of the data and assumes the caller manages the memory
	explicit FGnmStructuredBuffer(uint32 Stride, uint32 Size, void *Ptr, uint32 Usage);

	// Destructor
	~FGnmStructuredBuffer();

	/**
	 * @return the CPU address at an optional offset
	 */
	void* GetCPUAddress(uint32 Offset=0)
	{
		return (uint8*)BufferMemory.GetPointer() + Offset;
	}

	// the GPU memory
	FMemBlock BufferMemory;

	// the Gnm buffer descriptor
	Gnm::Buffer Buffer;

	bool bOwnsMemory;
};



class FGnmUnorderedAccessView : public FRHIUnorderedAccessView
{
public:
	
	FGnmUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBufferRHI, bool bUseUAVCounter, bool bAppendBuffer);
	FGnmUnorderedAccessView(FTextureRHIParamRef TextureRHI, uint32 MipLevel = 0);
	FGnmUnorderedAccessView(FVertexBufferRHIParamRef VertexBufferRHI, uint8 Format);

	virtual ~FGnmUnorderedAccessView();

	/** 
	 * Set this UAV into a shader slot
	 */
	void Set(FGnmCommandListContext& GnmCommandContext, Gnm::ShaderStage Stage, uint32 ResourceIndex, bool bUploadValue);
	void Set(FGnmComputeCommandListContext& GnmComputeContext, uint32 ResourceIndex, bool bUploadValue);

	/**
	* Accessors for the current resource transition state.  Only meaningful when parallel rendering is disabled.  Used for validation only.
	*/
	void SetResourceAccess(EResourceTransitionAccess InAccess);
	EResourceTransitionAccess GetResourceAccess() const;

	void SetResourceDirty(bool bDirty, uint32 CurrentFrame);
	bool IsResourceDirty() const;

	/**
	 * Set this UAV into a shader slot and prime the GDS counter with a specific value
	 */
	void SetAndClearCounter(FGnmCommandListContext& GnmCommandContext, Gnm::ShaderStage Stage, uint32 ResourceIndex, uint32 CounterVal);

	/**
	 * Stores the current counter value at the given index internally.
	 */
	void StoreCurrentCounterValue(FGnmCommandListContext& GnmCommandContext, uint32 ResourceIndex);

	/**
	 * Buffer descriptor needs to be updated to the current address of the base resource.  Base resource can change due to being
	 * dynamic, defragging, etc.
	 */
	void UpdateBufferDescriptor();

	// the potential resources to refer to with the UAV object
	TRefCountPtr<FGnmStructuredBuffer> SourceStructuredBuffer;
	TRefCountPtr<FGnmVertexBuffer> SourceVertexBuffer;
	TRefCountPtr<FRHITexture> SourceTexture;

	// number of components (1-4) in this buffer. zero if unknown
	uint32 NumComponents;

	// The Gnm buffer descriptor, initialized from structured buffer, vertex buffer, or a texture
	Gnm::Buffer Buffer;

	// Location where we DMA the current state of the append/consume counter.
	// The CPU must NEVER write to this except on initialization since the GPU could read/write to it at any time.
	FMemBlock DMAValueTargetBlock;
	int32* DMAValueTarget;

	// The mip level to target for the UAV
	uint32 SourceTextureMipLevel;
	bool bUseUAVCounters;
};


class FGnmShaderResourceView : public FRHIShaderResourceView
{
public:

	/**
	* Accessors for the current resource transition state.  Only meaningful when parallel rendering is disabled.  Used for validation only.
	*/
	void SetResourceAccess(EResourceTransitionAccess InAccess);
	EResourceTransitionAccess GetResourceAccess() const;

	bool IsResourceDirty() const;
	
	uint32 GetLastFrameWritten();

	// the Gnm buffer descriptor, just points to another resources allocated memory
	Gnm::Buffer Buffer;

	// The vertex buffer this SRV comes from (can be null)
	TRefCountPtr<FGnmVertexBuffer> SourceVertexBuffer;

	// The texture that this SRV come from
	TRefCountPtr<FRHITexture> SourceTexture;

	// The structured buffer that this SRV come from
	TRefCountPtr<FGnmStructuredBuffer> SourceStructuredBuffer;

	// Our view of the texture (may be different from source texture, eg min/max mip etc)
	Gnm::Texture Texture;

	~FGnmShaderResourceView();
};

class FGnmComputeFence : public FRHIComputeFence
{
public:

	/**
	* Constructor
	*/
	FGnmComputeFence(FName InName, uint64* InLabelLoc);
	virtual ~FGnmComputeFence();

	void WriteFenceGPU(FGnmComputeCommandListContext& ComputeContext, bool bDoFlush);
	void WriteFenceGPU(FGnmCommandListContext& CommandContext, bool bDoFlush);

	void WaitFence(FGnmCommandListContext& CommandContext);
	void WaitFence(FGnmComputeCommandListContext& ComputeContext);

	bool HasGPUWrittenFence() const
	{
		volatile uint64* LabelLookupLoc = LabelLoc;
		const uint64 LabelVal = *LabelLookupLoc;

		checkf(LabelVal == 0 || LabelVal == 1, TEXT("Label value is %llu. Fence %s is invalid"), LabelVal, *GetName().ToString());
		return (*LabelLoc) != 0;
	}

	/** Sampler state object */
	uint64* LabelLoc;
};


template<class T>
struct TGnmResourceTraits
{
};
template<>
struct TGnmResourceTraits<FRHIVertexDeclaration>
{
	typedef FGnmVertexDeclaration TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIVertexShader>
{
	typedef FGnmVertexShader TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIGeometryShader>
{
	typedef FGnmGeometryShader TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIHullShader>
{
	typedef FGnmHullShader TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIDomainShader>
{
	typedef FGnmDomainShader TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIPixelShader>
{
	typedef FGnmPixelShader TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIComputeShader>
{
	typedef FGnmComputeShader TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIBoundShaderState>
{
	typedef FGnmBoundShaderState TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHITexture3D>
{
	typedef FGnmTexture3D TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHITexture2D>
{
	typedef FGnmTexture2D TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHITexture2DArray>
{
	typedef FGnmTexture2DArray TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHITextureCube>
{
	typedef FGnmTextureCube TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIRenderQuery>
{
	typedef FGnmRenderQuery TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIUniformBuffer>
{
	typedef FGnmUniformBuffer TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIIndexBuffer>
{
	typedef FGnmIndexBuffer TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIStructuredBuffer>
{
	typedef FGnmStructuredBuffer TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIVertexBuffer>
{
	typedef FGnmVertexBuffer TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIShaderResourceView>
{
	typedef FGnmShaderResourceView TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIUnorderedAccessView>
{
	typedef FGnmUnorderedAccessView TConcreteType;
};

template<>
struct TGnmResourceTraits<FRHISamplerState>
{
	typedef FGnmSamplerState TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIRasterizerState>
{
	typedef FGnmRasterizerState TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIDepthStencilState>
{
	typedef FGnmDepthStencilState TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIBlendState>
{
	typedef FGnmBlendState TConcreteType;
};
template<>
struct TGnmResourceTraits<FRHIComputeFence>
{
	typedef FGnmComputeFence TConcreteType;
};


