// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

void DragDrop_Init(void* hwnd);
void DragDrop_Shutdown(void);
s32 DragDrop_OnDropFiles(u64 wparam);
void DragDrop_ProcessPath(const char* path);

#if defined(__cplusplus)
}
#endif
