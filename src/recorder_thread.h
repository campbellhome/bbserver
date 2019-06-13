// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include "bb.h"
#include "bb_thread.h"
#include "bbclient/bb_common.h"
#include "bbclient/bb_connection.h"

typedef struct bb_server_connection_data_s {
	bb_connection_t con;
	b32 *shutdownRequest;
	char applicationName[kBBSize_ApplicationName];
} bb_server_connection_data_t;

bb_thread_return_t recorder_thread(void *args);

#if defined(__cplusplus)
}
#endif
