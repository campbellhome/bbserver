// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

sb_t system_error_to_sb_from_loc(const char *file, int line, u32 err);
void system_error_to_log_from_loc(const char *file, int line, u32 err, const char *category, const char *text);

#define system_error_to_sb(err) system_error_to_sb_from_loc(__FILE__, __LINE__, err)
#define system_error_to_log(err, category, text) system_error_to_log_from_loc(__FILE__, __LINE__, err, category, text);

#if defined(__cplusplus)
}
#endif
