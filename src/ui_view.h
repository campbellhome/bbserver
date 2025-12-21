// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "wrap_imgui.h"

enum class EViewFilterCategory : u32
{
	Input,
	History,
	Config,
	SiteConfig,
	Count
};

void PushLogFont(void);
void PopLogFont(void);
void PushUIFont(void);
void PopUIFont(void);
void UIRecordedView_UpdateAll();
void UIRecordedView_Shutdown(void);
void UIRecordedView_TooltipLevelText(const char* fmt, u32 count, bb_log_level_e logLevel);
bool UIRecordedView_EnableTiledViews(void);
void UIRecordedView_TiledViewCheckbox(void);

extern const char* textColorNames[];
extern const char* normalColorStr;
extern const char* warningColorStr;
extern const char* errorColorStr;
