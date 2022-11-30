// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_criticalsection.h"
#include "bb_sockets.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
	kBBConnection_NotConnected,
	kBBConnection_Listening,
	kBBConnection_Connecting,
	kBBConnection_Connected,
	kBBConnection_Count
} bb_connection_state_e;

typedef enum {
	kBBCon_Client = 1 << 0,   // internal use only
	kBBCon_Server = 1 << 1,   // internal use only
	kBBCon_Blackbox = 1 << 2, // internal use only
} bb_connection_flag_e;

// post-discovery connection
typedef struct bb_connection_s {
	bb_socket socket;
	u8 sendBuffer[8192];
	u8 recvBuffer[32768];
	bb_critical_section cs;
	u64 sentBytesTotal;
	u64 receivedBytesTotal;
	u64 prevSendTime;
	u64 sendInterval;
	u64 connectTimeoutTime;
	u32 connectTimeoutInterval;
	u32 sendCursor;
	u32 recvCursor;
	u32 decodeCursor;
	u32 flags;
	bb_connection_state_e state;
} bb_connection_t;

void bbcon_init(bb_connection_t *con);
void bbcon_shutdown(bb_connection_t *con);
void bbcon_reset(bb_connection_t *con);

b32 bbcon_connect_client_async(bb_connection_t *con, u32 remoteAddr, u16 remotePort);
b32 bbcon_connect_client(bb_connection_t *con, u32 remoteAddr, u16 remotePort, u32 retries);
b32 bbcon_tick_connecting(bb_connection_t *con);
b32 bbcon_is_connecting(const bb_connection_t *con);

bb_socket bbcon_init_server(u32 *localIp, u16 *localPort);
b32 bbcon_connect_server(bb_connection_t *con, bb_socket testSocket, u32 localAddr, u16 localPort);
b32 bbcon_tick_listening(bb_connection_t *con);
b32 bbcon_is_listening(const bb_connection_t *con);

b32 bbcon_is_connected(const bb_connection_t *con);
void bbcon_disconnect(bb_connection_t *con);
void bbcon_disconnect_no_flush(bb_connection_t *con);

void bbcon_send(bb_connection_t *con, bb_decoded_packet_t *decoded);
void bbcon_send_raw(bb_connection_t *con, const void *pData, u32 nBytes);
void bbcon_flush(bb_connection_t *con);

b32 bbcon_try_send(bb_connection_t *con, bb_decoded_packet_t *decoded); // returns false if send would block
void bbcon_try_flush(bb_connection_t *con);

void bbcon_tick(bb_connection_t *con);
b32 bbcon_decodePacket(bb_connection_t *con, bb_decoded_packet_t *decoded);

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
