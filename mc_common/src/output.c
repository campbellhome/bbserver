// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "output.h"
#include "bb_array.h"
#include "sb.h"
#include "va.h"
#include <bb_wrap_stdio.h>
#include <bb_wrap_windows.h>
#include <stdarg.h>

u32 g_outputFlags = kOutputInit_None;
output_t g_output;
sb_t g_outputBuf;

void output_init(output_init_flags_t flags)
{
	g_outputFlags = flags;
}

void output_shutdown(void)
{
	sb_reset(&g_outputBuf);
	for(u32 i = 0; i < g_output.count; ++i) {
		sb_reset(&g_output.data[i].text);
	}
	bba_free(g_output);
}

static void output_add(const char *fmt, va_list args, output_level_t level)
{
	if((g_outputFlags & kOutputInit_ToStdout) != 0) {
		FILE *stream = (level == kOutput_Log) ? stdout : stderr;
		vfprintf(stream, fmt, args);
	}

	if((g_outputFlags & kOutputInit_ToOutputDebugString) != 0) {
		sb_va_list(&g_outputBuf, fmt, args);
		OutputDebugStringA(sb_get(&g_outputBuf));
		sb_clear(&g_outputBuf);
	}

	if((g_outputFlags & kOutputInit_ToBuffer) != 0) {
		if(bba_add_noclear(g_output, 1)) {
			output_line_t *line = &bba_last(g_output);
			line->level = level;
			sb_init(&line->text);
			sb_va_list(&line->text, fmt, args);
		}
	}
}

void output_log_impl(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	output_add(fmt, args, kOutput_Log);
	va_end(args);
}

void output_warning_impl(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	output_add(fmt, args, kOutput_Warning);
	va_end(args);
}

void output_error_impl(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	output_add(fmt, args, kOutput_Error);
	va_end(args);
}
