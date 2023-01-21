// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef uint32_t output_init_flags_t;

typedef enum output_level_e
{
	kOutput_Log,
	kOutput_Warning,
	kOutput_Error,
} output_level_t;

typedef struct output_line_s
{
	sb_t text;
	output_level_t level;
	u8 pad[4];
} output_line_t;

typedef struct output_s
{
	u32 count;
	u32 allocated;
	output_line_t* data;
} output_t;

extern u32 g_outputFlags;
extern output_t g_output;

typedef enum
{
	kOutputInit_None = 0x0,
	kOutputInit_ToBlackbox = 0x1,
	kOutputInit_ToStdout = 0x2,
	kOutputInit_ToOutputDebugString = 0x4,
	kOutputInit_ToBuffer = 0x8, // stores output into g_output
} output_init_flag_e;

void output_init(output_init_flags_t flags);
void output_shutdown(void);

void output_log_impl(const char* fmt, ...);
void output_warning_impl(const char* fmt, ...);
void output_error_impl(const char* fmt, ...);

#define output_log(...)                                    \
	{                                                      \
		if ((g_outputFlags & kOutputInit_ToBlackbox) != 0) \
		{                                                  \
			BB_LOG("output", __VA_ARGS__);                 \
		}                                                  \
		output_log_impl(__VA_ARGS__);                      \
	}
#define output_warning(...)                                \
	{                                                      \
		if ((g_outputFlags & kOutputInit_ToBlackbox) != 0) \
		{                                                  \
			BB_WARNING("output", __VA_ARGS__);             \
		}                                                  \
		output_warning_impl(__VA_ARGS__);                  \
	}
#define output_error(...)                                  \
	{                                                      \
		if ((g_outputFlags & kOutputInit_ToBlackbox) != 0) \
		{                                                  \
			BB_ERROR("output", __VA_ARGS__);               \
		}                                                  \
		output_error_impl(__VA_ARGS__);                    \
	}

#if defined(__cplusplus)
}
#endif
