// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "mc_callstack/exception_handler.h"
#include "bb_defines.h"
#include "common.h"

#if BB_USING(BB_PLATFORM_WINDOWS)

#include "bb_wrap_windows.h"

static b32 s_unhandledExceptionHandlerInstalled;
static exception_handler_callback* s_callback;

// see http://blog.kalmbach-software.de/2013/05/23/improvedpreventsetunhandledexceptionfilter/
static b32 PreventSetUnhandledExceptionFilter(void)
{
	b32 bRet = false;
#if _M_X64
	BB_WARNING_PUSH(4152);
	void* pOrgEntry = &SetUnhandledExceptionFilter;
	BB_WARNING_POP;

	// 33 C0                xor         eax,eax
	// C3                   ret
	unsigned char szExecute[] = { 0x33, 0xC0, 0xC3 };

	SIZE_T bytesWritten = 0;
	bRet = WriteProcessMemory(GetCurrentProcess(), pOrgEntry, szExecute, sizeof(szExecute), &bytesWritten);
#endif
	return bRet;
}

static LONG __stdcall unhandled_exception_handler(EXCEPTION_POINTERS* pExPtrs)
{
	if (s_callback)
	{
		(*s_callback)(pExPtrs);
	}
	else
	{
		const char* errorStr = "Unhandled exception!  Calling FatalAppExit...";
		FatalAppExit(~0u, errorStr);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

void install_unhandled_exception_handler(exception_handler_callback* callback)
{
	s_callback = callback;

	if (s_unhandledExceptionHandlerInstalled)
		return;

	SetUnhandledExceptionFilter(unhandled_exception_handler);
#if defined _M_X64 || defined _M_IX86
	PreventSetUnhandledExceptionFilter();
#endif
	s_unhandledExceptionHandlerInstalled = true;
}

#endif // #if BB_USING(BB_PLATFORM_WINDOWS)
