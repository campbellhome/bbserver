// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "crt_leak_check.h"
#include "common.h"

#if BB_USING(BB_PLATFORM_WINDOWS) && BB_USING(BB_COMPILER_MSVC) && defined(_DEBUG)
#include <crtdbg.h>
void crt_leak_check_init(void)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
}
#else
void crt_leak_check_init(void)
{
}
#endif
