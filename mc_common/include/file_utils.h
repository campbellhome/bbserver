// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

// sigh, VC 2019 16.4 throws C4668 in vcruntime.h, included from stdint.h so we include bb.h to get the warning suppression
#include "bb.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct fileData_s
{
	void* buffer;
	uint32_t bufferSize;
	uint8_t pad[4];
} fileData_t;

typedef enum fileData_writeResult
{
	kFileData_Error,
	kFileData_Success,
	kFileData_Unmodified
} fileData_writeResult;

void fileData_reset(fileData_t* result);
fileData_t fileData_read(const char* filename);
int fileData_write(const char* pathname, const char* tempPathname, fileData_t data);
fileData_writeResult fileData_writeIfChanged(const char* pathname, const char* tempPathname, fileData_t data);
int file_delete(const char* pathname);

int file_readable(const char* pathname);

typedef struct _FILETIME FILETIME;
int file_getTimestamps(const char* path, FILETIME* creationTime, FILETIME* accessTime, FILETIME* lastWriteTime);

#if defined(__cplusplus)
}
#endif
