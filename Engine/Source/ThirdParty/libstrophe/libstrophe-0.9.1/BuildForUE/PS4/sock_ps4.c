/* sock_ps4.c
** strophe XMPP client library -- socket abstraction implementation
**
** Copyright (C) 2005-2009 Collecta, Inc. 
**
**  This software is provided AS-IS with no warranty, either express
**  or implied.
**
** This program is dual licensed under the MIT and GPLv3 licenses.
*/

/** @file
 *  Socket abstraction.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net.h>

#include "sock.h"
#include "snprintf.h"

static int mem_pool_id = -1;
static SceNetId resolver_id = -1;

void sock_initialize(void)
{
}

void sock_shutdown(void)
{
	if (resolver_id >= 0)
	{
		sceNetResolverDestroy(resolver_id);
		resolver_id = -1;
	}
	if (mem_pool_id >= 0)
	{
		sceNetPoolDestroy(mem_pool_id);
		mem_pool_id = -1;
	}
}

int sock_error(void)
{
    return sce_net_errno;
}

static int _in_progress(int error)
{
    return (error == EINPROGRESS);
}

sock_t sock_connect(const char * const host, const unsigned short port)
{
    sock_t sock;
	SceNetSockaddrIn addr;
    int err;

	if (resolver_id == -1)
	{
		mem_pool_id = sceNetPoolCreate("libstrophe", 4096, 0);
		resolver_id = sceNetResolverCreate("libstrophe", mem_pool_id, 0);
	}

	SceNetResolverInfo info;
	err = sceNetResolverStartNtoaMultipleRecords(resolver_id, host, &info, 0, 0, 0);
	if (err < 0)
	{
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_len = sizeof(addr);
	addr.sin_port = sceNetHtons(port);
	for (int i = 0; i < info.records; ++i)
	{
		sock = sceNetSocket("strophe", info.addrs[i].af, SOCK_STREAM, IPPROTO_TCP);
		if (sock < 0)
			continue;

		err = sock_set_nonblocking(sock);
		if (err == 0)
		{
			addr.sin_family = info.addrs[i].af;
			addr.sin_addr.s_addr = info.addrs[i].un.addr.s_addr;
			err = sceNetConnect(sock, (SceNetSockaddr*)&addr, sizeof(addr));
			if (err == 0 || _in_progress(sock_error()))
			{
				break;
			}
		}
		sock_close(sock);
	}
	sock = info.records > 0 ? sock : -1;

    return sock;
}

int sock_set_keepalive(const sock_t sock, int timeout, int interval)
{
    int ret;
    int optval = (timeout && interval) ? 1 : 0;

    /* This function doesn't change maximum number of keepalive probes */

    ret = sceNetSetsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));

    return ret;
}

int sock_close(const sock_t sock)
{
    return sceNetSocketClose(sock);
}

int sock_set_blocking(const sock_t sock)
{
    int value = 0;
	return sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &value, sizeof(value));
}

int sock_set_nonblocking(const sock_t sock)
{
	int value = 1;
	return sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &value, sizeof(value));
}

int sock_read(const sock_t sock, void * const buff, const size_t len)
{
    return sceNetRecv(sock, buff, len, 0);
}

int sock_write(const sock_t sock, const void * const buff, const size_t len)
{
    return sceNetSend(sock, buff, len, 0);
}

int sock_is_recoverable(const int error)
{
    return (error == EAGAIN || error == EINTR);
}

int sock_connect_error(const sock_t sock)
{
    SceNetSockaddr sa;
    socklen_t len;
    char temp;

    memset(&sa, 0, sizeof(sa));
    sa.sa_family = SCE_NET_AF_INET;
    len = sizeof(sa);

    /* we don't actually care about the peer name, we're just checking if
     * we're connected or not */
    if (sceNetGetpeername(sock, &sa, &len) == 0)
    {
        return 0;
    }

    /* it's possible that the error wasn't ENOTCONN, so if it wasn't,
     * return that */
    if (sock_error() != ENOTCONN) return sock_error();

    /* load the correct error into errno through error slippage */
	sceNetRecv(sock, &temp, 1, 0);

    return sock_error();
}
