// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4MovieStreamer.h"
#include "GnmMemory.h"
#include "GrowableAllocator.h"
#include "HAL/PlatformFilemanager.h"
#include "Internationalization/Culture.h"
#include "StaticBoundShaderState.h"
#include "Misc/Paths.h"

#include "RenderingCommon.h"
#include "Slate/SlateTextures.h"
#include "MoviePlayer.h"

#include <libsysmodule.h>
#include "PS4File.h"
#include <pthread.h>
#include <audioout.h>

#include "MediaShaders.h"
#include "SceneUtils.h"

#include "GnmRHI.h"

#include <sceavplayer_ex.h>
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"

// Whether to use the new, faster Software2 decoder - uses additional memory and GPU compute
#define PS4_MOVIESTREAMER_SOFTWARE2_DECODER 1
// Additional performance for streams with a high peak bitrate - uses additional memory
#define PS4_MOVIESTREAMER_PERFORMANCE 0

DEFINE_LOG_CATEGORY_STATIC(LogMoviePlayer, Log, All);

#define MOVIE_FILE_EXTENSION TEXT("mp4")

static FCriticalSection GMovieStreamerGPUAllocationsCS;
static FCriticalSection GMovieStreamerCPUAllocationsCS;
static TMap<void*, FMemBlock>   GMovieStreamerGPUAllocationsMap;
static TMap<void*, FMemBlock>   GMovieStreamerCPUAllocationsMap;

/**
* The simple element vertex declaration resource type.
*/
class FPS4MediaVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor.
	virtual ~FPS4MediaVertexDeclaration() {}

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		uint16 Stride = sizeof( FMediaElementVertex );
		Elements.Add( FVertexElement( 0, STRUCT_OFFSET( FMediaElementVertex, Position ), VET_Float4, 0, Stride ) );
		Elements.Add( FVertexElement( 0, STRUCT_OFFSET( FMediaElementVertex, TextureCoordinate ), VET_Float2, 1, Stride ) );
		VertexDeclarationRHI = RHICreateVertexDeclaration( Elements );
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
TGlobalResource< FPS4MediaVertexDeclaration > GPS4MovieStreamerVertexDecl;

//
// Local functions
//

// File I/O
// In order to read from the network or a PAK file, these functions replace the native file system I/O for movie playback
// NOTE: The way the API is written, it assumes that there is only ever a single file open, so we can use a global file here to perform all the required operations
IFileHandle* MovieFileHandle = nullptr;

/**
 * Opens the given filename for reading
 */
static int32 OpenFile(void* ArgP, const char* ArgFilename)
{
	// Don't open a movie file until the previous file is closed
	if (!MovieFileHandle)
	{
		MovieFileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenRead(UTF8_TO_TCHAR(ArgFilename));
		if (MovieFileHandle)
		{
			// The docs say to return >= 0 for success. I'm not sure if the return result is treated as a file handle or not.
			// Since the close function doesn't provide a handle to close, I'll assume that the result is simply a success or failure code.
			// We'll use 1
			return 1;
		}
	}

	// Nothing worked
	return -1;
}

/**
 * Closes any file opened previously with OpenFile() above
 */
static int32 CloseFile(void* ArgP)
{
	if (MovieFileHandle)
	{
		// NOTE: Deleting the handle closes the file, by design
		delete MovieFileHandle;
		MovieFileHandle = nullptr;
	}

	// NOTE: This function can't fail
	return 1;
}

/**
 * Reads from the opened file at the given offset
 */
static int32 ReadOffsetFile(void* ArgP, uint8_t* ArgBuffer, uint64_t ArgPosition, uint32_t ArgLength)
{
	if (MovieFileHandle)
	{
		// NOTE: The IFileHandle can only read full buffers worth of data, so I assume that in a streaming situation it will block
		// until the read is completed. This might not be a good thing when streaming, but since that scenario is for development only
		// the resultant stuttering is acceptable.
		
		// First seek to the given position
		if (MovieFileHandle->Seek(ArgPosition))
		{
			if (MovieFileHandle->Read(ArgBuffer, ArgLength))
			{
				// All bytes were read, so inform the caller
				return ArgLength;
			}
		}
	}

	// Something failed
	return -1;
}

/**
 * Gets the Size of the opened file
 */
static uint64_t GetFileSize(void* ArgP)
{
	// NOTE: Since no filename is provided this can only be called after open.
	if (MovieFileHandle)
	{
		return MovieFileHandle->Size();
	}
	
	// Error condition
	return -1;
}

//
// Methods
//

FAVPlayerMovieStreamer::FAVPlayerMovieStreamer() :
	CurrentTexture(0),	
	SamplePlayer(nullptr),
	bIsPlayerLoaded(false),
	AudioHandle(-1),
	bWasActive(false),
	bIsInTick(false),
	CurrentStreamWidth(1),
	CurrentStreamHeight(1)
{
	UE_LOG(LogMoviePlayer, Log, TEXT("FAVMoviePlayer ctor..."));

	AudioThread = nullptr;

	//
	// Allocate resources here whose livetime spans the life of this streamer instance
	//
	MovieViewport = MakeShareable(new FMovieViewport());

	//
	// Initialize the movie player
	//

	// Load the movie player library
	int32 ReturnCode = sceSysmoduleLoadModule(SCE_SYSMODULE_AV_PLAYER);
  
	if (ReturnCode != SCE_OK)
	{
		UE_LOG(LogMoviePlayer, Error, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_AV_PLAYER) failed. Error code: 0x%08x"), ReturnCode);
		// Don't continue setting things up
		return;
	}

	// Record that the library was loaded so we can unload it on destruction
	bIsPlayerLoaded = true;
}

FAVPlayerMovieStreamer::~FAVPlayerMovieStreamer()
{
	UE_LOG(LogMoviePlayer, Log, TEXT("FAVMoviePlayer dtor..."));

	// Release all allocated resources, both for any currently playing movie and for any allocated in the contructor
	// or lazily allocated
	TeardownPlayback();

	// Clean up any remaining resources
	Cleanup();

	// Unload the player library if it was successfully loaded
	if (bIsPlayerLoaded)
	{
		int32 ReturnCode = sceSysmoduleUnloadModule(SCE_SYSMODULE_AV_PLAYER);
  
		if (ReturnCode != SCE_OK)
		{
			UE_LOG(LogMoviePlayer, Error, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_AV_PLAYER) failed. Error code: 0x%08x"), ReturnCode);
		}
		
		// Assume that the player isn't loaded any longer, regardless of an error or not
		bIsPlayerLoaded = false;
	}
}

bool FAVPlayerMovieStreamer::SetupPlayback()
{
	if (!bIsPlayerLoaded || SamplePlayer != nullptr)
	{
		// Can't setup playback when already set up.
		UE_LOG(LogMoviePlayer, Error, TEXT("can't setup FAVPlayerMovieStreamer because it is already set up"));
		return false;
	}

	//
	// Initialize a movie player instance
	//
	SceAvPlayerInitData PlayerInit;
	FMemory::Memset(&PlayerInit, 0, sizeof(SceAvPlayerInitData));

	// Configure the player with the memory allocation functions, etc.
	PlayerInit.memoryReplacement.allocate = PS4MediaAllocators::Allocate;
	PlayerInit.memoryReplacement.deallocate = PS4MediaAllocators::Deallocate;
	PlayerInit.memoryReplacement.allocateTexture = PS4MediaAllocators::AllocateTexture;
	PlayerInit.memoryReplacement.deallocateTexture = PS4MediaAllocators::DeallocateTexture;
	PlayerInit.memoryReplacement.objectPointer = nullptr;

	PlayerInit.fileReplacement.open = OpenFile;
	PlayerInit.fileReplacement.close = CloseFile;
	PlayerInit.fileReplacement.readOffset = ReadOffsetFile;
	PlayerInit.fileReplacement.size = GetFileSize;
	PlayerInit.basePriority = 0;
#if	PS4_MOVIESTREAMER_PERFORMANCE
	PlayerInit.numOutputVideoFrameBuffers = 6;
#else
	PlayerInit.numOutputVideoFrameBuffers = 2;
#endif
	PlayerInit.autoStart = false;
	// @todo Should FCulture::GetThreeLetterISOLanguageName() be used here?
	// PlayerInit.defaultLanguage = "eng";
	strncpy(LanguageName, TCHAR_TO_UTF8(*(FInternationalization::Get().GetCurrentCulture()->GetThreeLetterISOLanguageName())), sizeof(LanguageName) - 1);
	PlayerInit.defaultLanguage = LanguageName;
	PlayerInit.eventReplacement.objectPointer = this;
	PlayerInit.eventReplacement.eventCallback = (SceAvPlayerEventCallback) EventCallback;
//	PlayerInit.debugLevel = SCE_AVPLAYER_DBG_ALL;
//	PlayerInit.debugLevel = SCE_AVPLAYER_DBG_WARNINGS;
//	PlayerInit.debugLevel = SCE_AVPLAYER_DBG_INFO;
#if UE_BUILD_DEBUG
	PlayerInit.debugLevel = SCE_AVPLAYER_DBG_ALL;
#else
	PlayerInit.debugLevel = SCE_AVPLAYER_DBG_NONE;
#endif


	// Initialize the player using the configuration above
	SamplePlayer = sceAvPlayerInit(&PlayerInit);

	if( SamplePlayer == nullptr )
	{
		UE_LOG(LogMoviePlayer, Error, TEXT("failed to initialize AV player library"));
		return false; // don't continue setting things up
	}

	SceAvPlayerPostInitData AvPlayerPostInit;
	memset( &AvPlayerPostInit, 0, sizeof( SceAvPlayerPostInitData ) );

#if	PS4_MOVIESTREAMER_PERFORMANCE
	AvPlayerPostInit.demuxVideoBufferSize = (1 * 1024 * 1024);
#endif

#if PS4_MOVIESTREAMER_SOFTWARE2_DECODER
	// The new AVC2 decoder is faster and has fewer source limitations however requires more memory
	AvPlayerPostInit.videoDecoderInit.decoderType.videoType = SCE_AVPLAYER_VIDEO_DECODER_TYPE_SOFTWARE2;
	// Note that you can customize the performance and behaviour of the AVC2 decoder from the avcSw2 structure.
	// The most signficant value here is decodePipelineDepth, it can be set between 1 and 8. Increasing
	// this number increases performance and resource usage.
	AvPlayerPostInit.videoDecoderInit.decoderParams.avcSw2.decodePipelineDepth = 6;

	// The queue you use must not be mapped already and each instance of the player required a different queue.
	AvPlayerPostInit.videoDecoderInit.decoderParams.avcSw2.computePipeId = 0;
	AvPlayerPostInit.videoDecoderInit.decoderParams.avcSw2.computeQueueId = 0;
#endif

	int32 Ret = sceAvPlayerPostInit( SamplePlayer, &AvPlayerPostInit );
	checkf( Ret == SCE_OK, TEXT( "sceAvPlayerPostInit failed: 0x%x" ), Ret );

	// Initialize the audio player, which will create a thread for playing the audio
	if (!SetupAudio())
	{
		UE_LOG(LogMoviePlayer, Error, TEXT("SetupAudio() failed\n"));
		return false; // don't continue setting things up
	}

	UE_LOG(LogMoviePlayer, Log, TEXT("SetupPlayback() succeeded\n"));
	return true;
}

void FAVPlayerMovieStreamer::TeardownPlayback()
{
	// NOTE: Called from both the main thread (via ForceCompletion) and the render thread (via Tick)

	// Make sure the movie viewport doesn't hold on to the texture any longer
	MovieViewport->SetTexture(nullptr);

	// Shutdown the audio player, which will block until the audio thread is completed
	TeardownAudio();

	// Now close the player we allocated
	if (SamplePlayer != nullptr)
	{
		sceAvPlayerClose(SamplePlayer);
		SamplePlayer = nullptr;
	}

	DecodedLuma.Empty();
	DecodedChroma.Empty();

	// NOTE: Any textures allocated are still allocated at this point. They will get released in Cleanup()
	// NOTE: The AVPlayer module is still loaded. It will get cleaned up by the destructor
}

FTextureRHIRef FAVPlayerMovieStreamer::FindOrCreateRHITexture( const Gnm::Texture& GnmTexture, bool bLuma )
{
	TArray<FTextureRHIRef>& TextureArray = bLuma ? DecodedLuma : DecodedChroma;

	FTextureRHIRef RHITexture;
	for( int32 i = 0; i < TextureArray.Num(); ++i )
{
		FGnmSurface& Surface = GetGnmSurfaceFromRHITexture( TextureArray[i] );
		if( Surface.Texture->getBaseAddress() == GnmTexture.getBaseAddress() )
			{
			RHITexture = TextureArray[i];
			break;
			}
		}

	if( !RHITexture.IsValid() )
		{
		int32 NewTextureIndex = TextureArray.Emplace( new FGnmTexture2D( GnmTexture, EResourceTransitionAccess::EReadable ) );
		RHITexture = TextureArray[NewTextureIndex];
		}
	check( RHITexture.IsValid() );
	return RHITexture;
}

void FAVPlayerMovieStreamer::CheckForNextVideoFrame()
{
	check(IsInRenderingThread());

	// NOTE: Called from the render thread via Tick()
	if (!sceAvPlayerIsActive(SamplePlayer))
	{
		return;
	}

	bool dimensionsGood = false;
	{
		TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe>& VideoTexture = BufferedVideoTextures[0];		
		if (VideoTexture.IsValid() && VideoTexture->GetWidth() == CurrentStreamWidth && VideoTexture->GetHeight() == CurrentStreamHeight)
		{
			// Everything is good. Keep the existing texture
			dimensionsGood = true;
		}
	}

	if (!dimensionsGood)
	{
		// Resolution has changed or texture was not valid, so (re)allocate a texture at the proper resolution
		// Release any resources associated with the previous texture
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

		if (!SlateVideoTexture.IsValid())
		{
			SlateVideoTexture = MakeShareable(new FSlateTexture2DRHIRef(nullptr, 0, 0));
		}
		SlateVideoTexture->SetRHIRef(nullptr, 0, 0);
		
		for (int32 i = 0; i < NumBufferedTextures; ++i)
		{
			TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe>& VideoTexture = BufferedVideoTextures[i];
			if (VideoTexture.IsValid())
			{
				// Make sure that the viewport doesn't hold a reference to the texture when we release it
				MovieViewport->SetTexture(nullptr);

				// Release the underlying rendering resource and remove the reference to the texture object.
				// Since we are on the rendering thread, there is no need to schedule the release
				VideoTexture->ReleaseResource();
				VideoTexture.Reset();
			}

			// Allocate and initialize a new texture with no initial data since it will be filled in during the YUV conversion
			uint32 createFlags = (TexCreate_RenderTargetable);
			VideoTexture = MakeShareable(new FSlateTexture2DRHIRef(CurrentStreamWidth, CurrentStreamHeight, PF_B8G8R8A8, nullptr, createFlags, true));
			VideoTexture->InitResource();
		}
		
	}

	TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe>& VideoTexture = BufferedVideoTextures[CurrentTexture];

	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();

	uint32 FramePitch = 0;
	uint32 FrameWidth = 0;
	uint32 FrameHeight = 0;

	void* LumaAddress;
	void* ChromaAddress;
	sce::Gnm::Texture LumaTexture;
	sce::Gnm::Texture ChromaTexture;

	// frame buffer is in NV12 format (8-bit Y plane followed by interleaved U/V plane with 2x2 subsampling)
	SceAvPlayerFrameInfoEx AvFrameInfo;
	FMemory::Memzero( &AvFrameInfo, sizeof( SceAvPlayerFrameInfo ) );
	bool FrameAvailable = sceAvPlayerGetVideoDataEx( SamplePlayer, &AvFrameInfo );

	FTextureRHIRef LumaRHI;
	FTextureRHIRef ChromaRHI;

	if (FrameAvailable)
	{
		FrameWidth = AvFrameInfo.details.video.width;
		FrameHeight = AvFrameInfo.details.video.height;
		FramePitch = AvFrameInfo.details.video.pitch;

		sce::Gnm::SizeAlign SZ;
		Gnm::TextureSpec TextureSpec;

		TextureSpec.init();
		TextureSpec.m_width = FramePitch;
		TextureSpec.m_height = FrameHeight;
		TextureSpec.m_depth = 1;
		TextureSpec.m_pitch = 0;
		TextureSpec.m_numMipLevels = 1;
		TextureSpec.m_format = sce::Gnm::kDataFormatR8Unorm;
		TextureSpec.m_tileModeHint = sce::Gnm::kTileModeDisplay_LinearAligned;
		TextureSpec.m_minGpuMode = Gnm::getGpuMode();
		TextureSpec.m_numFragments = Gnm::kNumFragments1;

		int32 Ret = LumaTexture.init(&TextureSpec);
		check(Ret == SCE_OK);

		SZ = LumaTexture.getSizeAlign();

		if( SZ.m_size != FramePitch * FrameHeight )
		{
			UE_LOG( LogMoviePlayer, Warning, TEXT( "unexpected y_texture size. sz.m_size: %i" ), SZ.m_size );
			return;
		}

		LumaAddress = AvFrameInfo.pData;
		LumaTexture.setBaseAddress256ByteBlocks( (uint32_t)(reinterpret_cast<uint64_t>(LumaAddress) >> 8) );

		TextureSpec.init();
		TextureSpec.m_width = FramePitch / 2;
		TextureSpec.m_height = FrameHeight / 2;
		TextureSpec.m_depth = 1;
		TextureSpec.m_pitch = 0;
		TextureSpec.m_numMipLevels = 1;
		TextureSpec.m_format = sce::Gnm::kDataFormatR8G8Unorm;
		TextureSpec.m_tileModeHint = sce::Gnm::kTileModeDisplay_LinearAligned;
		TextureSpec.m_minGpuMode = Gnm::getGpuMode();
		TextureSpec.m_numFragments = Gnm::kNumFragments1;

		Ret = ChromaTexture.init(&TextureSpec);
		check(Ret == SCE_OK);

		SZ = ChromaTexture.getSizeAlign();

		if( SZ.m_size != FramePitch * (FrameHeight / 2) )
		{
			UE_LOG( LogMoviePlayer, Warning, TEXT( "unexpected c_texture size. sz.m_size: %i" ), SZ.m_size );
			return;
		}

		ChromaAddress = (uint8_t*)(AvFrameInfo.pData) + (FramePitch * FrameHeight);
		ChromaTexture.setBaseAddress256ByteBlocks( (uint32_t)(reinterpret_cast<uint64_t>(ChromaAddress) >> 8) );

		LumaRHI = FindOrCreateRHITexture( LumaTexture, true );
		ChromaRHI = FindOrCreateRHITexture( ChromaTexture, false );

		if( LumaRHI.IsValid() && ChromaRHI.IsValid() )
		{
			SCOPED_DRAW_EVENT( RHICmdList, MediaTextureConvert );

			auto ShaderMap = GetGlobalShaderMap( ERHIFeatureLevel::SM5 );

			// Set the new shaders
			TShaderMapRef<FMediaShadersVS> VertexShader( ShaderMap );
			TShaderMapRef<FYCbCrConvertPS> PixelShader( ShaderMap );


			TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe>& BoundSlateTextureTarget = BufferedVideoTextures[CurrentTexture];
			FTextureRHIParamRef BoundTextureTarget = BoundSlateTextureTarget->GetRHIRef();
			SetRenderTargets(RHICmdList, 1, &BoundTextureTarget, nullptr, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthNop_StencilNop);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GPS4MovieStreamerVertexDecl.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			PixelShader->SetParameters( RHICmdList, LumaRHI->GetTexture2D(), ChromaRHI->GetTexture2D(), MediaShaders::YuvToSrgbPs4, true );

			// Draw a fullscreen quad			
			FMediaElementVertex Vertices[4];
			Vertices[0].Position.Set( -1.0f, 1.0f, 1.0f, 1.0f );
			Vertices[0].TextureCoordinate.Set( 0.0f, 0.0f );

			Vertices[1].Position.Set( 1.0f, 1.0f, 1.0f, 1.0f );
			Vertices[1].TextureCoordinate.Set( 1.0f, 0.0f );

			Vertices[2].Position.Set( -1.0f, -1.0f, 1.0f, 1.0f );
			Vertices[2].TextureCoordinate.Set( 0.0f, 1.0f );

			Vertices[3].Position.Set( 1.0f, -1.0f, 1.0f, 1.0f );
			Vertices[3].TextureCoordinate.Set( 1.0f, 1.0f );

			// set viewport to RT size
			RHICmdList.SetViewport( 0, 0, 0.0f, FrameWidth, FrameHeight, 1.0f );

			DrawPrimitiveUP( RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof( Vertices[0] ) );
			RHICmdList.CopyToResolveTarget( BoundTextureTarget, BoundTextureTarget, true, FResolveParams() );

			CurrentTexture = (CurrentTexture + 1) % NumBufferedTextures;
		SlateVideoTexture->SetRHIRef(VideoTexture->GetRHIRef(), VideoTexture->GetWidth(), VideoTexture->GetHeight());
		MovieViewport->SetTexture(SlateVideoTexture);
	}
	}
}

void FAVPlayerMovieStreamer::ForceCompletion()
{
	// NOTE: Called from main thread
	// WARNING: Assumes that the Tick() function will never be called again
	// The canonical implementation for this shuts down playback before returning

	// If the tick function is currently executing, wait until it completes
	while (bIsInTick)	
	{
		// Don't hog the CPU
		sceKernelUsleep(10);
	}

	// Assuming that the caller has properly prevented Tick() from executing again, we are safe to proceed with
	// the normal tearing down of movie playback, which would normally occur at the end of a movie in the
	// Tick() function

	// Stop the player and make sure it doesn't attempt to start the next movie
	MovieQueue.Empty();
	sceAvPlayerStop(SamplePlayer);

	// Shutdown threads and AVPlayer resources
	TeardownPlayback();

	// All cleaned up except for the loaded module, which will be cleaned up in the destructor
}

bool FAVPlayerMovieStreamer::Init(const TArray<FString>& MoviePaths, TEnumAsByte<EMoviePlaybackType> inPlaybackType)
{
	// NOTE: Called from main thread
	// Initializes the streamer for audio and video playback of the given path(s).
	// NOTE: If multiple paths are provided, it is expect that they be played back seamlessly.
	UE_LOG(LogMoviePlayer, Warning, TEXT("FAVMoviePlayer init. Path count = %d..."), MoviePaths.Num());

	// Add the given paths to the movie queue
	MovieQueue.Append(MoviePaths);

	// Play the next movie in the queue
	return StartNextMovie();
}

void FAVPlayerMovieStreamer::EventCallback(void* Data, int32 WhichEventId, int32 WhichSourceId, void* EventData)
{
	FAVPlayerMovieStreamer* Streamer = (FAVPlayerMovieStreamer*) Data;

	switch ( WhichEventId )
	{
		case SCE_AVPLAYER_STATE_READY :
		{
			// The player is in the ready state, so enable all the audio and video streams.
			// *** What if there are multiple audio streams for different languages?
			const int32 StreamCount = sceAvPlayerStreamCount(Streamer->SamplePlayer);
			if (StreamCount > 0)
			{
				bool bDidFindAudio = false;
				for (int32 StreamIndex = 0; StreamIndex < StreamCount; StreamIndex++)
				{
					SceAvPlayerStreamInfo StreamInfo;
					int32 Result = sceAvPlayerGetStreamInfo(Streamer->SamplePlayer, StreamIndex, &StreamInfo);
					if (StreamInfo.type == SCE_AVPLAYER_VIDEO)
					{
						Result = sceAvPlayerEnableStream(Streamer->SamplePlayer, StreamIndex);
						if (Result != 0)
						{
							UE_LOG(LogMoviePlayer, Warning, TEXT("sceAvPlayerEnableStream() failed for video. Result code 0x%x"), Result);
						}						
						Streamer->CurrentStreamWidth = StreamInfo.details.video.width;
						Streamer->CurrentStreamHeight = StreamInfo.details.video.height;
					}
					else if(StreamInfo.type == SCE_AVPLAYER_AUDIO)
					{
						// Enable the appropriate audio stream. In this case, the first one found
						// @todo epic Determine how to handle language codes if there is more than one stream
						if (!bDidFindAudio) 
						{
							Result = sceAvPlayerEnableStream(Streamer->SamplePlayer, StreamIndex);
							if (Result != 0)
							{
								UE_LOG(LogMoviePlayer, Warning, TEXT("sceAvPlayerEnableStream() failed for audio. Result code 0x%x"), Result);
							}
						}
					}
				}

				// Start the streams that were just configured
				sceAvPlayerStart(Streamer->SamplePlayer);
			}
		}
		break;

		case SCE_AVPLAYER_STATE_PLAY:
			// Just entered the play state. Do anything that needs to be done on the state transition
			break;
		case SCE_AVPLAYER_STATE_STOP:
			// Just entered the stop state. Do anything that needs to be done on the state transition
			break;
		case SCE_AVPLAYER_STATE_PAUSE:
			// Just entered the pause state. Do anything that needs to be done on the state transition
			break;
		case SCE_AVPLAYER_STATE_BUFFERING:
			// Just entered the buffering state. Not sure what that means, perhaps it means not displaying a frame yet
			break;
		default:
			// Should never get here.
			break;
	};
}

bool FAVPlayerMovieStreamer::StartNextMovie()
{
	bool bDidStartMovie = false;
	if (MovieQueue.Num() > 0)
	{
		// Set everything up for playback. 
		if (SetupPlayback())
		{
			// Use the engine's file system to read the movie content
			FString MoviePath = FPaths::ProjectContentDir() + FString::Printf(TEXT("Movies/%s.%s"), *(MovieQueue[0]), MOVIE_FILE_EXTENSION);

			MovieQueue.RemoveAt(0);

			// NOTE: Paths need to be UTF8 for the Sony APIs
			int32 Result = sceAvPlayerAddSource(SamplePlayer, TCHAR_TO_UTF8(*MoviePath));
			if (Result < 0)
			{
				UE_LOG(LogMoviePlayer, Warning, TEXT("sceAvPlayerAddSource() failed. Filename [ %s ] not found"), *MoviePath);
			}
			else
			{
				UE_LOG(LogMoviePlayer, Log, TEXT("Queued movie path: [ %s ]"), *MoviePath);
				bDidStartMovie = true;
			}
		}
	}
	return bDidStartMovie;
}

FAVPlayerMovieStreamer::TickGuard::TickGuard(FAVPlayerMovieStreamer *MovieStreamer)
{
	Streamer = MovieStreamer;
	Streamer->bIsInTick = true;
}

FAVPlayerMovieStreamer::TickGuard::~TickGuard()
{
	Streamer->bIsInTick = false;
}

bool FAVPlayerMovieStreamer::Tick(float DeltaTime)
{
	FAVPlayerMovieStreamer::TickGuard(this);
	check(IsInRenderingThread());

	// Returns false if a movie is still playing, true if the movie list has completed
	// NOTE: Movie playback ignores DeltaTime. Time is wall time

	// If we aren't fully initialized and running, pretend the movie has completed
	if (!SamplePlayer)
	{
		return true;
	}

	// If the player is still doing work, check for video frames
	if (sceAvPlayerIsActive(SamplePlayer))
	{
		// Remember that we were active. Used to edge detect active/not-active transitions
		bWasActive = true;

		// Check for new video frames here, synchronously
		// NOTE: This can take a long time if a video frame is ready because it will
		// have to convert YUV to RGB for the entire frame, on the CPU, pixel by pixel
		CheckForNextVideoFrame();

		// Still playing, so tell the caller we are not done
		return false;
	}
	else
	{
		// We aren't active any longer, so stop the player
		if (bWasActive)
		{
			// Make sure we only teardown playback once
			bWasActive = false;

			// The previous playback is complete, so shutdown the threads and such.
			// NOTE: The texture resources are not freed here.
			TeardownPlayback();

			// If there are still movies to play, start the next movie
			if (MovieQueue.Num() > 0)
			{
				StartNextMovie();

				// Still playing a movie, so return that we aren't done yet
				return false;
			}
		}
	}

	// By default, we are done streaming
	return true;
}

TSharedPtr<class ISlateViewport> FAVPlayerMovieStreamer::GetViewportInterface()
{
	return MovieViewport;
}

float FAVPlayerMovieStreamer::GetAspectRatio() const
{
	return (float)MovieViewport->GetSize().X / (float)MovieViewport->GetSize().Y;
}

void FAVPlayerMovieStreamer::Cleanup()
{
	// NOTE: Called from the main thread
	for (int32 i = 0; i < NumBufferedTextures; ++i)
	{
		TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe>& VideoTexture = BufferedVideoTextures[i];
		if (VideoTexture.IsValid())
		{
			// Schedule the release of the video texture(s) and wait the release to complete before
			// losing the reference to the texture
			BeginReleaseResource(VideoTexture.Get());
			FlushRenderingCommands();
			VideoTexture.Reset();
		}
	}
	// Make sure the viewport doesn't have a lingering reference
	MovieViewport->SetTexture(nullptr);
	SlateVideoTexture.Reset();
}

//
// Audio playback functionality
//

void* FAVPlayerMovieStreamer::AudioThreadFunction(void* Arg)
{
	FAVPlayerMovieStreamer* Streamer = (FAVPlayerMovieStreamer*) Arg;

	int32 Result = 0;

#if USE_NEW_PS4_MEMORY_SYSTEM
	uint8* NoSound = (uint8*)FMemory::Malloc(4096 * 4, 0x20);
#else


#ifdef HACK_SELF_ALIGN
	uint8* NoSound = (uint8*)AllocateSelfAligned(4096 * 4, 0x20);
#else
	uint8* NoSound = (uint8*)FMemory::Malloc(4096 * 4, 0x20);
#endif

#endif

	FMemory::Memset(NoSound, 0, 4096*4);

	SceAvPlayerFrameInfo audioFrame;
	FMemory::Memset(&audioFrame, 0, sizeof(SceAvPlayerFrameInfo));

	while (Streamer->bAudioThreadShouldRun)
	{
		bool bFrameAvailable = sceAvPlayerGetAudioData(Streamer->SamplePlayer, &audioFrame);

		if (bFrameAvailable)
		{
			Result = Streamer->SendAudio((uint8*)audioFrame.pData, audioFrame.details.audio.sampleRate, audioFrame.details.audio.channelCount);
		}
		else
		{
			//E When no audio is available silence is output to avoid this thread from blocking other threads
			Result = Streamer->SendAudio(NoSound, 0, 0);
		}
	}

	// The thread is exiting, so make sure we don't have lingering audio
	Result = Streamer->SendAudioEnd();

#if USE_NEW_PS4_MEMORY_SYSTEM
	FMemory::Free(NoSound);
#else

#ifdef HACK_SELF_ALIGN
	FreeSelfAligned(NoSound);
#else
	FMemory::Free(NoSound);
#endif	

#endif

	scePthreadExit(nullptr);
	return nullptr;
}

bool FAVPlayerMovieStreamer::SetupAudio()
{
	AudioHandle	= -1;
	NumAudioChannels = DEFAULT_NUM_CHANNELS;
	AudioPortType = SCE_AUDIO_OUT_PORT_TYPE_MAIN;
	AudioSampleRate = BASE_SAMPLE_RATE;
	PCMBufferSize = SINGLE_PCM_BUFFER * DEFAULT_NUM_CHANNELS;

	int32 AudioFormat = (NumAudioChannels > 2) ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_8CH : SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO;

	int32 Result = sceAudioOutInit();
	if (Result < 0 && Result != SCE_AUDIO_OUT_ERROR_ALREADY_INIT)
	{
		// We failed for any reason other than the output system has already been initialized
		UE_LOG(LogMoviePlayer, Warning, TEXT("sceAudioOutInit failed in SetupAudio(). Error code 0x%x"), Result);
		return false;
	}

	AudioHandle = sceAudioOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, AudioPortType, 0, NUM_PCM_SAMPLES, BASE_SAMPLE_RATE, AudioFormat);

	if (AudioHandle < 0)
	{
		UE_LOG(LogMoviePlayer, Warning, TEXT("sceAudioOutOpen failed in SetupAudio(). Error code 0x%x"), AudioHandle);
		return false;
	}

	// Create the audio processing thread
	bAudioThreadShouldRun = true;

	ScePthreadAttr Threadattr;
	scePthreadAttrInit(&Threadattr);
	scePthreadAttrSetstacksize(&Threadattr, 1024 * 1024);
	Result = scePthreadCreate(&AudioThread, &Threadattr, AudioThreadFunction, this, "FAVPlayer_audio");
	scePthreadAttrDestroy(&Threadattr);
	if (Result < 0)
	{
		UE_LOG(LogMoviePlayer, Warning, TEXT("Failed to create audio thread. Error code 0x%x"), Result);
		return false;
	}

	return true;
}

void FAVPlayerMovieStreamer::TeardownAudio()
{
	bAudioThreadShouldRun = false;
	if (AudioThread)
	{
		scePthreadJoin(AudioThread, nullptr);
		AudioThread = nullptr;
	}

	if (AudioHandle != -1)
	{
		sceAudioOutOutput(AudioHandle, NULL);
		int32 Result = sceAudioOutClose(AudioHandle);
		if (Result < 0)
		{
			UE_LOG( LogMoviePlayer, Warning, TEXT( "sceAudioOutClose failed in TeardownAudio. Error code 0x%x" ), Result );
		}

		AudioHandle = -1;
	}
}

/**
	* __Hermite
	* @brief 4-point, 3rd-order Hermite (x-form) resample.
	* @param fX Position through current sample curve
	* @param fY0 -1 sample
	* @param fY1 0 sample
	* @param fY2 1 sample
	* @param fY3 2 sample
	* @return float value of resampled sample
*/
static inline float __Hermite(float fX, float fY0, float fY1, float fY2, float fY3)
{
	// 4-point, 3rd-order Hermite (x-form)
	float fC0 = fY1;
	float fC1 = 0.5f * (fY2 - fY0);
	float fC2 = fY0 - 2.5f * fY1 + 2.f * fY2 - 0.5f * fY3;
	float fC3 = 1.5f * (fY1 - fY2) + 0.5f * (fY3 - fY0);
	return ((fC3 * fX + fC2) * fX + fC1) * fX + fC0;
}

bool FAVPlayerMovieStreamer::SoundOutputResample(int16* Data, int32 NumSamples, uint32 SampleRate, uint32 ChannelCount)
{
	float OffsetStep = (float)SampleRate / BASE_SAMPLE_RATE;

	int32 OutputOffset = NumOutputSamples * ChannelCount;

	while (NumSamples > 0)
	{
		// If sufficient Data is available (three input samples plus one prior sample) then output a resample
		if (NumLeadInSamples >= 3)
		{
			for (uint32 Channel = 0; Channel < ChannelCount; Channel++)
			{
				ResampleOutputBuffer[OutputOffset++] = __Hermite(InputOffset, ResampleData[Channel][0], ResampleData[Channel][1], ResampleData[Channel][2], ResampleData[Channel][3]);
			}

			if (ChannelCount == 6)
			{
				// Need to re-map for 8 channel output
				float LeftSurround = ResampleOutputBuffer[OutputOffset - 2];
				float RightSurround = ResampleOutputBuffer[OutputOffset - 1];

				ResampleOutputBuffer[OutputOffset++] = LeftSurround;
				ResampleOutputBuffer[OutputOffset++] = RightSurround;
			}

			NumOutputSamples++;

			// If the output buffer has sufficient samples then output
			if (NumOutputSamples >= NUM_PCM_SAMPLES)
			{
				int32 Result = sceAudioOutOutput(AudioHandle, &ResampleOutputBuffer[0]);
				if (Result < 0)
				{
					UE_LOG(LogMoviePlayer, Warning, TEXT("sceAudioOutOutput failed in SoundOutputResample. Error code 0x%x"), Result);
					return false;
				}

				NumOutputSamples = 0;
				OutputOffset = 0;
			}
		}

		// Advance the input offset and re-fill the resample Data if necessary
		InputOffset += OffsetStep;

		while ((InputOffset >= 1.0f) && (NumSamples > 0))
		{
			for (uint32 Channel = 0; Channel < ChannelCount; Channel++)
			{
				ResampleData[Channel][0] = ResampleData[Channel][1];
				ResampleData[Channel][1] = ResampleData[Channel][2];
				ResampleData[Channel][2] = ResampleData[Channel][3];
				ResampleData[Channel][3] = ((float)*Data++) / SHRT_MAX;
			}

			InputOffset -= 1.0f;
			if (NumLeadInSamples < 3)
			{
				NumLeadInSamples++;
			}
			NumSamples--;
		}
	}

	return true;
}

bool FAVPlayerMovieStreamer::SoundOutputTo8Channel(int16* Data, int32 NumSamples, uint32 SampleRate, uint32 ChannelCount)
{
	int16* Output = (int16*)&ResampleOutputBuffer[0];

	// For now only conversion from 5.1 to 7.1 is supported
	switch (ChannelCount)
	{
	case 6:
		for (int32 Sample = 0; Sample < NumSamples; Sample++)
		{
			*Output++ = *Data++;	// L
			*Output++ = *Data++;	// R
			*Output++ = *Data++;	// C
			*Output++ = *Data++;	// LFE

			int16 LeftSurround = *Data++;
			int16 RightSurround = *Data++;

			*Output++ = LeftSurround;	// Lsurround
			*Output++ = RightSurround;	// Rsurround
			*Output++ = LeftSurround;	// Lextend
			*Output++ = RightSurround;	// Rextend
		}
		break;

	default:
		return false;
		break;
	}

	if (sceAudioOutOutput(AudioHandle, &ResampleOutputBuffer[0]) < 0)
	{
		return false;
	}

	return true;
}

bool FAVPlayerMovieStreamer::SendAudio(uint8* Data, uint32 SampleRate, uint32 ChannelCount)
{
	// Don't output anything if we don't have an initialized audio processor.
	if (AudioHandle < 0)
	{
		return 0;
	}

	// If the current configuration doesn't match the parameters, reconfigure to match the parameters
	if
	(
		(
			((int32)SampleRate != AudioSampleRate) || (NumAudioChannels != ChannelCount) || (SINGLE_PCM_BUFFER * ChannelCount != PCMBufferSize)
		)
		&& (ChannelCount != 0)
		&& (SampleRate != 0)
	)
	{
		// Close the current audio port
		sceAudioOutOutput(AudioHandle, NULL);
		int32 Result = sceAudioOutClose(AudioHandle);
		if (Result < 0)
		{
			UE_LOG(LogMoviePlayer, Warning, TEXT("sceAudioOutClose failed in SendAudio. Error code 0x%x"), Result);
			return false;
		}

		// If the sample rate is not 48kHz then the output will need to be re-sampled
		int32 ChannelFormat;

		if (SampleRate == BASE_SAMPLE_RATE)
		{
			if (ChannelCount == 1)
			{
				ChannelFormat = SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO;
			}
			else if (ChannelCount == 2)
			{
				ChannelFormat = SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO;
			}
			else
			{
				ChannelFormat = SCE_AUDIO_OUT_PARAM_FORMAT_S16_8CH;
			}
		}
		else
		{
			if (ChannelCount == 1)
			{
				ChannelFormat = SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_MONO;
			}
			else if (ChannelCount == 2)
			{
				ChannelFormat = SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO;
			}
			else
			{
				ChannelFormat = SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH;
			}

			// Reset resample data
			InputOffset = 0.0;
			FMemory::Memset(&ResampleData[0][0], 0, sizeof(ResampleData));
			NumLeadInSamples = 0;
			NumOutputSamples = 0;
		}

		PCMBufferSize = SINGLE_PCM_BUFFER * ChannelCount;
		NumAudioChannels = ChannelCount;
		AudioSampleRate = SampleRate;
		AudioHandle = sceAudioOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, AudioPortType, 0, NUM_PCM_SAMPLES, BASE_SAMPLE_RATE, ChannelFormat);

		if (AudioHandle < 0)
		{
			UE_LOG(LogMoviePlayer, Warning, TEXT("sceAudioOutOpen failed in SendAudio. Error code 0x%x"), AudioHandle);
			return false;
		}
		UE_LOG(LogMoviePlayer, Log, TEXT("Sample rate changed to %d"), AudioSampleRate);
	}

	// The configuration is valid, so send the data based on the configuration
	if ((SampleRate == BASE_SAMPLE_RATE) || (SampleRate == 0))
	{
		if ((ChannelCount > 2) && (ChannelCount < 8))
		{
			// Need to re-map for 8 channel output
			int32 Result = SoundOutputTo8Channel((int16*)Data, NUM_PCM_SAMPLES, SampleRate, ChannelCount);
			if (Result < 0)
			{
				UE_LOG(LogMoviePlayer, Warning, TEXT("SoundOutputTo8Channel failed in SendAudio. Error code 0x%x"), Result);
				return false;
			}
		}
		else
		{
			// The data can be used as-is
			int32 Result = sceAudioOutOutput(AudioHandle, Data);
			if (Result < 0)
			{
				UE_LOG(LogMoviePlayer, Warning, TEXT("sceAudioOutOutput failed in SendAudio. Error code 0x%x"), Result);
				return false;
			}
		}
	}
	else
	{
		// Re-sample to 48kHz and output
		int32 Result = SoundOutputResample((int16*)Data, NUM_PCM_SAMPLES, SampleRate, ChannelCount);
		if (Result < 0)
		{
			UE_LOG(LogMoviePlayer, Warning, TEXT("SoundOutputResample failed in SendAudio. Error code 0x%x"), Result);
			return false;
		}
	}

	return true;
}

bool FAVPlayerMovieStreamer::SendAudioEnd()
{
	if (sceAudioOutOutput(AudioHandle,nullptr) < 0)
	{
		return false;
	}

	return true;
}

FString FAVPlayerMovieStreamer::GetMovieName()
{
	return MovieQueue.Num() > 0 ? MovieQueue[0] : TEXT("");
}

bool FAVPlayerMovieStreamer::IsLastMovieInPlaylist()
{
	return MovieQueue.Num() <= 1;
}

