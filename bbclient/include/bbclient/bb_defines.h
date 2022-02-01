// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#define BB_ON +
#define BB_OFF -
#define BB_USING(x) (1 x 1 == 2)

//
// Platform
//

#define BB_PLATFORM_WINDOWS BB_OFF
#define BB_PLATFORM_LINUX BB_OFF
#define BB_PLATFORM_ANDROID BB_OFF
#define BB_PLATFORM_ORBIS BB_OFF
#define BB_PLATFORM_DURANGO BB_OFF
#define BB_PLATFORM_NX BB_OFF
#define BB_PLATFORM_PROSPERO BB_OFF
#define BB_PLATFORM_SCARLETT BB_OFF

#if defined(BB_FORCE_PLATFORM_WINDOWS) && BB_FORCE_PLATFORM_WINDOWS
#undef BB_PLATFORM_WINDOWS
#define BB_PLATFORM_WINDOWS BB_ON
#elif defined(BB_FORCE_PLATFORM_LINUX) && BB_FORCE_PLATFORM_LINUX
#undef BB_PLATFORM_LINUX
#define BB_PLATFORM_LINUX BB_ON
#elif defined(BB_FORCE_PLATFORM_ANDROID) && BB_FORCE_PLATFORM_ANDROID
#undef BB_PLATFORM_ANDROID
#define BB_PLATFORM_ANDROID BB_ON
#elif defined(BB_FORCE_PLATFORM_ORBIS) && BB_FORCE_PLATFORM_ORBIS
#undef BB_PLATFORM_ORBIS
#define BB_PLATFORM_ORBIS BB_ON
#elif defined(BB_FORCE_PLATFORM_DURANGO) && BB_FORCE_PLATFORM_DURANGO
#undef BB_PLATFORM_DURANGO
#define BB_PLATFORM_DURANGO BB_ON
#elif defined(BB_FORCE_PLATFORM_NX) && BB_FORCE_PLATFORM_NX
#undef BB_PLATFORM_NX
#define BB_PLATFORM_NX BB_ON
#elif defined(BB_FORCE_PLATFORM_PROSPERO) && BB_FORCE_PLATFORM_PROSPERO
#undef BB_PLATFORM_PROSPERO
#define BB_PLATFORM_PROSPERO BB_ON
#elif defined(BB_FORCE_PLATFORM_SCARLETT) && BB_FORCE_PLATFORM_SCARLETT
#undef BB_PLATFORM_SCARLETT
#define BB_PLATFORM_SCARLETT BB_ON
#elif defined(_MSC_VER) && _MSC_VER
#undef BB_PLATFORM_WINDOWS
#define BB_PLATFORM_WINDOWS BB_ON
#else // #if defined(_MSC_VER) && _MSC_VER
#undef BB_PLATFORM_LINUX
#define BB_PLATFORM_LINUX BB_ON
#endif // #else // #if defined(_MSC_VER) && _MSC_VER

#if BB_USING(BB_PLATFORM_WINDOWS) || BB_USING(BB_PLATFORM_DURANGO) || BB_USING(BB_PLATFORM_SCARLETT)
#define BB_COMPILER_MSVC BB_ON
#define BB_COMPILER_CLANG BB_OFF
#else // #if BB_USING(BB_PLATFORM_WINDOWS) || BB_USING(BB_PLATFORM_DURANGO) || BB_USING(BB_PLATFORM_SCARLETT)
#define BB_COMPILER_MSVC BB_OFF
#define BB_COMPILER_CLANG BB_ON
#endif // #else // #if BB_USING(BB_PLATFORM_WINDOWS) || BB_USING(BB_PLATFORM_DURANGO) || BB_USING(BB_PLATFORM_SCARLETT)

//
// Feature Toggles
//

#define BB_ASSERTS BB_ON

//
// Miscellaneous
//

#define BB_MIN(x, y) (((x) < (y)) ? (x) : (y))
#define BB_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define BB_CLAMP(x, xmin, xmax) ((x) < (xmin) ? (xmin) : (x) > (xmax) ? (xmax) : (x))
#define BB_CLAMP01(x) ((x) < 0.0f ? 0.0f : (x) > 1.0f ? 1.0f : (x))
#define BB_ARRAYSIZE(x) (sizeof(x) / sizeof(0 [x]))

#if BB_USING(BB_PLATFORM_WINDOWS)

#define BB_CTASSERT(x) static_assert((x), #x)

// MULTI_LINE_MACRO_BEGIN and __pragma logic taken from http://cnicholson.net/2009/03/stupid-c-tricks-dowhile0-and-c4127/
// clang-format off
#define BB_WARNING_PUSH( x, ... )                  \
	__pragma( warning( push ) )                    \
	__pragma( warning( disable : x __VA_ARGS__ ) )
// clang-format on

#define BB_WARNING_DISABLE(x) \
	__pragma(warning(disable  \
	                 : x))

#define BB_WARNING_PUSH_CONSTANT_EXPR BB_WARNING_PUSH(4127)

#define BB_WARNING_POP __pragma(warning(pop))

#else // #if USING( BB_PLATFORM_WINDOWS )

#define BB_CTASSERT(x) typedef char __BB_CT_ASSERT__[(x) ? 1 : -1]
#define BB_WARNING_PUSH(x)
#define BB_WARNING_DISABLE(x)
#define BB_WARNING_PUSH_CONSTANT_EXPR
#define BB_WARNING_POP

#endif // #else // #if USING( BB_PLATFORM_WINDOWS )

#define BB_MULTI_LINE_MACRO_BEGIN \
	do {

#define BB_MULTI_LINE_MACRO_END    \
	BB_WARNING_PUSH_CONSTANT_EXPR; \
	}                              \
	while(0)                       \
	BB_WARNING_POP

#define BB_UNUSED(x)           \
	BB_MULTI_LINE_MACRO_BEGIN; \
	(void)(x);                 \
	BB_MULTI_LINE_MACRO_END

#endif // #if BB_ENABLED
