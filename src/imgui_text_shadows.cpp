// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "imgui_text_shadows.h"
#include "bb_colors.h"
#include "config.h"
#include "imgui_core.h"
#include "ui_loglevel_colorizer.h"
#include "wrap_imgui.h"

bool PushTextShadows(styleColor_e styleColor)
{
	const bool ret = Imgui_Core_GetTextShadows() != 0;
	const bool bShadows = g_styleConfig.colors[styleColor].bTextShadows != 0;
	Imgui_Core_SetTextShadows(g_config.textShadows && bShadows);
	return ret;
}

void PopTextShadows(bool oldShadows)
{
	Imgui_Core_SetTextShadows(oldShadows);
}

ScopedTextShadows::ScopedTextShadows(bb_log_level_e logLevel)
{
	const styleColor_e styleColor = GetStyleColorForLogLevel(logLevel);
	bShadows = PushTextShadows(styleColor);
}

ScopedTextShadows::ScopedTextShadows(styleColor_e styleColor)
{
	bShadows = PushTextShadows(styleColor);
}

ScopedTextShadows::~ScopedTextShadows()
{
	PopTextShadows(bShadows);
}
