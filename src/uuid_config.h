// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

#include "uuid_rfc4122/sysdep.h"

AUTOJSON typedef struct uuidState_s {
	u32 timestampLow;
	u32 timestampHi;
	u16 clockSequence;
	uuid_node_t nodeId;
} uuidState_t;

void uuid_read_state(u16 *clockSequence, u32 *timestampLow, u32 *timestampHi, uuid_node_t *nodeId);
void uuid_write_state(u16 clockSequence, u32 timestampLow, u32 timestampHi, uuid_node_t nodeId);

#if defined(__cplusplus)
}
#endif
