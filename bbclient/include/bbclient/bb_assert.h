// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_defines.h"
#include "bb_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Assert concepts taken from the Power of Two LLC assert handler, as described at http://cnicholson.net/2009/02/stupid-c-tricks-adventures-in-assert/

#if BB_USING(BB_ASSERTS)

typedef enum
{
	kBBAssertAction_Break,
	kBBAssertAction_Continue,
} bbassert_action_e;

typedef bbassert_action_e (*bbassert_handler)(const char* condition, const char* message, const char* file, int line);

bbassert_handler bbassert_get_handler(void);
void bbassert_set_handler(bbassert_handler func);

bbassert_action_e bbassert_dispatch(const char* condition, const char* file, int line, const char* fmt, ...);

#if BB_USING(BB_PLATFORM_WINDOWS)
b32 bbassert_is_debugger_present(void);
#define BB_BREAK()                          \
	{                                       \
		if (bbassert_is_debugger_present()) \
			__debugbreak();                 \
	}
#else // #if BB_USING( BB_PLATFORM_WINDOWS )
#define BB_BREAK() \
	{              \
	}
#endif // #else // #if BB_USING( BB_PLATFORM_WINDOWS )

#define BB_ASSERT(condition)                                            \
	BB_MULTI_LINE_MACRO_BEGIN                                           \
	BB_WARNING_PUSH_CONSTANT_EXPR                                       \
	if (!(condition))                                                   \
		BB_WARNING_POP                                                  \
		{                                                               \
			if (bbassert_dispatch(#condition, __FILE__, __LINE__, 0) == \
			    kBBAssertAction_Break)                                  \
			{                                                           \
				BB_BREAK();                                             \
			}                                                           \
		}                                                               \
	BB_MULTI_LINE_MACRO_END

#define BB_ASSERT_MSG(condition, msg, ...)                                               \
	BB_MULTI_LINE_MACRO_BEGIN                                                            \
	BB_WARNING_PUSH_CONSTANT_EXPR                                                        \
	if (!(condition))                                                                    \
		BB_WARNING_POP                                                                   \
		{                                                                                \
			if (bbassert_dispatch(#condition, __FILE__, __LINE__, (msg), __VA_ARGS__) == \
			    kBBAssertAction_Break)                                                   \
			{                                                                            \
				BB_BREAK();                                                              \
			}                                                                            \
		}                                                                                \
	BB_MULTI_LINE_MACRO_END

#else // #if USING( BB_ASSERTS )

#define BB_ASSERT(condition)  \
	BB_MULTI_LINE_MACRO_BEGIN \
	BB_UNUSED(condition);     \
	BB_MULTI_LINE_MACRO_END

#define BB_ASSERT_MSG(condition, msg, ...) \
	BB_MULTI_LINE_MACRO_BEGIN              \
	BB_UNUSED(condition);                  \
	BB_UNUSED(msg);                        \
	BB_MULTI_LINE_MACRO_END

#endif // #else // #if BB_USING( BB_ASSERTS )

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
