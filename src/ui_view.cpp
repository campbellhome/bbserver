// Copyright (c) 2012-2020 Matt Campbell
// MIT license (see License.txt)

#include "ui_view.h"
#include "bb_array.h"
#include "bb_colors.h"
#include "bb_string.h"
#include "bb_wrap_stdio.h"
#include "bbserver_utils.h"
#include "fonts.h"
#include "imgui_core.h"
#include "imgui_themes.h"
#include "imgui_tooltips.h"
#include "imgui_utils.h"
#include "keys.h"
#include "message_queue.h"
#include "path_utils.h"
#include "recorded_session.h"
#include "recordings.h"
#include "sb.h"
#include "time_utils.h"
#include "ui_config.h"
#include "ui_loglevel_colorizer.h"
#include "ui_message_box.h"
#include "ui_recordings.h"
#include "ui_tags.h"
#include "ui_view_files.h"
#include "ui_view_pie_instances.h"
#include "ui_view_threads.h"
#include "va.h"
#include "view.h"
#include "view_config.h"
#include "wrap_imgui.h"
#include "wrap_imgui_internal.h"

BB_WARNING_PUSH(4820)
#include "span.h"
#include "tokenize.h"
#include "ui_view_console_command.h"
#include <math.h>
BB_WARNING_POP

#include "imgui_text_shadows.h"
#include "parson/parson.h"

extern float g_messageboxHeight;

typedef struct gathered_views_s {
	u32 count;
	u32 allocated;
	view_t **data;
} gathered_views_t;

static gathered_views_t s_gathered_views;
static sb_t s_strippedLine;
static float s_lastDpiScale = 1.0f;
static float s_textColumnCursorPosX;

using namespace ImGui;

enum {
	kColorIndexNormal = 8,
	kColorIndexWarning = 4,
	kColorIndexError = 2,
};

const char *normalColorStr = "^7";
const char *warningColorStr = "^3";
const char *errorColorStr = "^1";

const char *textColorNames[] = {
	"Default",                    //    kBBColor_Default,
	"Evergreen Black",            // ^0 kBBColor_Evergreen_Black,
	"Evergreen Red",              // ^1 kBBColor_Evergreen_Red,
	"Evergreen Green",            // ^2 kBBColor_Evergreen_Green,
	"Evergreen Yellow",           // ^3 kBBColor_Evergreen_Yellow,
	"Evergreen Blue",             // ^4 kBBColor_Evergreen_Blue,
	"Evergreen Cyan",             // ^5 kBBColor_Evergreen_Cyan,
	"Evergreen Pink",             // ^6 kBBColor_Evergreen_Pink,
	"Evergreen White",            // ^7 kBBColor_Evergreen_White,
	"Evergreen Light Blue",       // ^8 kBBColor_Evergreen_LightBlue,
	"Evergreen Orange",           // ^9 kBBColor_Evergreen_Orange,
	"Evergreen Light Blue (alt)", // ^: kBBColor_Evergreen_LightBlueAlt,
	"Evergreen Orange (alt)",     // ^; kBBColor_Evergreen_OrangeAlt,
	"Evergreen Medium Blue",      // ^< kBBColor_Evergreen_MediumBlue,
	"Evergreen Amber",            // ^= kBBColor_Evergreen_Amber,
	"UE4 Black",                  //    kBBColor_UE4_Black,
	"UE4 DarkRed",                //    kBBColor_UE4_DarkRed,
	"UE4 DarkGreen",              //    kBBColor_UE4_DarkGreen,
	"UE4 DarkBlue",               //    kBBColor_UE4_DarkBlue,
	"UE4 DarkYellow",             //    kBBColor_UE4_DarkYellow,
	"UE4 DarkCyan",               //    kBBColor_UE4_DarkCyan,
	"UE4 DarkPurple",             //    kBBColor_UE4_DarkPurple,
	"UE4 Gray",                   //    kBBColor_UE4_DarkWhite,
	"UE4 Red",                    //    kBBColor_UE4_Red,
	"UE4 Green",                  //    kBBColor_UE4_Green,
	"UE4 Blue",                   //    kBBColor_UE4_Blue,
	"UE4 Yellow",                 //    kBBColor_UE4_Yellow,
	"UE4 Cyan",                   //    kBBColor_UE4_Cyan,
	"UE4 Purple",                 //    kBBColor_UE4_Purple,
	"UE4 White",                  //    kBBColor_UE4_White,
};
BB_CTASSERT(BB_ARRAYSIZE(textColorNames) == kBBColor_Count);

static const char *logLevelNames[] = {
	"Log",         // kBBLogLevel_Log,
	"Warning",     // kBBLogLevel_Warning,
	"Error",       // kBBLogLevel_Error,
	"Display",     // kBBLogLevel_Display,
	"SetColor",    // kBBLogLevel_SetColor,
	"VeryVerbose", // kBBLogLevel_VeryVerbose,
	"Verbose",     // kBBLogLevel_Verbose,
	"Fatal",       // kBBLogLevel_Fatal,
};
BB_CTASSERT(BB_ARRAYSIZE(logLevelNames) == kBBLogLevel_Count);

static void PushLogFont()
{
	ImFontAtlas *fonts = ImGui::GetIO().Fonts;
	if(fonts->Fonts.size() > 1) {
		ImGui::PushFont(fonts->Fonts[1]);
	} else {
		ImGui::PushFont(fonts->Fonts[0]);
	}
}

static void PopLogFont()
{
	ImGui::PopFont();
}

static void PushUIFont()
{
	ImFontAtlas *fonts = ImGui::GetIO().Fonts;
	ImGui::PushFont(fonts->Fonts[0]);
}

static void PopUIFont()
{
	ImGui::PopFont();
}

inline char *strrstr(char *haystack, const char *needle, size_t advance)
{
	char *match = NULL;
	while((haystack = strstr(haystack, needle)) != NULL) {
		match = haystack;
		haystack += advance;
	}
	return match;
}

inline const char *GetCategoryLeafName(recorded_category_t *category)
{
	const char *categoryName = strrstr(category->categoryName, "::", 2);
	return (categoryName) ? categoryName + 2 : category->categoryName;
}

static inline char *GetSessionThreadName(recorded_session_t *session, u64 threadId)
{
	recorded_thread_t *t = recorded_session_find_thread(session, threadId);
	return t ? t->threadName : va("%d", threadId);
}

static inline char *GetSessionFilePath(recorded_session_t *session, u32 fileId)
{
	recorded_filename_t *f = recorded_session_find_filename(session, fileId);
	return f ? f->path : va("%d", f);
}

static const char *GetFilenameFromPath(const char *path)
{
	const char *sep = strrchr(path, '/');
	if(sep)
		return sep + 1;
	sep = strrchr(path, '\\');
	if(sep)
		return sep + 1;
	return path;
}

static inline const char *GetSessionFileName(recorded_session_t *session, u32 fileId)
{
	return GetFilenameFromPath(GetSessionFilePath(session, fileId));
}

static inline void ClearViewTail(view_t *view, const char *reason)
{
	if(view->tail) {
		view->tail = false;
		BB_LOG("Debug", "Disabled tail for '%s' - %s\n", view->session->appInfo.packet.appInfo.applicationName, reason);
	}
}

static SYSTEMTIME SystemTimeFromDecoded(recorded_session_t *session, bb_decoded_packet_t *decoded)
{
	// Windows uses 100 nanosecond intervals since Jan 1, 1601 UTC
	// https://support.microsoft.com/en-us/help/167296/how-to-convert-a-unix-time-t-to-a-win32-filetime-or-systemtime
	s64 elapsedTicks = (s64)decoded->header.timestamp - (s64)session->appInfo.header.timestamp;
	double elapsedMillis = elapsedTicks * session->appInfo.packet.appInfo.millisPerTick;
	u64 epoch100Nanoseconds = session->appInfo.packet.appInfo.microsecondsFromEpoch * 10 +
	                          (u64)(elapsedMillis * 10000);
	u64 win32100Nanoseconds = epoch100Nanoseconds + 116444736000000000ULL;

	FILETIME ft;
	ft.dwLowDateTime = (DWORD)win32100Nanoseconds;
	ft.dwHighDateTime = win32100Nanoseconds >> 32;
	FileTimeToLocalFileTime(&ft, &ft);

	SYSTEMTIME st = {};
	FileTimeToSystemTime(&ft, &st);
	return st;
}

const char *DateFromDecoded(recorded_session_t *session, bb_decoded_packet_t *decoded)
{
	char buffer[64] = "";
	if(session->appInfo.type >= kBBPacketType_AppInfo_v3) {
		SYSTEMTIME st = SystemTimeFromDecoded(session, decoded);
		GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buffer, sizeof(buffer));
		return va("%s", buffer);
	} else {
		return "";
	}
}

const char *TimeFromDecoded(recorded_session_t *session, bb_decoded_packet_t *decoded)
{
	char buffer[64] = "";
	if(session->appInfo.type >= kBBPacketType_AppInfo_v3) {
		SYSTEMTIME st = SystemTimeFromDecoded(session, decoded);
		GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, buffer, sizeof(buffer));
		return va("%s %u ms", buffer, st.wMilliseconds);
	} else {
		return "";
	}
}

static const char *BuildLogColumnText(view_t *view, view_log_t *viewLog, view_column_e column)
{
	u32 viewLogIndex = (u32)(viewLog - view->visibleLogs.data);
	u32 prevViewLogIndex;
	u32 sessionLogIndex = viewLog->sessionLogIndex;
	recorded_session_t *session = view->session;
	recorded_log_t *sessionLog = session->logs.data[sessionLogIndex];
	view_log_t *prevViewLog = nullptr;
	recorded_log_t *prevSessionLog;
	bb_decoded_packet_t *decoded = &sessionLog->packet;
	recorded_category_t *category;
	s64 elapsedTicks, prevElapsedTicks;
	double elapsedMillis, deltaMillis;

	switch(column) {
	case kColumn_AbsIndex:
		return va("%u", sessionLog->sessionLogIndex);
	case kColumn_ViewIndex:
		return va("%u", viewLog->viewLogIndex);
	case kColumn_Date:
		return DateFromDecoded(session, decoded);
	case kColumn_Time:
		return TimeFromDecoded(session, decoded);
	case kColumn_AbsMilliseconds:
		elapsedTicks = (s64)decoded->header.timestamp - (s64)session->appInfo.header.timestamp;
		elapsedMillis = elapsedTicks * session->appInfo.packet.appInfo.millisPerTick;
		return va("%.2f", elapsedMillis);
	case kColumn_AbsDeltaMilliseconds:
		elapsedTicks = (s64)decoded->header.timestamp - (s64)session->appInfo.header.timestamp;
		prevSessionLog = (sessionLogIndex) ? session->logs.data[sessionLogIndex - 1] : nullptr;
		prevElapsedTicks = (prevSessionLog) ? (s64)prevSessionLog->packet.header.timestamp - (s64)session->appInfo.header.timestamp : 0;
		deltaMillis = (elapsedTicks - prevElapsedTicks) * session->appInfo.packet.appInfo.millisPerTick;
		return va("%+.2f", deltaMillis);
	case kColumn_ViewDeltaMilliseconds:
		elapsedTicks = (s64)decoded->header.timestamp - (s64)session->appInfo.header.timestamp;
		prevViewLogIndex = viewLogIndex;
		while(prevViewLogIndex) {
			--prevViewLogIndex;
			if(view->visibleLogs.data[prevViewLogIndex].viewLogIndex != viewLog->viewLogIndex) {
				prevViewLog = view->visibleLogs.data + prevViewLogIndex;
				break;
			}
		}
		prevSessionLog = (prevViewLog) ? session->logs.data[prevViewLog->sessionLogIndex] : nullptr;
		prevElapsedTicks = (prevSessionLog) ? (s64)prevSessionLog->packet.header.timestamp - (s64)session->appInfo.header.timestamp : elapsedTicks;
		deltaMillis = (elapsedTicks - prevElapsedTicks) * session->appInfo.packet.appInfo.millisPerTick;
		return va("%+.2f", deltaMillis);
	case kColumn_Filename:
		return GetSessionFileName(session, decoded->header.fileId);
	case kColumn_Thread:
		return va("%s", GetSessionThreadName(session, decoded->header.threadId));
	case kColumn_Category:
		category = recorded_session_find_category(session, decoded->packet.logText.categoryId);
		return va("%s", category ? GetCategoryLeafName(category) : "");
	case kColumn_PIEInstance:
		if(view_pieInstance_t *pieInstance = view_find_pieInstance(view, decoded->packet.logText.pieInstance)) {
			return (pieInstance->primary) ? "" : va("%d", pieInstance->pieInstance);

		} else {
			return "";
		}
	case kColumn_Count:
	default:
		BB_ASSERT(false);
		return "";
	}
}

static size_t s_logColumnWidth[kColumn_Count];
static void ResetLogColumnWidth()
{
	for(u32 i = 0; i < kColumn_Count; ++i) {
		s_logColumnWidth[i] = 0;
	}
}
static void UpdateLogColumnWidth(view_t *view, view_log_t *viewLog)
{
	for(u32 i = 0; i < kColumn_Count; ++i) {
		view_column_e column = (view_column_e)i;
		if(view->columns[column].visible) {
			const char *text = BuildLogColumnText(view, viewLog, column);
			size_t len = strlen(text);
			if(s_logColumnWidth[column] < len) {
				s_logColumnWidth[column] = len;
			}
		}
	}
}

sb_t StripColorCodes(span_t span /*, bool bSingleLine*/)
{
	s_strippedLine.count = 0;
	if(s_strippedLine.data) {
		s_strippedLine.data[0] = '\0';
	}
	const char *text = span.start;
	if(text) {
		while(*text && text < span.end) {
			//if(bSingleLine && (*text == '\r' || *text == '\n'))
			//	break;
			if(*text == kColorKeyPrefix && text[1] >= kFirstColorKey && text[1] <= kLastColorKey) {
				++text;
			} else if(text[0] == '^' && text[1] == 'F') {
				++text;
			} else {
				sb_append_char(&s_strippedLine, *text);
			}
			++text;
		}
	}
	return s_strippedLine;
}

static b32 line_can_be_json(sb_t line)
{
	b32 result = false;
	if(line.data && line.count > 2) {
		if(line.data[0] == '{' && line.data[line.count - 2] == '}') {
			result = true;
		} else if(line.data[0] == '[' && line.data[line.count - 2] == ']') {
			result = true;
		}
	}
	return result;
}

enum crlf_e {
	kNoCRLF,
	kAppendCRLF,
};
enum columnSpacer_e {
	kColumnSpacer_Spaces,
	kColumnSpacer_Tab,
};
enum jsonExpansion_e {
	kJsonExpansion_Off,
	kJsonExpansion_On,
};
static void BuildLogLine(view_t *view, view_log_t *viewLog, bool allColumns, sb_t *sb, crlf_e crlf, columnSpacer_e spacer, jsonExpansion_e jsonExpansion)
{
	u32 logIndex = viewLog->sessionLogIndex;
	u32 subLine = viewLog->subLine;
	recorded_session_t *session = view->session;
	recorded_log_t *sessionLog = session->logs.data[logIndex];
	bb_decoded_packet_t *decoded = &sessionLog->packet;

	if(allColumns) {
		int i;
		for(i = 0; i < kColumn_Count; ++i) {
			view_column_e column = (view_column_e)i;
			if(view->columns[column].visible) {
				const char *line = BuildLogColumnText(view, viewLog, column);
				sb_append(sb, line);
				if(spacer == kColumnSpacer_Spaces) {
					size_t target = s_logColumnWidth[column] + 2;
					size_t len = strlen(line);
					while(len < target) {
						sb_append_char(sb, ' ');
						++len;
					}
				} else {
					sb_append_char(sb, '\t');
				}
			}
		}
	}

	span_t line = tokenizeNthLine(span_from_string(decoded->packet.logText.text), subLine);
	sb_t stripped = StripColorCodes(line);
	bool bJson = false;
	if(!allColumns && jsonExpansion == kJsonExpansion_On && line_can_be_json(stripped)) {
		JSON_Value *val = json_parse_string(sb_get(&stripped));
		if(val) {
			char *json = json_serialize_to_string_pretty(val);
			if(json) {
				bJson = true;
				sb_append(sb, json);
				json_free_serialized_string(json);
			}
			json_value_free(val);
		}
	}
	if(!bJson) {
		sb_append(sb, sb_get(&stripped));
	}

	if(crlf == kAppendCRLF) {
		sb_append(sb, "\r\n");
	}
}

static void CopySelectedLogsToClipboard(view_t *view, bool allColumns, columnSpacer_e spacer)
{
	u32 i;
	sb_t sb;
	sb_init(&sb);
	if(allColumns) {
		ResetLogColumnWidth();
		for(i = 0; i < view->visibleLogs.count; ++i) {
			view_log_t *log = view->visibleLogs.data + i;
			if(log->selected) {
				UpdateLogColumnWidth(view, log);
			}
		}
	}
	for(i = 0; i < view->visibleLogs.count; ++i) {
		view_log_t *log = view->visibleLogs.data + i;
		if(log->selected) {
			BuildLogLine(view, log, allColumns, &sb, kAppendCRLF, spacer, kJsonExpansion_On);
		}
	}
	const char *clipboardText = sb_get(&sb);
	SetClipboardText(clipboardText);
	sb_reset(&sb);
}

static void UIRecordedView_SaveLog(view_t *view, bool allColumns, columnSpacer_e spacer)
{
	sb_t sb;
	sb_init(&sb);
	if(allColumns) {
		ResetLogColumnWidth();
		for(u32 i = 0; i < view->visibleLogs.count; ++i) {
			view_log_t *log = view->visibleLogs.data + i;
			UpdateLogColumnWidth(view, log);
		}
	}
	for(u32 i = 0; i < view->visibleLogs.count; ++i) {
		view_log_t *log = view->visibleLogs.data + i;
		BuildLogLine(view, log, allColumns, &sb, kAppendCRLF, spacer, kJsonExpansion_On);
	}
	if(sb.count > 1) {
		sb_t path;
		sb_init(&path);
		sb_append(&path, view->session->path);
		char *ext = strrchr(path.data, '.');
		if(ext) {
			path.count = (u32)(ext + 1 - path.data);
		}
		sb_append(&path, ".log");
		if(path.count > 1) {
			FILE *fp = fopen(path.data, "wb");
			if(fp) {
				fwrite(sb.data, sb.count - 1, 1, fp);
				fclose(fp);
			} else {
				BB_ERROR("View", "Failed to save '%s'", path.data);
			}
		}
		sb_reset(&path);
	}
	sb_reset(&sb);
}

static void UIRecordedView_Logs_ClearSelection(view_t *view)
{
	view_logs_t *logs = &view->visibleLogs;
	UITags_Category_ClearSelection(view);
	for(u32 i = 0; i < logs->count; ++i) {
		logs->data[i].selected = false;
	}
}

static void UIRecordedView_Logs_SelectAll(view_t *view)
{
	view_logs_t *logs = &view->visibleLogs;
	UITags_Category_ClearSelection(view);
	for(u32 i = 0; i < logs->count; ++i) {
		logs->data[i].selected = true;
		UITags_Category_SetSelected(view, view_get_log_category_index(view, logs->data + i), true);
	}
}

static void UIRecordedView_Logs_AddSelection(view_t *view, view_log_t *log)
{
	log->selected = true;
	view->visibleLogs.lastClickIndex = (u32)(log - view->visibleLogs.data);
	UITags_Category_SetSelected(view, view_get_log_category_index(view, log), true);
}

static void UIRecordedView_Logs_ToggleSelection(view_t *view, view_log_t *log)
{
	log->selected = !log->selected;
	view->visibleLogs.lastClickIndex = log->selected ? (u32)(log - view->visibleLogs.data) : ~0u;
	UITags_Category_SetSelected(view, view_get_log_category_index(view, log), log->selected);
}

static void UIRecordedView_Logs_HandleClick(view_t *view, view_log_t *log)
{
	ImGuiIO &io = ImGui::GetIO();
	if(io.KeyAlt || (io.KeyCtrl && io.KeyShift))
		return;

	view->bookmarkThreshold = (int)log->viewLogIndex;

	if(io.KeyCtrl) {
		UIRecordedView_Logs_ToggleSelection(view, log);
	} else if(io.KeyShift) {
		view_logs_t *logs = &view->visibleLogs;
		if(logs->lastClickIndex < logs->count) {
			u32 startIndex = logs->lastClickIndex;
			u32 endIndex = (u32)(log - logs->data);
			logs->lastClickIndex = endIndex;
			if(endIndex < startIndex) {
				u32 tmp = endIndex;
				endIndex = startIndex;
				startIndex = tmp;
			}
			for(u32 i = startIndex; i <= endIndex; ++i) {
				view_log_t *otherLog = logs->data + i;
				otherLog->selected = true;
				u32 viewCategoryIndex = view_get_log_category_index(view, otherLog);
				UITags_Category_SetSelected(view, viewCategoryIndex, true);
			}
		}
	} else {
		UIRecordedView_Logs_ClearSelection(view);
		UIRecordedView_Logs_AddSelection(view, log);
	}
}

static void UIRecordedView_OpenContainingFolder(view_t *view)
{
	sb_t dir = sb_from_c_string(view->session->path);
	path_remove_filename(&dir);
	BBServer_OpenDirInExplorer(sb_get(&dir));
	sb_reset(&dir);
}

void UIRecordedView_TooltipLevelText(const char *fmt, u32 count, bb_log_level_e logLevel)
{
	if(count || logLevel == kBBLogLevel_Log) {
		LogLevelColorizer colorizer(logLevel);
		ScopedTextShadows shadows(logLevel);
		ImGui::TextShadowed(va(fmt, count));
	}
}

static void SetLogTooltip(bb_decoded_packet_t *decoded, recorded_category_t *category, recorded_session_t *session, view_t *view)
{
	if(IsTooltipActive()) {
		BeginTooltip();
		PushUIFont();
		if(session->appInfo.type >= kBBPacketType_AppInfo_v3) {
			TextShadowed(va("Timestamp: %s %s", DateFromDecoded(session, decoded), TimeFromDecoded(session, decoded)));
		} else {
			TextShadowed(va("Timestamp: %" PRIu64, decoded->header.timestamp));
		}
		TextShadowed(va("Category: %s", category->categoryName));
		TextShadowed(va("Verbosity: %s", decoded->packet.logText.level < kBBLogLevel_Count ? logLevelNames[decoded->packet.logText.level] : "(unknown)"));
		if(decoded->header.line) {
			TextShadowed(va("Source: %s:%u", GetSessionFilePath(session, decoded->header.fileId), decoded->header.line));
		} else {
			TextShadowed(va("Source: %s", GetSessionFilePath(session, decoded->header.fileId)));
		}
		TextShadowed(va("Thread: %s", GetSessionThreadName(session, decoded->header.threadId)));
		if(view_pieInstance_t *pieInstance = view_find_pieInstance(view, decoded->packet.logText.pieInstance)) {
			if(!pieInstance->primary) {
				TextShadowed(va("PIE Instance: %d", decoded->packet.logText.pieInstance));
			}
		}
		Separator();
		PopUIFont();
		span_t cursor = span_from_string(decoded->packet.logText.text);
		for(span_t line = tokenizeLine(&cursor); line.start; line = tokenizeLine(&cursor)) {
			bool bJson = false;
			sb_t stripped = StripColorCodes(line);
			if(line_can_be_json(stripped)) {
				JSON_Value *val = json_parse_string(sb_get(&stripped));
				if(val) {
					char *json = json_serialize_to_string_pretty(val);
					if(json) {
						bJson = true;
						PushTextWrapPos(1800.0f);
						// TODO: shadows are misplaced with json
						//if(Imgui_Core_GetTextShadows()) {
						//	TextWrappedShadowed(va("%s", json));
						//} else {
						TextWrapped("%s", json);
						//}
						PopTextWrapPos();
						json_free_serialized_string(json);
					}
					json_value_free(val);
				}
			}
			if(!bJson) {
				PushTextWrapPos(1200.0f);
				if(Imgui_Core_GetTextShadows()) {
					TextWrappedShadowed(va("%s", sb_get(&stripped)));
				} else {
					TextWrapped("%s", sb_get(&stripped));
				}
				PopTextWrapPos();
			}
		}
		EndTooltip();
	}
}

typedef struct colored_text_s {
	const char *start;
	const char *end;
	const char *next;
	int len;
	ImColor color;
	styleColor_e styleColor;
	b32 blink;
	u8 pad[4];
} colored_text_t;

static colored_text_t UIRecordedView_GetColoredTextInternal(colored_text_t prev)
{
	const char *start = prev.next;
	const char *marker = start;
	colored_text_t ret = { BB_EMPTY_INITIALIZER };
	ret.end = prev.end;
	if(!start || !*start) {
		return ret;
	}
	ret.styleColor = prev.styleColor;
	ret.color = prev.color;
	ret.blink = prev.blink;

	if(*marker == kColorKeyPrefix && marker[1] >= kFirstColorKey && marker[1] <= kLastColorKey) {
		int colorIndex = marker[1] - kFirstColorKey;
		ret.start = start + 2;
		ret.next = marker + 2;
		ret.styleColor = (styleColor_e)(colorIndex + kColorKeyOffset);
		ret.color = MakeColor(ret.styleColor);
		marker += 2;
	} else if(*marker == '^' && marker[1] == 'F') {
		ret.start = start + 2;
		ret.next = marker + 2;
		ret.blink = !prev.blink;
		marker += 2;
	} else {
		ret.start = start;
	}

	while(*marker) {
		if(*marker == kColorKeyPrefix && marker[1] >= kFirstColorKey && marker[1] <= kLastColorKey ||
		   *marker == '^' && marker[1] == 'F') {
			ret.next = marker;
			ret.len = (int)(marker - ret.start);
			return ret;
		} else {
			//char c = *marker;
			++marker;
			/*
			if(c == '\n') {
				ret.len = (int)(marker - ret.start);
				ret.next = nullptr;
				return ret;
			}
			*/
		}
	}
	if(marker > ret.start) {
		ret.len = (int)(marker - ret.start);
		ret.next = marker;
	}
	return ret;
}

static colored_text_t UIRecordedView_GetColoredText(colored_text_t prev)
{
	colored_text_t ret = UIRecordedView_GetColoredTextInternal(prev);
	if(ret.start && ret.start + ret.len > ret.end) {
		ret.len = (int)(ret.end - ret.start);
		ret.next = nullptr;
	}
	return ret;
}

static void UIRecordedView_SpanFromSelection(view_t *view, u32 startIndex, bool before, bool after)
{
	recorded_session_t *session = view->session;
	const char *applicationName = session->appInfo.packet.appInfo.applicationName;
	u32 start = startIndex;
	u32 end = startIndex;
	for(u32 i = 0; i < view->visibleLogs.count; ++i) {
		view_log_t *log = view->visibleLogs.data + i;
		if(log->selected) {
			start = BB_MIN(start, log->sessionLogIndex);
			end = BB_MAX(end, log->sessionLogIndex);
		}
	}
	sb_reset(&view->config.spansInput);
	if(before) {
		sb_va(&view->config.spansInput, "-%u", end);
	} else if(after) {
		sb_va(&view->config.spansInput, "%u-", start);
	} else {
		sb_va(&view->config.spansInput, "%u-%u", start, end);
	}
	view->visibleLogsDirty = true;
	view->spansActive = true;
	BB_LOG("Debug", "Set spans to '%s' for '%s'\n", sb_get(&view->config.spansInput), applicationName);
}

void UIRecordedView_LogPopup(view_t *view, view_log_t *viewLog)
{
	PushUIFont();

	u32 sessionLogIndex = viewLog->sessionLogIndex;
	recorded_session_t *session = view->session;
	recorded_log_t *sessionLog = session->logs.data[sessionLogIndex];
	recorded_filename_t *filename = recorded_session_find_filename(session, sessionLog->packet.header.fileId);
	if(ImGui::Selectable("Copy Text")) {
		CopySelectedLogsToClipboard(view, false, kColumnSpacer_Tab);
	}
	if(ImGui::Selectable("Copy All Columns")) {
		CopySelectedLogsToClipboard(view, true, kColumnSpacer_Spaces);
	}
	if(ImGui::Selectable("Copy All Columns (Tab-separated)")) {
		CopySelectedLogsToClipboard(view, true, kColumnSpacer_Tab);
	}
	if(session->logs.count) {
		if(ImGui::Selectable("Span: up to and including selection")) {
			UIRecordedView_SpanFromSelection(view, sessionLogIndex, true, false);
		}
		if(ImGui::Selectable("Span: selection")) {
			UIRecordedView_SpanFromSelection(view, sessionLogIndex, false, false);
		}
		if(ImGui::Selectable("Span: selection and following")) {
			UIRecordedView_SpanFromSelection(view, sessionLogIndex, false, true);
		}
	}
	if(filename) {
		for(u32 i = 0; i < g_config.openTargets.count; ++i) {
			openTargetEntry_t *openTarget = g_config.openTargets.data + i;
			if(sb_get(&openTarget->displayName) && sb_get(&openTarget->commandLine)) {
				if(ImGui::Selectable(sb_get(&openTarget->displayName))) {
					sb_t sb;
					sb_init(&sb);
					const char *src = sb_get(&openTarget->commandLine);
					while(src && *src) {
						const char *file = bb_stristr(src, "{File}");
						const char *line = bb_stristr(src, "{Line}");
						const char *end = nullptr;
						if(file && line) {
							end = (file < line) ? file : line;
						} else if(file) {
							end = file;
						} else {
							end = line;
						}
						if(end) {
							sb_append_range(&sb, src, end);
							if(end == file) {
								pathFixupEntry_t *fixup = nullptr;
								for(u32 fixupIndex = 0; fixupIndex < g_config.pathFixups.count; ++fixupIndex) {
									pathFixupEntry_t *entry = g_config.pathFixups.data + fixupIndex;
									if(bb_strnicmp(sb_get(&entry->src), filename->path, entry->src.count) == 0) {
										fixup = entry;
									}
								}
								if(fixup) {
									sb_append(&sb, sb_get(&fixup->dst));
									sb_append(&sb, filename->path + fixup->src.count);
								} else {
									sb_append(&sb, filename->path);
								}
								src = file + 6;
							} else if(end == line) {
								sb_append(&sb, va("%u", sessionLog->packet.header.line));
								src = line + 6;
							} else {
								break;
							}
						} else {
							sb_append(&sb, src);
							break;
						}
					}
					STARTUPINFOA startupInfo;
					memset(&startupInfo, 0, sizeof(startupInfo));
					startupInfo.cb = sizeof(startupInfo);
					PROCESS_INFORMATION procInfo;
					memset(&procInfo, 0, sizeof(procInfo));
					BOOL ret = CreateProcessA(nullptr, sb.data, nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS, nullptr, nullptr, &startupInfo, &procInfo);
					if(!ret) {
						BB_ERROR("View", "Failed to create process for '%s'", sb.data);
					} else {
						BB_LOG("View", "Created process for '%s'", sb.data);
					}
					CloseHandle(procInfo.hThread);
					CloseHandle(procInfo.hProcess);
					sb_reset(&sb);
				}
			}
		}
	}
	if(ImGui::Selectable("Hide this category")) {
		bb_decoded_packet_t *decoded = &sessionLog->packet;
		recorded_category_t *recordedCategory = recorded_session_find_category(session, decoded->packet.logText.categoryId);
		view_category_t *viewCategory = view_find_category_by_name(view, recordedCategory->categoryName);
		viewCategory->visible = false;
		view->visibleLogsDirty = true;
	}
	if(ImGui::Selectable("Hide all but this category")) {
		view_set_all_category_visibility(view, false);
		bb_decoded_packet_t *decoded = &sessionLog->packet;
		recorded_category_t *recordedCategory = recorded_session_find_category(session, decoded->packet.logText.categoryId);
		view_category_t *viewCategory = view_find_category_by_name(view, recordedCategory->categoryName);
		viewCategory->visible = true;
	}

	PopUIFont();
}

float UIRecordedView_LogLine(view_t *view, view_log_t *viewLog, float textOffset, verticalScrollDir_e &scrollDir)
{
	int i;
	u32 logIndex = viewLog->sessionLogIndex;
	recorded_session_t *session = view->session;
	recorded_log_t *sessionLog = session->logs.data[logIndex];
	bb_decoded_packet_t *decoded = &sessionLog->packet;
	recorded_category_t *category = recorded_session_find_category(session, decoded->packet.logText.categoryId);
	LogLevelColorizer colorizer((bb_log_level_e)decoded->packet.logText.level);

	const configColorUsage colorUsage = g_config.logColorUsage;
	ImColor targetBgColor;
	if(viewLog->bookmarked) {
		if(g_config.alternateRowBackground) {
			targetBgColor = (viewLog->viewLogIndex % 2)
			                    ? MakeBackgroundTintColor(kStyleColor_LogBackground_BookmarkedAlternate0, ImColor(0.0f, 1.0f, 0.0f, 0.15f))
			                    : MakeBackgroundTintColor(kStyleColor_LogBackground_BookmarkedAlternate1, ImColor(0.0f, 0.8f, 0.0f, 0.15f));
		} else {
			targetBgColor = MakeBackgroundTintColor(kStyleColor_LogBackground_Bookmarked, ImColor(0.0f, 1.0f, 0.0f, 0.15f));
		}
	} else {
		if(g_config.alternateRowBackground) {
			targetBgColor = (viewLog->viewLogIndex % 2)
			                    ? MakeBackgroundTintColor(kStyleColor_LogBackground_NormalAlternate0, ImColor(1.0f, 1.0f, 1.0f, 0.025f))
			                    : MakeBackgroundTintColor(kStyleColor_LogBackground_NormalAlternate1, ImColor(0.0f, 0.0f, 0.0f, 0.025f));
		} else {
			targetBgColor = MakeBackgroundTintColor(kStyleColor_LogBackground_Normal, ImColor(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}
	ImVec4 bgColor = (decoded->packet.logText.colors.bg == kBBColor_Default ||
	                  colorUsage == kConfigColors_BgAsFg ||
	                  colorUsage == kConfigColors_None ||
	                  colorUsage == kConfigColors_NoBg)
	                     ? ImVec4(ImColor(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)))
	                     : MakeColor((styleColor_e)(decoded->packet.logText.colors.bg));
	bgColor.x = bgColor.x * (1 - targetBgColor.Value.w) + targetBgColor.Value.x * targetBgColor.Value.w;
	bgColor.y = bgColor.y * (1 - targetBgColor.Value.w) + targetBgColor.Value.y * targetBgColor.Value.w;
	bgColor.z = bgColor.z * (1 - targetBgColor.Value.w) + targetBgColor.Value.z * targetBgColor.Value.w;

	int styleCount = 0;
	//ImGui::PushStyleColor(ImGuiCol_HeaderInactive, bgColor);
	if(viewLog->bookmarked) {
		styleCount += 3;
		ImColor headerColor = ImGui::GetColorU32(ImGuiCol_Header);
		headerColor.Value.y += 0.1f;
		ImGui::PushStyleColor(ImGuiCol_Header, headerColor);

		ImColor headerActiveColor = GetColorU32(ImGuiCol_HeaderActive);
		headerActiveColor.Value.y += 0.1f;
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerActiveColor);

		ImColor headerHoveredColor = GetColorU32(ImGuiCol_HeaderHovered);
		headerHoveredColor.Value.y += 0.1f;
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerHoveredColor);
	}
	SelectableWithBackground(va("###%u_%u", viewLog->sessionLogIndex, viewLog->subLine), viewLog->selected != 0, bgColor);
	ImGui::PopStyleColor(styleCount);
	if(ImGui::IsItemHovered()) {
		if(IsItemClicked()) {
			UIRecordedView_Logs_HandleClick(view, viewLog);
		}
		scrollDir = GetVerticalScrollDir();
	}
	if(!g_config.tooltips.onlyOverSelected || viewLog->selected) {
		ImGui::PushStyleColor(ImGuiCol_Text, MakeColor(kStyleColor_kBBColor_Default));
		if(ImGui::GetMousePos().x >= ImGui::GetWindowPos().x + s_textColumnCursorPosX - ImGui::GetScrollX()) {
			if(g_config.tooltips.overText) {
				SetLogTooltip(decoded, category, session, view);
			}
		} else {
			if(g_config.tooltips.overMisc) {
				SetLogTooltip(decoded, category, session, view);
			}
		}
		ImGui::PopStyleColor(1);
	}

	if(ImGui::BeginPopupContextItem(va("RecordedEntry_%u_%u_ContextMenu", logIndex, viewLog->subLine))) {
		if(!viewLog->selected) {
			UIRecordedView_Logs_ClearSelection(view);
			UIRecordedView_Logs_AddSelection(view, viewLog);
		}
		UIRecordedView_LogPopup(view, viewLog);
		ImGui::EndPopup();
	}

	float scrollX = ImGui::GetScrollX();
	bool oldShadows = false;
	if(viewLog->subLine != 0) {
		ImGui::PushStyleColor(ImGuiCol_Text, MakeColor(kStyleColor_Multiline));
		oldShadows = PushTextShadows(kStyleColor_Multiline);
	}
	for(i = 0; i < kColumn_Count; ++i) {
		view_column_e column = (view_column_e)i;
		if(view->columns[column].visible) {
			SameLine(view->columns[column].offset);

			float windowX = ImGui::GetWindowPos().x;
			float offset = view->columns[column].offset;
			float width = view->columns[column].width * Imgui_Core_GetDpiScale();

			float x1 = floorf(0.5f + windowX + offset - 1.0f) - scrollX;
			float x2 = floorf(0.5f + windowX + offset + width - 1.0f) - scrollX;
			ImGui::PushClipRect(ImVec2(x1, -FLT_MAX), ImVec2(x2, +FLT_MAX), true);
			TextShadowed(BuildLogColumnText(view, viewLog, column));
			ImGui::PopClipRect();
		}
	}
	if(viewLog->subLine != 0) {
		PopTextShadows(oldShadows);
		ImGui::PopStyleColor();
	}

	SameLine(textOffset);
	bool bNeedText = true;

	span_t subLineSpan = tokenizeNthLine(span_from_string(decoded->packet.logText.text), viewLog->subLine);

	bool first = true;

	colored_text_t span = { BB_EMPTY_INITIALIZER };
	if(colorUsage != kConfigColors_None) {
		span.styleColor = (styleColor_e)(decoded->packet.logText.colors.fg);
		if(colorUsage == kConfigColors_BgAsFg) {
			if(decoded->packet.logText.colors.bg != kBBColor_Default) {
				span.styleColor = (styleColor_e)(decoded->packet.logText.colors.bg);
			}
		}
	}
	if(decoded->packet.logText.colors.fg == kBBColor_Default ||
	   decoded->packet.logText.level == kBBLogLevel_Warning ||
	   decoded->packet.logText.level == kBBLogLevel_Error ||
	   decoded->packet.logText.level == kBBLogLevel_Fatal) {
		span.styleColor = GetStyleColorForLogLevel((bb_log_level_e)decoded->packet.logText.level);
	}
	span.color = MakeColor(span.styleColor);
	const ImColor fgColor = span.color;

	if(viewLog->subLine && subLineSpan.start) {
		colored_text_t other = { BB_EMPTY_INITIALIZER };
		other.color = fgColor;
		other.next = decoded->packet.logText.text;
		other.end = subLineSpan.start;
		do {
			other = UIRecordedView_GetColoredText(other);
			if(other.len && other.start) {
				span.color = other.color;
				span.blink = other.blink;
			}
		} while(other.next);
	}

	span.next = subLineSpan.start;
	span.end = subLineSpan.end;
	do {
		span = UIRecordedView_GetColoredText(span);
		if(g_config.logColorUsage == kConfigColors_None) {
			span.color = fgColor;
		}
		if(span.len && span.start) {
			if(first) {
				first = false;
			} else {
				SameLine(0.0f, 0.0f);
				bNeedText = true;
			}
			ImColor color = span.color;
			if(span.blink) {
				// we explicitly want the double version of sin() because if a machine has been up
				// for a few weeks, we lose all fractional precision in float32, making sinf()
				// stairstep.
				const double rate = 2.5;
				float s = (float)sin(Time_GetCurrentTime() * rate);
				float scale = fabsf(s);
				color.Value.w *= scale;
				Imgui_Core_RequestRender();
			}
			if(g_config.logColorUsage != kConfigColors_None) {
				oldShadows = PushTextShadows(span.styleColor);
			}
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			TextShadowed(va("%.*s", span.len, span.start));
			ImGui::PopStyleColor();
			if(g_config.logColorUsage != kConfigColors_None) {
				PopTextShadows(oldShadows);
			}
			bNeedText = false;
		}
	} while(span.next);

	if(bNeedText) {
		TextUnformatted("");
	}

	ImFont *font = GetFont();
	return textOffset + font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, decoded->packet.logText.text).x;
}

static void UIRecordedView_ColumnContextMenu(view_t *view, const char *menuName)
{
	if(ImGui::BeginPopup(menuName)) {
		for(u32 i = 0; i < kColumn_Count; ++i) {
			if(Checkbox(g_view_column_long_display_names[i], &view->columns[i].visible)) {
				recorded_session_t *session = view->session;
				const char *applicationName = session->appInfo.packet.appInfo.applicationName;
				BB_LOG("Debug", "Toggled %s for '%s'\n", g_view_column_config_names[i], applicationName);
				view_reset_column_offsets(view);
			}
		}
		ImGui::EndPopup();
	}
}

static float UIRecordedView_LogHeader(view_t *view)
{
	b32 anyActive = false;
	float indent = -1.0f;

	const char *columnNames[BB_ARRAYSIZE(view->columns)] = {};
	float columnOffsets[BB_ARRAYSIZE(view->columns) + 1] = {};
	float columnWidths[BB_ARRAYSIZE(view->columns)] = {};
	u32 columnIndices[BB_ARRAYSIZE(view->columns)] = {};

	u32 numColumns = 0;
	for(u32 i = 0; i < BB_ARRAYSIZE(view->columns); ++i) {
		if(view->columns[i].visible) {
			columnWidths[numColumns] = view->columns[i].width;
			columnNames[numColumns] = g_view_column_display_names[i];
			columnIndices[numColumns] = i;
			++numColumns;
		}
	}

	ImGui::columnDrawData data = {};
	data.columnWidths = columnWidths;
	data.columnOffsets = columnOffsets;
	data.columnNames = columnNames;
	data.numColumns = numColumns + 1;
	ImGui::TextUnformatted("");
	ImGui::SameLine(0.0f, indent);
	columnOffsets[0] = ImGui::GetCursorPosX();
	for(u32 i = 0; i < numColumns; ++i) {
		const char *contextMenuName = va("CategoriesContextMenu%u", i);
		ImGui::columnDrawResult res = ImGui::DrawColumnHeader(data, i, contextMenuName);
		anyActive = anyActive || res.active;
		UIRecordedView_ColumnContextMenu(view, contextMenuName);
	}

	for(u32 i = 0; i < numColumns; ++i) {
		view->columns[columnIndices[i]].width = columnWidths[i];
		view->columns[columnIndices[i]].offset = columnOffsets[i];
	}

	float textOffset = ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x;

	float startOffset = GetCursorPosX();
	s_textColumnCursorPosX = startOffset;
	ImGui::Button("###Text", kButton_ColumnHeaderNoSort, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
	const float itemPad = GetStyle().ItemSpacing.x;
	DrawColumnHeaderText(startOffset + GetStyle().ItemInnerSpacing.x, ImGui::GetContentRegionAvail().x - itemPad, "Text", nullptr, "CategoriesContextMenu");
	UIRecordedView_ColumnContextMenu(view, "CategoriesContextMenu");

	return textOffset;
}

static void PushExpandButtonColors(void)
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_CheckMark));
	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_FrameBg));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_FrameBgHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_FrameBgActive));
}

static void PopExpandButtonColors(void)
{
	ImGui::PopStyleColor(4);
}

static void DrawViewToggles(view_t *view, const char *applicationName)
{
//#define ICON_EXPAND_LEFT "<"
//#define ICON_EXPAND_RIGHT ">"
#define ICON_EXPAND_LEFT ICON_FK_CHEVRON_LEFT
#define ICON_EXPAND_RIGHT ICON_FK_CHEVRON_RIGHT

	PushExpandButtonColors();
	if(Button(view->config.showSelectorTarget ? ICON_EXPAND_LEFT "##ShowSelector" : ICON_EXPAND_RIGHT "##ShowSelector")) {
		view->config.showSelectorTarget = !view->config.showSelectorTarget;
		BB_LOG("Debug", "Toggled showSelector for '%s'\n", applicationName);
	}
	PopExpandButtonColors();

#undef ICON_EXPAND_LEFT
#undef ICON_EXPAND_RIGHT

	SameLine();
	if(Checkbox("VeryVerbose", &view->config.showVeryVerbose)) {
		view->visibleLogsDirty = true;
		BB_LOG("Debug", "Toggled showVeryVerbose for '%s'\n", applicationName);
	}
	SameLine();
	if(Checkbox("Verbose", &view->config.showVerbose)) {
		view->visibleLogsDirty = true;
		BB_LOG("Debug", "Toggled showVerbose for '%s'\n", applicationName);
	}
	SameLine();
	if(Checkbox("Logs", &view->config.showLogs)) {
		view->visibleLogsDirty = true;
		BB_LOG("Debug", "Toggled showLogs for '%s'\n", applicationName);
	}
	SameLine();
	if(Checkbox("Display", &view->config.showDisplay)) {
		view->visibleLogsDirty = true;
		BB_LOG("Debug", "Toggled showDisplay for '%s'\n", applicationName);
	}
	SameLine();
	if(Checkbox("Warnings", &view->config.showWarnings)) {
		view->visibleLogsDirty = true;
		BB_LOG("Debug", "Toggled showWarnings for '%s'\n", applicationName);
	}
	SameLine();
	if(Checkbox("Errors", &view->config.showErrors)) {
		view->visibleLogsDirty = true;
		BB_LOG("Debug", "Toggled showErrors for '%s'\n", applicationName);
	}
	SameLine();
	if(Checkbox("Fatal", &view->config.showFatal)) {
		view->visibleLogsDirty = true;
		BB_LOG("Debug", "Toggled showFatal for '%s'\n", applicationName);
	}
	SameLine();
	if(Checkbox("Tail", &view->tail)) {
		BB_LOG("Debug", "Toggled tail for '%s'\n", applicationName);
	}
	SameLine();

	if(Button("Columns...")) {
		OpenPopup("Columns");
	}
	if(BeginPopup("Columns")) {
		for(u32 i = 0; i < kColumn_Count; ++i) {
			if(Checkbox(g_view_column_long_display_names[i], &view->columns[i].visible)) {
				BB_LOG("Debug", "Toggled %s for '%s'\n", g_view_column_config_names[i], applicationName);
				view_reset_column_offsets(view);
			}
		}
		EndPopup();
	}

	SameLine();
	if(Checkbox("Auto Close", &view->autoClose)) {
		BB_LOG("Debug", "Toggled auto-close for '%s'\n", applicationName);
	}
	if(IsTooltipActive()) {
		SetTooltip("View will be auto-closed the next time a new session starts recording for the same application, if this view's session is already inactive");
	}
}

static char *UIRecordedView_GetViewId(view_t *view, bool autoTileViews)
{
	recorded_session_t *session = view->session;
	const char *applicationName = session->appInfo.packet.appInfo.applicationName;
	char *slash = strrchr(session->path, '\\');
	char *filename = (slash) ? slash + 1 : session->path;
	return va("%s##View_%s_%u_%d", applicationName, filename, view->viewId, autoTileViews);
}

static void view_close_and_write_config(view_t *view)
{
	if(view->open) {
		view->open = false;
		if(view_config_write(view)) {
			BB_LOG("View", "%s wrote config\n", view->session->appInfo.packet.appInfo.applicationName);
		} else {
			BB_ERROR("View", "%s failed to write config\n", view->session->appInfo.packet.appInfo.applicationName);
		}
	}
}

static void UIRecordedView_Update(view_t *view, bool autoTileViews)
{
	recorded_session_t *session = view->session;
	const char *applicationName = session->appInfo.packet.appInfo.applicationName;
	char *viewId = UIRecordedView_GetViewId(view, autoTileViews);
	bool initializedLogColumns = false;
	int windowFlags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus;
	b32 roundingPushed = false;
	if(autoTileViews) {
		PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		roundingPushed = true;
		if(view->tiled) {
			windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
		}
	}

	bool *viewOpen = (bool *)&view->open;
	if(Begin(viewId, viewOpen, windowFlags)) {
		ImGui::PushID(viewId);

		// Middle-click to close view
		ImGuiWindow *window = ImGui::GetCurrentContext()->CurrentWindow;
		if(!window->SkipItems) {
			if(IsMouseReleased(ImGuiMouseButton_Middle)) {
				if(IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
					view_close_and_write_config(view);
				}
			}
		}

		if(ImGui::BeginPopupContextItem("ViewContext")) {
			if(ImGui::Selectable("Close this view")) {
				view_close_and_write_config(view);
			}
			if(ImGui::Selectable("Close all views")) {
				for(u32 viewIndex = 0; viewIndex < s_gathered_views.count; ++viewIndex) {
					view_t *otherView = *(s_gathered_views.data + viewIndex);
					view_close_and_write_config(otherView);
				}
			}
			if(ImGui::Selectable("Close all but this view")) {
				for(u32 viewIndex = 0; viewIndex < s_gathered_views.count; ++viewIndex) {
					view_t *otherView = *(s_gathered_views.data + viewIndex);
					if(otherView != view) {
						view_close_and_write_config(otherView);
					}
				}
			}
			if(ImGui::Selectable("Close all inactive views")) {
				for(u32 viewIndex = 0; viewIndex < s_gathered_views.count; ++viewIndex) {
					view_t *otherView = *(s_gathered_views.data + viewIndex);
					const recording_t *recording = recordings_find_by_path(otherView->session->path);
					if(!recording || !recording->active) {
						view_close_and_write_config(otherView);
					}
				}
			}
			if(ImGui::Selectable("Close all inactive auto-close views")) {
				for(u32 viewIndex = 0; viewIndex < s_gathered_views.count; ++viewIndex) {
					view_t *otherView = *(s_gathered_views.data + viewIndex);
					if(otherView->autoClose) {
						const recording_t *recording = recordings_find_by_path(otherView->session->path);
						if(!recording || !recording->active) {
							view_close_and_write_config(otherView);
						}
					}
				}
			}
			if(ImGui::Selectable("Re-dock this view")) {
				view->redockCount = 1;
			}
			if(ImGui::Selectable("Re-dock all views")) {
				for(u32 viewIndex = 0; viewIndex < s_gathered_views.count; ++viewIndex) {
					view_t *otherView = *(s_gathered_views.data + viewIndex);
					otherView->redockCount = 1;
				}
			}
			ImGui::EndPopup();
		}

		const recording_t *recording = recordings_find_by_path(session->path);
		bool hasFocus = ImGui::IsWindowFocused() ||
		                ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow);

		ImGuiViewport *WindowViewport = ImGui::GetWindowViewport();
		ImGuiViewport *MainViewport = ImGui::GetMainViewport();
		b32 overTileRegion = WindowViewport == MainViewport;
		if(!overTileRegion) {
			ImVec2 tolerance(100.0f, 100.0f);
			ImVec2 WindowLo = WindowViewport->Pos + tolerance;
			ImVec2 WindowHi = WindowViewport->Pos + WindowViewport->Size - tolerance;
			ImVec2 MainLo = MainViewport->Pos;
			ImVec2 MainHi = MainViewport->Pos + MainViewport->Size;
			if(WindowLo.x >= MainLo.x && WindowLo.y >= MainLo.y &&
			   WindowHi.x <= MainHi.x && WindowHi.y <= MainHi.y) {
				overTileRegion = true;
			}
		}
		if(hasFocus && ImGui::IsCurrentWindowMoving()) {
			ImVec2 dragDelta = ImGui::GetMouseDragDelta();
			if(dragDelta.x != 0.0f || dragDelta.y != 0.0f) {
				view->beingDragged = true;
				view->tiled = false;
			}
		} else if(view->beingDragged) {
			view->beingDragged = false;
			if(overTileRegion) {
				view->tiled = true;
			} else {
				view->tiled = false;
			}
		} else if(!view->tiled) {
			if(overTileRegion) {
				view->tiled = true;
			}
		}

		float combinedColumnsWidth = ImGui::GetWindowWidth();
		if(view->combinedColumnsWidth != combinedColumnsWidth) {
			view->combinedColumnsWidth = combinedColumnsWidth;
			view_reset_column_offsets(view);
		}

		if(BeginMenuBar()) {
			PushStyleColor(ImGuiCol_Text, recording && recording->active ? MakeColor(kStyleColor_ActiveSession) : MakeColor(kStyleColor_InactiveSession));
			const char *menuText = "";
			if(recording) {
				const char *sep = strrchr(recording->path, '\\');
				const char *filename = sep ? sep + 1 : recording->path;
				menuText = va("%s - %s", view->session->appInfo.packet.appInfo.applicationName, filename);
			} else {
				menuText = va("%s", view->session->appInfo.packet.appInfo.applicationName);
			}
			bool bMenuOpen = ImGui::BeginMenu(menuText);
			if(ImGui::IsItemHovered()) {
				if(view->session->appInfo.packet.appInfo.platform == kBBPlatform_Unknown) {
					SetTooltip("%s", view->session->path);
				} else {
					SetTooltip("%s - %s", view->session->path, bb_platform_name((bb_platform_e)view->session->appInfo.packet.appInfo.platform));
				}
			}
			PopStyleColor();
			if(bMenuOpen) {
				if(ImGui::MenuItem("Save text")) {
					UIRecordedView_SaveLog(view, false, kColumnSpacer_Tab);
				}
				if(ImGui::MenuItem("Save all columns")) {
					UIRecordedView_SaveLog(view, true, kColumnSpacer_Spaces);
				}
				if(ImGui::MenuItem("Save all columns (tab-separated)")) {
					UIRecordedView_SaveLog(view, true, kColumnSpacer_Tab);
				}
				if(ImGui::MenuItem("Open containing folder")) {
					UIRecordedView_OpenContainingFolder(view);
				}
				if(g_config.showDebugMenu) {
					if(ImGui::MenuItem("Test message box")) {
						messageBox mb = { BB_EMPTY_INITIALIZER };
						sdict_add_raw(&mb.data, "title", u8"\uf06a Data Corruption");
						sdict_add_raw(&mb.data, "text", va("Failed to deserialize %s", session->path));
						sdict_add_raw(&mb.data, "button1", "Ok");
						mb_queue(mb, &view->messageboxes);
					}
				}
				ImGui::EndMenu();
			}
			EndMenuBar();
		}

		view->messageboxes.bgColor[0] = ImColor(MakeColor(kStyleColor_MessageBoxBackground0));
		view->messageboxes.bgColor[1] = ImColor(MakeColor(kStyleColor_MessageBoxBackground1));
		UIMessageBox_Update(&view->messageboxes);

		//Separator();

		bool consoleVisible = view->session->outgoingMqId != mq_invalid_id();
		float consoleInputHeight = consoleVisible ? -ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y * 2 : 0.0f;

		bool categoriesChildFocused = false;
		if(view->config.showSelectorTarget) {
			// ImGui::GetWindowContentRegionWidth() * 0.15f
			ImGui::BeginChild("CategoryChild", ImVec2(view->categoriesWidth, consoleInputHeight), false, ImGuiWindowFlags_HorizontalScrollbar);
			categoriesChildFocused = ImGui::IsWindowFocused();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
			UITags_Update(view);
			UIViewThreads_Update(view);
			UIViewFiles_Update(view);
			UIViewPieInstances_Update(view);
			ImGui::PopStyleVar();

			ImGui::EndChild();

			ImGui::SameLine();
			const float normal = 0.4f;
			const float hovered = 0.8f;
			const float active = 0.8f;
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(normal, normal, normal, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(hovered, hovered, hovered, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(active, active, active, 1.0f));
			ImGui::Button("|", ImVec2(config_get_resizeBarSize(&g_config), ImGui::GetContentRegionAvail().y - 2 + consoleInputHeight));
			ImGui::PopStyleColor(3);
			if(ImGui::IsItemActive() || ImGui::IsItemHovered()) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			}
			if(ImGui::IsItemActive()) {
				view->categoriesWidth += ImGui::GetIO().MouseDelta.x;
			}

			ImGui::SameLine();
		}

		if(view->visibleLogsDirty) {
			view_update_visible_logs(view);
			view->visibleLogsDirty = false;

			if(view->lastVisibleSelectedSessionIndexStart != ~0U) {
				for(u32 i = 0; i < view->visibleLogs.count; ++i) {
					view_log_t *log = view->visibleLogs.data + i;
					if(log->sessionLogIndex >= view->lastVisibleSelectedSessionIndexStart) {
						view->gotoTarget = (int)i;
						break;
					}
				}
			} else if(view->lastVisibleSessionIndexStart != ~0U) {
				u32 target = (view->lastVisibleSessionIndexStart + view->lastVisibleSessionIndexEnd) / 2;
				for(u32 i = 0; i < view->visibleLogs.count; ++i) {
					view_log_t *log = view->visibleLogs.data + i;
					if(log->sessionLogIndex >= target) {
						view->gotoTarget = (int)i;
						break;
					}
				}
			}
		} else if(view->prevDpiScale != Imgui_Core_GetDpiScale()) {
			if(view->lastVisibleSessionIndexStart != ~0U) {
				u32 target = (view->lastVisibleSessionIndexStart + view->lastVisibleSessionIndexEnd) / 2;
				for(u32 i = 0; i < view->visibleLogs.count; ++i) {
					view_log_t *log = view->visibleLogs.data + i;
					if(log->sessionLogIndex >= target) {
						view->gotoTarget = (int)i;
						break;
					}
				}
			}
		}

		BeginChild("scrollingParent", ImVec2(0, consoleInputHeight), false, 0);

		DrawViewToggles(view, applicationName);

		ImGui::PushItemWidth(200.0f);
		if(hasFocus && ImGui::IsKeyPressed('F') && ImGui::GetIO().KeyCtrl) {
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::TextUnformatted("Filter:");
		ImGui::SameLine();
		if(ImGui::Checkbox("###FilterActive", &view->config.filterActive)) {
			view->visibleLogsDirty = true;
			BB_LOG("Debug", "Set filterActive to '%d' for '%s'\n", view->config.filterActive, applicationName);
		}
		ImGui::SameLine();
		if(ImGui::InputText("###Filter", &view->config.filterInput, 256, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
			view->visibleLogsDirty = true;
			view->config.filterActive = true;
			BB_LOG("Debug", "Set filter to '%s' for '%s'\n", sb_get(&view->config.filterInput), applicationName);
		}
		Fonts_CacheGlyphs(sb_get(&view->config.filterInput));
		const bool filterFocused = IsItemActive() && Imgui_Core_HasFocus();
		if(filterFocused) {
			Imgui_Core_RequestRender();
		}

		ImGui::SameLine(0, 20 * Imgui_Core_GetDpiScale());
		ImGui::TextUnformatted("Line Spans:");
		ImGui::SameLine();
		if(ImGui::Checkbox("###SpansActive", &view->spansActive)) {
			view->visibleLogsDirty = true;
			BB_LOG("Debug", "Set spansActive to '%d' for '%s'\n", view->spansActive, applicationName);
		}
		ImGui::SameLine();
		if(ImGui::InputText("###Line Spans", &view->config.spansInput, 256, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
			view->visibleLogsDirty = true;
			view->spansActive = true;
			BB_LOG("Debug", "Set spans to '%s' for '%s'\n", sb_get(&view->config.spansInput), applicationName);
		}
		Fonts_CacheGlyphs(sb_get(&view->config.spansInput));
		const bool spansFocused = IsItemActive() && Imgui_Core_HasFocus();
		if(spansFocused) {
			Imgui_Core_RequestRender();
		}

		if(session->logs.count) {
			ImGui::SameLine();
			if(ImGui::Button("Clear")) {
				recorded_log_t *lastLog = session->logs.data[session->logs.count - 1];
				sb_reset(&view->config.spansInput);
				sb_va(&view->config.spansInput, "%u-", lastLog->sessionLogIndex + 1);
				view->visibleLogsDirty = true;
				view->spansActive = true;
				BB_LOG("Debug", "Set spans to '%s' for '%s'\n", sb_get(&view->config.spansInput), applicationName);
			}
			if(ImGui::IsTooltipActive()) {
				ImGui::SetTooltip("Clears view by setting Spans to [last log index + 1]-");
			}
		}

		bool selectedHackFocused = false;
		view_logs_t *visibleLogs = &view->visibleLogs;
		if(visibleLogs->lastClickIndex < visibleLogs->count) {
			view_log_t *viewLog = visibleLogs->data + visibleLogs->lastClickIndex;
			sb_t sb;
			sb_init(&sb);
			BuildLogLine(view, viewLog, false, &sb, kNoCRLF, kColumnSpacer_Tab, kJsonExpansion_Off);
			if(sb.data) {
				ImGui::PushItemWidth(-1.0f);
				ImGui::SameLine(0, 20 * Imgui_Core_GetDpiScale());
				ImGui::InputText("###SelectedHack", sb.data, sb.allocated, ImGuiInputTextFlags_ReadOnly);
				selectedHackFocused = IsItemActive() && Imgui_Core_HasFocus();
				if(selectedHackFocused) {
					Imgui_Core_RequestRender();
				}
				ImGui::PopItemWidth();
			}
			sb_reset(&sb);
		}

		ImGui::Separator();

		b32 otherControlFocused = categoriesChildFocused || filterFocused || spansFocused || selectedHackFocused || view->consoleInputFocused;
		b32 gotoWasVisible = view->gotoVisible;
		if(hasFocus) {
			if(ImGui::IsKeyPressed('G') && ImGui::GetIO().KeyCtrl) {
				gotoWasVisible = false;
				if(view->gotoVisible) {
					view->gotoViewRelative = !view->gotoViewRelative;
				} else {
					view->gotoTargetInput = 0;
					view->gotoVisible = true;
					view->gotoViewRelative = ImGui::GetIO().KeyShift;
				}
			} else if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				view->gotoVisible = false;
			} else if(!otherControlFocused) {
				if(ImGui::IsKeyPressed('A') && ImGui::GetIO().KeyCtrl) {
					UIRecordedView_Logs_SelectAll(view);
				} else if(ImGui::IsKeyPressed('C') && ImGui::GetIO().KeyCtrl) {
					CopySelectedLogsToClipboard(view, ImGui::GetIO().KeyShift, kColumnSpacer_Spaces);
				} else if(key_is_pressed_this_frame(Key_F2)) {
					if(ImGui::GetIO().KeyCtrl) {
						if(ImGui::GetIO().KeyShift) {
							view_remove_all_bookmarks(view);
						} else {
							view_toggle_bookmarks_for_selection(view);
						}
					} else {
						view_advance_to_next_bookmark(view, !ImGui::GetIO().KeyShift);
					}
				}
			}
		}

		if(view->gotoVisible) {
			if(!gotoWasVisible) {
				ImGui::SetKeyboardFocusHere();
			}
			if(InputInt((view->gotoViewRelative) ? "Goto (View-Relative)###Goto" : "Goto (Absolute)###Goto", &view->gotoTargetInput, 0, 0, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue)) {
				ClearViewTail(view, "goto target");
				view->gotoTarget = view->gotoTargetInput > 0 ? view->gotoTargetInput : 0;
				view->gotoVisible = 0;

				for(u32 i = 0; i < view->visibleLogs.count; ++i) {
					view_log_t *log = view->visibleLogs.data + i;
					if(view->gotoViewRelative) {
						if(log->viewLogIndex >= (u32)view->gotoTarget) {
							view->gotoTarget = (int)i;
							break;
						}
					} else {
						if(log->sessionLogIndex >= (u32)view->gotoTarget) {
							view->gotoTarget = (int)i;
							break;
						}
					}
				}
			}
		}

		float FramePaddingY = ImGui::GetStyle().FramePadding.y;
		ImGui::GetStyle().FramePadding.y = 0.0f;

		float textOffset = 0.0f;
		if(view->scrollWidth > 0.0f) {
			ImVec2 region = ImGui::GetWindowContentRegionMax();
			if(region.x < view->scrollWidth) {
				SetNextWindowContentSize(ImVec2(view->scrollWidth, 0.0f));
			}
		}
		int horizScrollingFlags = 0;
		if(BeginChild("logheader", ImVec2(0, ImGui::GetFrameHeightWithSpacing() + 4), false, horizScrollingFlags)) {
			SetScrollX(view->prevScrollX);
			textOffset = UIRecordedView_LogHeader(view);
			ImGui::Separator();
		}
		EndChild();

		view->lastVisibleSessionIndexStart = ~0U;
		view->lastVisibleSessionIndexEnd = 0U;
		view->lastVisibleSelectedSessionIndexStart = ~0U;
		view->lastVisibleSelectedSessionIndexEnd = 0U;

		if(view->scrollWidth > 0.0f) {
			ImVec2 region = ImGui::GetWindowContentRegionMax();
			if(region.x < view->scrollWidth) {
				SetNextWindowContentSize(ImVec2(view->scrollWidth, 0.0f));
			}
		}
		verticalScrollDir_e scrollDir = kVerticalScroll_None;
		if(BeginChild("logentries", ImVec2(0, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
			SetScrollX(view->prevScrollX);
			PushLogFont();
			bool hovered = ImGui::IsWindowHovered();
			ImGuiListClipper clipper((int)view->visibleLogs.count, ImGui::GetTextLineHeightWithSpacing());
			while(clipper.Step()) {
				for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
					view_log_t *viewLog = view->visibleLogs.data + i;
					float width = UIRecordedView_LogLine(view, viewLog, textOffset, scrollDir);
					if(view->scrollWidth < width) {
						view->scrollWidth = width;
					}

					view->lastVisibleSessionIndexStart = BB_MIN(view->lastVisibleSessionIndexStart, viewLog->sessionLogIndex);
					view->lastVisibleSessionIndexEnd = BB_MAX(view->lastVisibleSessionIndexEnd, viewLog->sessionLogIndex);
					if(viewLog->selected) {
						view->lastVisibleSelectedSessionIndexStart = BB_MIN(view->lastVisibleSelectedSessionIndexStart, viewLog->sessionLogIndex);
						view->lastVisibleSelectedSessionIndexEnd = BB_MAX(view->lastVisibleSelectedSessionIndexEnd, viewLog->sessionLogIndex);
					}
				}
			}
			if(otherControlFocused) {
				scrollDir = kVerticalScroll_None;
			}
			float curScrollY = GetScrollY();
			int visibleLines = clipper.DisplayEnd - clipper.DisplayStart;
			const float kScreenPercent = 0.5f;
			//view->bookmarkThreshold = (int)(clipper.DisplayStart + visibleLines * 0.5f);
			if(view->gotoTarget >= 0) {
				int adjustedTarget = view->gotoTarget - (int)(visibleLines * kScreenPercent);
				if(adjustedTarget < 0) {
					adjustedTarget = 0;
				}
				SetScrollY(adjustedTarget * GetTextLineHeightWithSpacing());
				if(view->prevDpiScale == Imgui_Core_GetDpiScale()) {
					view->gotoTarget = -1;
				}
			} else if(hovered && ImGui::GetIO().MouseWheel != 0.0f) {
				view->prevDpiScale = Imgui_Core_GetDpiScale();
				if(ImGui::GetIO().MouseWheel > 0) {
					ClearViewTail(view, "Mouse Wheel");
				} else {
					if(!view->tail && curScrollY >= view->prevScrollY && view->prevScrollY >= ImGui::GetScrollMaxY()) {
						view->tail = true;
						BB_LOG("Debug", "Set tail for '%s' - Mouse Wheel\n", applicationName);
					}
				}
			} else if(scrollDir != kVerticalScroll_None) {
				view->prevDpiScale = Imgui_Core_GetDpiScale();
				const int pageLines = 20;
				switch(scrollDir) {
				case kVerticalScroll_PageUp:
					SetScrollY(curScrollY - pageLines * GetTextLineHeightWithSpacing());
					ClearViewTail(view, "PageUp");
					break;
				case kVerticalScroll_PageDown:
					SetScrollY(curScrollY + pageLines * GetTextLineHeightWithSpacing());
					if(!view->tail && curScrollY >= view->prevScrollY && view->prevScrollY >= ImGui::GetScrollMaxY()) {
						view->tail = true;
						BB_LOG("Debug", "Set tail for '%s' - PageDown\n", applicationName);
					}
					break;
				case kVerticalScroll_Up:
					SetScrollY(curScrollY - GetTextLineHeightWithSpacing());
					ClearViewTail(view, "Up");
					break;
				case kVerticalScroll_Down:
					SetScrollY(curScrollY + GetTextLineHeightWithSpacing());
					if(!view->tail && curScrollY >= view->prevScrollY && view->prevScrollY >= ImGui::GetScrollMaxY()) {
						view->tail = true;
						BB_LOG("Debug", "Set tail for '%s' - Down\n", applicationName);
					}
					break;
				case kVerticalScroll_Start:
					SetScrollY(0.0f);
					ClearViewTail(view, "Home");
					break;
				case kVerticalScroll_End:
					ImGui::SetScrollHereY(0.0f);
					if(!view->tail) {
						view->tail = true;
						BB_LOG("Debug", "Set tail for '%s' - End\n", applicationName);
					}
					break;
				case kVerticalScroll_None:
					BB_ASSERT(false);
					break;
				}
			} else if(view->tail) {
				if(curScrollY < view->prevScrollY && view->prevScrollY <= ImGui::GetScrollMaxY()) {
					ClearViewTail(view, "not at bottom");
				}
				if(view->visibleLogsAdded) {
					ImGui::SetScrollHereY();
					view->visibleLogsAdded = false;
					view->prevDpiScale = Imgui_Core_GetDpiScale();
				}
			}
			view->prevScrollY = curScrollY;
			float dpiScale = Imgui_Core_GetDpiScale();
			if(view->prevDpiScale != dpiScale) {
				view->prevDpiScale = dpiScale;
			}
			PopLogFont();
		}
		EndChild(); // logentries

		if(view->scrollWidth > 0.0f) {
			ImVec2 region = ImGui::GetWindowContentRegionMax();
			if(region.x < view->scrollWidth) {
				SetNextWindowContentSize(ImVec2(view->scrollWidth, 0.0f));
			}
		}
		if(BeginChild("horizscrollbar", ImVec2(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize, ImGui::GetFrameHeight()),
		              false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar)) {
			view->prevScrollX = GetScrollX();
		}
		EndChild();

		ImGui::GetStyle().FramePadding.y = FramePaddingY;

		EndChild(); // scrollingParent

		UIRecordedView_Console(view, hasFocus);
		ImGui::PopID();
	}
	End();
	if(roundingPushed) {
		PopStyleVar(); // ImGuiStyleVar_WindowRounding
	}

	if(view->open) {
		if(initializedLogColumns) {
			view->initialized = true;
		}
	}
}

void UIRecordedView_RemoveClosedViews()
{
	for(u32 sessionIndex = 0; sessionIndex < recorded_session_count();) {
		recorded_session_t *session = recorded_session_get(sessionIndex);
		if(!session)
			break;

		for(u32 viewIndex = 0; viewIndex < session->views.count;) {
			view_t *view = session->views.data + viewIndex;
			if(!view->open) {
				view_reset(view);
				bba_erase(session->views, viewIndex);
			} else {
				++viewIndex;
			}
		}

		if(session->views.count) {
			++sessionIndex;
		} else {
			recorded_session_close(session);
		}
	}
}

static int GatheredViewSort(const void *_a, const void *_b)
{
	const view_t *a = *(const view_t **)_a;
	const view_t *b = *(const view_t **)_b;

	const bb_packet_app_info_t *aAppInfo = &a->session->appInfo.packet.appInfo;
	const bb_packet_app_info_t *bAppInfo = &b->session->appInfo.packet.appInfo;
	int res = _stricmp(aAppInfo->applicationName, bAppInfo->applicationName);
	if(res != 0)
		return res;
	return (int)(aAppInfo->initialTimestamp - bAppInfo->initialTimestamp);
}

void UIRecordedView_GatherViews(gathered_views_t &views)
{
	for(u32 sessionIndex = 0; sessionIndex < recorded_session_count(); ++sessionIndex) {
		recorded_session_t *session = recorded_session_get(sessionIndex);
		if(!session || session->appInfo.type == kBBPacketType_Invalid)
			continue;

		for(u32 viewIndex = 0; viewIndex < session->views.count; ++viewIndex) {
			view_t *view = session->views.data + viewIndex;
			bba_push(views, view);
		}
	}
	qsort(views.data, views.count, sizeof(views.data[0]), GatheredViewSort);
}

void UIRecordedView_UpdateAll(bool autoTileViews)
{
	for(u32 sessionIndex = 0; sessionIndex < recorded_session_count(); ++sessionIndex) {
		recorded_session_t *session = recorded_session_get(sessionIndex);
		if(session) {
			recorded_session_update(session);
		}
	}

	s_gathered_views.count = 0;
	UIRecordedView_GatherViews(s_gathered_views);

	if(s_lastDpiScale != Imgui_Core_GetDpiScale()) {
		s_lastDpiScale = Imgui_Core_GetDpiScale();
		for(u32 viewIndex = 0; viewIndex < s_gathered_views.count; ++viewIndex) {
			view_t *view = s_gathered_views.data[viewIndex];
			view_reset_column_widths(view);
		}
	}

	ImVec2 viewportPos(0.0f, 0.0f);
	ImGuiViewport *viewport = ImGui::GetViewportForWindow("Recordings");
	if(viewport) {
		viewportPos.x += viewport->Pos.x;
		viewportPos.y += viewport->Pos.y;
	}

	float startY = ImGui::GetFrameHeight() + g_messageboxHeight;
	ImGuiIO &io = ImGui::GetIO();
	float screenWidth = io.DisplaySize.x - UIRecordings_Width();
	float screenHeight = io.DisplaySize.y - startY;

	if(autoTileViews) {

		int tiledCount = 0;
		for(u32 viewIndex = 0; viewIndex < s_gathered_views.count; ++viewIndex) {
			view_t *view = *(s_gathered_views.data + viewIndex);
			if(view->tiled) {
				++tiledCount;
			}
		}

		int cols = (int)ceil(sqrt((double)tiledCount));
		cols = (cols < 1) ? 1 : cols;
		int rows = (int)ceil(tiledCount / (float)cols);
		rows = (rows < 1) ? 1 : rows;
		if(screenHeight > screenWidth) {
			int tmp = cols;
			cols = rows;
			rows = tmp;
		}
		ImVec2 windowSpacing(screenWidth / cols, screenHeight / rows);
		ImVec2 windowSize(windowSpacing.x - 1, windowSpacing.y - 1);
		int row = 0;
		int col = 0;
		for(u32 viewIndex = 0; viewIndex < s_gathered_views.count; ++viewIndex) {
			view_t *view = *(s_gathered_views.data + viewIndex);
			ImGuiCond positioningCond = (view->tiled) ? ImGuiCond_Always : ImGuiCond_Once;
			SetNextWindowSize(windowSize, positioningCond);
			SetNextWindowPos(ImVec2(viewportPos.x + windowSpacing.x * col, viewportPos.y + startY + windowSpacing.y * row), positioningCond);
			UIRecordedView_Update(view, autoTileViews);
			if(view->tiled) {
				++col;
				if(col == cols) {
					col = 0;
					++row;
				}
			}
		}
	} else if(s_gathered_views.count) {
		ImVec2 windowSpacing(screenWidth, screenHeight);
		ImVec2 windowSize(windowSpacing.x - 1, windowSpacing.y - 1);

		SetNextWindowSize(windowSize, ImGuiCond_Always);
		SetNextWindowPos(ImVec2(viewportPos.x, viewportPos.y + startY), ImGuiCond_Always);
		int windowFlags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus;
		windowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		bool bOpen = true;
		if(Begin("MainDockWindow", &bOpen, windowFlags)) {
			ImGuiID MainDockSpaceId = ImGui::GetID("MainDockSpace");
			ImGui::DockSpace(MainDockSpaceId);

			for(u32 viewIndex = 0; viewIndex < s_gathered_views.count; ++viewIndex) {
				view_t *view = *(s_gathered_views.data + viewIndex);
				bool redock = view->redockCount > 0;
				if(view->redockCount > 0) {
					--view->redockCount;
					Imgui_Core_RequestRender();
				}
				SetNextWindowDockID(MainDockSpaceId, redock ? ImGuiCond_Always : ImGuiCond_Appearing);
				SetNextWindowSize(windowSize, ImGuiCond_Once);
				SetNextWindowPos(ImVec2(viewportPos.x, viewportPos.y + startY), ImGuiCond_Once);
				UIRecordedView_Update(*(s_gathered_views.data + viewIndex), autoTileViews);
			}
		}
		End();
	}

	UIRecordedView_RemoveClosedViews();
}

void UIRecordedView_Shutdown(void)
{
	bba_free(s_gathered_views);
	sb_reset(&s_strippedLine);
}
