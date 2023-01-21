// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_criticalsection.h"
#include "bb_discovery_packet.h"
#include "bb_discovery_shared.h"
#include "bb_sockets.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct bb_discovery_response_s
{
	struct sockaddr_in clientAddr;
	u64 nextSendTime;
	int nTimesSent;
	int nMaxTimesSent;
	bb_decoded_discovery_packet_t packet;
	u16 port;
	u8 pad[2];
} bb_discovery_response_t;

typedef struct bb_discovery_pending_connection_s
{
	bb_socket socket;
	u32 localIp;
	u16 localPort;
	char applicationName[kBBSize_ApplicationName];
	u8 pad[2];
} bb_discovery_pending_connection_t;

typedef struct bb_discovery_server_s
{
	bb_socket socket;
	bb_discovery_response_t responses[64];
	bb_discovery_pending_connection_t pendingConnections[64];
	u32 numResponses;
	u32 numPendingConnections;
} bb_discovery_server_t;

b32 bb_discovery_server_init(bb_discovery_server_t* ds);
void bb_discovery_server_shutdown(bb_discovery_server_t* ds);

void bb_discovery_server_tick_responses(bb_discovery_server_t* ds);
int bb_discovery_server_recv_request(bb_discovery_server_t* ds, s8* buf, size_t bufSize,
                                     struct sockaddr_in* sin);
void bb_discovery_process_request(bb_discovery_server_t* ds, struct sockaddr_in* sin,
                                  bb_decoded_discovery_packet_t* decoded,
                                  bb_discovery_packet_type_e responseType, u64 delay);

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
