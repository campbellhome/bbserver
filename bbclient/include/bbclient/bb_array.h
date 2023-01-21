// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_common.h"
#include <memory.h> // for memmove etc

#ifndef bba__free
#define bba__free free
#endif

#ifndef bba__realloc
#include "bb_wrap_malloc.h"
#define bba__realloc realloc
#endif

#if defined(__cplusplus)
extern "C" {
#endif

void bba_set_logging(b32 allocs, b32 failedAllocs);
void bba_set_log_path(const char* path);
void bba_log_free(void* p, u32 allocated, u64 bytes, const char* file, int line);
void* bba__raw_add(void* base, ptrdiff_t data_offset, u32* count, u32* allocated, u32 increment, u32 itemsize, b32 clear, b32 reserve_only, const char* file, int line);

extern s64 g_bba_allocatedCount;
extern s64 g_bba_allocatedBytes;

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
template <class T, class U>
static BB_INLINE U* bba__template_add(T& a, U*, u32 n, b32 clear, b32 reserve_only, const char* file, int line)
{
	return (U*)bba__raw_add(&(a), (u8*)&((a).data) - (u8*)&(a), &(a).count, &(a).allocated, (n), sizeof(*((a).data)), clear, reserve_only, file, line);
}
#define bba_add(a, n) bba__template_add((a), (a).data, (n), true, false, __FILE__, __LINE__)
#define bba_add_noclear(a, n) bba__template_add((a), (a).data, (n), false, false, __FILE__, __LINE__)
#define bba_reserve(a, n) bba__template_add((a), (a).data, (n), false, true, __FILE__, __LINE__)
#define bba_add_from_loc(file, line, a, n) bba__template_add((a), (a).data, (n), true, false, (file), (line))
#define bba_add_noclear_from_loc(file, line, a, n) bba__template_add((a), (a).data, (n), false, false, (file), (line))
#define bba_reserve_from_loc(file, line, a, n) bba__template_add((a), (a).data, (n), false, true, (file), (line))
#else // #if defined(__cplusplus)
#define bba_add(a, n) bba__raw_add(&(a), (u8*)&((a).data) - (u8*)&(a), &(a).count, &(a).allocated, (n), sizeof(*((a).data)), true, false, __FILE__, __LINE__)
#define bba_add_noclear(a, n) bba__raw_add(&(a), (u8*)&((a).data) - (u8*)&(a), &(a).count, &(a).allocated, (n), sizeof(*((a).data)), false, false, __FILE__, __LINE__)
#define bba_reserve(a, n) bba__raw_add(&(a), (u8*)&((a).data) - (u8*)&(a), &(a).count, &(a).allocated, (n), sizeof(*((a).data)), true, true, __FILE__, __LINE__)
#define bba_add_from_loc(file, line, a, n) bba__raw_add(&(a), (u8*)&((a).data) - (u8*)&(a), &(a).count, &(a).allocated, (n), sizeof(*((a).data)), true, false, (file), (line))
#define bba_add_noclear_from_loc(file, line, a, n) bba__raw_add(&(a), (u8*)&((a).data) - (u8*)&(a), &(a).count, &(a).allocated, (n), sizeof(*((a).data)), false, false, (file), (line))
#define bba_reserve_from_loc(file, line, a, n) bba__raw_add(&(a), (u8*)&((a).data) - (u8*)&(a), &(a).count, &(a).allocated, (n), sizeof(*((a).data)), true, true, (file), (line))
#endif //#else // #if defined(__cplusplus)

#define bba_add_array(a, ptr, n)                                     \
	{                                                                \
		if (bba_add_noclear(a, n))                                   \
		{                                                            \
			memcpy((a).data + (a).count - n, ptr, n * sizeof(*ptr)); \
		}                                                            \
	}

#define bba_clear(a) (a).count = 0
#define bba_free(a) (((a).data) ? bba_log_free((a).data, (a).allocated, (a).allocated * sizeof(*((a).data)), __FILE__, __LINE__), bba__free((a).data), (a).data = 0, (a).count = 0, (a).allocated = 0, 0 : 0)
#define bba_free_from_loc(file, line, a) (((a).data) ? bba_log_free((a).data, (a).allocated, (a).allocated * sizeof(*((a).data)), (file), (line)), bba__free((a).data), (a).data = 0, (a).count = 0, (a).allocated = 0, 0 : 0)
#define bba_push(a, v)                     \
	{                                      \
		if (bba_add_noclear(a, 1) != NULL) \
		{                                  \
			bba_last(a) = (v);             \
		}                                  \
	}
#define bba_push_from_loc(file, line, a, v)                     \
	{                                                           \
		if (bba_add_noclear_from_loc(file, line, a, 1) != NULL) \
		{                                                       \
			bba_last(a) = (v);                                  \
		}                                                       \
	}
#define bba_last(a) ((a).data[(a).count - 1])

#define bba_erase(a, n)                                                                   \
	{                                                                                     \
		if (n < (a).count)                                                                \
		{                                                                                 \
			u32 remainder = (a).count - n - 1;                                            \
			if (remainder)                                                                \
			{                                                                             \
				memmove((a).data + n, (a).data + n + 1, remainder * sizeof((a).data[0])); \
			}                                                                             \
			--(a).count;                                                                  \
		}                                                                                 \
	}

#define bba_clone(src, dst)                                                    \
	{                                                                          \
		bba_free(dst);                                                         \
		bba_add(dst, (src).count);                                             \
		if ((dst).count)                                                       \
		{                                                                      \
			memcpy((dst).data, (src).data, (dst).count * sizeof(*(dst).data)); \
		}                                                                      \
	}

#endif // #if BB_ENABLED
