#pragma once

#include <Engine/TextureRenderTarget2D.h>

class SharedTextureProtocol
{
public:
	static bool IsActive();

	static int GetWidth();
	static int GetHeight();

	static void SubmitTexture(UTextureRenderTarget2D* Texture);

protected:
	static int CopyTextureWidth;
	static int CopyTextureHeight;

	static bool EnsureGraphicsDevice();

	static bool CreateCopyTexture();
	static void ReleaseCopyTexture();
	static bool AssignCopyTexture();
};
