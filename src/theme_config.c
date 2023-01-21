// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "theme_config.h"
#include "appdata.h"
#include "bb_json_generated.h"
#include "bb_string.h"
#include "bb_structs_generated.h"
#include "cmdline.h"

resolvedStyle g_styleConfig;

typedef struct tag_defaultStyle
{
	const char* name;
	const char* filename;
	resolvedStyle colors;
} defaultStyle;

// clang-format off
const defaultStyle s_defaultStyleConfigs[] = {
	{ "ImGui Dark", "bb_theme_imgui_dark.json",
		{ {
			{ 180, 180, 180, 255, true  }, // kStyleColor_kBBColor_Default,
			{  61,  61,  61, 255, true  }, // kStyleColor_kBBColor_Evergreen_Black,
			{ 255,  51,  51, 255, true  }, // kStyleColor_kBBColor_Evergreen_Red,
			{  64, 255,  64, 255, true  }, // kStyleColor_kBBColor_Evergreen_Green,
			{ 256, 256,  96, 255, true  }, // kStyleColor_kBBColor_Evergreen_Yellow,
			{   0,  60, 255, 255, true  }, // kStyleColor_kBBColor_Evergreen_Blue,
			{  64, 255, 255, 255, true  }, // kStyleColor_kBBColor_Evergreen_Cyan,
			{ 255,  92, 255, 255, true  }, // kStyleColor_kBBColor_Evergreen_Pink,
			{ 180, 180, 180, 255, true  }, // kStyleColor_kBBColor_Evergreen_White,
			{ 148, 214, 254, 255, true  }, // kStyleColor_kBBColor_Evergreen_LightBlue,
			{ 242,  97,  64, 255, true  }, // kStyleColor_kBBColor_Evergreen_Orange,
			{ 149, 214, 221, 255, true  }, // kStyleColor_kBBColor_Evergreen_LightBlueAlt,
			{ 221,  77,  48, 255, true  }, // kStyleColor_kBBColor_Evergreen_OrangeAlt,
			{  89, 156, 212, 255, true  }, // kStyleColor_kBBColor_Evergreen_MediumBlue,
			{ 250, 176,  46, 255, true  }, // kStyleColor_kBBColor_Evergreen_Amber,
			{  64,  64,  64, 255, true  }, // kStyleColor_kBBColor_UE4_Black,
			{ 192,   0,   0, 255, true  }, // kStyleColor_kBBColor_UE4_DarkRed,
			{  86, 158,  86, 255, true  }, // kStyleColor_kBBColor_UE4_DarkGreen,
			{  64,  64, 255, 255, true  }, // kStyleColor_kBBColor_UE4_DarkBlue,
			{ 192, 192,   0, 255, true  }, // kStyleColor_kBBColor_UE4_DarkYellow,
			{   0, 192, 192, 255, true  }, // kStyleColor_kBBColor_UE4_DarkCyan,
			{ 144, 109, 185, 255, true  }, // kStyleColor_kBBColor_UE4_DarkPurple,
			{ 128, 128, 128, 255, true  }, // kStyleColor_kBBColor_UE4_DarkWhite,
			{ 255,   0,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Red,
			{   0, 192,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Green,
			{  96,  96, 255, 255, true  }, // kStyleColor_kBBColor_UE4_Blue,
			{ 224, 224,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Yellow,
			{   0, 255, 255, 255, true  }, // kStyleColor_kBBColor_UE4_Cyan,
			{ 192,   0, 192, 255, true  }, // kStyleColor_kBBColor_UE4_Purple,
			{ 192, 192, 192, 255, true  }, // kStyleColor_kBBColor_UE4_White,
			{ 179, 255, 179, 255, true  }, // kStyleColor_ActiveSession,
			{ 153, 153, 153, 255, true  }, // kStyleColor_InactiveSession,
			{  96,  96,  96, 255, true  }, // kStyleColor_LogLevel_VeryVerbose,
			{ 128, 128, 128, 255, true  }, // kStyleColor_LogLevel_Verbose,
			{ 180, 180, 180, 255, true  }, // kStyleColor_LogLevel_Log,
			{ 255, 255, 255, 255, true  }, // kStyleColor_LogLevel_Display,
			{ 256, 256,  96, 255, true  }, // kStyleColor_LogLevel_Warning,
			{ 255,  51,  51, 255, true  }, // kStyleColor_LogLevel_Error,
			{ 255,   0,   0, 255, true  }, // kStyleColor_LogLevel_Fatal,
			{  61,  61,  61, 255, true  }, // kStyleColor_Multiline,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_Normal,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_NormalAlternate0,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_NormalAlternate1,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_Bookmarked,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_BookmarkedAlternate0,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_BookmarkedAlternate1,
			{   0,   0,   0,   0, true  }, // kStyleColor_ResizeNormal,
			{   0,   0,   0,   0, true  }, // kStyleColor_ResizeHovered,
			{   0,   0,   0,   0, true  }, // kStyleColor_ResizeActive,
			{ 190, 145, 100, 136, true  }, // kStyleColor_MessageBoxBackground0,
			{ 190, 145, 100,  34, true  }, // kStyleColor_MessageBoxBackground1,
			{   0,   0,   0, 255, false }, // kStyleColor_TextShadow,
		} }
	},
	{ "Light", "bb_theme_light.json",
		{ {
			{ 180, 180, 180, 255, false }, // kStyleColor_kBBColor_Default,
			{  61,  61,  61, 255, false }, // kStyleColor_kBBColor_Evergreen_Black,
			{ 255,  51,  51, 255, false }, // kStyleColor_kBBColor_Evergreen_Red,
			{  64, 255,  64, 255, true  }, // kStyleColor_kBBColor_Evergreen_Green,
			{ 224, 224,   0, 255, true  }, // kStyleColor_kBBColor_Evergreen_Yellow,
			{   0,  60, 255, 255, false }, // kStyleColor_kBBColor_Evergreen_Blue,
			{  64, 255, 255, 255, true  }, // kStyleColor_kBBColor_Evergreen_Cyan,
			{ 255,  92, 255, 255, false }, // kStyleColor_kBBColor_Evergreen_Pink,
			{ 180, 180, 180, 255, true  }, // kStyleColor_kBBColor_Evergreen_White,
			{ 148, 214, 254, 255, true  }, // kStyleColor_kBBColor_Evergreen_LightBlue,
			{ 242,  97,  64, 255, false }, // kStyleColor_kBBColor_Evergreen_Orange,
			{ 149, 214, 221, 255, true  }, // kStyleColor_kBBColor_Evergreen_LightBlueAlt,
			{ 221,  77,  48, 255, false }, // kStyleColor_kBBColor_Evergreen_OrangeAlt,
			{  89, 156, 212, 255, true  }, // kStyleColor_kBBColor_Evergreen_MediumBlue,
			{ 250, 176,  46, 255, true  }, // kStyleColor_kBBColor_Evergreen_Amber,
			{   0,   0,   0, 255, false }, // kStyleColor_kBBColor_UE4_Black,
			{ 192,   0,   0, 255, false }, // kStyleColor_kBBColor_UE4_DarkRed,
			{   0, 132,   0, 255, false }, // kStyleColor_kBBColor_UE4_DarkGreen,
			{   0,   0, 192, 255, false }, // kStyleColor_kBBColor_UE4_DarkBlue,
			{ 192, 192,   0, 255, true  }, // kStyleColor_kBBColor_UE4_DarkYellow,
			{   0, 192, 192, 255, true  }, // kStyleColor_kBBColor_UE4_DarkCyan,
			{ 144, 109, 185, 255, false }, // kStyleColor_kBBColor_UE4_DarkPurple,
			{ 128, 128, 128, 255, false }, // kStyleColor_kBBColor_UE4_DarkWhite,
			{ 255,   0,   0, 255, false }, // kStyleColor_kBBColor_UE4_Red,
			{   0, 192,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Green,
			{  48,  48, 255, 255, false }, // kStyleColor_kBBColor_UE4_Blue,
			{ 224, 224,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Yellow,
			{   0, 255, 255, 255, true  }, // kStyleColor_kBBColor_UE4_Cyan,
			{ 192,   0, 192, 255, false }, // kStyleColor_kBBColor_UE4_Purple,
			{ 192, 192, 192, 255, true  }, // kStyleColor_kBBColor_UE4_White,
			{   0, 132,   0, 255, false }, // kStyleColor_ActiveSession,
			{ 128, 128, 128, 255, false }, // kStyleColor_InactiveSession,
			{ 128, 128, 128, 255, false }, // kStyleColor_LogLevel_VeryVerbose,
			{  96,  96,  96, 255, false }, // kStyleColor_LogLevel_Verbose,
			{  64,  64,  64, 255, false }, // kStyleColor_LogLevel_Log,
			{ 224, 255, 255, 255, true  }, // kStyleColor_LogLevel_Display,
			{ 224, 224,   0, 255, true  }, // kStyleColor_LogLevel_Warning,
			{ 255,  51,  51, 255, false }, // kStyleColor_LogLevel_Error,
			{ 255,   0,   0, 255, false }, // kStyleColor_LogLevel_Fatal,
			{   0,   0,   0, 255, false }, // kStyleColor_Multiline,
			{   0,   0,   0,   0, false }, // kStyleColor_LogBackground_Normal,
			{   0,   0,   0,   0, false }, // kStyleColor_LogBackground_NormalAlternate0,
			{   0,   0,   0,   0, false }, // kStyleColor_LogBackground_NormalAlternate1,
			{   0,   0,   0,   0, false }, // kStyleColor_LogBackground_Bookmarked,
			{   0,   0,   0,   0, false }, // kStyleColor_LogBackground_BookmarkedAlternate0,
			{   0,   0,   0,   0, false }, // kStyleColor_LogBackground_BookmarkedAlternate1,
			{   0,   0,   0,   0, false }, // kStyleColor_ResizeNormal,
			{   0,   0,   0,   0, false }, // kStyleColor_ResizeHovered,
			{   0,   0,   0,   0, false }, // kStyleColor_ResizeActive,
			{ 190, 145, 100, 136, false }, // kStyleColor_MessageBoxBackground0,
			{ 190, 145, 100,  34, false }, // kStyleColor_MessageBoxBackground1,
			{   0,   0,   0, 128, false }, // kStyleColor_TextShadow,
		} }
	},
	{ "Classic", "bb_theme_classic.json",
		{ {
			{ 180, 180, 180, 255, true  }, // kStyleColor_kBBColor_Default,
			{  61,  61,  61, 255, true  }, // kStyleColor_kBBColor_Evergreen_Black,
			{ 255,  51,  51, 255, true  }, // kStyleColor_kBBColor_Evergreen_Red,
			{  64, 255,  64, 255, true  }, // kStyleColor_kBBColor_Evergreen_Green,
			{ 256, 256,  96, 255, true  }, // kStyleColor_kBBColor_Evergreen_Yellow,
			{   0,  60, 255, 255, true  }, // kStyleColor_kBBColor_Evergreen_Blue,
			{  64, 255, 255, 255, true  }, // kStyleColor_kBBColor_Evergreen_Cyan,
			{ 255,  92, 255, 255, true  }, // kStyleColor_kBBColor_Evergreen_Pink,
			{ 180, 180, 180, 255, true  }, // kStyleColor_kBBColor_Evergreen_White,
			{ 148, 214, 254, 255, true  }, // kStyleColor_kBBColor_Evergreen_LightBlue,
			{ 242,  97,  64, 255, true  }, // kStyleColor_kBBColor_Evergreen_Orange,
			{ 149, 214, 221, 255, true  }, // kStyleColor_kBBColor_Evergreen_LightBlueAlt,
			{ 221,  77,  48, 255, true  }, // kStyleColor_kBBColor_Evergreen_OrangeAlt,
			{  89, 156, 212, 255, true  }, // kStyleColor_kBBColor_Evergreen_MediumBlue,
			{ 250, 176,  46, 255, true  }, // kStyleColor_kBBColor_Evergreen_Amber,
			{  64,  64,  64, 255, true  }, // kStyleColor_kBBColor_UE4_Black,
			{ 192,   0,   0, 255, true  }, // kStyleColor_kBBColor_UE4_DarkRed,
			{  86, 158,  86, 255, true  }, // kStyleColor_kBBColor_UE4_DarkGreen,
			{  64,  64, 255, 255, true  }, // kStyleColor_kBBColor_UE4_DarkBlue,
			{ 192, 192,   0, 255, true  }, // kStyleColor_kBBColor_UE4_DarkYellow,
			{   0, 192, 192, 255, true  }, // kStyleColor_kBBColor_UE4_DarkCyan,
			{ 144, 109, 185, 255, true  }, // kStyleColor_kBBColor_UE4_DarkPurple,
			{ 128, 128, 128, 255, true  }, // kStyleColor_kBBColor_UE4_DarkWhite,
			{ 255,   0,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Red,
			{   0, 192,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Green,
			{  96,  96, 255, 255, true  }, // kStyleColor_kBBColor_UE4_Blue,
			{ 224, 224,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Yellow,
			{   0, 255, 255, 255, true  }, // kStyleColor_kBBColor_UE4_Cyan,
			{ 192,   0, 192, 255, true  }, // kStyleColor_kBBColor_UE4_Purple,
			{ 192, 192, 192, 255, true  }, // kStyleColor_kBBColor_UE4_White,
			{ 179, 255, 179, 255, true  }, // kStyleColor_ActiveSession,
			{ 153, 153, 153, 255, true  }, // kStyleColor_InactiveSession,
			{  96,  96,  96, 255, true  }, // kStyleColor_LogLevel_VeryVerbose,
			{ 128, 128, 128, 255, true  }, // kStyleColor_LogLevel_Verbose,
			{ 180, 180, 180, 255, true  }, // kStyleColor_LogLevel_Log,
			{ 255, 255, 255, 255, true  }, // kStyleColor_LogLevel_Display,
			{ 256, 256,  96, 255, true  }, // kStyleColor_LogLevel_Warning,
			{ 255,  51,  51, 255, true  }, // kStyleColor_LogLevel_Error,
			{ 255,   0,   0, 255, true  }, // kStyleColor_LogLevel_Fatal,
			{  61,  61,  61, 255, true  }, // kStyleColor_Multiline,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_Normal,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_NormalAlternate0,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_NormalAlternate1,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_Bookmarked,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_BookmarkedAlternate0,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_BookmarkedAlternate1,
			{   0,   0,   0,   0, true  }, // kStyleColor_ResizeNormal,
			{   0,   0,   0,   0, true  }, // kStyleColor_ResizeHovered,
			{   0,   0,   0,   0, true  }, // kStyleColor_ResizeActive,
			{ 190, 145, 100, 136, true  }, // kStyleColor_MessageBoxBackground0,
			{ 190, 145, 100,  34, true  }, // kStyleColor_MessageBoxBackground1,
			{   0,   0,   0, 255, false }, // kStyleColor_TextShadow,
		} }
	},
	{ "Visual Studio Dark", "bb_theme_visual_studio_dark.json",
		{ {
			{ 180, 180, 180, 255, true  }, // kStyleColor_kBBColor_Default,
			{  61,  61,  61, 255, true  }, // kStyleColor_kBBColor_Evergreen_Black,
			{ 255,  51,  51, 255, true  }, // kStyleColor_kBBColor_Evergreen_Red,
			{  64, 255,  64, 255, true  }, // kStyleColor_kBBColor_Evergreen_Green,
			{ 256, 256,  96, 255, true  }, // kStyleColor_kBBColor_Evergreen_Yellow,
			{   0,  60, 255, 255, true  }, // kStyleColor_kBBColor_Evergreen_Blue,
			{  64, 255, 255, 255, true  }, // kStyleColor_kBBColor_Evergreen_Cyan,
			{ 255,  92, 255, 255, true  }, // kStyleColor_kBBColor_Evergreen_Pink,
			{ 180, 180, 180, 255, true  }, // kStyleColor_kBBColor_Evergreen_White,
			{ 148, 214, 254, 255, true  }, // kStyleColor_kBBColor_Evergreen_LightBlue,
			{ 242,  97,  64, 255, true  }, // kStyleColor_kBBColor_Evergreen_Orange,
			{ 149, 214, 221, 255, true  }, // kStyleColor_kBBColor_Evergreen_LightBlueAlt,
			{ 221,  77,  48, 255, true  }, // kStyleColor_kBBColor_Evergreen_OrangeAlt,
			{  89, 156, 212, 255, true  }, // kStyleColor_kBBColor_Evergreen_MediumBlue,
			{ 250, 176,  46, 255, true  }, // kStyleColor_kBBColor_Evergreen_Amber,
			{  96,  96,  96, 255, true  }, // kStyleColor_kBBColor_UE4_Black,
			{ 192,   0,   0, 255, true  }, // kStyleColor_kBBColor_UE4_DarkRed,
			{  86, 158,  86, 255, true  }, // kStyleColor_kBBColor_UE4_DarkGreen,
			{  64,  64, 255, 255, true  }, // kStyleColor_kBBColor_UE4_DarkBlue,
			{ 192, 192,   0, 255, true  }, // kStyleColor_kBBColor_UE4_DarkYellow,
			{   0, 192, 192, 255, true  }, // kStyleColor_kBBColor_UE4_DarkCyan,
			{ 144, 109, 185, 255, true  }, // kStyleColor_kBBColor_UE4_DarkPurple,
			{ 128, 128, 128, 255, true  }, // kStyleColor_kBBColor_UE4_DarkWhite,
			{ 255,   0,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Red,
			{   0, 192,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Green,
			{  96,  96, 255, 255, true  }, // kStyleColor_kBBColor_UE4_Blue,
			{ 224, 224,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Yellow,
			{   0, 255, 255, 255, true  }, // kStyleColor_kBBColor_UE4_Cyan,
			{ 192,   0, 192, 255, true  }, // kStyleColor_kBBColor_UE4_Purple,
			{ 192, 192, 192, 255, true  }, // kStyleColor_kBBColor_UE4_White,
			{ 179, 255, 179, 255, true  }, // kStyleColor_ActiveSession,
			{ 153, 153, 153, 255, true  }, // kStyleColor_InactiveSession,
			{  96,  96,  96, 255, true  }, // kStyleColor_LogLevel_VeryVerbose,
			{ 128, 128, 128, 255, true  }, // kStyleColor_LogLevel_Verbose,
			{ 180, 180, 180, 255, true  }, // kStyleColor_LogLevel_Log,
			{ 255, 255, 255, 255, true  }, // kStyleColor_LogLevel_Display,
			{ 256, 256,  96, 255, true  }, // kStyleColor_LogLevel_Warning,
			{ 255,  51,  51, 255, true  }, // kStyleColor_LogLevel_Error,
			{ 255,   0,   0, 255, true  }, // kStyleColor_LogLevel_Fatal,
			{  61,  61,  61, 255, true  }, // kStyleColor_Multiline,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_Normal,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_NormalAlternate0,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_NormalAlternate1,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_Bookmarked,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_BookmarkedAlternate0,
			{   0,   0,   0,   0, true  }, // kStyleColor_LogBackground_BookmarkedAlternate1,
			{   0,   0,   0,   0, true  }, // kStyleColor_ResizeNormal,
			{   0,   0,   0,   0, true  }, // kStyleColor_ResizeHovered,
			{   0,   0,   0,   0, true  }, // kStyleColor_ResizeActive,
			{ 190, 145, 100, 136, true  }, // kStyleColor_MessageBoxBackground0,
			{ 190, 145, 100,  34, true  }, // kStyleColor_MessageBoxBackground1,
			{   0,   0,   0, 255, false }, // kStyleColor_TextShadow,
		} }
	},
	{ "Windows", "bb_theme_windows.json",
		{ {
			{ 180, 180, 180, 255, false }, // kStyleColor_kBBColor_Default,
			{  61,  61,  61, 255, false }, // kStyleColor_kBBColor_Evergreen_Black,
			{ 255,  51,  51, 255, false }, // kStyleColor_kBBColor_Evergreen_Red,
			{  64, 255,  64, 255, true  }, // kStyleColor_kBBColor_Evergreen_Green,
			{ 256, 256,  96, 255, true  }, // kStyleColor_kBBColor_Evergreen_Yellow,
			{   0,  60, 255, 255, false }, // kStyleColor_kBBColor_Evergreen_Blue,
			{  64, 255, 255, 255, true  }, // kStyleColor_kBBColor_Evergreen_Cyan,
			{ 255,  92, 255, 255, false }, // kStyleColor_kBBColor_Evergreen_Pink,
			{ 180, 180, 180, 255, true  }, // kStyleColor_kBBColor_Evergreen_White,
			{ 148, 214, 254, 255, true  }, // kStyleColor_kBBColor_Evergreen_LightBlue,
			{ 242,  97,  64, 255, false }, // kStyleColor_kBBColor_Evergreen_Orange,
			{ 149, 214, 221, 255, true  }, // kStyleColor_kBBColor_Evergreen_LightBlueAlt,
			{ 221,  77,  48, 255, false }, // kStyleColor_kBBColor_Evergreen_OrangeAlt,
			{  89, 156, 212, 255, true  }, // kStyleColor_kBBColor_Evergreen_MediumBlue,
			{ 250, 176,  46, 255, true  }, // kStyleColor_kBBColor_Evergreen_Amber,
			{   0,   0,   0, 255, false }, // kStyleColor_kBBColor_UE4_Black,
			{ 192,   0,   0, 255, false }, // kStyleColor_kBBColor_UE4_DarkRed,
			{   0, 132,   0, 255, false }, // kStyleColor_kBBColor_UE4_DarkGreen,
			{   0,   0, 192, 255, false }, // kStyleColor_kBBColor_UE4_DarkBlue,
			{ 192, 192,   0, 255, true  }, // kStyleColor_kBBColor_UE4_DarkYellow,
			{   0, 192, 192, 255, true  }, // kStyleColor_kBBColor_UE4_DarkCyan,
			{ 144, 109, 185, 255, false }, // kStyleColor_kBBColor_UE4_DarkPurple,
			{ 128, 128, 128, 255, false }, // kStyleColor_kBBColor_UE4_DarkWhite,
			{ 255,   0,   0, 255, false }, // kStyleColor_kBBColor_UE4_Red,
			{   0, 192,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Green,
			{  48,  48, 255, 255, false }, // kStyleColor_kBBColor_UE4_Blue,
			{ 206, 206,   0, 255, true  }, // kStyleColor_kBBColor_UE4_Yellow,
			{   0, 192, 192, 255, true  }, // kStyleColor_kBBColor_UE4_Cyan,
			{ 192,   0, 192, 255, false }, // kStyleColor_kBBColor_UE4_Purple,
			{ 128, 128, 128, 255, true  }, // kStyleColor_kBBColor_UE4_White,
			{   0, 132,   0, 255, false }, // kStyleColor_ActiveSession,
			{ 153, 153, 153, 255, false }, // kStyleColor_InactiveSession,
			{ 128, 128, 128, 255, false }, // kStyleColor_LogLevel_VeryVerbose,
			{  96,  96,  96, 255, false }, // kStyleColor_LogLevel_Verbose,
			{  64,  64,  64, 255, false }, // kStyleColor_LogLevel_Log,
			{ 224, 255, 255, 255, true  }, // kStyleColor_LogLevel_Display,
			{ 206, 206,   0, 255, true  }, // kStyleColor_LogLevel_Warning,
			{ 255,  51,  51, 255, false }, // kStyleColor_LogLevel_Error,
			{ 255,   0,   0, 255, false }, // kStyleColor_LogLevel_Fatal,
			{   0,   0,   0, 255, false }, // kStyleColor_Multiline,
			{   0,   0,   0,   0, false }, // kStyleColor_LogBackground_Normal,
			{   0,   0,   0,   0, false }, // kStyleColor_LogBackground_NormalAlternate0,
			{   0,   0,   0,   0, false }, // kStyleColor_LogBackground_NormalAlternate1,
			{   0,   0,   0,   0, false }, // kStyleColor_LogBackground_Bookmarked,
			{   0,   0,   0,   0, false }, // kStyleColor_LogBackground_BookmarkedAlternate0,
			{   0,   0,   0,   0, false }, // kStyleColor_LogBackground_BookmarkedAlternate1,
			{   0,   0,   0,   0, false }, // kStyleColor_ResizeNormal,
			{   0,   0,   0,   0, false }, // kStyleColor_ResizeHovered,
			{   0,   0,   0,   0, false }, // kStyleColor_ResizeActive,
			{ 190, 145, 100, 136, false }, // kStyleColor_MessageBoxBackground0,
			{ 190, 145, 100,  34, false }, // kStyleColor_MessageBoxBackground1,
			{   0,   0,   0, 128, false }, // kStyleColor_TextShadow,
		} }
	},
};
// clang-format on

static void Style_ReadConfigJson(const char* filename)
{
	sb_t path = appdata_get("bb");
	sb_va(&path, "\\%s", filename);
	JSON_Value* val = json_parse_file(sb_get(&path));
	if (val)
	{
		theme_config_t themeConfig = json_deserialize_theme_config_t(val);
		for (u32 colorFileIndex = 0; colorFileIndex < themeConfig.colors.count; ++colorFileIndex)
		{
			const color_config_t* colorConfig = themeConfig.colors.data + colorFileIndex;
			if (colorConfig->colorName >= 0 && colorConfig->colorName < kStyleColor_Count)
			{
				g_styleConfig.colors[colorConfig->colorName].r = colorConfig->r;
				g_styleConfig.colors[colorConfig->colorName].g = colorConfig->g;
				g_styleConfig.colors[colorConfig->colorName].b = colorConfig->b;
				g_styleConfig.colors[colorConfig->colorName].a = colorConfig->a;
				g_styleConfig.colors[colorConfig->colorName].bTextShadows = colorConfig->bTextShadows;
			}
		}
		json_value_free(val);
		theme_config_reset(&themeConfig);
	}
	sb_reset(&path);
}

b32 Style_ReadConfig(const char* colorscheme)
{
	Style_ResetConfig();

	b32 ret = false;
	for (u32 i = 0; i < BB_ARRAYSIZE(s_defaultStyleConfigs); ++i)
	{
		if (!bb_stricmp(colorscheme, s_defaultStyleConfigs[i].name))
		{
			g_styleConfig = s_defaultStyleConfigs[i].colors;
			Style_ReadConfigJson(s_defaultStyleConfigs[i].filename);
			ret = true;
			break;
		}
	}

	return ret;
}

void Style_ResetConfig(void)
{
}
