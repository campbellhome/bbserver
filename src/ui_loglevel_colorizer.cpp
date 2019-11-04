// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "ui_loglevel_colorizer.h"
#include "bb_colors.h"
#include "theme_config.h"
#include "wrap_imgui.h"

bb_log_level_e GetLogLevelBasedOnCounts(const u32 logCount[/*kBBLogLevel_Count*/])
{
	bb_log_level_e result = kBBLogLevel_VeryVerbose;
	if(logCount[kBBLogLevel_Fatal]) {
		result = kBBLogLevel_Fatal;
	} else if(logCount[kBBLogLevel_Error]) {
		result = kBBLogLevel_Error;
	} else if(logCount[kBBLogLevel_Warning]) {
		result = kBBLogLevel_Warning;
	} else if(logCount[kBBLogLevel_Display]) {
		result = kBBLogLevel_Display;
	} else if(logCount[kBBLogLevel_Log]) {
		result = kBBLogLevel_Log;
	} else if(logCount[kBBLogLevel_Verbose]) {
		result = kBBLogLevel_Verbose;
	} else if(logCount[kBBLogLevel_VeryVerbose]) {
		result = kBBLogLevel_VeryVerbose;
	}
	return result;
}

ImVec4 GetTextColorForLogLevel(u32 logLevel) // bb_log_level_e, but u32 in packet
{
	switch(logLevel) {
	case kBBLogLevel_Error:
		return MakeColor(kStyleColor_LogLevel_Error);
	case kBBLogLevel_Warning:
		return MakeColor(kStyleColor_LogLevel_Warning);
	case kBBLogLevel_Fatal:
		return MakeColor(kStyleColor_LogLevel_Fatal);
	case kBBLogLevel_Display:
		return MakeColor(kStyleColor_LogLevel_Display);
	case kBBLogLevel_Verbose:
		return MakeColor(kStyleColor_LogLevel_Verbose);
	case kBBLogLevel_VeryVerbose:
		return MakeColor(kStyleColor_LogLevel_VeryVerbose);
	case kBBLogLevel_Log:
	default:
		return MakeColor(kStyleColor_LogLevel_Log);
	}
}

LogLevelColorizer::LogLevelColorizer(bb_log_level_e logLevel)
{
	ImGui::PushStyleColor(ImGuiCol_Text, GetTextColorForLogLevel(logLevel));
}

LogLevelColorizer::~LogLevelColorizer()
{
	ImGui::PopStyleColor();
}
