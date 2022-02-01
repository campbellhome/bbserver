// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_thread.h"
#include "tasks.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct tag_task_thread {
	bb_thread_handle_t handle;
	bb_thread_func func;
	void *data;
	b32 shouldTerminate;
	taskState threadDesiredState;
} task_thread;

void task_thread_tick(task *);
void task_thread_statechanged(task *);
void task_thread_reset(task *);
task thread_task_create(const char *name, Task_StateChanged statechanged, bb_thread_func func, void *data);

#if defined(__cplusplus)
}
#endif
