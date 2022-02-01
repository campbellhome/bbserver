// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

char *va(const char *fmt, ...);
void va_buffer(char **buffer, size_t *capacity);

#if defined(__cplusplus)
}
#endif
