// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_common.h"
#include "bb_wrap_windows.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define BB_DEBUG_LOCKS BB_OFF

#if BB_USING(BB_DEBUG_LOCKS)
#include "bb_wrap_stdio.h"
#endif // #if BB_USING( BB_DEBUG_LOCKS )

#if BB_USING(BB_COMPILER_MSVC)
typedef struct _RTL_CRITICAL_SECTION CRITICAL_SECTION;
typedef CRITICAL_SECTION bb_critical_section_plat;
#else // #if BB_USING(BB_COMPILER_MSVC)
#include <pthread.h>
typedef pthread_mutex_t bb_critical_section_plat;
#endif // #else // #if BB_USING(BB_COMPILER_MSVC)

typedef struct bb_critical_section_s
{
	bb_critical_section_plat platform;
	b32 initialized;
	u8 pad[4];
} bb_critical_section;

void bb_critical_section_init(bb_critical_section* cs);
void bb_critical_section_shutdown(bb_critical_section* cs);

#if BB_USING(BB_COMPILER_MSVC)
_Acquires_lock_(cs->platform) void bb_critical_section_lock_impl(bb_critical_section* cs);
_Releases_lock_(cs->platform) void bb_critical_section_unlock_impl(bb_critical_section* cs);
#else  // #if BB_USING(BB_COMPILER_MSVC)
void bb_critical_section_lock_impl(bb_critical_section* cs);
void bb_critical_section_unlock_impl(bb_critical_section* cs);
#endif // #else // #if BB_USING(BB_COMPILER_MSVC)

#if BB_USING(BB_DEBUG_LOCKS)

#define bb_critical_section_lock(p)                                     \
	{                                                                   \
		printf("locking %p (%s) %s:%d\n", (p), #p, __FILE__, __LINE__); \
		bb_critical_section_lock_impl(p);                               \
		printf("locked %p (%s) %s:%d\n", (p), #p, __FILE__, __LINE__);  \
	}
#define bb_critical_section_unlock(p)                                     \
	{                                                                     \
		printf("unlocking %p (%s) %s:%d\n", (p), #p, __FILE__, __LINE__); \
		bb_critical_section_unlock_impl(p);                               \
		printf("unlocked %p (%s) %s:%d\n", (p), #p, __FILE__, __LINE__);  \
	}

#else // #if BB_USING( BB_DEBUG_LOCKS )

#define bb_critical_section_lock(p) bb_critical_section_lock_impl(p)
#define bb_critical_section_unlock(p) bb_critical_section_unlock_impl(p)

#endif // #else // #if BB_USING( BB_DEBUG_LOCKS )

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
