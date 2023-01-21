// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "bb.h"

#if BB_ENABLED

#include "bb_assert.h"
#include "bb_log.h"
#include "bb_thread.h"

#if BB_USING(BB_COMPILER_MSVC)

#include "bb_wrap_process.h"
#include "bb_wrap_windows.h"
#include <stdlib.h>

bb_thread_handle_t bbthread_create(bb_thread_func func, void* arg)
{
	bb_thread_handle_t thread = _beginthreadex(
	    NULL, // security,
	    0,    // stack_size,
	    func, // start_address
	    arg,  // arglist
	    0,    // initflag - CREATE_SUSPENDED waits for ResumeThread
	    NULL  // thrdaddr
	);
	BB_ASSERT_MSG(thread != 0, "failed to spawn thread");
	return thread;
}

void bbthread_join(bb_thread_handle_t thread)
{
	WaitForSingleObject((HANDLE)thread, INFINITE);
	CloseHandle((HANDLE)thread);
}

bb_thread_handle_t bbthread_get_current(void)
{
	return (bb_thread_handle_t)GetCurrentThread();
}

u64 bbthread_get_current_id(void)
{
	return GetCurrentThreadId();
}

static HRESULT SetThreadDescriptionShim(_In_ HANDLE hThread, _In_ PCWSTR lpThreadDescription)
{
	HMODULE hModule = GetModuleHandleA("kernel32.dll");
	if (hModule)
	{
		typedef HRESULT(WINAPI * Proc)(_In_ HANDLE hThread, _In_ PCWSTR lpThreadDescription);
		Proc proc = (Proc)(void*)(GetProcAddress(hModule, "SetThreadDescription"));
		if (proc)
		{
			return proc(hThread, lpThreadDescription);
		}
	}
	return STG_E_UNIMPLEMENTEDFUNCTION;
}

void bbthread_set_name(const char* name)
{
	size_t numCharsConverted;
	wchar_t buffer[kBBSize_ThreadName] = L"";
	mbstowcs_s(&numCharsConverted, buffer, kBBSize_ThreadName, name, _TRUNCATE);
	SetThreadDescriptionShim(GetCurrentThread(), buffer);
}

#else // #if BB_USING(BB_COMPILER_MSVC)

#if BB_USING(BB_PLATFORM_LINUX)
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#endif // #if BB_USING(BB_PLATFORM_LINUX)

bb_thread_handle_t bbthread_create(bb_thread_func func, void* arg)
{
	bb_thread_handle_t thread = 0;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_create(
	    &thread, // thread
	    &attr,   // attr
	    func,    // start_routine
	    arg      // arg
	);
	return thread;
}

void bbthread_join(bb_thread_handle_t thread)
{
	pthread_join(thread, NULL);
}

bb_thread_handle_t bbthread_get_current(void)
{
	return (bb_thread_handle_t)pthread_self();
}

u64 bbthread_get_current_id(void)
{
#if BB_USING(BB_PLATFORM_LINUX)
	return syscall(SYS_gettid);
#else  // #if BB_USING(BB_PLATFORM_LINUX)
	return (u64)pthread_self();
#endif // #else // #if BB_USING(BB_PLATFORM_LINUX)
}

void bbthread_set_name(const char* name)
{
	BB_UNUSED(name);
	bb_log("implement me: bbthread_set_name");
}

#endif // #else // #if BB_USING(BB_COMPILER_MSVC)

#endif // #if BB_ENABLED
