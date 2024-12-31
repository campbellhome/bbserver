// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

void bb_tracked_malloc_enable(b32 enabled);

void* bb_malloc_loc(const char* file, int line, size_t size);
#define bb_malloc(x) bb_malloc_loc(__FILE__, __LINE__, (x))

void* bb_realloc_loc(const char* file, int line, void* ptr, size_t size);
#define bb_realloc(x, y) bb_malloc_loc(__FILE__, __LINE__, (x), (y))

void bb_free_loc(const char* file, int line, void* ptr);
#define bb_free(x) bb_free_loc(__FILE__, __LINE__, (x))

void bb_log_external_alloc_loc(const char* file, int line, void* ptr);
#define bb_log_external_alloc(x) bb_log_external_alloc_loc(__FILE__, __LINE__, (x))

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
