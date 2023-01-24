// Copyright (c) 2012-2023 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#if defined(__cplusplus)
extern "C" {
#endif

// removed this because it breaks static analysis O.o
// instead, we manually implement strlen and memcpy inside bb_strncpy :P
#if !defined(WIN32)
#include <string.h> // strlen, memcpy, size_t
#endif

BB_LINKAGE size_t bb_strncpy(char* dest, const char* src, size_t destSize);
BB_LINKAGE char bb_tolower(char c);
BB_LINKAGE const char* bb_stristr(const char* src, const char* pattern);
BB_LINKAGE int bb_strnicmp(const char* s1, const char* s2, size_t len);
BB_LINKAGE int bb_stricmp(const char* s1, const char* s2);
BB_LINKAGE char* bb_strdup_loc(const char* file, int line, const char* s);
#define bb_strdup(x) bb_strdup_loc(__FILE__, __LINE__, (x))

#if BB_COMPILE_WIDECHAR
BB_LINKAGE size_t bb_wstrncpy(bb_wchar_t* dest, const bb_wchar_t* src, size_t destSize);
#endif // #if BB_COMPILE_WIDECHAR

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
