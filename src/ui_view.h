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

namespace ImGui
{
	enum verticalScrollDir_e : int;
}

typedef struct bb_decoded_packet_s bb_decoded_packet_t;
typedef struct recorded_category_s recorded_category_t;
typedef struct recorded_log_s recorded_log_t;
typedef struct recorded_session_s recorded_session_t;
typedef struct view_log_s view_log_t;
typedef struct view_s view_t;

void PushLogFont(void);
void PopLogFont(void);
void PushUIFont(void);
void PopUIFont(void);
void UIRecordedView_UpdateAll();
void UIRecordedView_Shutdown(void);
void UIRecordedView_TooltipLevelText(const char* fmt, u32 count, bb_log_level_e logLevel);
bool UIRecordedView_EnableTiledViews(void);
void UIRecordedView_TiledViewCheckbox(void);
void UIRecordedView_UpdateScrolling(view_t *view, b32 logsHovered, b32 otherControlFocused, float lineHeight, ImGui::verticalScrollDir_e verticalScrollDir);
void UIRecordedView_SetLogTooltip(bb_decoded_packet_t* decoded, recorded_category_t* category, recorded_session_t* session, view_t* view, recorded_log_t* sessionLog);
void UIRecordedView_Logs_ClearSelection(view_t* view);
void UIRecordedView_Logs_AddSelection(view_t* view, view_log_t* log);
void UIRecordedView_LogPopup(view_t* view, view_log_t* viewLog);

extern const char* textColorNames[];
extern const char* normalColorStr;
extern const char* warningColorStr;
extern const char* errorColorStr;
