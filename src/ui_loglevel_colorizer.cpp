// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "ui_loglevel_colorizer.h"
#include "bb_colors.h"
#include "bb_string.h"
#include "imgui_core.h"
#include "imgui_text_shadows.h"
#include "wrap_imgui.h"

bb_log_level_e GetLogLevelBasedOnCounts(const u32 logCount[/*kBBLogLevel_Count*/])
{
	bb_log_level_e result = kBBLogLevel_VeryVerbose;
	if (logCount[kBBLogLevel_Fatal])
	{
		result = kBBLogLevel_Fatal;
	}
	else if (logCount[kBBLogLevel_Error])
	{
		result = kBBLogLevel_Error;
	}
	else if (logCount[kBBLogLevel_Warning])
	{
		result = kBBLogLevel_Warning;
	}
	else if (logCount[kBBLogLevel_Display])
	{
		result = kBBLogLevel_Display;
	}
	else if (logCount[kBBLogLevel_Log])
	{
		result = kBBLogLevel_Log;
	}
	else if (logCount[kBBLogLevel_Verbose])
	{
		result = kBBLogLevel_Verbose;
	}
	else if (logCount[kBBLogLevel_VeryVerbose])
	{
		result = kBBLogLevel_VeryVerbose;
	}
	return result;
}

styleColor_e GetStyleColorForLogLevel(bb_log_level_e logLevel)
{
	switch (logLevel)
	{
	case kBBLogLevel_Error:
		return kStyleColor_LogLevel_Error;
	case kBBLogLevel_Warning:
		return kStyleColor_LogLevel_Warning;
	case kBBLogLevel_Fatal:
		return kStyleColor_LogLevel_Fatal;
	case kBBLogLevel_Display:
		return kStyleColor_LogLevel_Display;
	case kBBLogLevel_Verbose:
		return kStyleColor_LogLevel_Verbose;
	case kBBLogLevel_VeryVerbose:
		return kStyleColor_LogLevel_VeryVerbose;
	case kBBLogLevel_Log:
	case kBBLogLevel_SetColor:
	case kBBLogLevel_Count:
	default:
		return kStyleColor_LogLevel_Log;
	}
}

ImVec4 GetTextColorForLogLevel(u32 logLevel) // bb_log_level_e, but u32 in packet
{
	return MakeColor(GetStyleColorForLogLevel((bb_log_level_e)logLevel));
}

LogLevelColorizer::LogLevelColorizer(bb_log_level_e logLevel, bool bCanShadow)
{
	styleColor_e styleColor = GetStyleColorForLogLevel(logLevel);
	if (!bCanShadow)
	{
		if (!bb_stricmp(Imgui_Core_GetColorScheme(), "Light"))
		{
			styleColor = GetStyleColorForLogLevel(kBBLogLevel_Log);
		}
	}

	ImGui::PushStyleColor(ImGuiCol_Text, MakeColor(styleColor));
	bTextShadows = PushTextShadows(styleColor);
}

LogLevelColorizer::~LogLevelColorizer()
{
	PopTextShadows(bTextShadows);
	ImGui::PopStyleColor();
}
