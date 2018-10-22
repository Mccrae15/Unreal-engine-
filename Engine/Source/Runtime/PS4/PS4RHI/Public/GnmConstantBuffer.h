// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmConstantBuffer.h: Gnm Constant Buffer definitions.
=============================================================================*/

#pragma once 

/** Size of the default constant buffer (copied from D3D11) */
#define MAX_GLOBAL_CONSTANT_BUFFER_SIZE		4096

class FGnmCommandListContext;
class FGnmComputeCommandListContext;

/**
 * A Gnm constant buffer
 */
class FGnmConstantBuffer : public FRenderResource, public FRefCountedObject
{
public:

	FGnmConstantBuffer(uint32 InSize = 0);
	~FGnmConstantBuffer();

	// FRenderResource interface.
	virtual void	InitDynamicRHI() override;
	virtual void	ReleaseDynamicRHI() override;

	void			UpdateConstant(const uint8* Data, uint16 Offset, uint16 Size);

	/**
	 * Pushes the outstanding buffer data to the GPU
	 */
	bool CommitConstantsToDevice(FGnmCommandListContext& CommandListContext, Gnm::ShaderStage Stage, uint32 BufferIndex, bool bDiscardSharedConstants);
	bool CommitConstantsToDevice(FGnmComputeCommandListContext& CommandListContext, Gnm::ShaderStage Stage, uint32 BufferIndex, bool bDiscardSharedConstants);

private:

	template<typename TGnmCommandContext>
	bool SetupCommitConstantsToDevice(TGnmCommandContext& CommandListContext, Gnm::ShaderStage Stage, uint32 BufferIndex, bool bDiscardSharedConstants, Gnm::Buffer& OutBuffer);

	uint32	MaxSize;
	bool	bIsDirty;
	uint8*	ShadowData;

	/** Size of all constants that has been updated since the last call to Commit. */
	uint32	CurrentUpdateSize;

	/**
	 * Size of all constants that has been updated since the last Discard.
	 * Includes "shared" constants that don't necessarily gets updated between every Commit.
	 */
	uint32	TotalUpdateSize;
};
