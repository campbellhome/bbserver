// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_defines.h"
#if defined(_MSC_VER) && _MSC_VER < 1900
#define PRIu64 "I64u"
#define PRIx64 "I64x"
#define PRIX64 "I64X"
#else
#include <inttypes.h> // for PRIu64 etc
#endif
#include <stddef.h> // for size_t
#include <stdint.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef s32 b32;
typedef s8 b8;
#ifndef __cplusplus
#define true 1
#define false 0
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

#endif // #if BB_ENABLED
