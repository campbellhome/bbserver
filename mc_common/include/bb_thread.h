// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#if defined(__cplusplus)
extern "C" {
#endif

#include "bb_common.h"

#if BB_USING(BB_COMPILER_MSVC)

#include <bb_wrap_process.h>

typedef uintptr_t bb_thread_handle_t;
typedef unsigned bb_thread_return_t;
typedef bb_thread_return_t (*bb_thread_func)(void* args);
#define bb_thread_local __declspec(thread)
#define bb_thread_exit(ret) \
	{                       \
		_endthreadex(ret);  \
		return ret;         \
	}

#else // #if BB_USING(BB_COMPILER_MSVC)

#include <pthread.h>

typedef pthread_t bb_thread_handle_t;
typedef void* bb_thread_return_t;
typedef bb_thread_return_t (*bb_thread_func)(void* args);
#define bb_thread_local __thread
#define bb_thread_exit(ret) \
	{                       \
		return ret;         \
	}

#endif // #else // #if BB_USING(BB_COMPILER_MSVC)

#define bb_thread_name(x) #x

bb_thread_handle_t bbthread_create(bb_thread_func func, void* arg);
void bbthread_set_name(const char* name);
void bbthread_join(bb_thread_handle_t threadHandle);
bb_thread_handle_t bbthread_get_current(void);
u64 bbthread_get_current_id(void);

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
