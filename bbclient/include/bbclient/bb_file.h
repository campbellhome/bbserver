// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb.h"

#if BB_ENABLED

#include "bb_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef void* bb_file_handle_t;

#if BB_USING(BB_COMPILER_MSVC)
#define BB_INVALID_FILE_HANDLE ((bb_file_handle_t)~0)
#else
#define BB_INVALID_FILE_HANDLE ((bb_file_handle_t)0)
#endif

bb_file_handle_t bb_file_open_for_write(const char* pathname);
bb_file_handle_t bb_file_open_for_read(const char* pathname);
b32 bb_file_readable(const char* pathname);
u32 bb_file_write(bb_file_handle_t handle, void* data, u32 dataLen);
u32 bb_file_read(bb_file_handle_t handle, void* buffer, u32 bufferSize);
u32 bb_file_size(bb_file_handle_t handle);
void bb_file_close(bb_file_handle_t handle);
void bb_file_flush(bb_file_handle_t handle);

#if defined(__cplusplus)
}
#endif

#endif // #if BB_ENABLED
