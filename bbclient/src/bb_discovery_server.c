// Copyright (c) 2012-2024 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#include "bbclient/bb_discovery_server.h"

#include "bbclient/bb_assert.h"
#include "bbclient/bb_connection.h"
#include "bbclient/bb_log.h"
#include "bbclient/bb_string.h"
#include "bbclient/bb_time.h"
#include <stdlib.h> // for malloc
#include <string.h> // for memset

static b32 bb_discovery_server_init_addrFamily(bb_discovery_server_t* ds, const int addrFamily)
{
	struct sockaddr_storage sin;

	bb_socket* dsSocket = (addrFamily == AF_INET6) ? &ds->socket_in6 : &ds->socket_in4;
	*dsSocket = socket(addrFamily, SOCK_DGRAM, IPPROTO_UDP);
	if (BB_INVALID_SOCKET == *dsSocket)
	{
		BB_ERROR_A("Discovery", "Discovery server failed to create socket for addrFamily %d", addrFamily);
		return false;
	}

	// Bind the socket to any available address on the port to check
	memset(&sin, 0, sizeof(sin));
	sin.ss_family = (u8)addrFamily;
	if (addrFamily == AF_INET6)
	{
#if !BB_USING(BB_FEATURE_IPV6)
		BB_ERROR_A("Discovery", "Discovery server failed to create socket for addrFamily %d", addrFamily);
		return false;
#else  // #if !BB_USING(BB_FEATURE_IPV6)
		struct sockaddr_in6* addr = (struct sockaddr_in6*)&sin;
		addr->sin6_port = htons(BB_DISCOVERY_PORT);
		addr->sin6_addr = in6addr_any;
#endif // #if BB_USING(BB_FEATURE_IPV6)
	}
	else
	{
		struct sockaddr_in* addr = (struct sockaddr_in*)&sin;
		addr->sin_port = htons(BB_DISCOVERY_PORT);
		BB_S_ADDR_UNION(*addr) = INADDR_ANY;
	}

	bbnet_socket_reuseaddr(*dsSocket, true);
	if (addrFamily == AF_INET6)
	{
		bbnet_socket_ipv6only(*dsSocket, true);
	}

	char ipstr[64];
	bb_format_addr(ipstr, sizeof(ipstr), (const struct sockaddr*)&sin, sizeof(sin), true);
	BB_LOG_A("Discovery", "Binding %s for discovery...", ipstr);

	if (-1 == bind(*dsSocket, (struct sockaddr*)&sin, sizeof(sin)))
	{
		BB_ERROR_A("Discovery", "Discovery server failed to bind socket");
		bbnet_gracefulclose(dsSocket);
		return false;
	}

	bbnet_socket_linger(*dsSocket, true, 0);

	return true;
}

b32 bb_discovery_server_init(bb_discovery_server_t* ds, const int addrFamily)
{
	memset(ds, 0, sizeof(*ds));

	b32 b4 = (addrFamily == AF_INET || addrFamily == AF_UNSPEC) ? bb_discovery_server_init_addrFamily(ds, AF_INET) : false;
	b32 b6 = (addrFamily == AF_INET6 || addrFamily == AF_UNSPEC) ? bb_discovery_server_init_addrFamily(ds, AF_INET6) : false;

	if (!b4 && !b6)
	{
		BB_ERROR_A("Discovery", "Discovery server failed to bind socket on any addrFamily");
		return false;
	}

	return true;
}

void bb_discovery_server_shutdown(bb_discovery_server_t* ds)
{
	bbnet_gracefulclose(&ds->socket_in4);
	bbnet_gracefulclose(&ds->socket_in6);
}

static void bb_discovery_remove_response(bb_discovery_server_t* ds, const struct sockaddr_storage* sin)
{
	u32 index;
	for (index = 0; index < ds->numResponses; ++index)
	{
		bb_discovery_response_t* response = ds->responses + index;
		if (sin->ss_family == response->clientAddr.ss_family)
		{
			const struct sockaddr_in* incomingAddr4 = (const struct sockaddr_in*)sin;
			const struct sockaddr_in* responseAddr4 = (const struct sockaddr_in*)&response->clientAddr;
#if BB_USING(BB_FEATURE_IPV6)
			const struct sockaddr_in6* incomingAddr6 = (const struct sockaddr_in6*)sin;
			const struct sockaddr_in6* responseAddr6 = (const struct sockaddr_in6*)&response->clientAddr;
#endif // #if BB_USING(BB_FEATURE_IPV6)
			if ((sin->ss_family == AF_INET && !memcmp(&incomingAddr4->sin_addr, &responseAddr4->sin_addr, sizeof(responseAddr4->sin_addr)))
#if BB_USING(BB_FEATURE_IPV6)
			    || (sin->ss_family == AF_INET6 && !memcmp(&incomingAddr6->sin6_addr, &responseAddr6->sin6_addr, sizeof(responseAddr6->sin6_addr)))
#endif // #if BB_USING(BB_FEATURE_IPV6)
			)
			{
				if (index != ds->numResponses - 1)
				{
					bb_discovery_response_t* other = ds->responses + ds->numResponses - 1;
					*response = *other;
				}
				--ds->numResponses;
			}
		}
	}
}

static b32 bb_discovery_send_response(bb_socket socket, bb_discovery_response_t* response)
{
	u16 serializedLen;
	// char ipport[64];
	int nBytesSent;
	s8 buf[BB_MAX_DISCOVERY_PACKET_BUFFER_SIZE];
	BB_ASSERT(response->packet.type >= kBBDiscoveryPacketType_AnnouncePresence);

	serializedLen = bb_discovery_packet_serialize(&response->packet, buf, sizeof(buf));
	if (!serializedLen)
	{
		BB_ERROR_A("Discovery", "bb_discovery_send_response failed to encode packet");
		return false;
	}

	nBytesSent = sendto(socket, (const char*)buf, serializedLen, 0, (struct sockaddr*)&response->clientAddr, sizeof(response->clientAddr));
	// BB_LOG("Discovery", "Sent discovery response of %d bytes to %s",
	//        nBytesSent,
	//        bb_format_addr(ipport, sizeof(ipport),
	//                         (const struct sockaddr *)sin, sizeof(*sin), true);
	if (nBytesSent == serializedLen)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void bb_discovery_server_tick_responses(bb_discovery_server_t* ds)
{
	const u32 ResponseInterval = 50;

	// Send out any responses
	u64 now = bb_current_time_ms();
	u32 index;
	if (ds->numResponses > 0)
	{
		for (index = 0; index < ds->numResponses; ++index)
		{
			bb_discovery_response_t* response = ds->responses + index;
			if (response->nextSendTime > now)
				continue;

			++response->nTimesSent;
			response->nextSendTime = now + ResponseInterval;

			bb_socket* dsSocket = (response->clientAddr.ss_family == AF_INET6) ? &ds->socket_in6 : &ds->socket_in4;
			if (!bb_discovery_send_response(*dsSocket, response) ||
			    response->nTimesSent >= response->nMaxTimesSent)
			{
				// Failed to send or sent enough times - don't try again
				if (index != ds->numResponses - 1)
				{
					bb_discovery_response_t* other = ds->responses + ds->numResponses - 1;
					*response = *other;
				}
				--ds->numResponses;
			}
		}
	}
}

// Wait for someone to try to discover a server
int bb_discovery_server_recv_request(bb_discovery_server_t* ds, s8* buf, size_t bufSize, struct sockaddr_storage* sin)
{
	int nBytesRead;
	fd_set set;
	struct timeval tv;
	bb_recvfrom_sin_size sinSize = sizeof(*sin);

	FD_ZERO(&set);
	BB_FD_SET(ds->socket_in4, &set);
	BB_FD_SET(ds->socket_in6, &set);
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	if (1 != select((int)(BB_MAX(ds->socket_in4, ds->socket_in6) + 1), &set, 0, 0, &tv))
		return 0;

	if (FD_ISSET(ds->socket_in4, &set))
	{
		memset(sin, 0, sizeof(*sin));
		struct sockaddr_in* addr4 = (struct sockaddr_in*)sin;
		addr4->sin_family = AF_INET;
		addr4->sin_port = htons(BB_DISCOVERY_PORT);
		BB_S_ADDR_UNION(*addr4) = INADDR_ANY;
		nBytesRead = recvfrom(ds->socket_in4, (char*)buf, (int)bufSize, 0, (struct sockaddr*)sin, &sinSize);
		if (nBytesRead > 0)
		{
			// char addrBuf[256];
			// BB_LOG("Discovery", "Discovery server received %d bytes from %s", nBytesRead,
			//        bb_format_addr(addrBuf, sizeof(addrBuf),
			//                         (const struct sockaddr *)sin, sizeof(*sin), true));
			return nBytesRead;
		}
	}

	// Read the discovery request and verify version number.
	if (FD_ISSET(ds->socket_in6, &set))
	{
		memset(sin, 0, sizeof(*sin));
#if BB_USING(BB_FEATURE_IPV6)
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)sin;
		addr6->sin6_family = AF_INET6;
		addr6->sin6_port = htons(BB_DISCOVERY_PORT);
		addr6->sin6_addr = in6addr_any;
#endif // #if BB_USING(BB_FEATURE_IPV6)
		nBytesRead = recvfrom(ds->socket_in6, (char*)buf, (int)bufSize, 0, (struct sockaddr*)sin, &sinSize);
		if (nBytesRead > 0)
		{
			// char addrBuf[256];
			// BB_LOG("Discovery", "Discovery server received %d bytes from %s", nBytesRead,
			//        bb_format_addr(addrBuf, sizeof(addrBuf),
			//                         (const struct sockaddr *)sin, sizeof(*sin), true));
			return nBytesRead;
		}
	}

	return 0; // no data - invalid request, so go back to waiting for the next discovery request
}

static const char* s_bb_discovery_packet_name[] = {
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

const char* bb_discovery_packet_name(bb_discovery_packet_type_e type)
{
	if (type >= 0 && type < kBBDiscoveryPacketType_Count)
		return s_bb_discovery_packet_name[type];
	return "kBBDiscoveryPacketType_Invalid";
}

void bb_discovery_process_request(bb_discovery_server_t* ds, struct sockaddr_storage* sin,
                                  bb_decoded_discovery_packet_t* decoded,
                                  bb_discovery_packet_type_e responseType, u64 delay)
{
	char ip[256];
	bb_discovery_response_t* response;
	bb_discovery_remove_response(ds, sin);

	bb_format_addr(ip, sizeof(ip), (const struct sockaddr*)sin, sizeof(*sin), false);

	if (ds->numResponses >= BB_ARRAYSIZE(ds->responses))
	{
		BB_ERROR_A("Discovery", "response queue overflowed - ignoring request from %s", ip);
		return;
	}

	response = ds->responses + ds->numResponses;
	response->clientAddr = *sin;
	response->nextSendTime = delay ? bb_current_time_ms() + delay : 0;
	response->nTimesSent = 0;
	response->port = 0;

	switch (responseType)
	{
	case kBBDiscoveryPacketType_AnnouncePresence:
		response->nMaxTimesSent = kBBDiscoveryRetries;
		response->packet.type = kBBDiscoveryPacketType_AnnouncePresence;
		response->packet.packet.response.port = 0;
		response->packet.packet.response.protocolVersion = BB_PROTOCOL_VERSION;
		break;

	case kBBDiscoveryPacketType_ReservationAccept:
	{
		if (ds->numPendingConnections < BB_ARRAYSIZE(ds->pendingConnections))
		{
			bb_discovery_pending_connection_t* pending = ds->pendingConnections + ds->numPendingConnections;
			pending->localIp = 0;
			pending->localPort = 0;
			pending->socket = bbcon_init_server(&pending->localIp, &pending->localPort);
			if (pending->socket == BB_INVALID_SOCKET)
			{
				BB_ERROR_A("Discovery", "ignoring reservation accept request from %s - too many pending connections", ip);
			}
			else
			{
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
		// BB_LOG("Discovery", "ignoring request from %s", ip);
		return;
	}

	if (response->packet.type != kBBDiscoveryPacketType_Invalid)
	{
		// BB_LOG("Discovery", "%s: %s -> %s", ip, bb_discovery_packet_name(decoded->type),
		//        bb_discovery_packet_name(response->packet.type));
		++ds->numResponses; // keep the queued response we built up
	}
}

#endif // #if BB_ENABLED
