// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "output.h"
#include "sb.h"
#include "va.h"

#include "bb_array.h"

output_t g_output;

void output_init(void)
{
}

void output_shutdown(void)
{
	for(u32 i = 0; i < g_output.count; ++i) {
		sb_reset(&g_output.data[i].text);
	}
	bba_free(g_output);
}

static void output_add(const char *fmt, va_list args, output_level_t level)
{
	if(bba_add_noclear(g_output, 1)) {
		output_line_t *line = &bba_last(g_output);
		line->level = level;
		sb_init(&line->text);
		sb_va_list(&line->text, fmt, args);
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
