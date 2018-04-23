// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

FORCEINLINE void FGnmComputeCommandListContext::SetTextureForStage(FGnmSurface& Surface, uint32 TextureIndex, Gnm::ShaderStage Stage, FName TextureName)
{
	SetTexture(Surface, TextureIndex, TextureName);
}

FORCEINLINE void FGnmComputeCommandListContext::SetTextureForStage(FTextureRHIParamRef NewTextureRHI, uint32 TextureIndex, Gnm::ShaderStage Stage)
{
	SetTexture(NewTextureRHI, TextureIndex);
}

FORCEINLINE void FGnmComputeCommandListContext::SetSRVForStage(FShaderResourceViewRHIParamRef SRV, uint32 TextureIndex, Gnm::ShaderStage Stage)
{
	SetSRV(SRV, TextureIndex);
}

FORCEINLINE void FGnmComputeCommandListContext::UpdateCSConstant(uint32 BufferIndex, const void* NewValue, uint32 BaseIndex, uint32 Size)
{
	check(BufferIndex == 0);
	CSConstantBuffer->UpdateConstant((const uint8*)NewValue, (uint16)BaseIndex, (uint16)Size);
}