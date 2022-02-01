// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct bb_discovery_result_s {
	u32 serverIp;
	u16 serverPort;
	u8 pad[2];
} bb_discovery_result_t;

bb_discovery_result_t bb_discovery_client_start(const char *applicationName, const char *sourceApplicationName, const char *deviceCode,
                                                u32 sourceIp, u32 searchIp, u16 searchPort);

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
