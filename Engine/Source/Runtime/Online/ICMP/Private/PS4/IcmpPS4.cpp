// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Icmp.h"
#include "IcmpModule.h"
#include "IcmpPrivate.h"
#include "SocketSubsystem.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net.h>

#include <scetypes.h>

namespace IcmpPS4
{
	// 56 bytes is the default size in the PS4 sample
	const SIZE_T IcmpPayloadSize = 56;
	const uint8 IcmpPayload[IcmpPayloadSize] = "!>>>>>>>>>>>>>>>This string is 56 bytes<<<<<<<<<<<<<<<!";

	// A critical section that ensures we only have a single ping in flight at once.
	FCriticalSection gPingCS;
}

uint16 NtoHS(uint16 val)
{
	return sceNetNtohs(val);
}

uint16 HtoNS(uint16 val)
{
	return sceNetHtons(val);
}

uint32 NtoHL(uint32 val)
{
	return sceNetNtohl(val);
}

uint32 HtoNL(uint32 val)
{
	return sceNetHtonl(val);
}

FIcmpEchoResult IcmpEchoImpl(ISocketSubsystem* SocketSub, const FString& TargetAddress, float Timeout)
{
	static const SIZE_T IpHeaderSize = sizeof(struct SceNetIpHeader);
	static const SIZE_T IcmpHeaderSize = sizeof(struct SceNetIcmpHeader);

	// The packet we send is just the ICMP header plus our payload
	static const SIZE_T PacketSize = IcmpHeaderSize + IcmpPS4::IcmpPayloadSize;

	// The result read back will need a room for the IP header as well the icmp echo reply packet
	static const SIZE_T ResultPacketSize = IpHeaderSize + PacketSize;

	static int PingSequence = 1;

	uint8 Packet[PacketSize];
	// Clear packet buffer
	FMemory::Memset(Packet, 0, PacketSize);

	FIcmpEchoResult Result;
	Result.Status = EIcmpResponseStatus::InternalError;

	SceNetId Socket = sceNetSocket("QosPing", SCE_NET_AF_INET, SCE_NET_SOCK_RAW, SCE_NET_IPPROTO_ICMP);
	if (Socket < 0)
	{
		UE_LOG(LogIcmp, Warning, TEXT("sceNetSocket() failed (errno=%d)\n"), sce_net_errno);
		return Result;
	}

	const int SocketPolicy = 0;
	int ReturnVal = sceNetSetsockopt(Socket, SCE_NET_SOL_SOCKET, SCE_NET_SO_POLICY, &SocketPolicy, sizeof(SocketPolicy));
	if (ReturnVal < 0)
	{
		UE_LOG(LogIcmp, Warning, TEXT("sceNetSetsockopt(SO_POLICY) failed (errno=%d)\n"), sce_net_errno);
		sceNetSocketClose(Socket);
		return Result;
	}

	FString ResolvedAddress;
	if (!ResolveIp(SocketSub, TargetAddress, ResolvedAddress))
	{
		Result.Status = EIcmpResponseStatus::Unresolvable;
		return Result;
	}
	Result.ResolvedAddress = ResolvedAddress;

	SceNetSockaddrIn DstAddress;
	FMemory::Memset(&DstAddress, 0, sizeof(DstAddress));
	DstAddress.sin_len = sizeof(DstAddress);
	DstAddress.sin_family = SCE_NET_AF_INET;

	if (sceNetInetPton(SCE_NET_AF_INET, TCHAR_TO_ANSI(*ResolvedAddress), &DstAddress.sin_addr) >= 0)
	{
		uint16_t SentId = (uint16_t)FMath::Rand();
		uint16_t SentSeq = PingSequence++;

		// Build the ICMP header
		SceNetIcmpHeader* IcmpHeader = (SceNetIcmpHeader*)Packet;
		IcmpHeader->type = SCE_NET_ICMP_TYPE_ECHO_REQUEST;
		IcmpHeader->code = 0;
		IcmpHeader->un.echo.id = sceNetHtons(SentId);
		IcmpHeader->un.echo.sequence = sceNetHtons(SentSeq);
		IcmpHeader->checksum = 0;

		// Put some data into the packet payload
		uint8* Payload = Packet + IcmpHeaderSize;
		FMemory::Memcpy(Payload, IcmpPS4::IcmpPayload, IcmpPS4::IcmpPayloadSize);

		IcmpHeader->checksum = (uint16_t)CalculateChecksum((uint8*)IcmpHeader, PacketSize);

		uint8 ResultBuffer[ResultPacketSize];

		// We can only have one ping in flight at once, as otherwise we risk swapping echo replies between requests
		{
			FScopeLock PingLock(&IcmpPS4::gPingCS);
			double TimeLeft = Timeout;
			double StartTime = FPlatformTime::Seconds();

			ReturnVal = sceNetSendto(Socket, Packet, (size_t)PacketSize, 0, (SceNetSockaddr*)&DstAddress, sizeof(DstAddress));
			if (ReturnVal >= 0)
			{
				SceNetId EventId = sceNetEpollCreate("QosSendPing", 0);
				if (EventId < 0)
				{
					UE_LOG(LogIcmp, Warning, TEXT("sceNetEpollCreate() failed (0x%x errno=%d)\n"), EventId, sce_net_errno);
				}

				SceNetEpollEvent RecvEvent;
				FMemory::Memset(&RecvEvent, 0, sizeof(RecvEvent));
				RecvEvent.events = SCE_NET_EPOLLIN;

				ReturnVal = sceNetEpollControl(EventId, SCE_NET_EPOLL_CTL_ADD, Socket, &RecvEvent);
				if (ReturnVal < 0)
				{
					UE_LOG(LogIcmp, Warning, TEXT("sceNetEpollControl(ADD) failed (0x%x errno=%d)\n"), ReturnVal, sce_net_errno);
					sceNetEpollDestroy(EventId);
					return Result;
				}

				SceNetEpollEvent WaitEvents[1];
				bool bDone = false;
				while (!bDone)
				{
					int NumReady = sceNetEpollWait(EventId, WaitEvents, 1, int(TimeLeft * 1000.0 * 1000.0));
					if (NumReady == 0)
					{
						// timeout
						Result.Status = EIcmpResponseStatus::Timeout;
						Result.ReplyFrom.Empty();
						Result.Time = Timeout;

						double EndTime = FPlatformTime::Seconds();

						// Estimate elapsed time
						Result.Time = EndTime - StartTime;

						bDone = true;
					}
					else if (NumReady < 0)
					{
						UE_LOG(LogIcmp, Warning, TEXT("sceNetEpollWait() failed (0x%x errno=%d)\n"), NumReady, sce_net_errno);
						break;
					}
					else if (NumReady == 1)
					{
						double EndTime = FPlatformTime::Seconds();

						// Estimate elapsed time
						Result.Time = EndTime - StartTime;
						TimeLeft = FPlatformMath::Max(0.0, (double)Timeout - Result.Time);

						if (WaitEvents[0].events & (SCE_NET_EPOLLIN | SCE_NET_EPOLLHUP))
						{
							// Data available
							if (WaitEvents[0].events & (SCE_NET_EPOLLIN))
							{
								// Read the incoming packet
								int DataRead = sceNetRecv(Socket, ResultBuffer, sizeof(ResultBuffer), 0);
								if (DataRead >= 0)
								{
									SceNetIpHeader* IpHeader = (SceNetIpHeader*)ResultBuffer;

									// Make sure the amount read was greater than the IP header
									int IpHeaderLength = IpHeader->un.ip_ver_hl.hl * 4;
									if (DataRead > IpHeaderLength + (int)sizeof(SceNetIcmpHeader))
									{
										if (IpHeader->ip_p == SCE_NET_IPPROTO_ICMP)
										{
											SceNetIcmpHeader* RecvIcmpHeader = (SceNetIcmpHeader*)(ResultBuffer + IpHeaderSize);

											ANSICHAR TempAddr[SCE_NET_INET_ADDRSTRLEN];
											if (sceNetInetNtop(SCE_NET_AF_INET, &IpHeader->ip_src, TempAddr, sizeof(TempAddr)) != nullptr)
											{
												Result.ReplyFrom = FString(ANSI_TO_TCHAR(TempAddr));
											}

											// Validate the packet checksum
											const uint16_t RecvChecksum = RecvIcmpHeader->checksum;
											RecvIcmpHeader->checksum = 0;
											const uint16_t LocalChecksum = (uint16_t)CalculateChecksum((uint8*)RecvIcmpHeader, PacketSize);

											if (RecvChecksum == LocalChecksum)
											{
												// Convert values back from network byte order
												RecvIcmpHeader->un.echo.id = sceNetNtohs(RecvIcmpHeader->un.echo.id);
												RecvIcmpHeader->un.echo.sequence = sceNetNtohs(RecvIcmpHeader->un.echo.sequence);

												UE_LOG(LogIcmp, Verbose, TEXT("%d bytes from %s: type %d icmp_seq=%d ttl=%d time=%f"),
													DataRead - IpHeaderLength,
													*Result.ReplyFrom,
													RecvIcmpHeader->type,
													RecvIcmpHeader->un.echo.sequence,
													IpHeader->ip_ttl,
													Result.Time);

												switch (RecvIcmpHeader->type)
												{
													case SCE_NET_ICMP_TYPE_ECHO_REPLY:
													{
														if (Result.ReplyFrom == ResolvedAddress &&
															RecvIcmpHeader->un.echo.id == SentId && RecvIcmpHeader->un.echo.sequence == SentSeq)
														{
															Result.Status = EIcmpResponseStatus::Success;
															bDone = true;
														}
														break;
													}
													case SCE_NET_ICMP_TYPE_DEST_UNREACH:
													{
														Result.Status = EIcmpResponseStatus::Unreachable;
														bDone = true;
														break;
													}
													default:
														UE_LOG(LogIcmp, Verbose, TEXT("ICMP type %d ignored"), RecvIcmpHeader->type);
														break;
												}
											}
											else
											{
												UE_LOG(LogIcmp, Verbose, TEXT("ICMP checksum failure"));
											}
										}
										else
										{
											UE_LOG(LogIcmp, Verbose, TEXT("Non ICMP packet detected"));
										}
									}
								}
								else
								{
									// Failed to read data from the socket
									if (sce_net_errno != SCE_NET_EINTR)
									{
										UE_LOG(LogIcmp, Warning, TEXT("sceNetRecv() failed (errno=%d)\n"), sce_net_errno);
									}
									bDone = true;
								}
							}
							else
							{
								// Timeout related to an aborted socket
								Result.Status = EIcmpResponseStatus::Timeout;
								bDone = true;
							}
							
							break;
						}
					}
				}

				if (sceNetEpollDestroy(EventId) < 0)
				{
					UE_LOG(LogIcmp, Warning, TEXT("sceNetEpollDestroy() failed (errno=%d)\n"), sce_net_errno);
				}
			}
			else
			{
				UE_LOG(LogIcmp, Warning, TEXT("sceNetSendto() failed (errno=%d), ret = 0x%x\n"), sce_net_errno, ReturnVal);
				return Result;
			}
		}
	}
	else
	{
		UE_LOG(LogIcmp, Warning, TEXT("sceNetInetPton failed to resolve (errno=%d)\n"), sce_net_errno);
		Result.Status = EIcmpResponseStatus::Unresolvable;
	}

	if (sceNetSocketClose(Socket) < 0)
	{
		UE_LOG(LogIcmp, Warning, TEXT("sceNetSocketClose() failed (errno=%d)\n"), sce_net_errno);
	}

	return Result;
}