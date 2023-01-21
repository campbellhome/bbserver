// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "va.h"
#include "bb_thread.h"
#include "bb_wrap_stdio.h"
#include <stdarg.h>

enum
{
	kVA_NumSlots = 16
};
typedef struct
{
	size_t next;
	char buffer[kVA_NumSlots][2048];
} va_data_t;
static bb_thread_local va_data_t va_data;

char* va(const char* fmt, ...)
{
	int len;
	char* buffer = va_data.buffer[va_data.next++ % kVA_NumSlots];
	size_t bufferSize = sizeof(va_data.buffer[0]);

	va_list args;
	va_start(args, fmt);
	len = vsnprintf(buffer, bufferSize, fmt, args);
	if (len < 0)
	{
		buffer[bufferSize - 1] = '\0';
	}
	va_end(args);

	return buffer;
}

void va_buffer(char** buffer, size_t* capacity)
{
	*buffer = va_data.buffer[va_data.next++ % kVA_NumSlots];
	*capacity = sizeof(va_data.buffer[0]);
}
