// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

u64 bb_current_ticks(void);
double bb_millis_per_tick(void);
u64 bb_current_time_ms(void);
u64 bb_current_time_microseconds_from_epoch(void);
void bb_sleep_ms(u32 millis);

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
