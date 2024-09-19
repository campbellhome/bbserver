// Copyright (c) 2012-2024 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_common.h"

#if BB_USING(BB_PLATFORM_DURANGO) || BB_USING(BB_PLATFORM_SCARLETT)
#include <WS2tcpip.h>
#include <winsock2.h>
#endif // #if BB_USING(BB_PLATFORM_DURANGO) || BB_USING(BB_PLATFORM_SCARLETT)

#if BB_USING(BB_COMPILER_MSVC)

#include "bb_wrap_windows.h"
#include "bb_wrap_winsock2.h"

#else // #if BB_USING(BB_COMPILER_MSVC)

#include <arpa/inet.h>
#include <errno.h>
#if !BB_USING(BB_PLATFORM_ORBIS) && !BB_USING(BB_PLATFORM_PROSPERO)
#include <netdb.h>
#endif // #if !BB_USING(BB_PLATFORM_ORBIS) && !BB_USING(BB_PLATFORM_PROSPERO)
#include <netinet/in.h>
#include <netinet/tcp.h>
#if !BB_USING(BB_PLATFORM_ORBIS) && !BB_USING(BB_PLATFORM_PROSPERO)
#include <sys/ioctl.h>
#endif // #if !BB_USING(BB_PLATFORM_ORBIS) && !BB_USING(BB_PLATFORM_PROSPERO)
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
//#include <sys/time.h>

#endif // #else // #if BB_USING(BB_COMPILER_MSVC)

#if BB_USING(BB_COMPILER_MSVC)

#define BB_CLOSE closesocket
#define BB_SHUTDOWN_SEND SD_SEND
#define BB_SHUTDOWN_BOTH SD_BOTH
#define BB_SELECT select
#define BB_SIN_SIZE int
#define BB_SOCKET_ERROR SOCKET_ERROR
#define BBNET_ERRNO WSAGetLastError()
typedef SOCKET bb_socket;
typedef int bb_recvfrom_sin_size;

#define BB_FD_SET(fd, set)        \
	BB_MULTI_LINE_MACRO_BEGIN     \
	BB_WARNING_PUSH_CONSTANT_EXPR \
	BB_WARNING_PUSH(4548)         \
	FD_SET(fd, set);              \
	BB_WARNING_POP                \
	BB_WARNING_POP                \
	BB_MULTI_LINE_MACRO_END

#define BB_FD_CLR(fd, set)        \
	BB_MULTI_LINE_MACRO_BEGIN     \
	BB_WARNING_PUSH_CONSTANT_EXPR \
	FD_CLR(fd, set);              \
	BB_WARNING_POP                \
	BB_MULTI_LINE_MACRO_END

#define BB_TIMEVAL TIMEVAL
#define BB_S_ADDR_UNION(ADDR) ((ADDR).sin_addr.S_un.S_addr)

#else // #if BB_USING(BB_COMPILER_MSVC)

#define BB_CLOSE close
#define BB_SHUTDOWN_SEND SHUT_WR
#define BB_SHUTDOWN_BOTH SHUT_RDWR
#define BB_SELECT select
#define BB_SIN_SIZE int
#define BB_SOCKET_ERROR -1
#define BBNET_ERRNO errno
typedef int bb_socket;
#if BB_USING(BB_PLATFORM_ANDROID)
typedef int bb_recvfrom_sin_size;
#else  // #if BB_USING(BB_PLATFORM_ANDROID)
typedef unsigned int bb_recvfrom_sin_size;
#endif // #else // #if BB_USING(BB_PLATFORM_ANDROID)
#define BB_FD_SET(fd, set) FD_SET(fd, set)
#define BB_FD_CLR(fd, set) FD_CLR(fd, set)
#define ioctlsocket ioctl

#define BB_TIMEVAL struct timeval
#define BB_S_ADDR_UNION(ADDR) ((ADDR).sin_addr.s_addr)

#endif // #else // #if BB_USING(BB_COMPILER_MSVC)

#define BB_INVALID_SOCKET ((bb_socket)(~0))

#if BB_USING(BB_PLATFORM_PROSPERO)
#define BB_FEATURE_IPV6 BB_OFF
#else
#define BB_FEATURE_IPV6 BB_ON
#endif

#if defined(__cplusplus)
extern "C" {
#endif

b32 bbnet_init(void);
void bbnet_shutdown(void);
void bbnet_gracefulclose(bb_socket* socket);
int bbnet_socket_nodelay(bb_socket socket, b32 nodelay);
int bbnet_socket_linger(bb_socket socket, b32 enabled, u16 seconds);
int bbnet_socket_reuseaddr(bb_socket socket, int reuseAddr);
int bbnet_socket_nonblocking(bb_socket socket, b32 nonblocking);
int bbnet_socket_ipv6only(bb_socket socket, b32 ipv6only);
b32 bbnet_socket_is6to4(const struct sockaddr *addr); // returns true if addr is ipv4 mapped to ipv6
u16 bbnet_get_port_from_sockaddr(const struct sockaddr *addr);
void bbnet_set_port_on_sockaddr(struct sockaddr* addr, const u16 port);

#if BB_USING(BB_FEATURE_IPV6)
void bbnet_socket_build6to4(struct sockaddr_in6* addr, const u32 ip); // builds ipv4to6 addr from v4 ip
#endif

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
