// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#include "bbclient/bb_sockets.h"
#include <string.h>

#if BB_USING(BB_COMPILER_MSVC)
#pragma comment(lib, "ws2_32.lib")
#endif // #if BB_USING(BB_COMPILER_MSVC)

#if BB_USING(BB_COMPILER_CLANG)
#include <fcntl.h>
#endif // #if BB_USING(BB_COMPILER_CLANG)

b32 bbnet_init(void)
{
#if BB_USING(BB_COMPILER_MSVC)
	WSADATA wd;
	memset(&wd, 0, sizeof(wd));
	return WSAStartup(MAKEWORD(2, 2), &wd) == 0;
#else  // #if BB_USING(BB_COMPILER_MSVC)
	return true;
#endif // #else // #if BB_USING(BB_COMPILER_MSVC)
}

void bbnet_shutdown(void)
{
#if BB_USING(BB_COMPILER_MSVC)
	WSACleanup();
#endif // #if BB_USING(BB_COMPILER_MSVC)
}

void bbnet_gracefulclose(bb_socket* socket)
{
	bb_socket sock;
	fd_set set;
	BB_TIMEVAL tv;
	char buf[256];
	if (!socket || *socket == BB_INVALID_SOCKET)
		return;

	sock = *socket;
	*socket = BB_INVALID_SOCKET;

	shutdown(sock, BB_SHUTDOWN_SEND);

	// Read any remaining data from the socket and discard.
	for (;;)
	{
		FD_ZERO(&set);
		BB_FD_SET(sock, &set);

		tv.tv_sec = 0;
		tv.tv_usec = 5 * 1000;

		if (1 != select((int)sock + 1, &set, 0, 0, &tv))
		{
			break;
		}

		if (0 >= recv(sock, buf, sizeof(buf), 0))
		{
			break;
		}
	}

	shutdown(sock, BB_SHUTDOWN_BOTH);

	if (BB_CLOSE(sock))
	{
		BB_CLOSE(sock);
	}
}

int bbnet_socket_nodelay(bb_socket socket, b32 nodelay)
{
	int disableNagle = nodelay;
	return setsockopt(socket, SOL_SOCKET, TCP_NODELAY, (char*)&disableNagle, sizeof(disableNagle));
}

int bbnet_socket_linger(bb_socket socket, b32 enabled, u16 seconds)
{
	struct linger data = { BB_EMPTY_INITIALIZER };
	data.l_linger = seconds;
	data.l_onoff = enabled ? 1u : 0u;
	return setsockopt(socket, SOL_SOCKET, SO_LINGER, (char*)&data, sizeof(data));
}

int bbnet_socket_reuseaddr(bb_socket socket, int reuseAddr)
{
	return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddr, sizeof(reuseAddr));
}

int bbnet_socket_nonblocking(bb_socket socket, b32 nonblocking)
{
#if BB_USING(BB_COMPILER_MSVC)

	u_long nonBlocking = (u_long)nonblocking;
	return ioctlsocket(socket, (long)FIONBIO, &nonBlocking);

#elif BB_USING(BB_COMPILER_CLANG) // #if BB_USING(BB_COMPILER_MSVC)

	int flags = fcntl(socket, F_GETFL, 0);
	flags = (nonblocking) ? (flags | O_NONBLOCK) : (flags & (~O_NONBLOCK));
	return fcntl(socket, F_SETFL, flags);

#else // #elif BB_USING(BB_COMPILER_CLANG) // #if BB_USING(BB_COMPILER_MSVC)

#error

#endif // #else // #elif BB_USING(BB_COMPILER_CLANG) // #if BB_USING(BB_COMPILER_MSVC)
}

int bbnet_socket_ipv6only(bb_socket socket, b32 ipv6only)
{
#if !BB_USING(BB_FEATURE_IPV6)
	BB_UNUSED(socket);
	BB_UNUSED(ipv6only);
	return -1;
#else // #if !BB_USING(BB_FEATURE_IPV6)
	return setsockopt(socket, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&ipv6only, sizeof(ipv6only));
#endif // #if BB_USING(BB_FEATURE_IPV6)
}

b32 bbnet_socket_is6to4(const struct sockaddr* addr)
{
	BB_UNUSED(addr);
#if BB_USING(BB_FEATURE_IPV6)
	if (addr->sa_family == AF_INET6)
	{
		const struct sockaddr_in6* addr6 = (const struct sockaddr_in6*)addr;
		if (addr6->sin6_addr.s6_addr[0] == 0x00 &&
		    addr6->sin6_addr.s6_addr[1] == 0x00 &&
		    addr6->sin6_addr.s6_addr[2] == 0x00 &&
		    addr6->sin6_addr.s6_addr[3] == 0x00 &&
		    addr6->sin6_addr.s6_addr[4] == 0x00 &&
		    addr6->sin6_addr.s6_addr[5] == 0x00 &&
		    addr6->sin6_addr.s6_addr[6] == 0x00 &&
		    addr6->sin6_addr.s6_addr[7] == 0x00 &&
		    addr6->sin6_addr.s6_addr[8] == 0x00 &&
		    addr6->sin6_addr.s6_addr[9] == 0x00 &&
		    addr6->sin6_addr.s6_addr[10] == 0xff &&
		    addr6->sin6_addr.s6_addr[11] == 0xff)
		{
			return true;
		}
	}
#endif // #if BB_USING(BB_FEATURE_IPV6)
	return false;
}

#if BB_USING(BB_FEATURE_IPV6)
void bbnet_socket_build6to4(struct sockaddr_in6* addr, const u32 ip)
{
	memset(addr->sin6_addr.s6_addr, 0, sizeof(addr->sin6_addr));
	addr->sin6_addr.s6_addr[10] = 0xff;
	addr->sin6_addr.s6_addr[11] = 0xff;
	addr->sin6_addr.s6_addr[12] = (u8)(ip >> 24);
	addr->sin6_addr.s6_addr[13] = (u8)(ip >> 16);
	addr->sin6_addr.s6_addr[14] = (u8)(ip >> 8);
	addr->sin6_addr.s6_addr[15] = (u8)(ip);
}
#endif // #if BB_USING(BB_FEATURE_IPV6)

u16 bbnet_get_port_from_sockaddr(const struct sockaddr* addr)
{
	if (addr->sa_family == AF_INET)
	{
		return ntohs(((const struct sockaddr_in*)addr)->sin_port);
	}
#if BB_USING(BB_FEATURE_IPV6)
	else if (addr->sa_family == AF_INET6)
	{
		return ntohs(((const struct sockaddr_in6*)addr)->sin6_port);
	}
#endif // #if BB_USING(BB_FEATURE_IPV6)
	return 0;
}

void bbnet_set_port_on_sockaddr(struct sockaddr* addr, const u16 port)
{
	if (addr->sa_family == AF_INET)
	{
		((struct sockaddr_in*)addr)->sin_port = htons(port);
	}
#if BB_USING(BB_FEATURE_IPV6)
	else if (addr->sa_family == AF_INET6)
	{
		((struct sockaddr_in6*)addr)->sin6_port = htons(port);
	}
#endif // #if BB_USING(BB_FEATURE_IPV6)
}

socklen_t bbnet_get_addr_size(const struct sockaddr* addr, socklen_t storageSize)
{
	if (!addr)
		return 0;

	if (addr->sa_family == AF_INET)
		return BB_MIN(storageSize, (socklen_t)sizeof(struct sockaddr_in));

#if BB_USING(BB_FEATURE_IPV6)
	if (addr->sa_family == AF_INET6)
		return BB_MIN(storageSize, (socklen_t)sizeof(struct sockaddr_in6));
#endif // #if BB_USING(BB_FEATURE_IPV6)

	return storageSize;
}

socklen_t bbnet_get_addr_storage_size(const struct sockaddr_storage* addr, socklen_t storageSize)
{
	if (!addr)
		return 0;

	if (addr->ss_family == AF_INET)
		return BB_MIN(storageSize, (socklen_t)sizeof(struct sockaddr_in));

#if BB_USING(BB_FEATURE_IPV6)
	if (addr->ss_family == AF_INET6)
		return BB_MIN(storageSize, (socklen_t)sizeof(struct sockaddr_in6));
#endif // #if BB_USING(BB_FEATURE_IPV6)

	return storageSize;
}

#endif // #if BB_ENABLED
