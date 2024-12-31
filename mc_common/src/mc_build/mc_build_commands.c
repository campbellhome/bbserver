// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "mc_build/mc_build_commands.h"
#include "bb_array.h"
#include "mc_build/mc_build_structs_generated.h"
#include "process_task.h"
#include "tokenize.h"

void buildCommands_push(buildCommands_t* buildCommands, const char* title, const char* dir, const char* command)
{
	buildCommand_t buildCommand = { BB_EMPTY_INITIALIZER };
	buildCommand.title = sb_from_c_string(title);
	buildCommand.dir = sb_from_c_string(dir);
	buildCommand.command = sb_from_c_string(command);
	bba_push(*buildCommands, buildCommand);
}

typedef struct buildCommandLink_s
{
	buildCommand_t* command;
	buildCommandsState_t* state;
} buildCommandLink_t;

static void task_command_statechanged(task* t)
{
	task_process_statechanged(t);
	if (task_done(t) && t->userData) // process failing to start will enter twice
	{
		buildCommandLink_t* link = t->userData;
		buildCommand_t* command = link->command;
		buildCommandsState_t* state = link->state;
		--state->active;
		command->bFinished = true;
		if (t->state == kTaskState_Failed)
		{
			++state->errorCount;
		}

		task_process* p = t->taskData;
		if (p->process)
		{
			command->result = p->process->exitCode;
			command->stdoutBuffer = sb_from_c_string(p->process->stdoutBuffer.data ? p->process->stdoutBuffer.data : "");
			command->stderrBuffer = sb_from_c_string(p->process->stderrBuffer.data ? p->process->stderrBuffer.data : "");
		}
		else
		{
			sb_va(&command->stderrBuffer, "Failed to start task for %s\n", sb_get(&command->title));
		}

		t->userData = NULL;
		free(link);
	}
}

static void buildCommands_printBuffer(sb_t* sb)
{
	if (sb->count < 2)
		return;

	span_t dataCursor = { BB_EMPTY_INITIALIZER };
	dataCursor.start = sb->data;
	dataCursor.end = sb->data + sb->count - 1;
	do
	{
		span_t lineCursor = tokenizeLine(&dataCursor);
		if (!lineCursor.end)
			break;
		BB_LOG("Command", "%.*s", lineCursor.end - lineCursor.start, lineCursor.start);
	} while (1);
}

buildCommandsState_t buildCommands_dispatch(buildCommands_t* buildCommands, u32 maxTasks, b32 bStopOnError, b32 bShowCommands)
{
	buildCommandsState_t state = { BB_EMPTY_INITIALIZER };
	state.commands = buildCommands;

	b32 bDone = false;
	while (!bDone)
	{
		tasks_tick();

		while (state.printed < state.commands->count)
		{
			buildCommand_t* command = buildCommands->data + state.printed;

			if (command->bDispatched && !command->bTitlePrinted)
			{
				command->bTitlePrinted = true;
				if (bShowCommands && command->command.count > 1)
				{
					BB_LOG("Dispatch", "%s", sb_get(&command->command));
				}
				else
				{
					BB_LOG("Dispatch", "%s", sb_get(&command->title));
				}
			}

			if (command->bTitlePrinted && command->bFinished && !command->bResultPrinted)
			{
				command->bResultPrinted = true;
				buildCommands_printBuffer(&command->stdoutBuffer);
				buildCommands_printBuffer(&command->stderrBuffer);
			}

			if (command->bTitlePrinted && command->bResultPrinted)
			{
				++state.printed;
			}
			else
			{
				break;
			}
		}

		if (state.active < maxTasks && state.dispatched < buildCommands->count && (!bStopOnError || !state.errorCount))
		{
			buildCommand_t* command = buildCommands->data + state.dispatched;
			command->bDispatched = true;
			task t = process_task_create("command", kProcessSpawn_Tracked, sb_get(&command->dir), "%s", sb_get(&command->command));
			t.stateChanged = task_command_statechanged;
			buildCommandLink_t* link = malloc(sizeof(buildCommandLink_t));
			link->command = command;
			link->state = &state;
			t.userData = link;
			task_queue(t);
			++state.active;
			++state.dispatched;
		}
		else if (!state.active)
		{
			if (bStopOnError && state.errorCount)
			{
				if (state.printed == state.dispatched)
				{
					bDone = true;
				}
			}
			else
			{
				if (state.printed == buildCommands->count)
				{
					bDone = true;
				}
			}
		}
	};

	return state;
}
