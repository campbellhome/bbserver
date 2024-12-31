// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_common.h"
#include "bb_sockets.h"

#if defined(__cplusplus)
extern "C"
{
#endif

struct sockaddr;

typedef struct bb_discovery_result_s
{
	struct sockaddr_storage serverAddr;
	b32 success;
	u8 pad[4];
} bb_discovery_result_t;

bb_discovery_result_t bb_discovery_client_start(const char* applicationName, const char* sourceApplicationName, const char* deviceCode,
                                                u32 sourceIp, const struct sockaddr* discoveryAddr, size_t discoveryAddrSize);

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
