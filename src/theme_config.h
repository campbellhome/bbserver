// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "sb.h"

#if defined(__cplusplus)
extern "C" {
#endif

b32 Style_ReadConfig(const char *colorscheme);
void Style_ResetConfig(void);

typedef struct tag_styleColor {
	s32 r;
	s32 g;
	s32 b;
	s32 a;
	b32 bTextShadows;
	u8 pad[4];
} styleColor;

typedef struct tag_styleColors {
	styleColor *data;
	u32 count;
	u32 allocated;
} styleColors;

typedef struct tag_styleConfig {
	styleColors colors;
} styleConfig;

AUTOJSON typedef enum tag_styleColor_e {
	kStyleColor_kBBColor_Default,
	kStyleColor_kBBColor_Evergreen_Black,
	kStyleColor_kBBColor_Evergreen_Red,
	kStyleColor_kBBColor_Evergreen_Green,
	kStyleColor_kBBColor_Evergreen_Yellow,
	kStyleColor_kBBColor_Evergreen_Blue,
	kStyleColor_kBBColor_Evergreen_Cyan,
	kStyleColor_kBBColor_Evergreen_Pink,
	kStyleColor_kBBColor_Evergreen_White,
	kStyleColor_kBBColor_Evergreen_LightBlue,
	kStyleColor_kBBColor_Evergreen_Orange,
	kStyleColor_kBBColor_Evergreen_LightBlueAlt,
	kStyleColor_kBBColor_Evergreen_OrangeAlt,
	kStyleColor_kBBColor_Evergreen_MediumBlue,
	kStyleColor_kBBColor_Evergreen_Amber,
	kStyleColor_kBBColor_UE4_Black,
	kStyleColor_kBBColor_UE4_DarkRed,
	kStyleColor_kBBColor_UE4_DarkGreen,
	kStyleColor_kBBColor_UE4_DarkBlue,
	kStyleColor_kBBColor_UE4_DarkYellow,
	kStyleColor_kBBColor_UE4_DarkCyan,
	kStyleColor_kBBColor_UE4_DarkPurple,
	kStyleColor_kBBColor_UE4_DarkWhite,
	kStyleColor_kBBColor_UE4_Red,
	kStyleColor_kBBColor_UE4_Green,
	kStyleColor_kBBColor_UE4_Blue,
	kStyleColor_kBBColor_UE4_Yellow,
	kStyleColor_kBBColor_UE4_Cyan,
	kStyleColor_kBBColor_UE4_Purple,
	kStyleColor_kBBColor_UE4_White,
	kStyleColor_ActiveSession,
	kStyleColor_InactiveSession,
	kStyleColor_LogLevel_VeryVerbose,
	kStyleColor_LogLevel_Verbose,
	kStyleColor_LogLevel_Log,
	kStyleColor_LogLevel_Display,
	kStyleColor_LogLevel_Warning,
	kStyleColor_LogLevel_Error,
	kStyleColor_LogLevel_Fatal,
	kStyleColor_Multiline,
	kStyleColor_LogBackground_Normal,
	kStyleColor_LogBackground_NormalAlternate0,
	kStyleColor_LogBackground_NormalAlternate1,
	kStyleColor_LogBackground_Bookmarked,
	kStyleColor_LogBackground_BookmarkedAlternate0,
	kStyleColor_LogBackground_BookmarkedAlternate1,
	kStyleColor_ResizeNormal,
	kStyleColor_ResizeHovered,
	kStyleColor_ResizeActive,
	kStyleColor_MessageBoxBackground0,
	kStyleColor_MessageBoxBackground1,
	kStyleColor_TextShadow,
	kStyleColor_Count
} styleColor_e;

AUTOJSON typedef struct color_config_s {
	styleColor_e colorName;
	u8 r;
	u8 g;
	u8 b;
	u8 a;
	b32 bTextShadows;
	u8 pad[4];
} color_config_t;

AUTOJSON typedef struct colors_config_s {
	u32 count;
	u32 allocated;
	color_config_t *data;
} colors_config_t;

AUTOJSON typedef struct theme_config_s {
	colors_config_t colors;
} theme_config_t;

typedef struct tag_resolvedStyle {
	styleColor colors[kStyleColor_Count];
} resolvedStyle;

extern resolvedStyle g_styleConfig;

#if defined(__cplusplus)
}
#endif
