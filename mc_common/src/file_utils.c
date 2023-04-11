// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "file_utils.h"
#include "bb_file.h"
#include "common.h"

#if BB_USING(BB_PLATFORM_WINDOWS)

#include "bb_wrap_windows.h"

void fileData_reset(fileData_t* result)
{
	if (result->buffer)
	{
		VirtualFree(result->buffer, 0, MEM_RELEASE);
	}
	memset(result, 0, sizeof(*result));
}

fileData_t fileData_read(const char* filename)
{
	fileData_t result = { BB_EMPTY_INITIALIZER };

	HANDLE handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (handle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER fileSize;
		if (GetFileSizeEx(handle, &fileSize))
		{
			u32 fileSize32 = (u32)fileSize.QuadPart;
			result.buffer = VirtualAlloc(0, (size_t)fileSize32 + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if (result.buffer)
			{
				DWORD bytesRead;
				if (ReadFile(handle, result.buffer, fileSize32, &bytesRead, 0) &&
				    (fileSize32 == bytesRead))
				{
					result.bufferSize = fileSize32;
					((char*)result.buffer)[result.bufferSize] = '\0';
				}
				else
				{
					fileData_reset(&result);
				}
			}
		}

		CloseHandle(handle);
	}

	return result;
}

b32 file_readable(const char* pathname)
{
	b32 result = false;
	HANDLE handle = CreateFileA(pathname, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (handle != INVALID_HANDLE_VALUE)
	{
		result = true;
		CloseHandle(handle);
	}

	return result;
}

b32 fileData_write(const char* pathname, const char* tempPathname, fileData_t data)
{
	b32 result = false;
	if (!tempPathname || !*tempPathname || !file_readable(pathname))
	{
		tempPathname = pathname;
	}
	HANDLE handle = CreateFileA(tempPathname, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (handle != INVALID_HANDLE_VALUE)
	{
		DWORD bytesWritten = 0;
		result = WriteFile(handle, data.buffer, data.bufferSize, &bytesWritten, 0) != 0;
		if (bytesWritten != data.bufferSize)
		{
			result = false;
		}
		CloseHandle(handle);

		if (result && tempPathname != pathname)
		{
			result = ReplaceFile(pathname, tempPathname, 0, 0, 0, 0) != 0;
		}
	}

	return result;
}

fileData_writeResult fileData_writeIfChanged(const char* pathname, const char* tempPathname, fileData_t data)
{
	fileData_writeResult result = kFileData_Error;
	fileData_t orig = fileData_read(pathname);
	if (orig.buffer && data.buffer && orig.bufferSize == data.bufferSize && !memcmp(orig.buffer, data.buffer, data.bufferSize))
	{
		result = kFileData_Unmodified;
	}
	else
	{
		result = fileData_write(pathname, tempPathname, data) ? kFileData_Success : kFileData_Error;
	}
	fileData_reset(&orig);
	return result;
}

b32 file_delete(const char* pathname)
{
	SetFileAttributesA(pathname, FILE_ATTRIBUTE_TEMPORARY);
	return DeleteFileA(pathname);
}

int file_getTimestamps(const char* path, FILETIME* creationTime, FILETIME* accessTime, FILETIME* lastWriteTime)
{
	int ret = false;
	HANDLE handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (handle != INVALID_HANDLE_VALUE)
	{
		ret = GetFileTime(handle, creationTime, accessTime, lastWriteTime) != 0;
		CloseHandle(handle);
	}
	return ret;
}

#else // #if BB_USING(BB_PLATFORM_WINDOWS)

#include "bb_malloc.h"
#include "bb_wrap_stdio.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void fileData_reset(fileData_t* result)
{
	if (result->buffer)
	{
		bb_free(result->buffer);
	}
	memset(result, 0, sizeof(*result));
}

fileData_t fileData_read(const char* filename)
{
	fileData_t result = { BB_EMPTY_INITIALIZER };

	FILE* fp = fopen(filename, "rb");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		u32 fileSize32 = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		result.buffer = bb_malloc(fileSize32 + 1);
		if (result.buffer)
		{
			size_t bytesRead = fread(result.buffer, 1, fileSize32, fp);
			if (fileSize32 == bytesRead)
			{
				result.bufferSize = fileSize32;
				((char*)result.buffer)[result.bufferSize] = '\0';
			}
			else
			{
				fileData_reset(&result);
			}
		}

		fclose(fp);
	}

	return result;
}

b32 file_readable(const char* pathname)
{
	b32 result = false;
	FILE* fp = fopen(pathname, "rb");
	if (fp)
	{
		result = true;
		fclose(fp);
	}

	return result;
}

b32 fileData_write(const char* pathname, const char* tempPathname, fileData_t data)
{
	b32 result = false;
	if (!tempPathname || !*tempPathname || !file_readable(pathname))
	{
		tempPathname = pathname;
	}
	FILE* fp = fopen(tempPathname, "wb");
	if (fp)
	{
		size_t bytesWritten = fwrite(data.buffer, 1, data.bufferSize, fp);
		if (bytesWritten != data.bufferSize)
		{
			result = false;
		}
		fclose(fp);

		if (result && tempPathname != pathname)
		{
			result = rename(tempPathname, pathname) == 0;
		}
	}

	return result;
}

fileData_writeResult fileData_writeIfChanged(const char* pathname, const char* tempPathname, fileData_t data)
{
	fileData_writeResult result = kFileData_Error;
	fileData_t orig = fileData_read(pathname);
	if (orig.buffer && data.buffer && orig.bufferSize == data.bufferSize && !memcmp(orig.buffer, data.buffer, data.bufferSize))
	{
		result = kFileData_Unmodified;
	}
	else
	{
		result = fileData_write(pathname, tempPathname, data) ? kFileData_Success : kFileData_Error;
	}
	fileData_reset(&orig);
	return result;
}

b32 file_delete(const char* pathname)
{
	return unlink(pathname) == 0 || errno == ENOENT;
}

#endif // #else // #if BB_USING(BB_PLATFORM_WINDOWS)
