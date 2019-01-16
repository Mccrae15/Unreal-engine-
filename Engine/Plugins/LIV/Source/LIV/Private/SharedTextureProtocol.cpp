#include "SharedTextureProtocol.h"
#ifndef UE_4_16_OR_LATER
#include <TextureResource.h>
#endif

#include <cstdint>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>

struct SharedTextureInfo
{
	uint32_t Version;
	uint32_t Width;
	uint32_t Height;
	uint32_t VFlip;
	uint64_t TextureHandle;
	bool bIsActive;
};
#define SHM_NAME L"liv-client-shared-texture-info"

struct SharedTextureState
{
	SharedTextureState()
	{
		const auto ProcessId = GetCurrentProcessId();

		MappedDataHandle = CreateFileMappingW(
			INVALID_HANDLE_VALUE,
			nullptr,
			PAGE_READWRITE,
			0,
			sizeof(SharedTextureInfo),
			(SHM_NAME + std::to_wstring(ProcessId)).c_str()
		);

		if (MappedDataHandle == nullptr)
			return;

		Data = static_cast<SharedTextureInfo*>(MapViewOfFile(
			MappedDataHandle,
			FILE_MAP_ALL_ACCESS,
			0,
			0,
			sizeof(SharedTextureInfo)
		));

		if (Data == nullptr)
			return;

		Data->Version = 1;
	}

	~SharedTextureState()
	{
		if (Data)
		{
			UnmapViewOfFile(Data);
			Data = nullptr;
		}

		if (MappedDataHandle)
		{
			CloseHandle(MappedDataHandle);
			MappedDataHandle = nullptr;
		}
	}

	bool IsValid() const
	{
		return Data != nullptr;
	}

	HANDLE MappedDataHandle = nullptr;
	SharedTextureInfo* Data = nullptr;
};

static SharedTextureState ProtocolState;


ID3D11Device* GraphicsDevice = nullptr;
ID3D11DeviceContext* GraphicsDeviceContext = nullptr;

ID3D11Texture2D* GraphicsCopyTexture = nullptr;
HANDLE GraphicsCopyTextureHandle = nullptr;

int SharedTextureProtocol::CopyTextureWidth = 0;
int SharedTextureProtocol::CopyTextureHeight = 0;


bool SharedTextureProtocol::IsActive()
{
	return ProtocolState.IsValid() && ProtocolState.Data->bIsActive;
}

int SharedTextureProtocol::GetWidth()
{
	if (!ProtocolState.IsValid()) return 0;

	return ProtocolState.Data->Width;
}

int SharedTextureProtocol::GetHeight()
{
	if (!ProtocolState.IsValid()) return 0;

	return ProtocolState.Data->Height;
}


void SetGraphicsDevice(ID3D11DeviceChild* DeviceChild)
{
	if (GraphicsDevice && GraphicsDeviceContext) return;

	DeviceChild->GetDevice(&GraphicsDevice);
	GraphicsDevice->GetImmediateContext(&GraphicsDeviceContext);
}
bool SharedTextureProtocol::EnsureGraphicsDevice()
{
	return !!GraphicsDevice;
}

bool SharedTextureProtocol::CreateCopyTexture()
{
	if (!GraphicsDevice) return false;
	if (!ProtocolState.IsValid()) return false;

	D3D11_TEXTURE2D_DESC TextureDescription = {};
	TextureDescription.Width = GetWidth();
	TextureDescription.Height = GetHeight();
	TextureDescription.MipLevels = 1;
	TextureDescription.ArraySize = 1;
	TextureDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	TextureDescription.SampleDesc.Count = 1;
	TextureDescription.Usage = D3D11_USAGE_DEFAULT;
	TextureDescription.BindFlags = 0;
	TextureDescription.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	ID3D11Texture2D* NewCopyTexture = nullptr;
	auto GraphicsResult = GraphicsDevice->CreateTexture2D(&TextureDescription, nullptr, &NewCopyTexture);

	if (!SUCCEEDED(GraphicsResult)) return false;


	IDXGIResource* ShareResource = nullptr;
	GraphicsResult = NewCopyTexture->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void**>(&ShareResource));

	if (!SUCCEEDED(GraphicsResult)) return false;


	GraphicsResult = ShareResource->GetSharedHandle(&GraphicsCopyTextureHandle);
	ShareResource->Release();

	if (!SUCCEEDED(GraphicsResult)) return false;


	CopyTextureWidth = TextureDescription.Width;
	CopyTextureHeight = TextureDescription.Height;

	GraphicsCopyTexture = NewCopyTexture;
	ProtocolState.Data->TextureHandle = reinterpret_cast<uint64_t>(GraphicsCopyTextureHandle);

	return true;
}

void SharedTextureProtocol::ReleaseCopyTexture()
{
	if (IsActive())
		ProtocolState.Data->TextureHandle = reinterpret_cast<uint64_t>(nullptr);

	if (GraphicsCopyTextureHandle)
	{
		GraphicsCopyTextureHandle = nullptr;
	}

	if (GraphicsCopyTexture)
	{
		GraphicsCopyTexture->Release();
		GraphicsCopyTexture = nullptr;
	}

	CopyTextureWidth = 0;
	CopyTextureHeight = 0;
}

bool SharedTextureProtocol::AssignCopyTexture()
{
	if (!IsActive()) return false;
	if (!EnsureGraphicsDevice()) return false;

	if (GraphicsCopyTexture)
	{
		if (GetWidth() != CopyTextureWidth ||
			GetHeight() != CopyTextureHeight)
		{
			ReleaseCopyTexture();
		}
	}

	if (!GraphicsCopyTexture)
		if (!CreateCopyTexture()) return false;

	return true;
}


void SharedTextureProtocol::SubmitTexture(UTextureRenderTarget2D* Texture)
{
	if (!IsActive()) return;

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		void,
		UTextureRenderTarget2D*, Texture, Texture,
		{
			const auto RenderTargetResource = static_cast<FTextureRenderTarget2DResource*>(Texture->Resource); // NOLINT
			const auto TextureRHI = RenderTargetResource->GetTextureRHI();

			if (!TextureRHI) return; // Resource isn't quite ready yet!

			const auto DXTexture = static_cast<ID3D11Texture2D*>(TextureRHI->GetNativeResource());

			if (!EnsureGraphicsDevice())
				SetGraphicsDevice(DXTexture);

			if (!EnsureGraphicsDevice()) return;
			if (!AssignCopyTexture()) return;

			GraphicsDeviceContext->CopyResource(GraphicsCopyTexture, DXTexture);
		}
	);
}
