// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "process_utils.h"
#include "sb.h"
#include "tasks.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct tag_task_process
{
	process_t* process;
	sb_t dir;
	sb_t cmdline;
	processSpawnType_t spawnType;
	processVisiblityType_t visibilityType;
} task_process;

void task_process_tick(task*);
void task_process_statechanged(task*);
void task_process_reset(task*);
task process_task_create(const char* name, processSpawnType_t spawnType, const char* dir, const char* cmdlineFmt, ...);
task process_task_create_with_visibility(const char* name, processSpawnType_t spawnType, processVisiblityType_t visibilityType, const char* dir, const char* cmdlineFmt, ...);

#if defined(__cplusplus)
}
#endif
