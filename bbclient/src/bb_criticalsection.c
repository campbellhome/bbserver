// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#if defined(_MSC_VER)
__pragma(warning(disable : 4464)); // warning C4464: relative include path contains '..'
#endif

#include "../include/bb.h" // relative path so we don't rely on include path being set up for configurations that have BB_ENABLED 0

#if BB_ENABLED

#include "bbclient/bb_criticalsection.h"

#if BB_USING(BB_COMPILER_MSVC)

void bb_critical_section_init(bb_critical_section *cs)
{
	InitializeCriticalSection(&cs->platform);
	cs->initialized = true;
}

void bb_critical_section_shutdown(bb_critical_section *cs)
{
	cs->initialized = false;
	DeleteCriticalSection(&cs->platform);
}

_Acquires_lock_(cs->platform) void bb_critical_section_lock_impl(bb_critical_section *cs)
{
	EnterCriticalSection(&cs->platform);
}

_Releases_lock_(cs->platform) void bb_critical_section_unlock_impl(bb_critical_section *cs)
{
	LeaveCriticalSection(&cs->platform);
}

#else // #if BB_USING(BB_COMPILER_MSVC)

void bb_critical_section_init(bb_critical_section *cs)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&cs->platform, &attr);
	pthread_mutexattr_destroy(&attr);
	cs->initialized = true;
}

void bb_critical_section_shutdown(bb_critical_section *cs)
{
	cs->initialized = false;
	pthread_mutex_destroy(&cs->platform);
}

void bb_critical_section_lock_impl(bb_critical_section *cs)
{
	pthread_mutex_lock(&cs->platform);
}

void bb_critical_section_unlock_impl(bb_critical_section *cs)
{
	pthread_mutex_unlock(&cs->platform);
}

#endif // #else // #if BB_USING(BB_COMPILER_MSVC)

#endif // #if BB_ENABLED
