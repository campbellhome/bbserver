// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

void bb_log_init(void);
void bb_log_shutdown(void);

void bb_log(const char *fmt, ...);
void bb_warning(const char *fmt, ...);
void bb_error(const char *fmt, ...);

char *bb_format_ipport(char *buf, size_t len, u32 addr, u16 port);
char *bb_format_ip(char *buf, size_t len, u32 addr);

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
