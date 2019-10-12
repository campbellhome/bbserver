// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "config.h"
#include "config_whitelist_push.h"
#include "message_queue.h"
#include "recorder_thread.h"

#include "bb.h"
#include "bb_array.h"
#include "bb_discovery_client.h"
#include "bb_discovery_server.h"
#include "bb_log.h"
#include "bb_packet.h"
#include "bb_sockets.h"
#include "bb_string.h"
#include "bb_thread.h"
#include "bb_time.h"
#include <stdlib.h>

#if BB_USING(BB_PLATFORM_WINDOWS)
BB_WARNING_DISABLE(4710) // snprintf not inlined - can't push/pop because it happens later
#endif                   // #if BB_USING( BB_PLATFORM_WINDOWS )

const char *bb_discovery_packet_name(bb_discovery_packet_type_e type);

typedef struct
{
	bb_discovery_server_t ds;
	resolved_whitelist_t whitelist;
	bb_server_connection_data_t con[64];
	bb_critical_section whitelist_cs;
	bb_thread_handle_t thread_id;
	b32 shutdownRequest;
	u8 pad[4];
} discovery_data_t;

static discovery_data_t s_discovery_data; // too large for stack

static void discovery_init(discovery_data_t *host)
{
	bb_critical_section_init(&host->whitelist_cs);
	host->shutdownRequest = false;
}

static void discovery_shutdown(discovery_data_t *host)
{
	bba_free(host->whitelist);
	bb_critical_section_shutdown(&host->whitelist_cs);
}

static resolved_whitelist_entry_t *find_whitelist_match(discovery_data_t *host, struct sockaddr_in *sin,
                                                        bb_decoded_discovery_packet_t *decoded)
{
	if(decoded->packet.request.protocolVersion == BB_PROTOCOL_VERSION) {
		const char *sourceApplicationName = decoded->packet.request.sourceApplicationName;
		const char *applicationName = (*sourceApplicationName) ? sourceApplicationName : decoded->packet.request.applicationName;
		u32 sourceIp = decoded->packet.request.sourceIp;
		u32 addr = (sourceIp) ? sourceIp : ntohl(BB_S_ADDR_UNION(*sin));
		u32 i;
		for(i = 0; i < host->whitelist.count; ++i) {
			resolved_whitelist_entry_t *entry = host->whitelist.data + i;
			if((addr & entry->mask) == (entry->ip & entry->mask)) {
				if(!entry->applicationName[0] ||
				   !strcmp(entry->applicationName, applicationName)) {
					return entry;
				}
			}
		}
	}
	return NULL;
}

static bb_discovery_packet_type_e get_discovery_response(discovery_data_t *host, struct sockaddr_in *sin,
                                                         bb_decoded_discovery_packet_t *decoded, u64 *delay)
{
	bb_discovery_packet_type_e result = kBBDiscoveryPacketType_Invalid;

	bb_critical_section_lock(&s_discovery_data.whitelist_cs);

	if(decoded->packet.request.protocolVersion == BB_PROTOCOL_VERSION) {
		const char *applicationName = decoded->packet.request.applicationName;
		resolved_whitelist_entry_t *entry = find_whitelist_match(host, sin, decoded);
		u32 addr = ntohl(BB_S_ADDR_UNION(*sin));
		char ip[32];
		bb_format_ip(ip, sizeof(ip), addr);
		if(entry && entry->allow) {
			switch(decoded->type) {
			case kBBDiscoveryPacketType_RequestDiscovery:
			case kBBDiscoveryPacketType_RequestDiscovery_v1:
				*delay = entry->delay;
				result = kBBDiscoveryPacketType_AnnouncePresence;
				break;
			case kBBDiscoveryPacketType_RequestReservation:
			case kBBDiscoveryPacketType_RequestReservation_v1:
				result = kBBDiscoveryPacketType_ReservationAccept;
				break;
			default:
				break;
			}
		}
		BB_LOG("Discovery::Response",
		       "Request %s from %s @ %s: response %s\n",
		       bb_discovery_packet_name(decoded->type), applicationName, ip,
		       bb_discovery_packet_name(result));
	}

	bb_critical_section_unlock(&s_discovery_data.whitelist_cs);
	return result;
}

void discovery_push_whitelist(resolved_whitelist_t *resolvedWhitelist)
{
	u32 i;
	resolved_whitelist_t oldWhitelist;

	bb_critical_section_lock(&s_discovery_data.whitelist_cs);
	oldWhitelist = s_discovery_data.whitelist;
	s_discovery_data.whitelist = *resolvedWhitelist;
	bb_critical_section_unlock(&s_discovery_data.whitelist_cs);
	bba_free(oldWhitelist);

	bb_log("whitelist has %u entries (%u allocated):", resolvedWhitelist->count, resolvedWhitelist->allocated);
	for(i = 0; i < resolvedWhitelist->count; ++i) {
		resolved_whitelist_entry_t *entry = resolvedWhitelist->data + i;
		char ip[32], mask[32];
		bb_format_ip(ip, sizeof(ip), entry->ip);
		bb_format_ip(mask, sizeof(mask), entry->mask);
		bb_log("%u: %s %s (mask %s) application: '%s'", i,
		       entry->allow ? "(allow)" : "(deny)",
		       ip, mask, entry->applicationName);
	}
}

static bb_thread_return_t discovery_thread_func(void *args)
{
	u32 i, c;
	discovery_data_t *host = (discovery_data_t *)args;
	bb_discovery_server_t *ds = &host->ds;

	BB_THREAD_START("discovery");
	bbthread_set_name("discovery_thread_func");

	to_ui(kToUI_DiscoveryStatus, "Starting");

	while(!host->shutdownRequest) {
		if(bb_discovery_server_init(&host->ds))
			break;

		to_ui(kToUI_DiscoveryStatus, "Retrying");
		bb_sleep_ms(1000);
	}

	if(!host->shutdownRequest) {
		to_ui(kToUI_DiscoveryStatus, "Running");

		for(i = 0; i < BB_ARRAYSIZE(host->con); ++i) {
			bbcon_init(&host->con[i].con);
			host->con[i].shutdownRequest = &host->shutdownRequest;
		}
	}

	while(!host->shutdownRequest) {
		int nBytesRead;
		s8 buf[BB_MAX_DISCOVERY_PACKET_BUFFER_SIZE];
		struct sockaddr_in sin;

		bb_discovery_server_tick_responses(ds);
		nBytesRead = bb_discovery_server_recv_request(ds, buf, sizeof(buf), &sin);
		if(nBytesRead > 0 && nBytesRead <= BB_MAX_DISCOVERY_PACKET_BUFFER_SIZE) {
			bb_decoded_discovery_packet_t decoded;
			bb_discovery_response_t *response = ds->responses + ds->numResponses;
			response->packet.type = kBBDiscoveryPacketType_Invalid;
			if(bb_discovery_packet_deserialize(buf, (u16)nBytesRead, &decoded)) {
				u64 delay = 0;
				bb_discovery_packet_type_e responsePacketType = get_discovery_response(host, &sin, &decoded, &delay);
				bb_discovery_process_request(ds, &sin, &decoded, responsePacketType, delay);
			}
		}

		if(ds->numPendingConnections) {
			for(i = 0; i < ds->numPendingConnections; ++i) {
				const bb_discovery_pending_connection_t *pending = ds->pendingConnections + i;

				b32 found = false;
				for(c = 0; c < BB_ARRAYSIZE(host->con); ++c) {
					bb_server_connection_data_t *data = host->con + c;
					bb_connection_t *con = &data->con;
					if(!bbcon_is_connected(con) && !bbcon_is_listening(con) && con->socket == BB_INVALID_SOCKET && !data->bInUse) {
						data->bInUse = true;
						found = true;
						BB_LOG("bb:discovery", "pending con %u using con %u / %p with socket %d state %d", i, c, con, con->socket, con->state);
						bbcon_init(con);
						bb_strncpy(data->applicationName, pending->applicationName, sizeof(data->applicationName));
						if(!bbcon_connect_server(con, pending->socket, pending->localIp, pending->localPort)) {
							BB_ERROR("bb::discovery", "failed to start listening for client connection");
						}
						//BB_LOG("bb:discovery", "used con %p with socket %d state %d", con, con->socket, con->state);
						bbthread_create(recorder_thread, data);
						break;
					}
				}
				if(!found) {
					bb_error("no free connections to start listening for client connection");
				}
			}
			ds->numPendingConnections = 0;
		}
	}

	to_ui(kToUI_DiscoveryStatus, "Shutting down");
	bb_discovery_server_shutdown(&host->ds);
	BB_THREAD_END();
	bb_thread_exit(0);
}

int discovery_thread_init(void)
{
	memset(&s_discovery_data, 0, sizeof(s_discovery_data));
	discovery_init(&s_discovery_data);
	s_discovery_data.thread_id = bbthread_create(discovery_thread_func, &s_discovery_data);
	return s_discovery_data.thread_id != 0;
}

void discovery_thread_shutdown(void)
{
	if(s_discovery_data.thread_id != 0) {
		s_discovery_data.shutdownRequest = true;
		bbthread_join(s_discovery_data.thread_id);
		s_discovery_data.thread_id = 0;
	}
	discovery_shutdown(&s_discovery_data);
}
