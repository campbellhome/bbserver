// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#include "bbclient/bb_discovery_client.h"
#include "bbclient/bb_discovery_packet.h"
#include "bbclient/bb_log.h"
#include "bbclient/bb_packet.h"
#include "bbclient/bb_socket_errors.h"
#include "bbclient/bb_sockets.h"
#include "bbclient/bb_string.h"
#include "bbclient/bb_time.h"
#include <string.h> // for memset

typedef struct bb_discovery_client_s
{
	u32 discoveryTimeoutMillis;
	u32 discoveryRequestMillis;
	struct sockaddr_storage reservationAddr;
	u32 serverIp;
	u16 serverPort;
	u8 pad[2];
} bb_discovery_client_t;

static b32 bb_discovery_send_request(bb_socket discoverySocket, bb_discovery_packet_type_e type,
                                     const char* applicationName, const char* sourceApplicationName,
                                     const char* deviceCode, u32 sourceIp, const struct sockaddr_storage* addrStorage)
{
	int nBytesSent;
	char ipport[256];
	u16 serializedLen;
	s8 buf[BB_MAX_DISCOVERY_PACKET_BUFFER_SIZE];
	bb_decoded_discovery_packet_t decoded;
	decoded.type = type;
	decoded.packet.request.sourceIp = sourceIp;
	bb_strncpy(decoded.packet.request.deviceCode, deviceCode, sizeof(decoded.packet.request.deviceCode));
	bb_strncpy(decoded.packet.request.sourceApplicationName, sourceApplicationName, sizeof(decoded.packet.request.sourceApplicationName));
	bb_strncpy(decoded.packet.request.applicationName, applicationName, sizeof(decoded.packet.request.applicationName));
	decoded.packet.request.platform = (u32)(bb_platform_e)bb_platform();

	serializedLen = bb_discovery_packet_serialize(&decoded, buf, sizeof(buf));
	if (!serializedLen)
	{
		bb_error("bb_discovery_send_request failed to encode packet");
		return false;
	}

	nBytesSent = sendto(discoverySocket, (const char*)buf, serializedLen, 0, (const struct sockaddr*)addrStorage, sizeof(*addrStorage));
	if (nBytesSent < 0)
	{
		int err = BBNET_ERRNO;
		bb_error("Failed to send discovery request to %s - err %d (%s)",
		         bb_format_addr(ipport, sizeof(ipport), (const struct sockaddr*)addrStorage, sizeof(*addrStorage), true),
		         err, bbnet_error_to_string(err));
	}
	else
	{
		bb_log("Sent discovery request of %d bytes to %s",
		       nBytesSent,
		       bb_format_addr(ipport, sizeof(ipport), (const struct sockaddr*)addrStorage, sizeof(*addrStorage), true));
	}
	if (nBytesSent == serializedLen)
	{
		return true;
	}
	else
	{
		return false;
	}
}

static b32 bb_discovery_client_recvfrom(bb_socket socket, struct sockaddr_storage* addr, bb_decoded_discovery_packet_t* decoded, u32 timeoutMillis)
{
	u64 startMillis = bb_current_time_ms();
	u64 endMillis = startMillis + timeoutMillis;

	s8 buf[BB_MAX_DISCOVERY_PACKET_BUFFER_SIZE];

	BB_WARNING_PUSH_CONSTANT_EXPR;
	while (1)
	{
		u64 now = bb_current_time_ms();
		if (now >= endMillis)
		{
			break;
		}
		else
		{
			int ret;
			int nBytesRead;
			fd_set set;
			BB_TIMEVAL tv;
			long remainingMillis = (long)(endMillis - now);
			socklen_t sinSize = sizeof(*addr);

			tv.tv_sec = 0;
			tv.tv_usec = remainingMillis * 1000;

			FD_ZERO(&set);
			BB_FD_SET(socket, &set);

			//	select for response
			ret = BB_SELECT((int)socket + 1, &set, 0, 0, &tv);
			if (1 != ret)
			{
				// bb_log( "discovery client select ret:%d" );
				continue;
			}

			memset(addr, 0, sizeof(*addr));
			nBytesRead = recvfrom(socket, (char*)buf, sizeof(buf), 0, (struct sockaddr*)addr, &sinSize);
			if (nBytesRead <= 0)
			{
				bb_warning("discovery recvfrom failed ret:%d", nBytesRead);
				continue; // no data - invalid request, so go back to waiting for the next discovery request
			}

			if (bb_discovery_packet_deserialize(buf, (u16)nBytesRead, decoded))
			{
				if (decoded->type >= kBBDiscoveryPacketType_AnnouncePresence &&
				    decoded->type < kBBDiscoveryPacketType_Count)
				{
					return true;
				}
			}
		}
	}
	BB_WARNING_POP;
	return false;
}

bb_discovery_result_t bb_discovery_client_start(const char* applicationName, const char* sourceApplicationName, const char* deviceCode,
                                                u32 sourceIp, const struct sockaddr* discoveryAddr, size_t discoveryAddrSize)
{
	const u32 kDiscoveryTimeoutMillis = 500;
	const u32 kDiscoveryRequestMillis = 100;
	int val = 1;
	bb_socket discoverySocket;
	bb_discovery_client_t client = { BB_EMPTY_INITIALIZER };
	bb_discovery_client_t* dc;
	u64 startTime = bb_current_time_ms();
	u64 endTime = startTime + kDiscoveryTimeoutMillis;
	u64 prevRequestTime = 0;
	bb_discovery_result_t result;
	memset(&result, 0, sizeof(result));

	dc = &client;
	dc->discoveryTimeoutMillis = kDiscoveryTimeoutMillis;
	dc->discoveryRequestMillis = kDiscoveryRequestMillis;
	dc->serverIp = 0;
	dc->serverPort = 0;
	memcpy(&dc->reservationAddr, discoveryAddr, BB_MIN(sizeof(dc->reservationAddr), discoveryAddrSize));

	const u16 reservationPort = bbnet_get_port_from_sockaddr(discoveryAddr);

	bb_log("Client discovery started");

	discoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (discoverySocket == BB_INVALID_SOCKET)
	{
		bb_error("Client discovery failed - could not bind discovery socket");
		return result;
	}

	// put the socket into broadcast mode so we can send the same packet to everyone on the subnet
	if (-1 == setsockopt(discoverySocket, SOL_SOCKET, SO_BROADCAST, (char*)&val, sizeof(val)))
	{
		bb_error("Client discovery failed - could not set discovery socket to broadcast");
		BB_CLOSE(discoverySocket);
		return result;
	}

	bbnet_socket_nonblocking(discoverySocket, true);

	BB_WARNING_PUSH_CONSTANT_EXPR;
	while (1)
	{
		const u32 ReservationMilliseconds = 2000;
		u64 reservationStartTime;
		u64 reservationEndTime;
		u64 currentTime;
		char serverIpStr[256];
		struct sockaddr_storage serverAddr = { BB_EMPTY_INITIALIZER };
		bb_decoded_discovery_packet_t decoded;
		u64 now = bb_current_time_ms();
		if (now > endTime)
			break;

		if (now >= prevRequestTime + dc->discoveryRequestMillis)
		{
			bb_discovery_send_request(discoverySocket, kBBDiscoveryPacketType_RequestDiscovery, applicationName,
			                          sourceApplicationName, deviceCode, sourceIp, &dc->reservationAddr);
			prevRequestTime = now;
		}

		if (!bb_discovery_client_recvfrom(discoverySocket, &serverAddr, &decoded, 100))
			continue;

		bbnet_set_port_on_sockaddr((struct sockaddr *)&serverAddr, reservationPort);
		bb_format_addr(serverIpStr, sizeof(serverIpStr), (const struct sockaddr*)&serverAddr, sizeof(serverAddr), false);

		if (decoded.type != kBBDiscoveryPacketType_AnnouncePresence)
		{
			bb_log("Ignoring response %d - expected %d", decoded.type, kBBDiscoveryPacketType_AnnouncePresence);
			continue; // ignore reservation accept/decline - they're in error, maybe intended for a previous run
		}

		//	send reservation request
		bb_log("Sending reservation request");
		bb_discovery_send_request(discoverySocket, kBBDiscoveryPacketType_RequestReservation, applicationName,
		                          sourceApplicationName, deviceCode, sourceIp, &serverAddr);

		// wait for reservation response
		reservationStartTime = bb_current_time_ms();
		reservationEndTime = reservationStartTime + ReservationMilliseconds;

		// Loop while not timed out:
		// BB_TRACE( "Waiting for reservation response time:%d", reservationStartTime );
		currentTime = reservationStartTime;
		while (currentTime < reservationEndTime)
		{
			currentTime = bb_current_time_ms();

			struct sockaddr_storage newServerAddr = { BB_EMPTY_INITIALIZER };
			if (!bb_discovery_client_recvfrom(discoverySocket, &newServerAddr, &decoded, 100))
				continue;

			b32 bMismatch = (serverAddr.ss_family != newServerAddr.ss_family);
			if (!bMismatch)
			{
				if (serverAddr.ss_family == AF_INET)
				{
					bMismatch = BB_S_ADDR_UNION(*(const struct sockaddr_in*)&serverAddr) != BB_S_ADDR_UNION(*(const struct sockaddr_in*)&newServerAddr);
				}
				else if (serverAddr.ss_family == AF_INET6)
				{
					bMismatch = memcmp(&((const struct sockaddr_in6*)&serverAddr)->sin6_addr, &((const struct sockaddr_in6*)&newServerAddr)->sin6_addr, 16) != 0;
				}
				else
				{
					bMismatch = true;
				}
			}

			if (bMismatch)
			{
				char packetIp[256];
				bb_format_addr(packetIp, sizeof(packetIp), (const struct sockaddr*)&newServerAddr, sizeof(newServerAddr), false);
				bb_log("Ignoring response from server %s - reserving from %s", packetIp, serverIpStr);
				continue;
			}

			if (decoded.type == kBBDiscoveryPacketType_ReservationAccept)
			{
				// We win!
				char ipport[256];
				result.serverAddr = serverAddr;
				result.success = true;
				bbnet_set_port_on_sockaddr((struct sockaddr*)&result.serverAddr, decoded.packet.response.port);
				bbnet_gracefulclose(&discoverySocket);

				bb_format_addr(ipport, sizeof(ipport), (const struct sockaddr*)&result.serverAddr, sizeof(result.serverAddr), true);
				bb_log("Client discovery reserved server at %s", ipport);
				return result;
			}
		}

		bb_warning("Timed out waiting for reservation response from %s", serverIpStr);
	}
	BB_WARNING_POP;

	bbnet_gracefulclose(&discoverySocket);
	bb_error("Client discovery failed");
	return result;
}

#endif // #if BB_ENABLED
