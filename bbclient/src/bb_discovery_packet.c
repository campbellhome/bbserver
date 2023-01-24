// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#include "bbclient/bb_assert.h"
#include "bbclient/bb_discovery_packet.h"
#include "bbclient/bb_serialize.h"
#include <string.h>

static b32 bb_discovery_packet_serialize_request(bb_serialize_t* ser, bb_decoded_discovery_packet_t* source)
{
	u16 len;
	source->packet.request.protocolVersion = BB_PROTOCOL_VERSION;
	bbserialize_buffer(ser, (char*)BB_PROTOCOL_IDENTIFIER, sizeof(BB_PROTOCOL_IDENTIFIER));
	bbserialize_u32(ser, &source->packet.request.protocolVersion);
	bbserialize_u32(ser, &source->packet.request.sourceIp);
	if (source->type == kBBDiscoveryPacketType_RequestDiscovery || source->type == kBBDiscoveryPacketType_RequestDiscovery_v1)
	{
		bbserialize_u32(ser, &source->packet.request.platform);
	}
	if (source->type == kBBDiscoveryPacketType_RequestDiscovery || source->type == kBBDiscoveryPacketType_RequestReservation)
	{
		bbserialize_text_(ser, source->packet.request.deviceCode, sizeof(source->packet.request.deviceCode), &len);
	}
	bbserialize_text_(ser, source->packet.request.sourceApplicationName, sizeof(source->packet.request.sourceApplicationName), &len);
	return bbserialize_buffer(ser, &source->packet.request.applicationName, (u16)strlen(source->packet.request.applicationName));
}

static b32 bb_discovery_packet_deserialize_request(bb_serialize_t* ser, bb_decoded_discovery_packet_t* decoded)
{
	u16 len;
	u32 remaining;
	char identifier[sizeof(BB_PROTOCOL_IDENTIFIER)];

	if (!bbserialize_buffer(ser, identifier, sizeof(BB_PROTOCOL_IDENTIFIER)))
		return false;

	if (memcmp(identifier, BB_PROTOCOL_IDENTIFIER, sizeof(BB_PROTOCOL_IDENTIFIER)))
		return false;

	bbserialize_u32(ser, &decoded->packet.request.protocolVersion);
	bbserialize_u32(ser, &decoded->packet.request.sourceIp);
	if (decoded->type == kBBDiscoveryPacketType_RequestDiscovery || decoded->type == kBBDiscoveryPacketType_RequestDiscovery_v1)
	{
		bbserialize_u32(ser, &decoded->packet.request.platform);
	}
	else
	{
		decoded->packet.request.platform = kBBPlatform_Unknown;
	}
	if (decoded->type == kBBDiscoveryPacketType_RequestDiscovery || decoded->type == kBBDiscoveryPacketType_RequestReservation)
	{
		bbserialize_text_(ser, decoded->packet.request.deviceCode, sizeof(decoded->packet.request.deviceCode), &len);
	}
	bbserialize_text_(ser, decoded->packet.request.sourceApplicationName, sizeof(decoded->packet.request.sourceApplicationName), &len);

	remaining = ser->nBufferBytes - ser->nCursorBytes;
	if (remaining >= sizeof(decoded->packet.request.applicationName))
		return false;

	bbserialize_buffer(ser, &decoded->packet.request.applicationName, (u16)remaining);
	decoded->packet.request.applicationName[remaining] = 0;
	return ser->state == kBBSerialize_Ok;
}

static b32 bb_discovery_packet_serialize_response(bb_serialize_t* ser, bb_decoded_discovery_packet_t* source)
{
	source->packet.response.protocolVersion = BB_PROTOCOL_VERSION;
	bbserialize_buffer(ser, (char*)BB_PROTOCOL_IDENTIFIER, sizeof(BB_PROTOCOL_IDENTIFIER));
	bbserialize_u32(ser, &source->packet.response.protocolVersion);
	return bbserialize_u16(ser, &source->packet.response.port);
}

static b32 bb_discovery_packet_deserialize_response(bb_serialize_t* ser, bb_decoded_discovery_packet_t* decoded)
{
	char identifier[sizeof(BB_PROTOCOL_IDENTIFIER)];

	if (!bbserialize_buffer(ser, identifier, sizeof(BB_PROTOCOL_IDENTIFIER)))
		return false;

	if (memcmp(identifier, BB_PROTOCOL_IDENTIFIER, sizeof(BB_PROTOCOL_IDENTIFIER)))
		return false;

	bbserialize_u32(ser, &decoded->packet.response.protocolVersion);
	return bbserialize_u16(ser, &decoded->packet.response.port);
}

b32 bb_discovery_packet_deserialize(s8* buffer, u16 len, bb_decoded_discovery_packet_t* decoded)
{
	u8 type;
	bb_serialize_t ser;
	bbserialize_init_read(&ser, buffer, len);
	type = kBBDiscoveryPacketType_Count;
	bbserialize_u8(&ser, &type);
	decoded->type = (bb_discovery_packet_type_e)type;
	switch (decoded->type)
	{
	case kBBDiscoveryPacketType_AnnouncePresence:
	case kBBDiscoveryPacketType_ReservationAccept:
	case kBBDiscoveryPacketType_ReservationRefuse:
		return bb_discovery_packet_deserialize_response(&ser, decoded);

	case kBBDiscoveryPacketType_RequestDiscovery_v2:
	case kBBDiscoveryPacketType_RequestDiscovery_v1:
	case kBBDiscoveryPacketType_RequestDiscovery:
	case kBBDiscoveryPacketType_RequestReservation_v2:
	case kBBDiscoveryPacketType_RequestReservation_v1:
	case kBBDiscoveryPacketType_RequestReservation:
		return bb_discovery_packet_deserialize_request(&ser, decoded);

	case kBBDiscoveryPacketType_DeclineReservation_v1:
	case kBBDiscoveryPacketType_DeclineReservation:
	case kBBDiscoveryPacketType_Invalid:
	case kBBDiscoveryPacketType_Count:
		break;
	}

	return false;
}

u16 bb_discovery_packet_serialize(bb_decoded_discovery_packet_t* source, s8* buffer, u16 len)
{
	u8 type;
	bb_serialize_t ser;
	bbserialize_init_write(&ser, buffer, len);
	type = (u8)source->type;
	bbserialize_u8(&ser, &type);
	switch (source->type)
	{
	case kBBDiscoveryPacketType_AnnouncePresence:
	case kBBDiscoveryPacketType_ReservationAccept:
	case kBBDiscoveryPacketType_ReservationRefuse:
		bb_discovery_packet_serialize_response(&ser, source);
		break;

	case kBBDiscoveryPacketType_RequestDiscovery_v2:
	case kBBDiscoveryPacketType_RequestDiscovery_v1:
	case kBBDiscoveryPacketType_RequestDiscovery:
	case kBBDiscoveryPacketType_RequestReservation_v2:
	case kBBDiscoveryPacketType_RequestReservation_v1:
	case kBBDiscoveryPacketType_RequestReservation:
		bb_discovery_packet_serialize_request(&ser, source);
		break;

	case kBBDiscoveryPacketType_DeclineReservation_v1:
	case kBBDiscoveryPacketType_DeclineReservation:
	case kBBDiscoveryPacketType_Invalid:
	case kBBDiscoveryPacketType_Count:
		BB_ASSERT(false);
		return 0;
	}

	if (ser.state == kBBSerialize_Ok)
	{
		return (u16)ser.nCursorBytes; // #TODO: bb_serialize_t len should be u16 (or everything else should be u32)
	}
	else
	{
		return 0;
	}
}

#endif // #if BB_ENABLED
