// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#include "bbclient/bb_socket_errors.h"

const char* bbnet_error_to_string(int err)
{
#define BBNET_SOCKET_ERROR_CASE(x) \
	case x: return #x

	switch (err)
	{
		BBNET_SOCKET_ERROR_CASE(BBNET_EWOULDBLOCK);
		BBNET_SOCKET_ERROR_CASE(BBNET_EINPROGRESS);
		BBNET_SOCKET_ERROR_CASE(BBNET_EALREADY);
		BBNET_SOCKET_ERROR_CASE(BBNET_ENOTSOCK);
		BBNET_SOCKET_ERROR_CASE(BBNET_EDESTADDRREQ);
		BBNET_SOCKET_ERROR_CASE(BBNET_EMSGSIZE);
		BBNET_SOCKET_ERROR_CASE(BBNET_EPROTOTYPE);
		BBNET_SOCKET_ERROR_CASE(BBNET_ENOPROTOOPT);
		BBNET_SOCKET_ERROR_CASE(BBNET_EPROTONOSUPPORT);
		BBNET_SOCKET_ERROR_CASE(BBNET_ESOCKTNOSUPPORT);
		BBNET_SOCKET_ERROR_CASE(BBNET_EOPNOTSUPP);
		BBNET_SOCKET_ERROR_CASE(BBNET_EPFNOSUPPORT);
		BBNET_SOCKET_ERROR_CASE(BBNET_EAFNOSUPPORT);
		BBNET_SOCKET_ERROR_CASE(BBNET_EADDRINUSE);
		BBNET_SOCKET_ERROR_CASE(BBNET_EADDRNOTAVAIL);
		BBNET_SOCKET_ERROR_CASE(BBNET_ENETDOWN);
		BBNET_SOCKET_ERROR_CASE(BBNET_ENETUNREACH);
		BBNET_SOCKET_ERROR_CASE(BBNET_ENETRESET);
		BBNET_SOCKET_ERROR_CASE(BBNET_ECONNABORTED);
		BBNET_SOCKET_ERROR_CASE(BBNET_ECONNRESET);
		BBNET_SOCKET_ERROR_CASE(BBNET_ENOBUFS);
		BBNET_SOCKET_ERROR_CASE(BBNET_EISCONN);
		BBNET_SOCKET_ERROR_CASE(BBNET_ENOTCONN);
		BBNET_SOCKET_ERROR_CASE(BBNET_ESHUTDOWN);
		BBNET_SOCKET_ERROR_CASE(BBNET_ETOOMANYREFS);
		BBNET_SOCKET_ERROR_CASE(BBNET_ETIMEDOUT);
		BBNET_SOCKET_ERROR_CASE(BBNET_ECONNREFUSED);
		BBNET_SOCKET_ERROR_CASE(BBNET_ELOOP);
		BBNET_SOCKET_ERROR_CASE(BBNET_ENAMETOOLONG);
		BBNET_SOCKET_ERROR_CASE(BBNET_EHOSTDOWN);
		BBNET_SOCKET_ERROR_CASE(BBNET_EHOSTUNREACH);
		BBNET_SOCKET_ERROR_CASE(BBNET_ENOTEMPTY);
#if defined(BBNET_EPROCLIM)
		BBNET_SOCKET_ERROR_CASE(BBNET_EPROCLIM);
#endif
		BBNET_SOCKET_ERROR_CASE(BBNET_EUSERS);
		BBNET_SOCKET_ERROR_CASE(BBNET_EDQUOT);
		BBNET_SOCKET_ERROR_CASE(BBNET_ESTALE);
		BBNET_SOCKET_ERROR_CASE(BBNET_EREMOTE);
#if defined(BBNET_EPIPE)
		BBNET_SOCKET_ERROR_CASE(BBNET_EPIPE);
#endif
	case 0:
		return "no error";
	default:
		return "Unknown";
	}
}

#endif // #if BB_ENABLED
