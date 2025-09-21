// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct sb_s sb_t;

typedef struct fileOpenFilter_s
{
	const wchar_t *name;
	const wchar_t *extensions;
} fileOpenFilter_t;

void FileOpenDialog_Init(void);
void FileOpenDialog_Shutdown(void);
void FileOpenDialog_SetFilters(const fileOpenFilter_t* filters, size_t numFilters, unsigned int defaultIndex, const wchar_t* defaultExtension);
sb_t FileOpenDialog_Show(bool bSave = false);

#if defined(__cplusplus)
}
#endif
