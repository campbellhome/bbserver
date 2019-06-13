// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bbclient/bb_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

b32 SystemTray_Init(void *hInstance);
void SystemTray_Shutdown(void);

#if defined(__cplusplus)
}
#endif
