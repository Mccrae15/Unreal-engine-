// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Misc/Build.h"
#include "PS4/PS4SystemIncludes.h"
#include <libdbg.h>
#include <perf.h>
#include <x86intrin.h>

#if UE_BUILD_SHIPPING
#define UE_DEBUG_BREAK() ((void)0)
#else
#define UE_DEBUG_BREAK() (FPS4Misc::DebugBreakInternal())
#endif

/**
 * PS4 implementation of the misc OS functions
 */
struct CORE_API FPS4Misc : public FGenericPlatformMisc
{
	static void PlatformInit();
	static void PlatformHandleSplashScreen(bool ShowSplashScreen = false);
	static class IPlatformChunkInstall* GetPlatformChunkInstall();
	static bool SupportsMessaging();
	static FString GetDefaultLocale();
	static FString GetTimeZoneId();
	static bool IsRunningOnDevKit();
	static const TCHAR* RootDir();
	static const TCHAR* EngineDir();
	static const TCHAR* ProjectDir();
	static FString CloudDir();
	static const TCHAR* GamePersistentDownloadDir();
	static FString GetCPUVendor();
	static FString GetCPUBrand();
	static FString GetPrimaryGPUBrand();
	static void GetOSVersions(FString& out_OSVersionLabel, FString& out_OSSubVersionLabel);
	static FString GetOSVersion();
	static void RequestExit(bool Force);
	DEPRECATED(4.14, "GetMacAddress is deprecated. It is not reliable on all platforms")
	static TArray<uint8> GetMacAddress();
	/**
	* Implemented using sceNetGetMacAddress,
	* so all the caveats that apply to that API call apply here.
	* WARNING: Specifically, Sony requires that you get a waiver to use this method
	*          with restrictions on how it can be used.
	*/
	static FString GetDeviceId();

#if !UE_BUILD_SHIPPING
	FORCEINLINE static bool IsDebuggerPresent()
	{
		extern CORE_API bool GIgnoreDebugger;
		if(GIgnoreDebugger == false && IsRunningOnDevKit() )
		{
			return !!::sceDbgIsDebuggerAttached();
		}
		else
		{
			return false;
		}
	}

	FORCEINLINE static void DebugBreakInternal()
	{
		if (IsDebuggerPresent())
		{
			SCE_BREAK();
		}
	}

	DEPRECATED(4.19, "FPlatformMisc::DebugBreak is deprecated. Use the UE_DEBUG_BREAK() macro instead.")
	FORCEINLINE static void DebugBreak()
	{
		UE_DEBUG_BREAK();
	}
#endif

	/** Break into debugger. Returning false allows this function to be used in conditionals. */
	DEPRECATED(4.19, "FPlatformMisc::DebugBreakReturningFalse is deprecated. Use the (UE_DEBUG_BREAK(), false) expression instead.")
	FORCEINLINE static bool DebugBreakReturningFalse()
	{
#if !UE_BUILD_SHIPPING
		UE_DEBUG_BREAK();
#endif
		return false;
	}

	/** Prompts for remote debugging if debugger is not attached. Regardless of result, breaks into debugger afterwards. Returns false for use in conditionals. */
	DEPRECATED(4.19, "FPlatformMisc::DebugBreakAndPromptForRemoteReturningFalse() is deprecated.")
	static FORCEINLINE bool DebugBreakAndPromptForRemoteReturningFalse(bool bIsEnsure = false)
	{
#if !UE_BUILD_SHIPPING
		if (!IsDebuggerPresent())
		{
			PromptForRemoteDebugging(bIsEnsure);
		}
		UE_DEBUG_BREAK();
#endif

		return false;
	}

	FORCEINLINE static int32 NumberOfCores()
	{
		//machine has 8 cores, but the OS steals 2 for its exclusive use.
		return 6;
	}

	FORCEINLINE static const TCHAR* GetNullRHIShaderFormat()  
	{
		return TEXT("SF_PS4"); 
	}

	FORCEINLINE static const TCHAR* GetPlatformFeaturesModuleName()
	{
		return TEXT("PS4PlatformFeatures");
	}

	FORCEINLINE static bool SupportsFullCrashDumps()
	{
		return false;
	}

	FORCEINLINE static void MemoryBarrier()
	{
		// Equivalent to __faststorefence on MSVC for AMD64 platforms.
		// Guarantees that every previous memory reference, including both load and store 
		// memory references, is globally visible before any subsequent memory reference.
		//
		//		2.6 times faster than _mm_sfence() on PS4.
		//
		// Volatile and the "memory" clobber makes the compiler treat this block of assembly
		// as a memory read/write fence, so the optimizer doesn't reorder things.
		//
		asm volatile ("lock; orl $0,(%%rsp)" ::: "memory");
	}

	FORCEINLINE static bool SupportsLocalCaching()
	{
		return true;
	}

	static bool HasVariableHardware()
	{
		return false;
	}

	static int32 GetCacheLineSize() { return 64; }

	FORCEINLINE static void PrefetchBlock(const void* InPtr, int32 NumBytes = 1)
	{
		const char* Ptr = (const char*)InPtr;
		const int32 CacheLineSize = GetCacheLineSize();
		for (int32 LinesToPrefetch = (NumBytes + CacheLineSize - 1) / CacheLineSize; LinesToPrefetch; --LinesToPrefetch)
		{
			_mm_prefetch(Ptr, _MM_HINT_T0);
			Ptr += CacheLineSize;
		}
	}
	
	FORCEINLINE static void Prefetch(void const* x, int32 offset = 0)
	{
		_mm_prefetch((char const*)(x)+offset, _MM_HINT_T0);
	}

	static bool GetSHA256Signature(const void* Data, uint32 ByteSize, FSHA256Signature& OutSignature);

	// Disable anonymous usage data network traffic on PS4
	static bool AllowSendAnonymousGameUsageDataToEpic()
	{
		return false;
	}

#if PS4_PROFILING_ENABLED
	/**
	 * Platform specific function for adding a named event that can be viewed in RazorCPU
	 */
	static void BeginNamedEvent(const struct FColor& Color, const TCHAR* Text);
	static void BeginNamedEvent(const struct FColor& Color, const ANSICHAR* Text);

	/**
	* Platform specific function for closing a named event that can be viewed in RazorCPU
	*/
	static void EndNamedEvent();

    /**
	* Platform specific function for initializing storage of tagged memory buffers
	*/
	static void InitTaggedStorage(uint32 NumTags);
	
   /**
	* Platform specific function for freeing storage of tagged memory buffers
	*/
	static void ShutdownTaggedStorage();
	
	
    /**
	* Platform specific function for tagging a memory buffer with a label. Helps see memory access in Razor
	*/
	static void TagBuffer(const char* Label, uint32 Category, const void* Buffer, size_t BufferSize);
#else
	FORCEINLINE static void BeginNamedEvent(const struct FColor& Color, const TCHAR* Text) { }
	FORCEINLINE static void BeginNamedEvent(const struct FColor& Color, const ANSICHAR* Text) { }
	FORCEINLINE static void EndNamedEvent() { }
	FORCEINLINE static void InitTaggedStorage(uint32 ){}
	FORCEINLINE static void ShutdownTaggedStorage(){}
	FORCEINLINE static void TagBuffer(const char* Label, uint32 Category, const void* Buffer, size_t BufferSize) {}
#endif

	static EAppReturnType::Type MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption);

};


typedef FPS4Misc FPlatformMisc;
