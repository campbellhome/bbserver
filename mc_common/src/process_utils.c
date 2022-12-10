// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "process_utils.h"
#include "bb_array.h"
#include "bb_criticalsection.h"
#include "bb_malloc.h"
#include "bb_string.h"
#include "bb_thread.h"
#include "bb_time.h"
#include "dlist.h"
#include "sb.h"
#include "system_error_utils.h"
#include "time_utils.h"
#include "va.h"
#include <stdlib.h>

#if BB_USING(BB_PLATFORM_WINDOWS)

#include <Psapi.h>

static bb_thread_return_t process_io_thread(void *);

typedef struct win32Process_s {
	process_t base;

	struct win32Process_s *next;
	struct win32Process_s *prev;

	SYSTEMTIME startLocalTime;
	SYSTEMTIME endLocalTime;
	char startLocalTimeStr[256];
	char endLocalTimeStr[256];
	UINT64 startMS;
	u64 endMS;

	HANDLE hProcess;
	HANDLE hOutputRead;
	HANDLE hErrorRead;
	HANDLE hInputWrite;

	bb_critical_section cs;
	bb_thread_handle_t hThread;
	processIO stdoutThread;
	processIO stderrThread;
	b32 threadWanted;
	b32 threadDone;
} win32Process_t;
win32Process_t sentinelSubprocess;

bb_critical_section processListCs;

void process_init(void)
{
	bb_critical_section_init(&processListCs);
	DLIST_INIT(&sentinelSubprocess);
}

void process_shutdown(void)
{
	bb_critical_section_shutdown(&processListCs);
}

static void process_report_error(const char *apiName, const char *cmdline, processLogType_t processLogType)
{
	if(processLogType != kProcessLog_None) {
		system_error_to_log(GetLastError(), "process", va("Failed to create process (%s):\n  %s", apiName, cmdline));
	}
}

processSpawnResult_t process_spawn(const char *dir, const char *cmdline, processSpawnType_t processSpawnType, processLogType_t processLogType)
{
	processSpawnResult_t result = { BB_EMPTY_INITIALIZER };

	if(!cmdline || !*cmdline)
		return result;

	if(!dir || !*dir)
		return result;

	if(processSpawnType == kProcessSpawn_OneShot) {
		PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));

		STARTUPINFO si;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);

		char *cmdlineDup = bb_strdup(cmdline);
		result.success = CreateProcessA(NULL, cmdlineDup, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi);
		bb_free(cmdlineDup);

		if(result.success) {
			if(processLogType == kProcessLog_All) {
				BB_LOG("process", "Created process: %s\n", cmdline);
			}
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			AllowSetForegroundWindow(pi.dwProcessId);
			return result;
		} else {
			process_report_error("CreateProcess", cmdline, processLogType);
			return result;
		}
	}

	if(processSpawnType == kProcessSpawn_Tracked) {
		HANDLE hOutputReadTmp, hOutputRead, hOutputWrite;
		HANDLE hInputWriteTmp, hInputRead, hInputWrite;
		HANDLE hErrorReadTmp, hErrorRead, hErrorWrite;
		SECURITY_ATTRIBUTES sa;

		// Set up the security attributes struct.
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = TRUE;

		// Create the child stdout pipe.
		if(CreatePipe(&hOutputReadTmp, &hOutputWrite, &sa, 16 * 1024 * 1024)) {
			// Create the child stderr pipe.
			if(CreatePipe(&hErrorReadTmp, &hErrorWrite, &sa, 0)) {
				// Create the child input pipe.
				if(CreatePipe(&hInputRead, &hInputWriteTmp, &sa, 0)) {
					// Create new output read handle and the input write handles. Set
					// the Properties to FALSE. Otherwise, the child inherits the
					// properties and, as a result, non-closeable handles to the pipes
					// are created.
					if(DuplicateHandle(GetCurrentProcess(), hOutputReadTmp,
					                   GetCurrentProcess(),
					                   &hOutputRead, // Address of new handle.
					                   0, FALSE,     // Make it uninheritable.
					                   DUPLICATE_SAME_ACCESS)) {
						if(DuplicateHandle(GetCurrentProcess(), hErrorReadTmp,
						                   GetCurrentProcess(),
						                   &hErrorRead, // Address of new handle.
						                   0, FALSE,    // Make it uninheritable.
						                   DUPLICATE_SAME_ACCESS)) {
							if(DuplicateHandle(GetCurrentProcess(), hInputWriteTmp,
							                   GetCurrentProcess(),
							                   &hInputWrite, // Address of new handle.
							                   0, FALSE,     // Make it uninheritable.
							                   DUPLICATE_SAME_ACCESS)) {
								PROCESS_INFORMATION pi;
								STARTUPINFO si;

								// Close inheritable copies of the handles you do not want to be
								// inherited.
								CloseHandle(hOutputReadTmp);
								CloseHandle(hInputWriteTmp);
								CloseHandle(hErrorReadTmp);

								ZeroMemory(&si, sizeof(STARTUPINFO));
								si.cb = sizeof(STARTUPINFO);
								si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
								si.hStdOutput = hOutputWrite;
								si.hStdInput = hInputRead;
								si.hStdError = hErrorWrite;
								si.wShowWindow = SW_HIDE;

								char *cmdlineDup = bb_strdup(cmdline);
								result.success = CreateProcess(NULL, cmdlineDup, NULL, NULL, TRUE,
								                               CREATE_NEW_CONSOLE, NULL, dir, &si, &pi);
								bb_free(cmdlineDup);
								if(result.success) {
									if(processLogType == kProcessLog_All) {
										BB_LOG("process", "Created tracked process: %s\n", cmdline);
									}
									AllowSetForegroundWindow(pi.dwProcessId);
									result.process = bb_malloc(sizeof(win32Process_t));
									win32Process_t *process = (win32Process_t *)result.process;
									if(process) {
										memset(process, 0, sizeof(*process));

										process->base.command = bb_strdup(cmdline);
										process->base.dir = bb_strdup(dir);
										bb_critical_section_lock(&processListCs);
										DLIST_INSERT_AFTER(&sentinelSubprocess, process);
										bb_critical_section_unlock(&processListCs);

										process->startMS = GetTickCount64();
										GetLocalTime(&process->startLocalTime);
										GetTimeFormatA(LOCALE_USER_DEFAULT, 0, &process->startLocalTime, "h':'mm':'ss tt",
										               process->startLocalTimeStr, sizeof(process->startLocalTimeStr));

										process->hProcess = pi.hProcess;
										process->hOutputRead = hOutputRead;
										process->hInputWrite = hInputWrite;
										process->hErrorRead = hErrorRead;

										bb_critical_section_init(&process->cs);
										process->hThread = bbthread_create(process_io_thread, process);
										process->threadWanted = true;
									}
								} else {
									process_report_error("CreateProcess", cmdline, processLogType);
								}

								// Close any unnecessary handles.
								CloseHandle(pi.hThread);

								// Close pipe handles (do not continue to modify the parent).
								// You need to make sure that no handles to the write end of the
								// output pipe are maintained in this process or else the pipe will
								// not close when the child process exits and the ReadFile will hang.
								CloseHandle(hOutputWrite);
								CloseHandle(hInputRead);
								CloseHandle(hErrorWrite);
							} else {
								process_report_error("DuplicateHandle", cmdline, processLogType);
							}
						} else {
							process_report_error("DuplicateHandle", cmdline, processLogType);
						}
					} else {
						process_report_error("DuplicateHandle", cmdline, processLogType);
					}
				} else {
					process_report_error("CreatePipe", cmdline, processLogType);
				}
			} else {
				process_report_error("CreatePipe", cmdline, processLogType);
			}
		} else {
			process_report_error("CreatePipe", cmdline, processLogType);
		}
	}

	return result;
}

static b32 process_thread_tick_io_buffer(win32Process_t *process, HANDLE handle, processIO *io)
{
	DWORD nBytesAvailable = 0;
	BOOL bAnyBytesAvailable = PeekNamedPipe(handle, NULL, 0, NULL, &nBytesAvailable, NULL);
	if(bAnyBytesAvailable) {
		if(nBytesAvailable) {
			bb_critical_section_lock(&process->cs);
			if(bba_reserve(*io, nBytesAvailable)) {
				char *buffer = io->data + io->count;
				DWORD nBytesRead = 0;
				BOOL ok = ReadFile(handle, buffer, nBytesAvailable, &nBytesRead, NULL);
				if(ok) {
					if(nBytesRead) {
						io->count += nBytesRead;
						bb_critical_section_unlock(&process->cs);
						return true;
					} else {
						process->threadDone = true;
					}
				} else {
					DWORD err = GetLastError();
					if(err == ERROR_BROKEN_PIPE) {
						process->threadDone = true;
					}
				}
			}
			bb_critical_section_unlock(&process->cs);
		}
	} else {
		DWORD err = GetLastError();
		if(err == ERROR_BROKEN_PIPE) {
			process->threadDone = true;
		}
	}
	return false;
}

static bb_thread_return_t process_io_thread(void *_process)
{
	u32 delayCur = 16;
	u32 delayMin = 1;
	u32 delayIncrement = 1;
	u32 delayMax = 16;

	win32Process_t *process = _process;
	while(process->threadWanted) {
		b32 readData = process_thread_tick_io_buffer(process, process->hOutputRead, &process->stdoutThread);
		if(readData) {
			delayCur = delayMin;
		} else {
			readData = readData || process_thread_tick_io_buffer(process, process->hErrorRead, &process->stderrThread);
			if(!readData) {
				delayCur = BB_MIN(delayCur + delayIncrement, delayMax);
				bb_sleep_ms(delayCur);
			}
		}
	}
	process->threadDone = true;
	return 0;
}

static void process_tick_io_buffer(win32Process_t *process, processIOPtr *ioOut, processIO *ioMain, processIO *ioThread)
{
	if(ioThread->count) {
		bb_critical_section_lock(&process->cs);

		if(bba_reserve(*ioMain, ioMain->count + ioThread->count + 1)) {
			memcpy(ioMain->data + ioMain->count, ioThread->data, ioThread->count);
			ioOut->buffer = ioMain->data + ioMain->count;
			ioOut->nBytes = ioThread->count;
			ioMain->count += ioThread->count;
			ioMain->data[ioMain->count] = '\0';
			ioThread->count = 0;
		}

		bb_critical_section_unlock(&process->cs);
	}
}

processTickResult_t process_tick(process_t *base)
{
	processTickResult_t result = { 0, 0, 0 };
	win32Process_t *process = (win32Process_t *)(base);
	b32 wasDone = process->base.done;
	if(process->threadDone) {
		process->base.done = true;
	}

	process_tick_io_buffer(process, &result.stdoutIO, &process->base.stdoutBuffer, &process->stdoutThread);
	process_tick_io_buffer(process, &result.stderrIO, &process->base.stderrBuffer, &process->stderrThread);

	if(process->base.done && !wasDone) {
		process->endMS = GetTickCount64();
		GetLocalTime(&process->endLocalTime);
		GetTimeFormatA(LOCALE_USER_DEFAULT, 0, &process->endLocalTime, "h':'mm':'ss tt",
		               process->endLocalTimeStr, sizeof(process->endLocalTimeStr));
	}

	result.done = process->base.done;
	if(result.done) {
		DWORD exitCode = ~0u;
		GetExitCodeProcess(process->hProcess, &exitCode);
		result.exitCode = exitCode;
		base->exitCode = exitCode;
	}
	return result;
}

void process_free(process_t *base)
{
	win32Process_t *process = (win32Process_t *)(base);

	if(process->hThread) {
		process->threadWanted = false;
		bbthread_join(process->hThread);
	}

	bb_critical_section_shutdown(&process->cs);

	bba_free(process->stdoutThread);
	bba_free(process->stderrThread);
	bba_free(process->base.stdoutBuffer);
	bba_free(process->base.stderrBuffer);

	CloseHandle(process->hOutputRead);
	CloseHandle(process->hErrorRead);
	CloseHandle(process->hInputWrite);
	CloseHandle(process->hProcess);

	bb_critical_section_lock(&processListCs);
	DLIST_REMOVE(process);
	bb_critical_section_unlock(&processListCs);
	bb_free(process->base.command);
	bb_free(process->base.dir);
	bb_free(process);
}

void process_get_timings(process_t *base, const char **start, const char **end, u64 *elapsed)
{
	win32Process_t *process = (win32Process_t *)(base);
	*start = process->startLocalTimeStr;
	*end = process->endLocalTimeStr;
	*elapsed = (process->base.done) ? process->endMS - process->startMS : GetTickCount64() - process->startMS;
}

typedef struct tag_processIds {
	u32 count;
	u32 allocated;
	DWORD *data;
} processIds;
static u32 s_numProcessesNeeded = 16 * 1024;

b32 process_is_running(const char *processName)
{
	b32 ret = false;
	processIds ids = { BB_EMPTY_INITIALIZER };
	bba_add(ids, s_numProcessesNeeded);
	if(ids.data) {
		DWORD selfProcessId = GetCurrentProcessId();
		DWORD bytesUsed = 0;
		u32 bytesNeeded = ids.count * sizeof(DWORD);
		if(EnumProcesses(ids.data, bytesNeeded, &bytesUsed)) {
			char moduleName[_MAX_PATH];
			for(u32 i = 0; i < bytesUsed / sizeof(DWORD); ++i) {
				HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, ids.data[i]);
				if(hProcess) {
					DWORD processId = GetProcessId(hProcess);
					if(processId != selfProcessId) {
						HMODULE hModule;
						DWORD unused;
						if(EnumProcessModules(hProcess, &hModule, sizeof(hModule), &unused)) {
							if(GetModuleBaseNameA(hProcess, hModule, moduleName, sizeof(moduleName))) {
								if(!bb_stricmp(processName, moduleName)) {
									CloseHandle(hProcess);
									ret = true;
									break;
								}
							}
						}
					}
					CloseHandle(hProcess);
				}
			}
		}
		bba_free(ids);
	}
	return ret;
}

b32 process_terminate(const char *processName, u32 exitCode)
{
	b32 ret = false;
	processIds ids = { BB_EMPTY_INITIALIZER };
	bba_add(ids, s_numProcessesNeeded);
	if(ids.data) {
		DWORD selfProcessId = GetCurrentProcessId();
		DWORD bytesUsed = 0;
		u32 bytesNeeded = ids.count * sizeof(DWORD);
		if(EnumProcesses(ids.data, bytesNeeded, &bytesUsed)) {
			char moduleName[_MAX_PATH];
			for(u32 i = 0; i < bytesUsed / sizeof(DWORD); ++i) {
				HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, ids.data[i]);
				if(hProcess) {
					DWORD processId = GetProcessId(hProcess);
					if(processId != selfProcessId) {
						HMODULE hModule;
						DWORD unused;
						if(EnumProcessModules(hProcess, &hModule, sizeof(hModule), &unused)) {
							if(GetModuleBaseNameA(hProcess, hModule, moduleName, sizeof(moduleName))) {
								if(!bb_stricmp(processName, moduleName)) {
									TerminateProcess(hProcess, exitCode);
									CloseHandle(hProcess);
									ret = true;
									break;
								}
							}
						}
					}
					CloseHandle(hProcess);
				}
			}
		}
		bba_free(ids);
	}
	return ret;
}

#else // #if BB_USING(BB_PLATFORM_WINDOWS)

void process_init(void)
{
}

processSpawnResult_t process_spawn(const char *dir, const char *cmdline, processSpawnType_t processSpawnType, processLogType_t processLogType)
{
	processSpawnResult_t result = { BB_EMPTY_INITIALIZER };
	BB_UNUSED(dir);
	BB_UNUSED(cmdline);
	BB_UNUSED(processSpawnType);
	BB_UNUSED(processLogType);
	return result;
}

processTickResult_t process_tick(process_t *process)
{
	processTickResult_t result = { BB_EMPTY_INITIALIZER };
	BB_UNUSED(process);
	return result;
}

void process_free(process_t *process)
{
	BB_UNUSED(process);
}

void process_get_timings(process_t *process, const char **start, const char **end, u64 *elapsed)
{
	BB_UNUSED(process);
	BB_UNUSED(start);
	BB_UNUSED(end);
	BB_UNUSED(elapsed);
}

b32 process_is_running(const char *processName)
{
	BB_UNUSED(processName);
	return false;
}

b32 process_terminate(const char *processName, u32 exitCode)
{
	BB_UNUSED(processName);
	BB_UNUSED(exitCode);
	return false;
}

#endif // #else // #if BB_USING(BB_PLATFORM_WINDOWS)

processTickResult_t process_await(process_t *process)
{
	processTickResult_t result = { BB_EMPTY_INITIALIZER };
	if(process) {
		while(!process->done) {
			result = process_tick(process);
			if(!result.done) {
				bb_sleep_ms(10);
			}
		}
	}
	return result;
}
