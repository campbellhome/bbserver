// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#include "bbclient/bb_array.h"
#include "bbclient/bb_file.h"
#include <string.h>

// warning C4710: 'int snprintf(char *const ,const std::size_t,const char *const ,...)': function not inlined
BB_WARNING_DISABLE(4710)

#ifndef bba__free
#define bba__free free
#endif

#include "bbclient/bb_wrap_stdio.h"
#include "bbclient/bb_wrap_windows.h"

#if defined(__cplusplus)
extern "C" { // needed to allow inclusion in .cpp unity files
#endif

s64 g_bba_allocatedCount;
s64 g_bba_allocatedBytes;

static b32 s_bba_logAllocs;
static b32 s_bba_logFailedAllocs;
static bb_file_handle_t s_bba_logFile;
void bba_set_logging(b32 allocs, b32 failedAllocs)
{
	s_bba_logAllocs = allocs;
	s_bba_logFailedAllocs = failedAllocs;
}
void bba_set_log_path(const char *path)
{
	if(s_bba_logFile != BB_INVALID_FILE_HANDLE) {
		bb_file_close(s_bba_logFile);
		s_bba_logFile = BB_INVALID_FILE_HANDLE;
	}
	if(path && *path) {
		if(s_bba_logFile == BB_INVALID_FILE_HANDLE) {
			s_bba_logFile = bb_file_open_for_write(path);
		}
	}
}

#if BB_USING(BB_COMPILER_MSVC)
#define PRIPtr "0x%p"
#else
#define PRIPtr "%p"
#endif

void bba_log_free(void *p, u32 allocated, u64 bytes, const char *file, int line)
{
	BB_UNUSED(allocated);
	BB_UNUSED(bytes);
#ifdef _MSC_VER
	InterlockedAdd64(&g_bba_allocatedCount, -1);
	InterlockedAdd64(&g_bba_allocatedBytes, -(s64)bytes);
#endif

	if(s_bba_logAllocs) {
		char buf[256];
		if(bb_snprintf(buf, sizeof(buf), "%s(%d) : bba_free(" PRIPtr ") (%" PRIu64 " bytes)\n", file, line, p, bytes) < 0) {
			buf[sizeof(buf) - 1] = '\0';
		}
		if(s_bba_logFile) {
			bb_file_write(s_bba_logFile, buf, (u32)strlen(buf));
		} else {
#if BB_USING(BB_COMPILER_MSVC)
			OutputDebugStringA(buf);
#else
			puts(buf);
#endif
		}
	}
}

static BB_INLINE void bba_log_realloc(u64 oldp, void *newp, u32 allocated, u32 desired, u64 allocatedSize, u64 desiredSize, const char *file, int line)
{
	BB_UNUSED(allocated);
	BB_UNUSED(desired);
	BB_UNUSED(allocatedSize);
#ifdef _MSC_VER
	if(!oldp) {
		InterlockedAdd64(&g_bba_allocatedCount, 1);
	}
	InterlockedAdd64(&g_bba_allocatedBytes, desiredSize - allocatedSize);
#endif

	if(s_bba_logAllocs) {
		char buf[256];
		if(bb_snprintf(buf, sizeof(buf), "%s(%d) : bba_realloc(" PRIPtr ", " PRIPtr ") %" PRIu64 " bytes -> %" PRIu64 " bytes (%" PRIu64 " bytes)\n", file, line, (void *)oldp, newp, allocatedSize, desiredSize, desiredSize - allocatedSize) < 0) {
			buf[sizeof(buf) - 1] = '\0';
		}
		if(s_bba_logFile) {
			bb_file_write(s_bba_logFile, buf, (u32)strlen(buf));
		} else {
#if BB_USING(BB_COMPILER_MSVC)
			OutputDebugStringA(buf);
#else
			puts(buf);
#endif
		}
	}
}

static BB_INLINE void bba_log_overflowed_realloc(u64 oldp, u32 count, u32 increment, u32 allocated, u32 requested, const char *file, int line)
{
	if(s_bba_logFailedAllocs) {
		char buf[256];
		if(bb_snprintf(buf, sizeof(buf), "%s(%d) : bba_realloc(" PRIPtr ") bytes OVERFLOWED - count:%u increment:%u allocated:%u requested:%u\n",
		               file, line, (void *)oldp, count, increment, allocated, requested) < 0) {
			buf[sizeof(buf) - 1] = '\0';
		}
		if(s_bba_logFile) {
			bb_file_write(s_bba_logFile, buf, (u32)strlen(buf));
		} else {
#if BB_USING(BB_COMPILER_MSVC)
			OutputDebugStringA(buf);
#else
			puts(buf);
#endif
		}
	}
}
static BB_INLINE void bba_log_failed_realloc(u64 oldp, u64 size, const char *file, int line)
{
	if(s_bba_logFailedAllocs) {
		char buf[256];
		if(bb_snprintf(buf, sizeof(buf), "%s(%d) : bba_realloc(" PRIPtr ") %" PRIu64 " bytes FAILED\n", file, line, (void *)oldp, size) < 0) {
			buf[sizeof(buf) - 1] = '\0';
		}
#if BB_USING(BB_COMPILER_MSVC)
		OutputDebugStringA(buf);
#else
		puts(buf);
#endif
	}
}

void *bba__raw_add(void *base, ptrdiff_t data_offset, u32 *count, u32 *allocated, u32 increment, u32 itemsize, b32 clear, b32 reserve_only, const char *file, int line)
{
	void **parr = (void **)((u8 *)base + data_offset);
	void *arr = *parr;

	if(*count + increment > *allocated) {
		u32 dbl_cur = 2 * *allocated;
		u32 min_needed = *count + increment;
		u32 desired = dbl_cur > min_needed ? dbl_cur : min_needed;

		if(itemsize * desired > itemsize * *allocated) {
			void *p = (void *)bba__realloc(arr, itemsize * (u64)desired);
			if(p) {
				void *ret = (u8 *)p + itemsize * (u64)*count;
				bba_log_realloc((u64)arr, p, *allocated, desired, (u64)itemsize * *allocated, (u64)itemsize * desired, file, line);
				if(clear && !reserve_only) {
					u32 bytes = itemsize * (desired - *allocated);
					memset(ret, 0, bytes);
				}
				*parr = p;
				*allocated = desired;
				if(!reserve_only) {
					*count += increment;
				}
				return ret;
			} else {
				bba_log_failed_realloc((u64)arr, (u64)itemsize * desired, file, line);
				return NULL;
			}
		} else {
			bba_log_overflowed_realloc((u64)arr, *count, increment, *allocated, itemsize * desired, file, line);
			return NULL;
		}
	} else {
		void *ret = (u8 *)arr + itemsize * (u64)*count;
		if(!reserve_only) {
			if(clear) {
				u32 bytes = itemsize * increment;
				memset(ret, 0, bytes);
			}
			*count += increment;
		}
		return ret;
	}
}

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
