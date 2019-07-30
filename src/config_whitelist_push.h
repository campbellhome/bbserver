// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct resolved_whitelist_entry_s {
	u32 ip;
	u32 mask;
	u32 delay;
	b32 allow;
	char applicationName[kBBSize_ApplicationName];
} resolved_whitelist_entry_t;

typedef struct resolved_whitelist_s {
	u32 count;
	u32 allocated;
	resolved_whitelist_entry_t *data;
} resolved_whitelist_t;

typedef struct configWhitelist_s configWhitelist_t;
void config_push_whitelist(configWhitelist_t *configWhitelist);

#if defined(__cplusplus)
}
#endif
