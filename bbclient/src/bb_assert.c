// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#if !defined(BB_ENABLED) || BB_ENABLED

#include "bb.h"

#include "bbclient/bb_assert.h"
#include "bbclient/bb_common.h"
#include "bbclient/bb_string.h"

#if BB_USING(BB_ASSERTS)

#include "bbclient/bb_wrap_stdio.h"

#if BB_USING(BB_PLATFORM_WINDOWS)
BB_WARNING_DISABLE(4710) // snprintf not inlined - can't push/pop because it happens later
#endif                   // #if BB_USING( BB_PLATFORM_WINDOWS )

static bbassert_action_e default_assert_handler(const char *condition, const char *message, const char *file, const int line);
static bbassert_handler s_assert_func = &default_assert_handler;
static b32 s_break_on_assert = true;

static bbassert_action_e default_assert_handler(const char *condition, const char *message, const char *file, const int line)
{
// static buffers so we can safely assert in a memory allocator :)
#if BB_USING(BB_COMPILER_MSVC)
	static __declspec(thread) char output[1024];
#else
	static __thread char output[1024];
#endif
	int len;
	if(message && *message) {
		len = snprintf(output, sizeof(output), "%s(%d): Assert Failure: '%s': %s\n", file, line, condition, message);
	} else {
		len = snprintf(output, sizeof(output), "%s(%d): Assert Failure: '%s'\n", file, line, condition);
	}
	len = (len > 0 && len < (int)sizeof(output)) ? len : (int)sizeof(output) - 1;
	output[len] = '\0';

#if BB_USING(BB_PLATFORM_WINDOWS)
	OutputDebugStringA(output);
#else
	printf("%s", output);
#endif // #if BB_USING( BB_PLATFORM_WINDOWS )

	if(message && *message) {
		BB_ERROR_A("Assert", "Assert Failure: '%s': %s", condition, message);
	} else {
		BB_ERROR_A("Assert", "Assert Failure: '%s'", condition);
	}

	return (s_break_on_assert) ? kBBAssertAction_Break : kBBAssertAction_Continue;
}

bbassert_handler bbassert_get_handler(void)
{
	return s_assert_func;
}

void bbassert_set_handler(bbassert_handler func)
{
	s_assert_func = func;
}

bbassert_action_e bbassert_dispatch(const char *condition, const char *file, int line, const char *fmt, ...)
{
	if(s_assert_func) {
		static char message[1024];
		message[0] = 0;

		if(fmt != NULL) {
			va_list args;
			va_start(args, fmt);
			vsnprintf(message, 1024, fmt, args);
			va_end(args);
		}

		return s_assert_func(condition, message, file, line);
	}

	return kBBAssertAction_Continue;
}

#endif // #if BB_USING( BB_ASSERTS )

#endif // #if BB_ENABLED
