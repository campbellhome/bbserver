// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

b32 SystemTray_Init(void* hInstance);
void SystemTray_Shutdown(void);

#if defined(__cplusplus)
}
#endif
