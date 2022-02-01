// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

void callstack_init(b32 bSymbolServer);
void callstack_shutdown(void);
sb_t callstack_generate_sb(int linesToSkip);
sbs_t callstack_generate_sbs(int linesToSkip);
sb_t callstack_generate_crash_sb(void);

#if defined(__cplusplus)
}
#endif
