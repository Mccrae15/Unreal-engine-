// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4MediaCallbacks.h"
#include "PS4MediaPrivate.h"

#include "GlobalShader.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "MediaSamples.h"
#include "MediaShaders.h"
#include "Misc/FileHelper.h"
#include "Misc/Timespan.h"
#include "PipelineStateCache.h"
#include "PS4/PS4PlatformHttp.h"
#include "RHIStaticStates.h"
#include "SceneUtils.h"
#include "Serialization/ArrayReader.h"
#include "StaticBoundShaderState.h"

#include "PS4MediaAudioSample.h"
#include "PS4MediaOverlaySample.h"
#include "PS4MediaTextureSample.h"
#include "PS4MediaUtils.h"

#include <sceavplayer_ex.h>

#define PS4MEDIACALLBACKS_TRACE_FILE_IO 0
#define PS4MEDIACALLBACKS_TRACE_SAMPLES 0

#define LOCTEXT_NAMESPACE "FPS4MediaCallbacks"


/* FPS4MediaCallbacks structors
 *****************************************************************************/

FPS4MediaCallbacks::FPS4MediaCallbacks(const TSharedRef<FMediaSamples, ESPMode::ThreadSafe>& InSamples)
	: AudioSamplePool(new FPS4MediaAudioSamplePool)
	, Handle(nullptr)
	, LastAudioSampleTime(FTimespan::MinValue())
	, LastVideoSampleTime(FTimespan::MinValue())
	, Samples(InSamples)
	, VideoSamplePool(new FPS4MediaTextureSamplePool)
{ }


FPS4MediaCallbacks::~FPS4MediaCallbacks()
{
	if (Handle != nullptr)
	{
		sceAvPlayerStop(Handle);
		sceAvPlayerClose(Handle);
	}

	delete AudioSamplePool;
	AudioSamplePool = nullptr;

	delete VideoSamplePool;
	VideoSamplePool = nullptr;
}


/* FPS4MediaCallbacks interface
 *****************************************************************************/

void FPS4MediaCallbacks::GetEvents(TArray<EMediaEvent>& OutEvents)
{
	EMediaEvent Event;

	while (DeferredEvents.Dequeue(Event))
	{
		OutEvents.Add(Event);
	}
}


void FPS4MediaCallbacks::Initialize(TSharedPtr<FArchive, ESPMode::ThreadSafe> InArchive, const FString& Url, bool Precache, int32 NumVideoBuffers, int32 VideoBufferSizeMB)
{
	if (!InitializeHandle(NumVideoBuffers, VideoBufferSizeMB) ||
		!InitializeSource(InArchive, Url, Precache))
	{
		DeferredEvents.Enqueue(EMediaEvent::MediaOpenFailed);
	}

	Restart();
}


void FPS4MediaCallbacks::Restart()
{
	UE_LOG(LogPS4Media, Verbose, TEXT("Callbacks %p: Restarting sample processing"), this);

	AudioSampleRange = TRange<FTimespan>::Empty();
	VideoSampleRange = TRange<FTimespan>::Empty();

	LastAudioSampleTime = FTimespan::MinValue();
	LastVideoSampleTime = FTimespan::MinValue();
}


void FPS4MediaCallbacks::TickAudio(float Rate, FTimespan Time)
{
	if (Rate != 1.0f)
	{
		return; // no audio in reverse or fast forward
	}

	AudioSampleRange = TRange<FTimespan>::AtMost(Time + FTimespan::FromSeconds(Rate));

	if (AudioSampleRange.IsEmpty())
	{
		return; // nothing to play
	}

	while (true)
	{
		if ((LastAudioSampleTime != FTimespan::MinValue()) && !AudioSampleRange.Contains(LastAudioSampleTime))
		{
			return; // no new sample needed
		}

		SceAvPlayerFrameInfo FrameInfo;

		if (!sceAvPlayerGetAudioData(Handle, &FrameInfo))
		{
			break;
		}

		ProcessAudio(FrameInfo);
	}
}


void FPS4MediaCallbacks::TickVideo(float Rate, FTimespan Time)
{
	check(IsInRenderingThread());

	if (Rate > 0.0f)
	{
		VideoSampleRange = TRange<FTimespan>::AtMost(Time);
	}
	else if (Rate < 0.0f)
	{
		VideoSampleRange = TRange<FTimespan>::AtLeast(Time);
	}

	if (VideoSampleRange.IsEmpty())
	{
		return; // nothing to play
	}

	while (true)
	{
		if ((LastVideoSampleTime != FTimespan::MinValue()) && !VideoSampleRange.Contains(LastVideoSampleTime))
		{
			break; // no new sample needed
		}

		SceAvPlayerFrameInfoEx FrameInfo;

		if (!sceAvPlayerGetVideoDataEx(Handle, &FrameInfo))
		{
			break;
		}

		ProcessVideo(FrameInfo);
	}
}


/* FPS4MediaCallbacks implementation
 *****************************************************************************/

TRefCountPtr<FRHITexture> FPS4MediaCallbacks::FindOrCreateRHITexture(const Gnm::Texture& GnmTexture, uint32 Width, uint32 Height, bool Luma)
{
	TArray<TRefCountPtr<FRHITexture>>& TextureArray = Luma ? DecodedLuma : DecodedChroma;

	TRefCountPtr<FRHITexture> RHITexture;
	int32 FoundTextureIndex = INDEX_NONE;

	for (int32 TextureIndex = 0; TextureIndex < TextureArray.Num(); ++TextureIndex)
	{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture(TextureArray[TextureIndex]);

		if (Surface.Texture->getBaseAddress() == GnmTexture.getBaseAddress())
		{
			FRHITexture2D* Texture2D = TextureArray[TextureIndex]->GetTexture2D();
			FoundTextureIndex = TextureIndex;

			if ((Texture2D != nullptr) && (Texture2D->GetSizeX() == Width) && (Texture2D->GetSizeY() == Height))
			{
				RHITexture = TextureArray[TextureIndex];
			}

			break;
		}
	}

	if (!RHITexture.IsValid())
	{
		RHITexture = new FGnmTexture2D(GnmTexture, EResourceTransitionAccess::EReadable);

		if (FoundTextureIndex != INDEX_NONE)
		{
			TextureArray[FoundTextureIndex] = RHITexture;
		}
		else
		{
			TextureArray.Emplace(RHITexture);
		}
	}

	return RHITexture;
}


bool FPS4MediaCallbacks::InitializeHandle(int32 NumVideoBuffers, int32 VideoBufferSizeMB)
{
	// initialize AvPlayer instance
	SceAvPlayerInitData InitData;
	{
		FMemory::Memset(&InitData, 0, sizeof(SceAvPlayerInitData));

		InitData.memoryReplacement.allocate = PS4MediaAllocators::Allocate;
		InitData.memoryReplacement.deallocate = PS4MediaAllocators::Deallocate;
		InitData.memoryReplacement.allocateTexture = PS4MediaAllocators::AllocateTexture;
		InitData.memoryReplacement.deallocateTexture = PS4MediaAllocators::DeallocateTexture;
		InitData.memoryReplacement.objectPointer = nullptr;

		InitData.fileReplacement.open = &FPS4MediaCallbacks::OpenFile;
		InitData.fileReplacement.close = &FPS4MediaCallbacks::CloseFile;
		InitData.fileReplacement.readOffset = &FPS4MediaCallbacks::ReadOffsetFile;
		InitData.fileReplacement.size = &FPS4MediaCallbacks::GetFileSize;
		InitData.fileReplacement.objectPointer = this;

		InitData.basePriority = 700;
		InitData.numOutputVideoFrameBuffers = NumVideoBuffers;

		InitData.autoStart = false;
		InitData.defaultLanguage = nullptr;
		InitData.eventReplacement.eventCallback = &FPS4MediaCallbacks::EventCallback;
		InitData.eventReplacement.objectPointer = this;

#if UE_BUILD_DEBUG
		InitData.debugLevel = SCE_AVPLAYER_DBG_ALL;
#elif UE_BUILD_SHIPPING
		InitData.debugLevel = SCE_AVPLAYER_DBG_NONE;
#else
		InitData.debugLevel = SCE_AVPLAYER_DBG_WARNINGS;
#endif
	}

	Handle = sceAvPlayerInit(&InitData);

	if (Handle == nullptr)
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Callbacks %p: Failed to initialize AvPlayer instance"), this);
		return false;
	}

	// post-initialize AvPlayer instance
	static int32 NextPipeID = 0;
	static int32 NextQueueID = 0;

	SceAvPlayerPostInitData PostInitData;
	{
		memset(&PostInitData, 0, sizeof(SceAvPlayerPostInitData));

		PostInitData.demuxVideoBufferSize = 1024 * 1024 * VideoBufferSizeMB;

#if USE_SOFTWARE2_DECODER
		PostInitData.httpCtx.httpCtxId = FPS4PlatformHttp::GetLibHttpCtxId();
		PostInitData.httpCtx.sslCtxId = FPS4PlatformHttp::GetLibSslCtxId();

		PostInitData.videoDecoderInit.decoderType.videoType = SCE_AVPLAYER_VIDEO_DECODER_TYPE_SOFTWARE2;
		PostInitData.videoDecoderInit.decoderParams.avcSw2.decodePipelineDepth = 3;
		PostInitData.videoDecoderInit.decoderParams.avcSw2.computePipeId = NextPipeID; // range 0 - 4
		PostInitData.videoDecoderInit.decoderParams.avcSw2.computeQueueId = NextQueueID; // range 0 - 7

		if (NextQueueID >= 7)
		{
			NextPipeID = (NextPipeID + 1) % 5;
		}

		NextQueueID = (NextQueueID + 1) % 8;
#endif
	}

	int32 Result = sceAvPlayerPostInit(Handle, &PostInitData);

	if (Result != SCE_OK)
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Callbacks %p: Failed to post-initialize AvPlayer instance: %s"), this, *PS4Media::ResultToString(Result));
		return false;
	}

	sceAvPlayerSetAvSyncMode(Handle, SCE_AVPLAYER_AV_SYNC_MODE_NONE);

	return true;
}


bool FPS4MediaCallbacks::InitializeSource(TSharedPtr<FArchive, ESPMode::ThreadSafe> InArchive, const FString& Url, bool Precache)
{
	// load media source
	if (!InArchive.IsValid() && Url.StartsWith(TEXT("file://")))
	{
		const TCHAR* FilePath = &Url[7];

		if (Precache)
		{
			FArrayReader* ArrayReader = new FArrayReader;

			if (FFileHelper::LoadFileToArray(*ArrayReader, FilePath))
			{
				InArchive = MakeShareable(ArrayReader);
			}
			else
			{
				delete ArrayReader;
			}
		}
		else
		{
			InArchive = MakeShareable(IFileManager::Get().CreateFileReader(FilePath));
		}

		if (!InArchive.IsValid())
		{
			UE_LOG(LogPS4Media, Verbose, TEXT("Callbacks %p: Failed to open or read media file %s"), this, FilePath);
			return false;
		}

		if (InArchive->TotalSize() == 0)
		{
			UE_LOG(LogPS4Media, Verbose, TEXT("Callbacks %p: Cannot open media from empty file %s"), this, FilePath);
			return false;
		}
	}

	Archive = InArchive;

	// open media source
	UE_LOG(LogPS4Media, Verbose, TEXT("Callbacks %p: Opening URL %s"), this, *Url);

	if (Archive.IsValid())
	{
		int32_t SceResult = sceAvPlayerAddSource(Handle, TCHAR_TO_UTF8(*Url));

		if (SceResult < 0)
		{
			UE_LOG(LogPS4Media, Verbose, TEXT("Callbacks %p: Failed to add archive media source %s: %s"), this, *Url, *PS4Media::ResultToString(SceResult));
			return false;
		}
	}
	else
	{
		SceAvPlayerSourceDetails SourceDetails;
		{
			memset(&SourceDetails, 0, sizeof(SourceDetails));
			SourceDetails.uri.name = TCHAR_TO_UTF8(*Url);
			SourceDetails.uri.length = Url.Len();
			SourceDetails.sourceType = SCE_AVPLAYER_SOURCE_TYPE_HLS;
		}

		int32_t SceResult = sceAvPlayerAddSourceEx(Handle, SCE_AVPLAYER_URI_TYPE_SOURCE, &SourceDetails);

		if (SceResult < 0)
		{
			UE_LOG(LogPS4Media, Verbose, TEXT("Callbacks %p: Failed to add URL media source %s: %s"), this, *Url, *PS4Media::ResultToString(SceResult));
			return false;
		}
	}

	return true;
}


void FPS4MediaCallbacks::ProcessAudio(SceAvPlayerFrameInfo& FrameInfo)
{
	if (FrameInfo.pData == nullptr)
	{
		return;
	}

	// create & add sample to queue
	TSharedRef<FPS4MediaAudioSample, ESPMode::ThreadSafe> AudioSample = AudioSamplePool->AcquireShared();

	if (AudioSample->Initialize(FrameInfo))
	{
		LastAudioSampleTime = AudioSample->GetTime();
		Samples->AddAudio(AudioSample);

		#if PS4MEDIACALLBACKS_TRACE_SAMPLES
			UE_LOG(LogPS4Media, VeryVerbose, TEXT("Callbacks %p: Audio sample processed: %s"), this, *LastAudioSampleTime.ToString());
		#endif
	}
}


void FPS4MediaCallbacks::ProcessEvent(int32 EventId, int32 SourceId, void* EventData)
{
	if (EventId == SCE_AVPLAYER_TIMED_TEXT_DELIVERY)
	{
		// forward timed text immediately
		if (EventData != nullptr)
		{
			ProcessTimedText(*(SceAvPlayerFrameInfo*)EventData);
		}
	}
	else if (EventId == SCE_AVPLAYER_WARNING_ID)
	{
		// warnings & errors are processed on main thread
		int32 WarningId = *reinterpret_cast<int32*>(EventData);

		UE_LOG(LogPS4Media, Verbose, TEXT("Callbacks %p: WarningId %d (%s)"), this, WarningId, *PS4Media::ResultToString(WarningId));

		switch (WarningId)
		{
		case SCE_AVPLAYER_ERR_NOT_SUPPORTED:
			DeferredEvents.Enqueue(EMediaEvent::MediaOpenFailed);
			break;

		case SCE_AVPLAYER_WAR_FILE_NONINTERLEAVED:
			UE_LOG(LogPS4Media, Warning, TEXT("Media is non-interleaved, which may decrease performance"));
			break;

		case SCE_AVPLAYER_WAR_LOOPING_BACK:
			DeferredEvents.Enqueue(EMediaEvent::PlaybackEndReached);
			break;

		case SCE_AVPLAYER_WAR_JUMP_COMPLETE:
			DeferredEvents.Enqueue(EMediaEvent::SeekCompleted);
			break;
		}
	}
	else
	{
		// player events are processed on main thread
		UE_LOG(LogPS4Media, Verbose, TEXT("Calbacks %p: EventId %d (%s)"), this, EventId, *PS4Media::EventToString(EventId));

		switch (EventId)
		{
		case SCE_AVPLAYER_STATE_STOP:
			DeferredEvents.Enqueue(EMediaEvent::PlaybackSuspended);
			break;

		case SCE_AVPLAYER_STATE_READY:
			DeferredEvents.Enqueue(EMediaEvent::TracksChanged);
			DeferredEvents.Enqueue(EMediaEvent::MediaOpened);
			break;

		case SCE_AVPLAYER_STATE_PLAY:
			DeferredEvents.Enqueue(EMediaEvent::PlaybackResumed);
			break;

		case SCE_AVPLAYER_STATE_PAUSE:
			DeferredEvents.Enqueue(EMediaEvent::PlaybackSuspended);
			break;

		case SCE_AVPLAYER_STATE_BUFFERING:
			DeferredEvents.Enqueue(EMediaEvent::MediaBuffering);
			break;

		case SCE_AVPLAYER_ENCRYPTION:
			UE_LOG(LogPS4Media, Warning, TEXT("Media is encrypted and may not play"));
			break;

		case SCE_AVPLAYER_DRM_ERROR:
			DeferredEvents.Enqueue(EMediaEvent::MediaOpenFailed);
			break;
		};
	}
}


void FPS4MediaCallbacks::ProcessTimedText(SceAvPlayerFrameInfo& FrameInfo)
{
	// create & add sample to queue
	auto OverlaySample = MakeShared<FPS4MediaOverlaySample, ESPMode::ThreadSafe>();

	if (OverlaySample->Initialize(FrameInfo))
	{
		Samples->AddCaption(OverlaySample);

		#if PS4MEDIACALLBACKS_TRACE_SAMPLES
			UE_LOG(LogPS4Media, VeryVerbose, TEXT("Callbacks %p: Caption sample processed: %s"), this, *OverlaySample->GetTime().ToString());
		#endif
	}
}


void FPS4MediaCallbacks::ProcessVideo(SceAvPlayerFrameInfoEx& FrameInfo)
{
	// frame buffer is in NV12 format (8-bit Y plane followed
	// by interleaved U/V plane with 2x2 sub-sampling)

	const SceAvPlayerVideoEx& Details = FrameInfo.details.video;

	// initialize luma texture
	sce::Gnm::Texture LumaTexture;
	{
		Gnm::TextureSpec TextureSpec;
		{
			TextureSpec.init();
			TextureSpec.m_width = Details.pitch;
			TextureSpec.m_height = Details.height;
			TextureSpec.m_depth = 1;
			TextureSpec.m_pitch = 0;
			TextureSpec.m_numMipLevels = 1;
			TextureSpec.m_format = sce::Gnm::kDataFormatR8Unorm;
			TextureSpec.m_tileModeHint = sce::Gnm::kTileModeDisplay_LinearAligned;
			TextureSpec.m_minGpuMode = Gnm::getGpuMode();
			TextureSpec.m_numFragments = Gnm::kNumFragments1;
		}

		int32 Ret = LumaTexture.init(&TextureSpec);
		check(Ret == SCE_OK);

		sce::Gnm::SizeAlign SZ = LumaTexture.getSizeAlign();
		uint32 ExpectedLumaSize = Details.pitch * Details.height;

		if (SZ.m_size != ExpectedLumaSize)
		{
			UE_LOG(LogPS4Media, VeryVerbose, TEXT("Callbacks %p: Unexpected luma texture size %i instead of %i"), this, SZ.m_size, ExpectedLumaSize);
			return;
		}

		void* LumaAddress = FrameInfo.pData;
		LumaTexture.setBaseAddress256ByteBlocks((uint32_t)(reinterpret_cast<uint64_t>(LumaAddress) >> 8));
	}

	// initialize chroma texture
	sce::Gnm::Texture ChromaTexture;
	{
		Gnm::TextureSpec TextureSpec;
		{
			TextureSpec.init();
			TextureSpec.m_width = Details.pitch / 2;
			TextureSpec.m_height = Details.height / 2;
			TextureSpec.m_depth = 1;
			TextureSpec.m_pitch = 0;
			TextureSpec.m_numMipLevels = 1;
			TextureSpec.m_format = sce::Gnm::kDataFormatR8G8Unorm;
			TextureSpec.m_tileModeHint = sce::Gnm::kTileModeDisplay_LinearAligned;
			TextureSpec.m_minGpuMode = Gnm::getGpuMode();
			TextureSpec.m_numFragments = Gnm::kNumFragments1;
		}

		int32 Ret = ChromaTexture.init(&TextureSpec);
		check(Ret == SCE_OK);

		sce::Gnm::SizeAlign SZ = ChromaTexture.getSizeAlign();
		const uint32 ExpectedChromaSize = Details.pitch * (Details.height / 2);

		if (SZ.m_size != ExpectedChromaSize)
		{
			UE_LOG(LogPS4Media, VeryVerbose, TEXT("Callbacks %pUnexpected chroma texture size %i instead of %i"), this, SZ.m_size, ExpectedChromaSize);
			return;
		}

		void* ChromaAddress = (uint8_t*)(FrameInfo.pData) + (Details.pitch * Details.height);
		ChromaTexture.setBaseAddress256ByteBlocks((uint32_t)(reinterpret_cast<uint64_t>(ChromaAddress) >> 8));
	}

	// find or create render textures
	TRefCountPtr<FRHITexture> LumaRHI = FindOrCreateRHITexture(LumaTexture, Details.width, Details.height, true);

	if (!LumaRHI.IsValid())
	{
		UE_LOG(LogPS4Media, VeryVerbose, TEXT("Callbacks %p: No luma texture found"), this);
		return;
	}

	TRefCountPtr<FRHITexture> ChromaRHI = FindOrCreateRHITexture(ChromaTexture, Details.width / 2, Details.height / 2, false);

	if (!ChromaRHI.IsValid())
	{
		UE_LOG(LogPS4Media, VeryVerbose, TEXT("Callbacks %p: No chroma texture found"), this);
		return;
	}

	const uint32 CroppedWidth = Details.width - (Details.cropRightOffset + Details.cropLeftOffset);
	const uint32 CroppedHeight = Details.height - (Details.cropTopOffset + Details.cropBottomOffset);
	const FIntPoint Dimensions(CroppedWidth, CroppedHeight);

	// create & initialize video sample
	TSharedRef<FPS4MediaTextureSample, ESPMode::ThreadSafe> VideoSample = VideoSamplePool->AcquireShared();

	if (!VideoSample->Initialize(Dimensions, FTimespan(FrameInfo.timeStamp * ETimespan::TicksPerMillisecond)))
	{
		return;
	}

	// render video frame into output texture
	FRHICommandListImmediate& CommandList = FRHICommandListExecutor::GetImmediateCommandList();
	{
		SCOPED_DRAW_EVENT(CommandList, PS4MediaOutputConvertTexture);

		// configure media shaders
		auto ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);

		TShaderMapRef<FMediaShadersVS> VertexShader(ShaderMap);
		TShaderMapRef<FYCbCrConvertPS> PixelShader(ShaderMap);

		FRHITexture* RenderTarget = VideoSample->GetTexture();
		SetRenderTargets(CommandList, 1, &RenderTarget, nullptr, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthNop_StencilNop);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		{
			CommandList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMediaVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
		}

		SetGraphicsPipelineState(CommandList, GraphicsPSOInit);
		PixelShader->SetParameters(CommandList, LumaRHI->GetTexture2D(), ChromaRHI->GetTexture2D(), MediaShaders::YuvToSrgbPs4, true);

		// draw full-size quad
		const float ULeft = float(Details.cropLeftOffset) / float(Details.width);
		const float URight = float(Details.cropLeftOffset + CroppedWidth) / float(Details.width);
		const float VTop = float(Details.cropTopOffset) / float(Details.height);
		const float VBottom = float(Details.cropTopOffset + CroppedHeight) / float(Details.height);

		FMediaElementVertex Vertices[4];
		{
			Vertices[0].Position.Set(-1.0f, 1.0f, 1.0f, 1.0f); // top-left
			Vertices[1].Position.Set(1.0f, 1.0f, 1.0f, 1.0f); // top-right
			Vertices[2].Position.Set(-1.0f, -1.0f, 1.0f, 1.0f); // bottom-left
			Vertices[3].Position.Set(1.0f, -1.0f, 1.0f, 1.0f); // bottom-right

			Vertices[0].TextureCoordinate.Set(ULeft, VTop);
			Vertices[1].TextureCoordinate.Set(URight, VTop);
			Vertices[2].TextureCoordinate.Set(ULeft, VBottom);
			Vertices[3].TextureCoordinate.Set(URight, VBottom);
		}

		CommandList.SetViewport(0, 0, 0.0f, Details.width, Details.height, 1.0f);
		DrawPrimitiveUP(CommandList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));

		CommandList.TransitionResource(EResourceTransitionAccess::EReadable, RenderTarget);
	}

	// add sample to queue
	LastVideoSampleTime = VideoSample->GetTime();
	Samples->AddVideo(VideoSample);

	#if PS4MEDIACALLBACKS_TRACE_SAMPLES
		UE_LOG(LogPS4Media, VeryVerbose, TEXT("Callbacks %p: Video sample processed: %s"), this, *LastVideoSampleTime.ToString());
	#endif
}


/* FPS4MediaCallbacks callbacks
 *****************************************************************************/

int FPS4MediaCallbacks::CloseFile(void* P)
{
	auto Callbacks = (FPS4MediaCallbacks*)P;

	if ((Callbacks == nullptr) || !Callbacks->Archive.IsValid())
	{
		return -1;
	}

	UE_LOG(LogPS4Media, Verbose, TEXT("Callbacks %p: Closed file"), Callbacks);

	return 1;
}


void FPS4MediaCallbacks::EventCallback(void* P, int32_t EventId, int32_t SourceId, void* EventData)
{
	auto Callbacks = (FPS4MediaCallbacks*)P;

	if (Callbacks != nullptr)
	{
		Callbacks->ProcessEvent(EventId, SourceId, EventData);
	}
}


uint64_t FPS4MediaCallbacks::GetFileSize(void* P)
{
	auto Callbacks = (FPS4MediaCallbacks*)P;

	if (Callbacks == nullptr)
	{
		return -1;
	}

	auto& Archive = Callbacks->Archive;

	if (!Archive.IsValid())
	{
		return -1;
	}

	const int64 FileSize = Archive->TotalSize();

	#if PS4MEDIACALLBACKS_TRACE_FILE_IO
		UE_LOG(LogPS4Media, Verbose, TEXT("Callbacks %p: Getting file size (%i bytes)"), Callbacks, FileSize);
	#endif

	return FileSize;
}


int FPS4MediaCallbacks::OpenFile(void* P, const char* Filename)
{
	auto Callbacks = (FPS4MediaCallbacks*)P;

	if ((Callbacks == nullptr) || !Callbacks->Archive.IsValid())
	{
		return -1;
	}

	UE_LOG(LogPS4Media, Verbose, TEXT("Callbacks %p: Opening %s"), Callbacks, ANSI_TO_TCHAR(Filename));

	return 1;
}


int FPS4MediaCallbacks::ReadOffsetFile(void* P, uint8_t* Buffer, uint64_t Position, uint32_t Length)
{
	auto Callbacks = (FPS4MediaCallbacks*)P;

	if (Callbacks == nullptr)
	{
		return -1;
	}

	auto& Archive = Callbacks->Archive;

	if (!Archive.IsValid())
	{
		return -1;
	}

	int64 DataSize = Archive->TotalSize();

	if (Position >= DataSize)
	{
		UE_LOG(LogPS4Media, VeryVerbose, TEXT("Callbacks %p: Read position %i is beyond file size %i"), Callbacks, Position, DataSize);
		return -1;
	}

	int64 BytesToRead = FMath::Min((int64)Length, DataSize - (int64)Position);

	Archive->Seek(Position);
	Archive->Serialize(Buffer, BytesToRead);

	#if PS4MEDIACALLBACKS_TRACE_FILE_IO
		UE_LOG(LogPS4Media, VeryVerbose, TEXT("Callbacks %p: Read %i of %i bytes from position %i"), Callbacks, BytesToRead, Length, Position);
	#endif

	return (int32)BytesToRead;
}


#undef LOCTEXT_NAMESPACE
