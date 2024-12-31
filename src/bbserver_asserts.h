// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

void BBServer_InitAsserts(const char* logPath);
void BBServer_ShutdownAsserts(void);

#if defined(__cplusplus)
}
#endif
