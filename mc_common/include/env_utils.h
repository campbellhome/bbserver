// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

sb_t env_get(const char *name);    // usage: env_get("TEMP");
sb_t env_resolve(const char *src); // usage: env_resolve("%TEMP%")

#if defined(__cplusplus)
}
#endif
