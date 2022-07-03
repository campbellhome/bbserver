// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif
#include "bb_common.h"
#if defined(__cplusplus)
}
#endif

#define AUTOJSON
#define AUTOHEADERONLY
#define AUTODEFAULT(val)
#define AUTOVALIDATE
#define AUTOFROMLOC
#define AUTOSTRUCT
#define AUTOSTRINGHASH

#if BB_USING(BB_PLATFORM_WINDOWS)

// can't push/pop because these happen after initial compilation phase:
BB_WARNING_DISABLE(4514) // unreferenced inline function has been removed
BB_WARNING_DISABLE(4710) // snprintf not inlined

#endif // #if BB_USING( BB_PLATFORM_WINDOWS )
