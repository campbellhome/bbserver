// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

#include "bb.h"
#include "sb.h"
#include "theme_config.h"

typedef struct recorded_log_s recorded_log_t;
typedef struct view_log_s view_log_t;
typedef struct view_s view_t;

AUTOJSON typedef struct log_color_config_entry_t
{
	sb_t name;
	sb_t filter;
	styleColor_e bgStyle;
	styleColor_e fgStyle;
	float bgColor[4];
	float fgColor[4];
	b32 enabled;
	b32 allowBgColors;
	b32 allowFgColors;
	b32 testSelected;
	b32 selected;
	b32 testBookmarked;
	b32 bookmarked;
	u32 pad;
} log_color_config_entry_t;

AUTOJSON typedef struct log_color_config_t
{
	u32 count;
	u32 allocated;
	log_color_config_entry_t* data;
} log_color_config_t;

extern log_color_config_t g_log_color_config;

b32 log_color_config_read(void);
b32 log_color_config_write(void);
void log_color_config_shutdown(void);

log_color_config_entry_t* log_color_config_resolve(view_t* view, view_log_t* viewLog, recorded_log_t* log);

#if defined(__cplusplus)
}
#endif
