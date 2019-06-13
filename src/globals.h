// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct globals_s {
	char viewerPath[kBBSize_MaxPath];
	char viewerName[kBBSize_MaxPath];
	b32 viewer;
	u8 pad[4];
} globals_t;

extern globals_t globals;

#if defined(__cplusplus)
}
#endif
