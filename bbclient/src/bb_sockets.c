// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#if defined(_MSC_VER)
__pragma(warning(disable : 4464)); // warning C4464: relative include path contains '..'
#endif

#include "../include/bb.h" // relative path so we don't rely on include path being set up for configurations that have BB_ENABLED 0

#if BB_ENABLED

#include "bbclient/bb_sockets.h"

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

void bbnet_gracefulclose(bb_socket *socket)
{
	bb_socket sock;
	fd_set set;
	BB_TIMEVAL tv;
	char buf[256];
	if(!socket || *socket == BB_INVALID_SOCKET)
		return;

	sock = *socket;
	*socket = BB_INVALID_SOCKET;

	shutdown(sock, BB_SHUTDOWN_SEND);

	// Read any remaining data from the socket and discard.
	for(;;) {
		FD_ZERO(&set);
		BB_FD_SET(sock, &set);

		tv.tv_sec = 0;
		tv.tv_usec = 5 * 1000;

		if(1 != select((int)sock + 1, &set, 0, 0, &tv)) {
			break;
		}

		if(0 >= recv(sock, buf, sizeof(buf), 0)) {
			break;
		}
	}

	shutdown(sock, BB_SHUTDOWN_BOTH);

	if(BB_CLOSE(sock)) {
		BB_CLOSE(sock);
	}
}

int bbnet_socket_nodelay(bb_socket socket, b32 nodelay)
{
	int disableNagle = nodelay;
	return setsockopt(socket, SOL_SOCKET, TCP_NODELAY, (char *)&disableNagle, sizeof(disableNagle));
}

int bbnet_socket_linger(bb_socket socket, b32 enabled, u16 seconds)
{
	struct linger data = { BB_EMPTY_INITIALIZER };
	data.l_linger = seconds;
	data.l_onoff = enabled ? 1u : 0u;
	return setsockopt(socket, SOL_SOCKET, SO_LINGER, (char *)&data, sizeof(data));
}

int bbnet_socket_reuseaddr(bb_socket socket, int reuseAddr)
{
	return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseAddr, sizeof(reuseAddr));
}

int bbnet_socket_nonblocking(bb_socket socket, b32 nonblocking)
{
#if BB_USING(BB_COMPILER_MSVC)

	u_long nonBlocking = (u_long)nonblocking;
	return ioctlsocket(socket, FIONBIO, &nonBlocking);

#elif BB_USING(BB_COMPILER_CLANG) // #if BB_USING(BB_COMPILER_MSVC)

	int flags = fcntl(socket, F_GETFL, 0);
	flags = (nonblocking) ? (flags | O_NONBLOCK) : (flags & (~O_NONBLOCK));
	return fcntl(socket, F_SETFL, flags);

#else // #elif BB_USING(BB_COMPILER_CLANG) // #if BB_USING(BB_COMPILER_MSVC)

#error

#endif // #else // #elif BB_USING(BB_COMPILER_CLANG) // #if BB_USING(BB_COMPILER_MSVC)
}

#endif // #if BB_ENABLED
