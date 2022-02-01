// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum output_level_e {
	kOutput_Log,
	kOutput_Warning,
	kOutput_Error,
} output_level_t;

typedef struct output_line_s {
	sb_t text;
	output_level_t level;
	u8 pad[4];
} output_line_t;

typedef struct output_s {
	u32 count;
	u32 allocated;
	output_line_t *data;
} output_t;

extern output_t g_output;

void output_init(void);
void output_shutdown(void);

void output_log_impl(const char *fmt, ...);
void output_warning_impl(const char *fmt, ...);
void output_error_impl(const char *fmt, ...);

#define output_log(...)                \
	{                                  \
		BB_LOG("output", __VA_ARGS__); \
		output_log_impl(__VA_ARGS__);  \
	}
#define output_warning(...)                \
	{                                      \
		BB_WARNING("output", __VA_ARGS__); \
		output_warning_impl(__VA_ARGS__);  \
	}
#define output_error(...)                \
	{                                    \
		BB_ERROR("output", __VA_ARGS__); \
		output_error_impl(__VA_ARGS__);  \
	}

#if defined(__cplusplus)
}
#endif
