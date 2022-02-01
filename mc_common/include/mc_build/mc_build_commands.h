// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

AUTOSTRUCT AUTOFROMLOC typedef struct buildCommand_s {
	sb_t title;
	sb_t dir;
	sb_t command;
	sb_t stdoutBuffer;
	sb_t stderrBuffer;
	u32 result;
	b8 bDispatched;
	b8 bFinished;
	b8 bTitlePrinted;
	b8 bResultPrinted;
} buildCommand_t;

AUTOSTRUCT AUTOFROMLOC typedef struct buildCommands_s {
	u32 count;
	u32 allocated;
	buildCommand_t *data;
} buildCommands_t;

typedef struct buildCommandsState_s {
	u32 dispatched;
	u32 active;
	u32 errorCount;
	u32 printed;
	buildCommands_t *commands;
} buildCommandsState_t;

void buildCommands_push(buildCommands_t *buildCommands, const char *title, const char *dir, const char *command);
buildCommandsState_t buildCommands_dispatch(buildCommands_t *buildCommands, u32 maxTasks, b32 bStopOnError, b32 bShowCommands);

#if defined(__cplusplus)
} // extern "C"
#endif
