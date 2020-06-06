// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/Config.h"

#if UE_TRACE_ENABLED

#include <net.h>
#include <kernel.h>

namespace Trace {
namespace Private {

////////////////////////////////////////////////////////////////////////////////
static const SIZE_T ReservePageMask = (2 * 1024 * 1024) - 1;

////////////////////////////////////////////////////////////////////////////////
uint8* MemoryReserve(SIZE_T Size)
{
	Size = (Size + ReservePageMask) & ~ReservePageMask;
	void* Addr = (void*)(SCE_KERNEL_APP_MAP_AREA_END_ADDR - (1 << 30));
	sceKernelMemoryPoolReserve(Addr, Size, ReservePageMask + 1, SCE_KERNEL_MAP_NO_OVERWRITE, &Addr);
	return (uint8*)Addr;
}

////////////////////////////////////////////////////////////////////////////////
void MemoryFree(void* Address, SIZE_T Size)
{
	Size = (Size + ReservePageMask) & ~ReservePageMask;
	sceKernelMunmap(Address, Size);
}

////////////////////////////////////////////////////////////////////////////////
void MemoryMap(void* Address, SIZE_T Size)
{
	sceKernelMemoryPoolCommit(Address, Size, SCE_KERNEL_WB_ONION, SCE_KERNEL_PROT_CPU_RW, 0);
}

////////////////////////////////////////////////////////////////////////////////
void MemoryUnmap(void* Address, SIZE_T Size)
{
	sceKernelMemoryPoolDecommit(Address, Size, 0);
}



////////////////////////////////////////////////////////////////////////////////
UPTRINT ThreadCreate(const ANSICHAR* Name, void (*Entry)())
{
	auto Thunk = [] (void* Param) -> void*
	{
		typedef void (*EntryType)(void);
		(EntryType(Param))();
		return nullptr;
	};

	ScePthread Thread;
	if (scePthreadCreate(&Thread, nullptr, Thunk, (void*)Entry, Name) != 0)
	{
		return 0;
	}

	return UPTRINT(Thread);
}

////////////////////////////////////////////////////////////////////////////////
void ThreadSleep(uint32 Milliseconds)
{
	sceKernelUsleep(Milliseconds << 10);
}

////////////////////////////////////////////////////////////////////////////////
void ThreadJoin(UPTRINT Handle)
{
	scePthreadJoin(ScePthread(Handle), nullptr);
}

////////////////////////////////////////////////////////////////////////////////
void ThreadDestroy(UPTRINT Handle)
{
	scePthreadDetach(ScePthread(Handle));
}



////////////////////////////////////////////////////////////////////////////////
uint64 TimeGetFrequency()
{
	return sceKernelGetProcessTimeCounterFrequency();
}

////////////////////////////////////////////////////////////////////////////////
uint64 TimeGetTimestamp()
{
	return sceKernelGetProcessTimeCounter();
}



////////////////////////////////////////////////////////////////////////////////
static bool TcpSocketSetNonBlocking(int Socket, bool bNonBlocking)
{
	int NonBlocking = !!bNonBlocking;
	return sceNetSetsockopt(Socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &NonBlocking, sizeof(NonBlocking)) == SCE_OK;
}

////////////////////////////////////////////////////////////////////////////////
static UPTRINT TcpSocketFinalize(SceNetId Socket)
{
	static_assert(sizeof(UPTRINT) >= 8, "");
	static_assert(sizeof(SceNetId) <= 4, "");

	UPTRINT Ret = UPTRINT(Socket + 1);

	SceNetId Epoll = sceNetEpollCreate("TraceEpoll", 0);
	if (Epoll < 0)
	{
		return Ret;
	}

	SceNetEpollEvent Event = { SCE_NET_EPOLLIN };
	int Result = sceNetEpollControl(Epoll, SCE_NET_EPOLL_CTL_ADD, Socket, &Event);
	if (Result < 0)
	{
		sceNetEpollDestroy(Epoll);
		return Ret;
	}

	Ret |= UPTRINT(Epoll + 1) << 32;
	return Ret;
}

////////////////////////////////////////////////////////////////////////////////
UPTRINT TcpSocketConnect(const ANSICHAR* Host, uint16 Port)
{
	SceNetId Socket = sceNetSocket("Trace", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	if (Socket < 0)
	{
		return 0;
	}
	
	uint32 HostIp = 0;
	sceNetInetPton(SCE_NET_AF_INET, Host, &HostIp);
	Port = sceNetHtons(Port);

	SceNetSockaddrIn AddrIn = {
		sizeof(AddrIn),
		SCE_NET_AF_INET,
		Port,
		{ HostIp },
		Port,
	};

	const SceNetSockaddr* Addr = (SceNetSockaddr*)&AddrIn;
	int Result = sceNetConnect(Socket, Addr, Addr->sa_len);
	if (Result)
	{
		sceNetSocketClose(Socket);
		return 0;
	}

	if (!TcpSocketSetNonBlocking(Socket, false))
	{
		sceNetSocketClose(Socket);
		return 0;
	}

	return TcpSocketFinalize(Socket);
}

////////////////////////////////////////////////////////////////////////////////
UPTRINT TcpSocketListen(uint16 Port)
{
	Port = sceNetHtons(Port);

	SceNetSockaddrIn AddrIn = {
		sizeof(AddrIn),
		SCE_NET_AF_INET,
		Port,
		{ 0 },
		Port,
	};

	SceNetId Socket = sceNetSocket("Trace", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	if (Socket < 0)
	{
		return 0;
	}

	const SceNetSockaddr* Addr = (SceNetSockaddr*)&AddrIn;
	int Result = sceNetBind(Socket, Addr, Addr->sa_len);
	if (Result)
	{
		sceNetSocketClose(Socket);
		return 0;
	}

	Result = sceNetListen(Socket, 1);
	if (Result)
	{
		sceNetSocketClose(Socket);
		return 0;
	}

	if (!TcpSocketSetNonBlocking(Socket, true))
	{
		sceNetSocketClose(Socket);
		return 0;
	}

	return UPTRINT(Socket) + 1;
}

////////////////////////////////////////////////////////////////////////////////
int32 TcpSocketAccept(UPTRINT Socket, UPTRINT& Out)
{
	SceNetId Inner = SceNetId(Socket - 1);

	Inner = sceNetAccept(Inner, nullptr, nullptr);
	if (Inner < 0)
	{
		return (Inner == SCE_NET_ERROR_EWOULDBLOCK) - 1;
	}

	if (!TcpSocketSetNonBlocking(Socket, false))
	{
		sceNetSocketClose(Socket);
		return 0;
	}

	Out = TcpSocketFinalize(Inner);
	return 1;
}

////////////////////////////////////////////////////////////////////////////////
bool TcpSocketHasData(UPTRINT Socket)
{
	Socket >>= 32;
	if (!Socket)
	{
		return false;
	}

	SceNetEpollEvent Event;
	SceNetId Inner = SceNetId(Socket - 1);
	int Result = sceNetEpollWait(Inner, &Event, 1, 0);

	return (Result > 0) & !!(Event.events & SCE_NET_EPOLLIN);
}

////////////////////////////////////////////////////////////////////////////////
bool IoWrite(UPTRINT Handle, const void* Data, uint32 Size)
{
	// We could use sceKernelWrite() here for both file and socket descriptors but
	// the OS doesn't gracefully handle sockets that closed at the other end.

	int Inner = int(Handle);
	if (Inner < 0)
	{
		Inner = -Inner - 1;
		return sceKernelWrite(Inner, Data, Size) == Size;
	}

	Inner -= 1;
	return sceNetSend(Inner, Data, Size, 0);
}

////////////////////////////////////////////////////////////////////////////////
int32 IoRead(UPTRINT Handle, void* Data, uint32 Size)
{
	// See IoWrite() for why we can't use sceKernelRead() for all descriptor types

	int Inner = int(Handle);
	if (Inner < 0)
	{
		Inner = -Inner - 1;
		return int32(sceKernelRead(Inner, Data, Size));
	}

	Inner -= 1;
	return int32(sceNetRecv(Inner, Data, Size, 0));
}

////////////////////////////////////////////////////////////////////////////////
void IoClose(UPTRINT Handle)
{
	if (int(Handle) < 0)
	{
		int Inner = -int(Handle) - 1;
		sceKernelClose(Inner);
		return;
	}

	SceNetId Inner = SceNetId(Handle - 1);
	sceNetSocketClose(Inner);

	Handle >>= 32;
	if (Handle)
	{
		Inner = SceNetId(Handle - 1);
		sceNetEpollDestroy(Inner);
	}
}



////////////////////////////////////////////////////////////////////////////////
UPTRINT FileOpen(const ANSICHAR* Path)
{
	int Flags = SCE_KERNEL_O_RDWR|SCE_KERNEL_O_CREAT;
	int Out = sceKernelOpen(Path, Flags, SCE_KERNEL_S_IRWU);
	if (Out < 0)
	{
		return 0;
	}

	// We will negate file handles so we can identify them from other IO handle
	// types later on.
	return -(PTRINT(Out) + 1);
}

} // namespace Private
} // namespace Trace

#endif // UE_TRACE_ENABLED
