// Copyright (c) 2012-2023 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_defines.h"
#include "bb_types.h"

#if BB_USING(BB_PLATFORM_WINDOWS) && _MSC_VER <= 1800 && !defined(__cplusplus)
#define BB_INLINE
#else
#define BB_INLINE inline
#endif

#if BB_USING(BB_PLATFORM_WINDOWS) && _MSC_VER <= 1800
#define bb_snprintf _snprintf
#else
#define bb_snprintf snprintf
#endif

#if BB_USING(BB_COMPILER_MSVC)

// warning C4711 : function 'bbpacket_serialize_user' selected for automatic inline expansion
// warning C5045: Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
BB_WARNING_DISABLE(4711 5045)

// MSVC C does not allow empty = {} initialization in legacy mode, C11, or C17
#if defined(__cplusplus)
#define BB_EMPTY_INITIALIZER
#else
#define BB_EMPTY_INITIALIZER 0
#endif

#else // #if BB_USING( BB_COMPILER_MSVC )

#define BB_EMPTY_INITIALIZER

#endif // #else // #if BB_USING( BB_COMPILER_MSVC )

#endif // #if BB_ENABLED
