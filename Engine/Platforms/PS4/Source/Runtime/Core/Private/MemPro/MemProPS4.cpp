/*
	This software is provided 'as-is', without any express or implied warranty.
	In no event will the author(s) be held liable for any damages arising from
	the use of this software.

	Permission is granted to anyone to use this software for any purpose, including
	commercial applications, and to alter it and redistribute it freely, subject to
	the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.

	Author: Stewart Lynch
	www.puredevsoftware.com
	slynch@puredevsoftware.com

	This code is released to the public domain, as explained at
	http://creativecommons.org/publicdomain/zero/1.0/

	MemProLib is the library that allows the MemPro application to communicate
	with your application.
*/
//------------------------------------------------------------------------
#if defined(__UNREAL__)
	#include "MemPro/MemProPS4.h"
#else
	#include "MemProPS4.hpp"
#endif

//------------------------------------------------------------------------
#if MEMPRO_ENABLED

//------------------------------------------------------------------------
#ifdef MEMPRO_PLATFORM_PS4

//------------------------------------------------------------------------
#include <thread>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <net.h>
#include <pthread.h>
#include <fios2.h>
#include <libdbg.h>

#if defined(__UNREAL__)
	#include "HAL/LowLevelMemTracker.h"
	#include "HAL/PlatformFileManager.h"
	#include "Misc/Paths.h"
	#include "PS4PlatformFile.h"
	#include "PS4PlatformStackWalk.h"
#endif

//------------------------------------------------------------------------
#define SOMAXCONN 0x7fffffff

//------------------------------------------------------------------------
//#define MEMPRO_PS4_USE_POSIX_THREADING

//------------------------------------------------------------------------
namespace MemPro
{
	//------------------------------------------------------------------------
	const int g_StackTraceSize = 128;

	//------------------------------------------------------------------------
	typedef int SOCKET;
	typedef int DWORD;
	typedef int* PDWORD;
	enum SocketValues { INVALID_SOCKET = -1 };
#ifndef UINT_MAX
	enum MaxValues { UINT_MAX = 0xffffffff };
#endif
#define _T(s) s
	enum SocketErrorCodes { SOCKET_ERROR = -1 };
	enum SystemDefines { MAX_PATH = 256 };

	//------------------------------------------------------------------------
	void Platform::CreateLock(void* p_os_lock_mem, int os_lock_mem_size)
	{
		MEMPRO_ASSERT(os_lock_mem_size >= sizeof(ScePthreadMutex));
		new (p_os_lock_mem)ScePthreadMutex();
		scePthreadMutexInit((ScePthreadMutex*)p_os_lock_mem, NULL, "MemProLock");
	}

	//------------------------------------------------------------------------
	void Platform::DestroyLock(void* p_os_lock_mem)
	{
		((ScePthreadMutex*)p_os_lock_mem)->~ScePthreadMutex();
		scePthreadMutexDestroy((ScePthreadMutex*)p_os_lock_mem);
	}

	//------------------------------------------------------------------------
	void Platform::TakeLock(void* p_os_lock_mem)
	{
		scePthreadMutexLock((ScePthreadMutex*)p_os_lock_mem);
	}

	//------------------------------------------------------------------------
	void Platform::ReleaseLock(void* p_os_lock_mem)
	{
		scePthreadMutexUnlock((ScePthreadMutex*)p_os_lock_mem);
	}

	//------------------------------------------------------------------------
#ifndef MEMPRO_WRITE_DUMP
	bool g_SocketsInitialised = false;
#endif

	//------------------------------------------------------------------------
#ifndef MEMPRO_WRITE_DUMP
	bool InitialiseSockets()
	{
		if (!g_SocketsInitialised)
		{
			g_SocketsInitialised = true;
			sceNetInit();
		}

		return true;
	}
#endif

	//------------------------------------------------------------------------
#ifndef MEMPRO_WRITE_DUMP
	void Platform::UninitialiseSockets()
	{
		if (g_SocketsInitialised)
		{
			sceNetTerm();
			g_SocketsInitialised = false;
		}
	}
#endif

	//------------------------------------------------------------------------
#ifndef MEMPRO_WRITE_DUMP
	void Platform::CreateSocket(void* p_os_socket_mem, int os_socket_mem_size)
	{
		MEMPRO_ASSERT(os_socket_mem_size >= sizeof(SceNetId));
		*(SceNetId*)p_os_socket_mem = INVALID_SOCKET;
	}
#endif

	//------------------------------------------------------------------------
#ifndef MEMPRO_WRITE_DUMP
	bool Platform::IsValidSocket(const void* p_os_socket_mem)
	{
		return *(const SceNetId*)p_os_socket_mem != INVALID_SOCKET;
	}
#endif

	//------------------------------------------------------------------------
#ifndef MEMPRO_WRITE_DUMP
	void HandleError()
	{
		MEMPRO_ASSERT(false);
	}
#endif

	//------------------------------------------------------------------------
#ifndef MEMPRO_WRITE_DUMP
	void Platform::Disconnect(void* p_os_socket_mem)
	{
		SceNetId& socket = *(SceNetId*)p_os_socket_mem;

#if defined(MEMPRO_PS4_USE_POSIX_THREADING)
		if (shutdown(socket, SHUT_RDWR) == SOCKET_ERROR)
			HandleError();
#else
		if (sceNetShutdown(socket, SHUT_RDWR) == SOCKET_ERROR)
			HandleError();
#endif

		// loop until the socket is closed to ensure all data is sent
		unsigned int buffer = 0;
		size_t ret = 0;
#if defined(MEMPRO_PS4_USE_POSIX_THREADING)
		do { ret = recv(socket, (char*)&buffer, sizeof(buffer), 0); } while (ret != 0 && ret != (size_t)SOCKET_ERROR);
#else
		do { ret = sceNetRecv(socket, (char*)&buffer, sizeof(buffer), 0); } while (ret != 0 && ret != (size_t)SOCKET_ERROR);
#endif

		sceNetSocketClose(socket);

#if defined(MEMPRO_PS4_USE_POSIX_THREADING)
		close(socket);
#else
		sceNetSocketClose(socket);
#endif

		socket = INVALID_SOCKET;
	}
#endif

	//------------------------------------------------------------------------
#ifndef MEMPRO_WRITE_DUMP
	bool Platform::StartListening(void* p_os_socket_mem)
	{
		SceNetId& socket = *(SceNetId*)p_os_socket_mem;

		MEMPRO_ASSERT(socket != INVALID_SOCKET);

#if defined(MEMPRO_PS4_USE_POSIX_THREADING)
		if (listen(socket, SOMAXCONN) == SOCKET_ERROR)
#else
		if (sceNetListen(socket, SOMAXCONN) == SOCKET_ERROR)
#endif
		{
			HandleError();
			return false;
		}
		return true;
	}
#endif

	//------------------------------------------------------------------------
#ifndef MEMPRO_WRITE_DUMP
	bool Platform::BindSocket(void* p_os_socket_mem, const char* p_port)
	{
		SceNetId& socket = *(SceNetId*)p_os_socket_mem;

		MEMPRO_ASSERT(socket == INVALID_SOCKET);

		if (!InitialiseSockets())
			return false;

#if defined(MEMPRO_PS4_USE_POSIX_THREADING)
		socket = ::socket(
			AF_INET,
			SOCK_STREAM,
			IPPROTO_TCP);
#else
		socket = sceNetSocket(
			"MemPro",
			SCE_NET_AF_INET,
			SCE_NET_SOCK_STREAM,
			0);
#endif

		if (socket == INVALID_SOCKET)
		{
			HandleError();
			return false;
		}

		// Setup the TCP listening socket
		// Bind to INADDR_ANY
#if defined(MEMPRO_PS4_USE_POSIX_THREADING)
		SOCKADDR_IN sa;
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = INADDR_ANY;
		int iport = atoi(p_port);
		sa.sin_port = htons(iport);
		int result = ::bind(socket, (const sockaddr*)(&sa), sizeof(SOCKADDR_IN));
#else
		SceNetSockaddrIn sa;
		sa.sin_family = SCE_NET_AF_INET;
		sa.sin_addr.s_addr = SCE_NET_INADDR_ANY;
		int iport = atoi(p_port);
		sa.sin_port = sceNetHtons(iport);
		int result = sceNetBind(socket, (const SceNetSockaddr*)(&sa), sizeof(SceNetSockaddrIn));
#endif

		if (result == SOCKET_ERROR)
		{
			HandleError();
			Disconnect(p_os_socket_mem);
			return false;
		}

		return true;
	}
#endif

	//------------------------------------------------------------------------
#ifndef MEMPRO_WRITE_DUMP
	bool Platform::AcceptSocket(void* p_os_socket_mem, void* p_client_os_socket_mem)
	{
		SceNetId& socket = *(SceNetId*)p_os_socket_mem;
		SceNetId& client_socket = *(SceNetId*)p_client_os_socket_mem;

		MEMPRO_ASSERT(client_socket == INVALID_SOCKET);
#if defined(MEMPRO_PS4_USE_POSIX_THREADING)
		client_socket = accept(socket, NULL, NULL);
#else
		client_socket = sceNetAccept(socket, NULL, NULL);
#endif
		return client_socket != INVALID_SOCKET;
	}
#endif

	//------------------------------------------------------------------------
#ifndef MEMPRO_WRITE_DUMP
	bool Platform::SocketSend(void* p_os_socket_mem, void* p_buffer, int size)
	{
		SceNetId& socket = *(SceNetId*)p_os_socket_mem;

		int bytes_to_send = size;
		while (bytes_to_send != 0)
		{
#if defined(MEMPRO_PS4_USE_POSIX_THREADING)
			int bytes_sent = (int)send(socket, (char*)p_buffer, bytes_to_send, 0);
#else
			int bytes_sent = (int)sceNetSend(socket, (char*)p_buffer, bytes_to_send, 0);
#endif

			if (bytes_sent == SOCKET_ERROR)
			{
				HandleError();
				Disconnect(p_os_socket_mem);
				return false;
			}
			p_buffer = (char*)p_buffer + bytes_sent;
			bytes_to_send -= bytes_sent;
		}

		return true;
	}
#endif

	//------------------------------------------------------------------------
#ifndef MEMPRO_WRITE_DUMP
	int Platform::SocketReceive(void* p_os_socket_mem, void* p_buffer, int size)
	{
		SceNetId& socket = *(SceNetId*)p_os_socket_mem;

		int total_bytes_received = 0;
		while (size)
		{
#if defined(MEMPRO_PS4_USE_POSIX_THREADING)
			int bytes_received = (int)recv(socket, (char*)p_buffer, size, 0);
#else
			int bytes_received = (int)sceNetRecv(socket, (char*)p_buffer, size, 0);
#endif

			total_bytes_received += bytes_received;

			if (bytes_received == 0)
			{
				Disconnect(p_os_socket_mem);
				return bytes_received;
			}
			else if (bytes_received == SOCKET_ERROR)
			{
				HandleError();
				Disconnect(p_os_socket_mem);
				return bytes_received;
			}

			size -= bytes_received;
			p_buffer = (char*)p_buffer + bytes_received;
		}

		return total_bytes_received;
	}
#endif

	//------------------------------------------------------------------------
	struct PlatformEvent
	{
		mutable ScePthreadCond m_Cond;
		mutable ScePthreadMutex m_Mutex;
		mutable volatile bool m_Signalled;
		bool m_AutoReset;
	};

	//------------------------------------------------------------------------
	void Platform::MemProCreateEvent(
		void* p_os_event_mem,
		int os_event_mem_size,
		bool initial_state,
		bool auto_reset)
	{
		MEMPRO_ASSERT(os_event_mem_size >= sizeof(PlatformEvent));

		PlatformEvent& platform_event = *(PlatformEvent*)p_os_event_mem;

		scePthreadCondInit(&platform_event.m_Cond, NULL, NULL);
		scePthreadMutexInit(&platform_event.m_Mutex, NULL, NULL);
		platform_event.m_Signalled = false;
		platform_event.m_AutoReset = auto_reset;

		if (initial_state)
			SetEvent(p_os_event_mem);
	}

	//------------------------------------------------------------------------
	void Platform::DestroyEvent(void* p_os_event_mem)
	{
		PlatformEvent& platform_event = *(PlatformEvent*)p_os_event_mem;

		scePthreadMutexDestroy(&platform_event.m_Mutex);
		scePthreadCondDestroy(&platform_event.m_Cond);
	}

	//------------------------------------------------------------------------
	void Platform::SetEvent(void* p_os_event_mem)
	{
		PlatformEvent& platform_event = *(PlatformEvent*)p_os_event_mem;

		scePthreadMutexLock(&platform_event.m_Mutex);
		platform_event.m_Signalled = true;
		scePthreadMutexUnlock(&platform_event.m_Mutex);
		scePthreadCondSignal(&platform_event.m_Cond);
	}

	//------------------------------------------------------------------------
	void Platform::ResetEvent(void* p_os_event_mem)
	{
		PlatformEvent& platform_event = *(PlatformEvent*)p_os_event_mem;

		scePthreadMutexLock(&platform_event.m_Mutex);
		platform_event.m_Signalled = false;
		scePthreadMutexUnlock(&platform_event.m_Mutex);
	}

	//------------------------------------------------------------------------
	int Platform::WaitEvent(void* p_os_event_mem, int timeout)
	{
		PlatformEvent& platform_event = *(PlatformEvent*)p_os_event_mem;

		scePthreadMutexLock(&platform_event.m_Mutex);

		if (platform_event.m_Signalled)
		{
			platform_event.m_Signalled = false;
			scePthreadMutexUnlock(&platform_event.m_Mutex);
			return true;
		}

		if (timeout == -1)
		{
			while (!platform_event.m_Signalled)
				scePthreadCondWait(&platform_event.m_Cond, &platform_event.m_Mutex);

			if (!platform_event.m_AutoReset)
				platform_event.m_Signalled = false;

			scePthreadMutexUnlock(&platform_event.m_Mutex);
			return true;
		}
		else
		{
			int ret = 0;
			do
			{
				ret = scePthreadCondTimedwait(&platform_event.m_Cond, &platform_event.m_Mutex, timeout / 100);
			} while (!platform_event.m_Signalled && ret != SCE_KERNEL_ERROR_ETIMEDOUT);

			if (ret != SCE_KERNEL_ERROR_ETIMEDOUT)
			{
				if (!platform_event.m_AutoReset)
					platform_event.m_Signalled = false;

				scePthreadMutexUnlock(&platform_event.m_Mutex);
				return true;
			}

			scePthreadMutexUnlock(&platform_event.m_Mutex);
			return false;
		}
	}

	//------------------------------------------------------------------------
	struct PlatformThread
	{
		ScePthread m_Handle;
		bool m_Alive;
		ThreadMain mp_ThreadMain;
		void* mp_Param;
	};

	//------------------------------------------------------------------------
	void Platform::CreateThread(void* p_os_thread_mem, int os_thread_mem_size)
	{
		MEMPRO_ASSERT(os_thread_mem_size >= sizeof(PlatformThread));
		new (p_os_thread_mem)PlatformThread();
		PlatformThread& platform_thread = *(PlatformThread*)p_os_thread_mem;
		platform_thread.m_Alive = false;
		platform_thread.mp_ThreadMain = NULL;
		platform_thread.mp_Param = NULL;
	}

	//------------------------------------------------------------------------
	void Platform::DestroyThread(void* p_os_thread_mem)
	{
		PlatformThread& platform_thread = *(PlatformThread*)p_os_thread_mem;
		platform_thread.~PlatformThread();
	}

	//------------------------------------------------------------------------
	void* PlatformThreadMain(void* p_param)
	{
		PlatformThread& platform_thread = *(PlatformThread*)p_param;

		platform_thread.m_Alive = true;
		platform_thread.mp_ThreadMain(platform_thread.mp_Param);
		platform_thread.m_Alive = false;

		return NULL;
	}

	//------------------------------------------------------------------------
	int Platform::StartThread(void* p_os_thread_mem, ThreadMain p_thread_main, void* p_param)
	{
		PlatformThread & platform_thread = *(PlatformThread*)p_os_thread_mem;
		platform_thread.mp_ThreadMain = p_thread_main;
		platform_thread.mp_Param = p_param;

		scePthreadCreate(&platform_thread.m_Handle, NULL, PlatformThreadMain, p_os_thread_mem, NULL);

		return 0;
	}

	//------------------------------------------------------------------------
	bool Platform::IsThreadAlive(const void* p_os_thread_mem)
	{
		const PlatformThread& platform_thread = *(PlatformThread*)p_os_thread_mem;
		return platform_thread.m_Alive;
	}

	//------------------------------------------------------------------------
	// global lock for all CAS instructions. This is Ok because CAS is only used by RingBuffer
	class PS4CriticalSection
	{
	public:
		PS4CriticalSection() { Platform::CreateLock(m_OSLockMem, sizeof(m_OSLockMem)); }
		~PS4CriticalSection() { Platform::DestroyLock(m_OSLockMem); }
		void Enter() { Platform::TakeLock(m_OSLockMem); }
		void Leave() { Platform::ReleaseLock(m_OSLockMem); }

	private:
		static const int m_OSLockMaxSize = 40;
		char m_OSLockMem[m_OSLockMaxSize];
	} MEMPRO_ALIGN_SUFFIX(16);
	PS4CriticalSection g_CASCritSec;

	//------------------------------------------------------------------------
	int64 Platform::MemProInterlockedCompareExchange(int64 volatile *dest, int64 exchange, int64 comperand)
	{
		g_CASCritSec.Enter();
		int64 old_value = *dest;
		if (*dest == comperand)
			*dest = exchange;
		g_CASCritSec.Leave();
		return old_value;
	}

	//------------------------------------------------------------------------
	int64 Platform::MemProInterlockedExchangeAdd(int64 volatile *Addend, int64 Value)
	{
		g_CASCritSec.Enter();
		int64 old_value = *Addend;
		*Addend += Value;
		g_CASCritSec.Leave();
		return old_value;
	}

	//------------------------------------------------------------------------
	void Platform::SwapEndian(unsigned int& value)
	{
		value =
			((value >> 24) & 0x000000ff) |
			((value >> 8) & 0x0000ff00) |
			((value << 8) & 0x00ff0000) |
			((value << 24) & 0xff000000);
	}

	//------------------------------------------------------------------------
	void Platform::SwapEndian(uint64& value)
	{
		value =
			((value >> 56) & 0x00000000000000ffLL) |
			((value >> 40) & 0x000000000000ff00LL) |
			((value >> 24) & 0x0000000000ff0000LL) |
			((value >> 8) & 0x00000000ff000000LL) |
			((value << 8) & 0x000000ff00000000LL) |
			((value << 24) & 0x0000ff0000000000LL) |
			((value << 40) & 0x00ff000000000000LL) |
			((value << 56) & 0xff00000000000000LL);
	}

	//------------------------------------------------------------------------
	void Platform::DebugBreak()
	{
		__builtin_trap();
	}

	//------------------------------------------------------------------------
	void* Platform::Alloc(int size)
	{
#ifdef __UNREAL__
		LLM_SCOPED_PAUSE_TRACKING(ELLMAllocType::System);
#endif

		return malloc(size);
	}

	//------------------------------------------------------------------------
	void Platform::Free(void* p, int size)
	{
#ifdef __UNREAL__
		LLM_SCOPED_PAUSE_TRACKING(ELLMAllocType::System);
#endif

		free(p);
	}

	//------------------------------------------------------------------------
	int64 Platform::GetHiResTimer()
	{
		return sceKernelGetProcessTimeCounter();
	}

	//------------------------------------------------------------------------
	// very innacurate, but portable. Doesn't have to be exact for our needs
	int64 Platform::GetHiResTimerFrequency()
	{
		static bool calculated_frequency = false;
		static int64 frequency = 1;
		if (!calculated_frequency)
		{
			calculated_frequency = true;
			Platform::Sleep(100);
			int64 start = GetHiResTimer();
			Platform::Sleep(1000);
			int64 end = GetHiResTimer();
			frequency = end - start;
		}

		return frequency;
	}

	//------------------------------------------------------------------------
	void Platform::SetThreadName(unsigned int thread_id, const char* p_name)
	{
		// not supported on this platform
	}

	//------------------------------------------------------------------------
	void Platform::Sleep(int ms)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}

	//------------------------------------------------------------------------
	inline unsigned int PS4GetHash(void** p_stack, int stack_size)
	{
		const unsigned int prime = 0x01000193;
		unsigned int hash = prime;
		void** p = p_stack;
		for(int i=0; i<stack_size; ++i)
		{
			uint64 key = (uint64)(*p++);
			key = (~key) + (key << 18);
			key = key ^ (key >> 31);
			key = key * 21;
			key = key ^ (key >> 11);
			key = key + (key << 6);
			key = key ^ (key >> 22);
			hash = hash ^ (unsigned int)key;
		}

		return hash;
	}

	//------------------------------------------------------------------------
	void Platform::GetStackTrace(void** stack, int& stack_size, unsigned int& hash)
	{
		SceDbgCallFrame frames[g_StackTraceSize];
		unsigned int frame_count = 0;
		sceDbgBacktraceSelf(frames, sizeof(frames), &frame_count, SCE_DBG_BACKTRACE_MODE_DONT_EXCEED);

		stack_size = frame_count;
		for (unsigned int i = 0; i < frame_count; ++i)
			stack[i] = (void*)frames[i].pc;

		hash = PS4GetHash(stack, stack_size);
	}

	//------------------------------------------------------------------------
	void Platform::SendPageState(bool, SendPageStateFunction, void*)
	{
		// not supported on this platform
	}

	//------------------------------------------------------------------------
	void Platform::GetVirtualMemStats(size_t& reserved, size_t& committed)
	{
		// not supported on this platform
		reserved = 0;
		committed = 0;
	}

	//------------------------------------------------------------------------
	bool Platform::GetExtraModuleInfo(int64, int&, void*, int, char*, int)
	{
		// not supported on this platform
		return false;
	}

	//------------------------------------------------------------------------
	void Platform::MemProEnumerateLoadedModules(
		EnumerateLoadedModulesCallbackFunction p_callback_function,
		void* p_context)
	{
		size_t module_count = 0;

#ifdef ENUMERATE_ALL_MODULES
		const size_t max_module_count = 512;
		SceDbgModule modules[max_module_count];
		MEMPRO_ASSERT(sceDbgGetModuleList(modules, max_module_count, &module_count) == SCE_OK);

		for (int i = 0; i < module_count; ++i)
		{
			SceDbgModuleInfo info;
			info.size = sizeof(SceDbgModuleInfo);
			MEMPRO_ASSERT(sceDbgGetModuleInfo(modules[i], &info) == SCE_OK);

			// NOTE: If you get garbage symbols try enabling this
#if 0
			int64 module_base = (int64)info.segmentInfo[0].baseAddr;
#else
			int64 module_base = 0;
#endif

			p_callback_function(module_base, info.name, p_context);
		}
#endif

		// if ENUMERATE_ALL_MODULES is disabled or enumeration failed for some reason, fall back
		// to getting the base address for the main module. This will always for for all platforms.
		if (!module_count)
		{
			// let MemPro know we are sending the lookup function address, not the base address
			uint64 use_module_base_addr_marker = 0xabcdefabcdef1LL;

			// send the module name
			char char_filename[MAX_PATH];

			// get the module name
			sceDbgGetExecutablePath(char_filename, sizeof(char_filename));

			p_callback_function(use_module_base_addr_marker, char_filename, p_context);
		}
	}

	//------------------------------------------------------------------------
	void Platform::DebugWrite(const char* p_message)
	{
		printf("%s", p_message);
	}

	//------------------------------------------------------------------------
	void Platform::MemProMemoryBarrier()
	{
		__sync_synchronize();
	}

	//------------------------------------------------------------------------
	EPlatform Platform::GetPlatform()
	{
		return Platform_PS4;
	}

	//------------------------------------------------------------------------
	int Platform::GetStackTraceSize()
	{
		return g_StackTraceSize;
	}

	//------------------------------------------------------------------------
	void Platform::MemCpy(void* p_dest, int dest_size, const void* p_source, int source_size)
	{
		MEMPRO_ASSERT(dest_size >= source_size);
		memcpy(p_dest, p_source, source_size);
	}

	//------------------------------------------------------------------------
	void Platform::SPrintF(char* p_dest, int dest_size, const char* p_format, const char* p_str)
	{
		sprintf(p_dest, p_format, p_str);
	}

	//------------------------------------------------------------------------
	struct PS4FileHandle
	{
		SceFiosFH m_FiosHandle;
		SceFiosSize m_Offset;
	};

	//------------------------------------------------------------------------
	PS4FileHandle& GetOSFile(void* p_os_file_mem)
	{
		return *(PS4FileHandle*)p_os_file_mem;
	}

	//------------------------------------------------------------------------
	const PS4FileHandle& GetOSFile(const void* p_os_file_mem)
	{
		return *(PS4FileHandle*)p_os_file_mem;
	}

	//------------------------------------------------------------------------
	void Platform::MemProCreateFile(void* p_os_file_mem, int os_file_mem_size)
	{
		MEMPRO_ASSERT(os_file_mem_size >= sizeof(PS4FileHandle));
		new (p_os_file_mem)PS4FileHandle();
	}

	//------------------------------------------------------------------------
	void Platform::DestroyFile(void* p_os_file_mem)
	{
		PS4FileHandle& file_handle = GetOSFile(p_os_file_mem);
		file_handle.~PS4FileHandle();
	}

	//------------------------------------------------------------------------
	bool Platform::OpenFileForWrite(void* p_os_file_mem, const char* p_filename)
	{
		PS4FileHandle& file_handle = GetOSFile(p_os_file_mem);

		SceFiosOpenParams params;
		memset(&params, 0, sizeof(params));
		params.openFlags = SCE_FIOS_O_WRITE | SCE_FIOS_O_CREAT | SCE_FIOS_O_TRUNC;

		int result = sceFiosFHOpenSync(NULL, &file_handle.m_FiosHandle, p_filename, &params);

		if (!result)
			file_handle.~PS4FileHandle();

		return result == SCE_FIOS_OK;
	}

	//------------------------------------------------------------------------
	void Platform::CloseFile(void* p_os_file_mem)
	{
		PS4FileHandle& file_handle = GetOSFile(p_os_file_mem);
		sceFiosFHCloseSync(NULL, file_handle.m_FiosHandle);
	}

	//------------------------------------------------------------------------
	void Platform::FlushFile(void* p_os_file_mem)
	{
		// not implemented
	}

	//------------------------------------------------------------------------
	bool Platform::WriteFile(void* p_os_file_mem, const void* p_data, int size)
	{
		PS4FileHandle& file_handle = GetOSFile(p_os_file_mem);

		SceFiosSize bytes_left_to_write = size;
		while (bytes_left_to_write)
		{
			SceFiosSize bytes_written = sceFiosFHPwriteSync(NULL, file_handle.m_FiosHandle, p_data, bytes_left_to_write, file_handle.m_Offset);
			MEMPRO_ASSERT(bytes_written > 0);
			file_handle.m_Offset += bytes_written;
			bytes_left_to_write -= bytes_written;
		}

		return true;
	}

	//------------------------------------------------------------------------
	#ifdef MEMPRO_WRITE_DUMP
	void Platform::GetDumpFilename(char* p_filename, int max_length)
	{
		#ifdef __UNREAL__
			FPS4PlatformFile& PlatformFile = (FPS4PlatformFile&)FPlatformFileManager::Get().GetPlatformFile();

			FString Directory = FPaths::ProfilingDir() / "MemPro";
			PlatformFile.CreateDirectoryTree(*Directory);

			const FDateTime FileDate = FDateTime::Now();
			FString Filename = FString::Printf(TEXT("%s/MemPro_%s.mempro_dump"), *Directory, *FileDate.ToString()).ToLower();
			Filename = PlatformFile.NormalizeFileName(*Filename);

			strcpy_s(p_filename, max_length, TCHAR_TO_ANSI(*Filename));
		#else
			GenericPlatform::GetDumpFilename(p_filename, max_length);
		#endif
	}
	#endif
}

//------------------------------------------------------------------------
#endif		// #ifdef MEMPRO_PLATFORM_PS4

//------------------------------------------------------------------------
#endif		// #if MEMPRO_ENABLED

