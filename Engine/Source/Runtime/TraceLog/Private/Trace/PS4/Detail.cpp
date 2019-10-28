// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Trace/Trace.h"

#if UE_TRACE_ENABLED

#include <net.h>
#include <kernel.h>

namespace Trace
{

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
uint32 ThreadGetCurrentId()
{
	return scePthreadGetthreadid();
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

	SceNetId Socket = sceNetSocket("Trace", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	if (Socket < 0)
	{
		return 0;
	}
	
	const SceNetSockaddr* Addr = (SceNetSockaddr*)&AddrIn;
	int Result = sceNetConnect(Socket, Addr, Addr->sa_len);
	if (Result)
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

	return TcpSocketFinalize(Socket);
}

////////////////////////////////////////////////////////////////////////////////
UPTRINT TcpSocketAccept(UPTRINT Socket)
{
	SceNetId Inner = SceNetId(Socket - 1);

	Inner = sceNetAccept(Inner, nullptr, nullptr);
	if (Inner < 0)
	{
		return 0;
	}

	return TcpSocketFinalize(Inner);
}

////////////////////////////////////////////////////////////////////////////////
void TcpSocketClose(UPTRINT Socket)
{
	SceNetId Inner = SceNetId(Socket - 1);
	sceNetSocketClose(Inner);

	Socket >>= 32;
	if (Socket)
	{
		Inner = SceNetId(Socket - 1);
		sceNetEpollDestroy(Inner);
	}
}

////////////////////////////////////////////////////////////////////////////////
bool TcpSocketSelect(UPTRINT Socket)
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
bool TcpSocketSend(UPTRINT Socket, const void* Data, uint32 Size)
{
	SceNetId Inner = SceNetId(Socket - 1);
	return sceNetSend(Inner, Data, Size, 0) == Size;
}

////////////////////////////////////////////////////////////////////////////////
int32 TcpSocketRecv(UPTRINT Socket, void* Data, uint32 Size)
{
	SceNetId Inner = SceNetId(Socket - 1);
	return sceNetRecv(Inner, Data, Size, 0);
}

} // namespace Trace

#endif // UE_TRACE_ENABLED
