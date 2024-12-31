// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct recorded_log_s recorded_log_t;
typedef struct view_s view_t;

b32 view_filter_visible_legacy(view_t* view, recorded_log_t* log);

#if defined(__cplusplus)
}
#endif
