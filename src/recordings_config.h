// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(_MSC_VER)
__pragma(warning(disable : 4710)); // warning C4710 : 'int printf(const char *const ,...)' : function not inlined
#endif                             // #if defined( _MSC_VER )

#if defined(__cplusplus)
extern "C" {
#endif

#include "bb.h"
#include "bbclient/bb_common.h"

typedef struct recordings_s recordings_t;
b32 recordings_read_config(recordings_t *recordings);
b32 recordings_write_config(recordings_t *recordings);

#if defined(__cplusplus)
}
#endif
