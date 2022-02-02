// Copyright (c) 2012-202 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#include "bbclient/bb_discovery_server.h"

#include "bbclient/bb_connection.h"
#include "bbclient/bb_log.h"
#include "bbclient/bb_string.h"
#include "bbclient/bb_time.h"
#include <stdlib.h> // for malloc
#include <string.h> // for memset

b32 bb_discovery_server_init(bb_discovery_server_t *ds)
{
	struct sockaddr_in sin;
	memset(ds, 0, sizeof(*ds));

	ds->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(BB_INVALID_SOCKET == ds->socket) {
		BB_ERROR_A("Discovery", "Discovery server failed to create socket");
		return false;
	}

	// Bind the socket to any available address on the port to check
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(BB_DISCOVERY_PORT);
	BB_S_ADDR_UNION(sin) = INADDR_ANY;

	bbnet_socket_reuseaddr(ds->socket, true);

	char ipstr[32];
	bb_format_ip(ipstr, sizeof(ipstr), INADDR_ANY);
	BB_LOG_A("Discovery", "Binding %s:%d for discovery...", ipstr, BB_DISCOVERY_PORT);

	if(-1 == bind(ds->socket, (struct sockaddr *)&sin, sizeof(sin))) {
		BB_ERROR_A("Discovery", "Discovery server failed to bind socket");
		bbnet_gracefulclose(&ds->socket);
		return false;
	}

	bbnet_socket_linger(ds->socket, true, 0);

	return true;
}

void bb_discovery_server_shutdown(bb_discovery_server_t *ds)
{
	bbnet_gracefulclose(&ds->socket);
}

static void bb_discovery_remove_response(bb_discovery_server_t *ds, const struct sockaddr_in *sin)
{
	u32 index;
	for(index = 0; index < ds->numResponses; ++index) {
		bb_discovery_response_t *response = ds->responses + index;
		if(BB_S_ADDR_UNION(*sin) == BB_S_ADDR_UNION(response->clientAddr)) {
			if(index != ds->numResponses - 1) {
				bb_discovery_response_t *other = ds->responses + ds->numResponses - 1;
				*response = *other;
			}
			--ds->numResponses;
		}
	}
}

static b32 bb_discovery_send_response(bb_socket socket, bb_discovery_response_t *response)
{
	u16 serializedLen;
	//char ipport[32];
	int nBytesSent;
	s8 buf[BB_MAX_DISCOVERY_PACKET_BUFFER_SIZE];
	BB_ASSERT(response->packet.type >= kBBDiscoveryPacketType_AnnouncePresence);

	serializedLen = bb_discovery_packet_serialize(&response->packet, buf, sizeof(buf));
	if(!serializedLen) {
		BB_ERROR_A("Discovery", "bb_discovery_send_response failed to encode packet");
		return false;
	}

	nBytesSent = sendto(socket, (const char *)buf, serializedLen, 0, (struct sockaddr *)&response->clientAddr, sizeof(response->clientAddr));
	//BB_LOG("Discovery", "Sent discovery response of %d bytes to %s",
	//       nBytesSent,
	//       bb_format_ipport(ipport, sizeof(ipport),
	//                        ntohl(BB_S_ADDR_UNION(response->clientAddr)),
	//                        ntohs(response->clientAddr.sin_port)));
	if(nBytesSent == serializedLen) {
		return true;
	} else {
		return false;
	}
}

void bb_discovery_server_tick_responses(bb_discovery_server_t *ds)
{
	const u32 ResponseInterval = 50;

	// Send out any responses
	u64 now = bb_current_time_ms();
	u32 index;
	if(ds->numResponses > 0) {
		for(index = 0; index < ds->numResponses; ++index) {
			bb_discovery_response_t *response = ds->responses + index;
			if(response->nextSendTime > now)
				continue;

			++response->nTimesSent;
			response->nextSendTime = now + ResponseInterval;

			if(!bb_discovery_send_response(ds->socket, response) ||
			   response->nTimesSent >= response->nMaxTimesSent) {
				// Failed to send or sent enough times - don't try again
				if(index != ds->numResponses - 1) {
					bb_discovery_response_t *other = ds->responses + ds->numResponses - 1;
					*response = *other;
				}
				--ds->numResponses;
			}
		}
	}
}

// Wait for someone to try to discover a server
int bb_discovery_server_recv_request(bb_discovery_server_t *ds, s8 *buf, size_t bufSize, struct sockaddr_in *sin)
{
	//char ipport[32];
	int nBytesRead;
	fd_set set;
	struct timeval tv;
	bb_recvfrom_sin_size sinSize = sizeof(*sin);

	FD_ZERO(&set);
	BB_FD_SET(ds->socket, &set);
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	if(1 != select((int)ds->socket + 1, &set, 0, 0, &tv))
		return 0;

	// Read the discovery request and verify version number.
	memset(sin, 0, sizeof(*sin));
	sin->sin_family = AF_INET;
	sin->sin_port = htons(BB_DISCOVERY_PORT);
	BB_S_ADDR_UNION(*sin) = INADDR_ANY;
	nBytesRead = recvfrom(ds->socket, (char *)buf, (int)bufSize, 0, (struct sockaddr *)sin, &sinSize);
	if(nBytesRead <= 0)
		return 0; // no data - invalid request, so go back to waiting for the next discovery request

	//BB_LOG("Discovery", "Discovery server received %d bytes from %s", nBytesRead,
	//       bb_format_ipport(ipport, sizeof(ipport),
	//                        ntohl(BB_S_ADDR_UNION(*sin)),
	//                        ntohs(sin->sin_port)));

	return nBytesRead;
}

static const char *s_bb_discovery_packet_name[] = {
	"kBBDiscoveryPacketType_RequestDiscovery_v1",
	"kBBDiscoveryPacketType_RequestReservation_v1",
	"kBBDiscoveryPacketType_DeclineReservation_v1",
	"kBBDiscoveryPacketType_AnnouncePresence",
	"kBBDiscoveryPacketType_ReservationAccept",
	"kBBDiscoveryPacketType_ReservationRefuse",
	"kBBDiscoveryPacketType_RequestDiscovery_v2",
	"kBBDiscoveryPacketType_RequestReservation_v2",
	"kBBDiscoveryPacketType_DeclineReservation",
	"kBBDiscoveryPacketType_RequestDiscovery",
	"kBBDiscoveryPacketType_RequestReservation",
};
BB_CTASSERT(BB_ARRAYSIZE(s_bb_discovery_packet_name) == kBBDiscoveryPacketType_Count);

const char *bb_discovery_packet_name(bb_discovery_packet_type_e type)
{
	if(type >= 0 && type < kBBDiscoveryPacketType_Count)
		return s_bb_discovery_packet_name[type];
	return "kBBDiscoveryPacketType_Invalid";
}

void bb_discovery_process_request(bb_discovery_server_t *ds, struct sockaddr_in *sin,
                                  bb_decoded_discovery_packet_t *decoded,
                                  bb_discovery_packet_type_e responseType, u64 delay)
{
	char ip[32];
	bb_discovery_response_t *response;
	bb_discovery_remove_response(ds, sin);

	bb_format_ip(ip, sizeof(ip), ntohl(BB_S_ADDR_UNION(*sin)));

	if(ds->numResponses >= BB_ARRAYSIZE(ds->responses)) {
		BB_ERROR_A("Discovery", "response queue overflowed - ignoring request from %s", ip);
		return;
	}

	response = ds->responses + ds->numResponses;
	response->clientAddr = *sin;
	response->nextSendTime = delay ? bb_current_time_ms() + delay : 0;
	response->nTimesSent = 0;
	response->port = 0;

	switch(responseType) {
	case kBBDiscoveryPacketType_AnnouncePresence:
		response->nMaxTimesSent = kBBDiscoveryRetries;
		response->packet.type = kBBDiscoveryPacketType_AnnouncePresence;
		response->packet.packet.response.port = 0;
		response->packet.packet.response.protocolVersion = BB_PROTOCOL_VERSION;
		break;

	case kBBDiscoveryPacketType_ReservationAccept: {
		if(ds->numPendingConnections < BB_ARRAYSIZE(ds->pendingConnections)) {
			bb_discovery_pending_connection_t *pending = ds->pendingConnections + ds->numPendingConnections;
			pending->localIp = 0;
			pending->localPort = 0;
			pending->socket = bbcon_init_server(&pending->localIp, &pending->localPort);
			if(pending->socket == BB_INVALID_SOCKET) {
				BB_ERROR_A("Discovery", "ignoring reservation accept request from %s - too many pending connections", ip);
			} else {
				BB_LOG_A("Discovery", "pending connection %u using socket %d", ds->numPendingConnections, pending->socket);
				bb_strncpy(pending->applicationName, decoded->packet.request.applicationName, sizeof(pending->applicationName));
				response->nMaxTimesSent = kBBDiscoveryRetries;
				response->packet.type = kBBDiscoveryPacketType_ReservationAccept;
				response->packet.packet.response.port = pending->localPort;
				response->packet.packet.response.protocolVersion = BB_PROTOCOL_VERSION;
				++ds->numPendingConnections;
			}
		}
		break;
	}

	case kBBDiscoveryPacketType_RequestDiscovery_v2:
	case kBBDiscoveryPacketType_RequestDiscovery_v1:
	case kBBDiscoveryPacketType_RequestReservation_v2:
	case kBBDiscoveryPacketType_RequestReservation_v1:
	case kBBDiscoveryPacketType_DeclineReservation_v1:
	case kBBDiscoveryPacketType_RequestDiscovery:
	case kBBDiscoveryPacketType_RequestReservation:
	case kBBDiscoveryPacketType_DeclineReservation:
	case kBBDiscoveryPacketType_ReservationRefuse:
	case kBBDiscoveryPacketType_Invalid:
	case kBBDiscoveryPacketType_Count:
	default:
		//BB_LOG("Discovery", "ignoring request from %s", ip);
		return;
	}

	if(response->packet.type != kBBDiscoveryPacketType_Invalid) {
		//BB_LOG("Discovery", "%s: %s -> %s", ip, bb_discovery_packet_name(decoded->type),
		//       bb_discovery_packet_name(response->packet.type));
		++ds->numResponses; // keep the queued response we built up
	}
}

#endif // #if BB_ENABLED
