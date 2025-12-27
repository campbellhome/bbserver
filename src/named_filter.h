// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

#include "bb.h"
#include "sb.h"

#if defined(BB_STANDALONE)
#include "standalone_theme_config.h"
#else
#include "theme_config.h"
#endif

typedef struct recorded_log_s recorded_log_t;
typedef struct view_log_s view_log_t;
typedef struct view_s view_t;

AUTOJSON typedef struct named_filter_t
{
	sb_t name;
	sb_t text;
	styleColor_e bgStyle;
	styleColor_e fgStyle;
	float bgColor[4];
	float bgColorActive[4];
	float bgColorHovered[4];
	float fgColor[4];
	b32 filterEnabled;
	b32 filterSelectable;
	b32 allowBgColors;
	b32 allowFgColors;
	b32 testSelected;
	b32 selected;
	b32 testBookmarked;
	b32 bookmarked;
	b32 customColorsEnabled;
	u32 pad;
} named_filter_t;

AUTOJSON typedef struct named_filters_t
{
	u32 count;
	u32 allocated;
	named_filter_t* data;
} named_filters_t;

void named_filter_reset(named_filter_t *value);

b32 named_filters_read(void);
b32 named_filters_write(void);
void named_filters_rebuild(void);
void named_filters_shutdown(void);

named_filter_t* named_filters_resolve(view_t* view, view_log_t* viewLog, recorded_log_t* log, b32 customColors);

extern named_filters_t g_user_named_filters;

#if defined(__cplusplus)
}
#endif
