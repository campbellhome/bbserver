// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_common.h"
#include "bb_discovery_shared.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
	kBBDiscoveryPacketType_Invalid = -1,

	// Client --> Server
	kBBDiscoveryPacketType_RequestDiscovery_v1,
	kBBDiscoveryPacketType_RequestReservation_v1,
	kBBDiscoveryPacketType_DeclineReservation_v1,

	// Server --> Client
	kBBDiscoveryPacketType_AnnouncePresence,
	kBBDiscoveryPacketType_ReservationAccept,
	kBBDiscoveryPacketType_ReservationRefuse,

	kBBDiscoveryPacketType_RequestDiscovery_v2,
	kBBDiscoveryPacketType_RequestReservation_v2,
	kBBDiscoveryPacketType_DeclineReservation,

	kBBDiscoveryPacketType_RequestDiscovery,
	kBBDiscoveryPacketType_RequestReservation,

	kBBDiscoveryPacketType_Count
} bb_discovery_packet_type_e;

typedef struct
{
	u32 protocolVersion;
	u32 sourceIp;
	u32 platform;
	char deviceCode[kBBSize_ApplicationName];
	char sourceApplicationName[kBBSize_ApplicationName];
	char applicationName[kBBSize_ApplicationName];
} bb_packet_discovery_request_t;

typedef struct
{
	u32 protocolVersion;
	u16 port;
	u8 pad[2];
} bb_packet_discovery_response_t;

typedef struct bb_decoded_discovery_packet_s {
	bb_discovery_packet_type_e type;
	u8 pad[4];
	union {
		bb_packet_discovery_request_t request;
		bb_packet_discovery_response_t response;
	} packet;
} bb_decoded_discovery_packet_t;

enum {
	BB_MAX_DISCOVERY_PACKET_BUFFER_SIZE = sizeof(bb_decoded_discovery_packet_t) + sizeof(BB_PROTOCOL_IDENTIFIER)
};

b32 bb_discovery_packet_deserialize(s8 *buffer, u16 len, bb_decoded_discovery_packet_t *decoded);
u16 bb_discovery_packet_serialize(bb_decoded_discovery_packet_t *source, s8 *buffer, u16 len);

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
