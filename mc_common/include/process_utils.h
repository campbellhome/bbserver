// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

void process_init(void);
void process_shutdown(void);

typedef struct tag_processIO
{
	u32 count;
	u32 allocated;
	char* data;
} processIO;

typedef struct process_s
{
	char* command;
	char* dir;

	processIO stdoutBuffer;
	processIO stderrBuffer;
	b32 done;
	u32 exitCode;
} process_t;

typedef struct processSpawnResult_s
{
	process_t* process;
	b32 success;
	u8 pad[4];
} processSpawnResult_t;

typedef enum processSpawnType_e
{
	kProcessSpawn_OneShot,
	kProcessSpawn_Tracked,
} processSpawnType_t;

typedef enum processLogType_e
{
	kProcessLog_None,
	kProcessLog_Errors,
	kProcessLog_All
} processLogType_t;

processSpawnResult_t process_spawn(const char* dir, const char* cmdline, processSpawnType_t processSpawnType, processLogType_t processLogType);

typedef struct tag_processIOPtr
{
	const char* buffer;
	u32 nBytes;
	u8 pad[4];
} processIOPtr;

typedef struct processTickResult_s
{
	processIOPtr stdoutIO;
	processIOPtr stderrIO;
	b32 done;
	u32 exitCode;
} processTickResult_t;

processTickResult_t process_tick(process_t* process);
processTickResult_t process_await(process_t* process);
void process_free(process_t* process);
void process_get_timings(process_t* process, const char** start, const char** end, u64* elapsed);
b32 process_is_running(const char* processName);
b32 process_terminate(const char* processName, u32 exitCode);

#if defined(__cplusplus)
}
#endif
