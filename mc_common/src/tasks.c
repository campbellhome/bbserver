// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "tasks.h"
#include "bb_array.h"
#include "bb_time.h"
#include "bb_wrap_stdio.h"
#include "va.h"

static tasks s_tasks;
static taskId s_lastId;

static const char *s_taskStateNames[] = {
	"kTaskState_Pending",
	"kTaskState_Running",
	"kTaskState_Canceling",
	"kTaskState_Succeeded",
	"kTaskState_Failed",
	"kTaskState_Canceled",
	"kTaskState_Count"
};
BB_CTASSERT(BB_ARRAYSIZE(s_taskStateNames) == kTaskState_Count + 1);

b32 task_started(task *t)
{
	return t->state > kTaskState_Pending;
}

b32 task_done(task *t)
{
	return t->state > kTaskState_Canceling;
}

static const char *task_get_name(task *t)
{
	return va("^=%s^7", sb_get(&t->name));
}

static void task_reset(task *t, b32 quiet)
{
	if(!quiet && t->debug) {
		BB_LOG("task::task_reset", "reset %s", task_get_name(t));
	}
	if(t->reset) {
		t->reset(t);
	}
	for(u32 i = 0; i < t->subtasks.count; ++i) {
		task *s = t->subtasks.data + i;
		task_reset(s, quiet);
	}
	bba_free(t->subtasks);
	sdict_reset(&t->extraData);
	sb_reset(&t->name);
	sb_reset(&t->prefix);
}

static void task_update_parent_ptrs(task *t)
{
	for(u32 i = 0; i < t->subtasks.count; ++i) {
		t->subtasks.data[i].parent = t;
		task_update_parent_ptrs(t->subtasks.data + i);
	}
}

void tasks_update_parent_ptrs(void)
{
	for(u32 i = 0; i < s_tasks.count; ++i) {
		task_update_parent_ptrs(s_tasks.data + i);
	}
}

void tasks_startup(void)
{
}

void tasks_shutdown(void)
{
	for(u32 i = 0; i < s_tasks.count; ++i) {
		task *t = s_tasks.data + i;
		task_reset(t, false);
	}
	bba_free(s_tasks);
}

void task_discard(task t)
{
	if(t.state == kTaskState_Pending) {
		task_reset(&t, true);
	}
}

static void task_prep(task *t, const char *prefix)
{
	if(!t->id) {
		t->id = ++s_lastId;
	}
	if(sb_len(&t->prefix) == 0) {
		const char *name = va("%s%u:%s", prefix, t->id, sb_get(&t->name));
		sb_reset(&t->name);
		sb_append(&t->name, name);
		sb_reset(&t->prefix);
		sb_va(&t->prefix, "%s%u:", prefix, t->id);
	}
	for(u32 i = 0; i < t->subtasks.count; ++i) {
		task_prep(t->subtasks.data + i, sb_get(&t->prefix));
		t->subtasks.data[i].parent = t;
	}
}

void task_set_state(task *t, taskState state)
{
	if(t->state != state) {
		if(t->debug) {
			BB_LOG("task::task_set_state", "^<%s^7 -> ^:%s^7 %s",
			       s_taskStateNames[t->state], s_taskStateNames[state], task_get_name(t));
		}
		t->prevState = t->state;
		t->state = state;
		if(t->stateChanged) {
			t->stateChanged(t);
		} else if(state == kTaskState_Canceling) {
			task_set_state(t, kTaskState_Canceled);
		}
	}
}

task *task_queue(task t)
{
	if(t.tick) {
		if(bba_add_noclear(s_tasks, 1)) {
			task_prep(&t, "");
			bba_last(s_tasks) = t;
			if(t.debug) {
				BB_LOG("task::task_queue", "queued %s", task_get_name(&t));
			}
			tasks_update_parent_ptrs();
			return &bba_last(s_tasks);
		}
	}

	BB_ERROR("task::task_queue", "failed to queue %s", task_get_name(&t));
	task_reset(&t, false);
	return NULL;
}

task *task_queue_subtask(task *parent, task t)
{
	task *subtask = NULL;
	if(!parent->id) {
		task_prep(parent, (parent->parent ? sb_get(&parent->parent->prefix) : ""));
	}
	if(bba_add_noclear(parent->subtasks, 1)) {
		bba_last(parent->subtasks) = t;
		subtask = &bba_last(parent->subtasks);
		task_prep(subtask, sb_get(&parent->prefix));
		subtask->parent = parent;
		if(t.debug || parent->debug) {
			BB_LOG("task::task_queue", "queued %s", task_get_name(subtask));
		}
		task_update_parent_ptrs(parent);
	} else {
		BB_ERROR("task::task_queue", "failed to queue %s", task_get_name(&t));
		task_reset(&t, false);
	}
	return subtask;
}

void task_tick_subtasks(task *t)
{
	if(t->subtasks.count) {
		u32 states[kTaskState_Count] = { BB_EMPTY_INITIALIZER };
		for(u32 i = 0; i < t->subtasks.count; ++i) {
			task *s = t->subtasks.data + i;
			if(!task_started(s)) {
				if(t->parallel || !(states[kTaskState_Running] || states[kTaskState_Canceling])) {
					task_set_state(s, kTaskState_Running);
				}
			}
			if(task_started(s) && !task_done(s)) {
				s->tick(s);
			}
			states[s->state]++;
		}
		if(!states[kTaskState_Pending] && !states[kTaskState_Running] && !states[kTaskState_Canceling]) {
			// all children are done - finish current task too
			if(states[kTaskState_Canceled]) {
				task_set_state(t, kTaskState_Canceled);
			} else if(states[kTaskState_Failed]) {
				task_set_state(t, kTaskState_Failed);
			} else if(states[kTaskState_Succeeded]) {
				task_set_state(t, kTaskState_Succeeded);
			}
		}
	} else {
		task_set_state(t, kTaskState_Succeeded);
	}
}

u32 tasks_tick(void)
{
	u32 active = 0;
	for(u32 i = 0; i < s_tasks.count;) {
		task *t = s_tasks.data + i;
		if(task_started(t)) {
			if(task_done(t)) {
				task_reset(t, false);
				bba_erase(s_tasks, i);
			} else {
				t->tick(t);
				++active;
				++i;
			}
		} else {
			++i;
		}
	}

	if(!active || 1) {
		for(u32 i = 0; i < s_tasks.count; ++i) {
			task *t = s_tasks.data + i;
			if(!task_started(t)) {
				task_set_state(t, kTaskState_Running);
			}
		}
	}

	return s_tasks.count;
}

void tasks_flush(u32 sleepMillis)
{
	while(tasks_tick() > 0) {
		BB_FLUSH();
		bb_sleep_ms(sleepMillis);
	}
}
