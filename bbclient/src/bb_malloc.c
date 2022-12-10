// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"
#include "bbclient/bb_common.h"
#include "bbclient/bb_wrap_malloc.h"
#include "bbclient/bb_wrap_stdio.h"
#include "bbclient/bb_wrap_windows.h"

#if BB_USING(BB_COMPILER_MSVC)
#define BB_PTR_PREFIX "0x"
#else
#define BB_PTR_PREFIX
#endif

static b32 s_bbTrackMalloc;

void bb_tracked_malloc_enable(b32 enabled)
{
	s_bbTrackMalloc = enabled;
}

void *bb_malloc_loc(const char *file, int line, size_t size)
{
	void *ptr = malloc(size);
	if(s_bbTrackMalloc) {
		char buf[256];
		if(bb_snprintf(buf, sizeof(buf), "%s(%d) : bb_malloc(%zu) " BB_PTR_PREFIX "%p\n", file, line, size, ptr) < 0) {
			buf[sizeof(buf) - 1] = '\0';
		}
#if BB_USING(BB_COMPILER_MSVC)
		OutputDebugStringA(buf);
#else
		puts(buf);
#endif
	}
	return ptr;
}

void *bb_realloc_loc(const char *file, int line, void *ptr, size_t size)
{
	u64 oldPtr = (u64)ptr;
	void *newPtr = realloc(ptr, size);
	if(s_bbTrackMalloc) {
		char buf[256];
		if(bb_snprintf(buf, sizeof(buf), "%s(%d) : bb_realloc(0x%16.16llx, %zu) " BB_PTR_PREFIX "%p\n", file, line, oldPtr, size, newPtr) < 0) {
			buf[sizeof(buf) - 1] = '\0';
		}
#if BB_USING(BB_COMPILER_MSVC)
		OutputDebugStringA(buf);
#else
		puts(buf);
#endif
	}
	return ptr;
}

void bb_free_loc(const char *file, int line, void *ptr)
{
	if(s_bbTrackMalloc) {
		char buf[256];
		if(bb_snprintf(buf, sizeof(buf), "%s(%d) : bb_free(" BB_PTR_PREFIX "%p)\n", file, line, ptr) < 0) {
			buf[sizeof(buf) - 1] = '\0';
		}
#if BB_USING(BB_COMPILER_MSVC)
		OutputDebugStringA(buf);
#else
		puts(buf);
#endif
	}
	free(ptr);
}

void bb_log_external_alloc_loc(const char *file, int line, void *ptr)
{
	if(s_bbTrackMalloc) {
		char buf[256];
		if(bb_snprintf(buf, sizeof(buf), "%s(%d) : bb_external_alloc(" BB_PTR_PREFIX "%p)\n", file, line, ptr) < 0) {
			buf[sizeof(buf) - 1] = '\0';
		}
#if BB_USING(BB_COMPILER_MSVC)
		OutputDebugStringA(buf);
#else
		puts(buf);
#endif
	}
}

#endif // #if BB_ENABLED
