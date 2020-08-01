// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "wrap_imgui.h"

void UIRecordedView_UpdateAll(bool autoTileViews);
void UIRecordedView_Shutdown(void);
void UIRecordedView_TooltipLevelText(const char *fmt, u32 count, bb_log_level_e logLevel);

const char kColorKeyPrefix = '^';
const char kFirstColorKey = '0';
const char kLastColorKey = '=';
enum {
	kNumColorKeys = kLastColorKey - kFirstColorKey + 1,
	kColorKeyOffset = 1
};

extern const char *textColorNames[];
extern const char *normalColorStr;
extern const char *warningColorStr;
extern const char *errorColorStr;
