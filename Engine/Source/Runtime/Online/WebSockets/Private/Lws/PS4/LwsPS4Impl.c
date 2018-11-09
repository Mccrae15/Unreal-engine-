#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#include "private-libwebsockets.h"

#include <sys/time.h>
#include <sys/select.h>
#include <netinet/tcp.h>
#include <sys/fcntl.h>
#include <kernel.h>

#define LIBNET_POOLSIZE		(4 * 1024)

const int LWS_POLLIN = SCE_NET_EPOLLIN;
const int LWS_POLLOUT = SCE_NET_EPOLLOUT;
const int LWS_POLLHUP = SCE_NET_EPOLLHUP;

static int LibWebSocketPS4MemPoolId = -1;
static SceNetId LibWebSocketPS4ResolverId = -1;

int lws_getaddrinfo(const char *node, const char *service, const struct lws_addrinfo *hints, struct lws_addrinfo **res)
{
	int result = 1;
	if (LibWebSocketPS4MemPoolId == -1)
	{
		LibWebSocketPS4MemPoolId = sceNetPoolCreate("LWS_NetMemPool", LIBNET_POOLSIZE, 0);
		LibWebSocketPS4ResolverId = sceNetResolverCreate("LWS_Resolver", LibWebSocketPS4MemPoolId, 0);
	}

	if (LibWebSocketPS4MemPoolId >= 0 && LibWebSocketPS4ResolverId >= 0)
	{
		struct lws_addrinfo* tempaddrinfoptr = lws_malloc(sizeof(struct lws_addrinfo));
		struct sockaddr* tempsockaddrptr = lws_malloc(sizeof(struct sockaddr));
		if (tempaddrinfoptr != 0 && tempsockaddrptr != 0)
		{
			SceNetInAddr AddrResult;
			if (sceNetResolverStartNtoa(LibWebSocketPS4ResolverId, node, &AddrResult, 0, 0, 0) >= 0)
			{
				memset(tempaddrinfoptr, 0, sizeof(struct lws_addrinfo));
				tempaddrinfoptr->ai_family = AF_INET;
				tempaddrinfoptr->ai_addr = tempsockaddrptr;
				tempaddrinfoptr->ai_addr->sa_len = sizeof(struct sockaddr);
				tempaddrinfoptr->ai_addr->sa_family = AF_INET;
				((struct sockaddr_in*)tempaddrinfoptr->ai_addr)->sin_addr.s_addr = AddrResult.s_addr;
				*res = tempaddrinfoptr;

				result = 0;
			}

		}
	}

	return result;
}

LWS_VISIBLE int
lws_plat_set_socket_options(struct lws_vhost *vhost, lws_sockfd_type fd)
{
	int optval = 1;
	socklen_t optlen = sizeof(optval);

	if (vhost->ka_time) {
		/* enable keepalive on this socket */
		optval = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
			(const void *)&optval, optlen) < 0)
			return 1;

	}

	/* Disable Nagle */
	optval = 1;
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, optlen) < 0)
		return 1;

	/* We are nonblocking... */

	int Param = 1;
	if (sceNetSetsockopt(fd, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &Param, sizeof Param) != SCE_OK)
	{
		lwsl_warn("Couldn't set flag, error code: %d", errno);
//		return 1;
	}

	return 0;
}

LWS_VISIBLE int
lws_plat_check_connection_error(struct lws *wsi)
{
	int OptVal;
	SceNetSocklen_t OptLen = sizeof(OptVal);

	int GetSockOptRet = sceNetGetsockopt(wsi->desc.sockfd, SCE_NET_SOL_SOCKET, SCE_NET_SO_ERROR, &OptVal, &OptLen);
	if (GetSockOptRet >= 0 && OptVal &&
		OptVal != SCE_NET_EALREADY && OptVal != SCE_NET_EINPROGRESS &&
		OptVal != SCE_NET_EWOULDBLOCK && OptVal != SCE_NET_EINVAL) {
		   lwsl_debug("Connect failed SO_ERROR=%d\n", OptVal);
		   return 1;
	}

	return 0;
}
