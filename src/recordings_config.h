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
#include "bb_common.h"

typedef struct recordings_config_s recordings_config_t;
b32 recordings_config_read(recordings_config_t* config);
b32 recordings_config_write(recordings_config_t* config);

#if defined(__cplusplus)
}
#endif
