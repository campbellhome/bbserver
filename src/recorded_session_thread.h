// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "config.h"

#include "bb_thread.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct recorded_session_s recorded_session_t;
typedef struct bb_decoded_packet_s bb_decoded_packet_t;

bb_thread_return_t recorded_session_read_thread(void* args);
b32 recorded_session_consume(recorded_session_t* session, bb_decoded_packet_t* decoded);

#if defined(__cplusplus)
}
#endif
