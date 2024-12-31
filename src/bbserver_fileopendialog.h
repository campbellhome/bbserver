// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct sb_s sb_t;

void FileOpenDialog_Init(void);
void FileOpenDialog_Shutdown(void);
sb_t FileOpenDialog_Show(void);

#if defined(__cplusplus)
}
#endif
