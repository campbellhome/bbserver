// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "thread_task.h"
#include "bb_malloc.h"
#include <stdlib.h>
#include <string.h>

void task_thread_tick(task *t)
{
	task_thread *th = t->taskData;
	if(th && th->handle) {
		if(th->threadDesiredState > kTaskState_Running && t->state != th->threadDesiredState) {
			task_set_state(t, th->threadDesiredState);
		}
	}
}

void task_thread_statechanged(task *t)
{
	if(t->state == kTaskState_Running) {
		task_thread *th = t->taskData;
		th->handle = bbthread_create(th->func, th);
	}
}

void task_thread_reset(task *t)
{
	task_thread *th = t->taskData;
	if(th) {
		if(th->handle) {
			th->shouldTerminate = true;
			bbthread_join(th->handle);
		}
		bb_free(th);
		t->taskData = NULL;
	}
}

task thread_task_create(const char *name, Task_StateChanged statechanged, bb_thread_func func, void *data)
{
	task t = { BB_EMPTY_INITIALIZER };
	sb_append(&t.name, name);
	t.tick = task_thread_tick;
	t.stateChanged = statechanged ? statechanged : task_thread_statechanged;
	t.reset = task_thread_reset;
	t.taskData = bb_malloc(sizeof(task_thread));
	if(t.taskData) {
		task_thread *th = t.taskData;
		memset(th, 0, sizeof(*th));
		th->func = func;
		th->data = data;
	}
	return t;
}
