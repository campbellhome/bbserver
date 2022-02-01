// Copyright (c) 2012-2020 Matt Campbell
// MIT license (see License.txt)

#include "ui_view_console_command.h"
#include "bb_array.h"
#include "bb_string.h"
#include "file_utils.h"
#include "fonts.h"
#include "imgui_core.h"
#include "imgui_themes.h"
#include "imgui_utils.h"
#include "keys.h"
#include "message_queue.h"
#include "recorded_session.h"
#include "recordings.h"
#include "sb.h"
#include "span.h"
#include "time_utils.h"
#include "tokenize.h"
#include "ui_config.h"
#include "ui_recordings.h"
#include "va.h"
#include "view.h"

extern "C" void view_console_history_entry_reset(view_console_history_entry_t *val);

static int UIRecordedView_ConsoleInputCallback(ImGuiInputTextCallbackData *CallbackData)
{
	view_t *view = (view_t *)CallbackData->UserData;
	switch(CallbackData->EventFlag) {
	case ImGuiInputTextFlags_CallbackCharFilter:
		if(CallbackData->EventChar == '`')
			return 1;
		break;

	case ImGuiInputTextFlags_CallbackCompletion:
		break;

	case ImGuiInputTextFlags_CallbackHistory:
		if(view->consoleHistory.entries.count) {
			u32 prevHistoryPos = view->consoleHistory.pos;
			if(CallbackData->EventKey == ImGuiKey_UpArrow) {
				if(view->consoleHistory.pos == ~0U) {
					view->consoleHistory.pos = view->consoleHistory.entries.count - 1;
				} else if(view->consoleHistory.pos) {
					--view->consoleHistory.pos;
				}
			} else if(CallbackData->EventKey == ImGuiKey_DownArrow) {
				if(view->consoleHistory.pos != ~0U) {
					++view->consoleHistory.pos;
					if(view->consoleHistory.pos >= view->consoleHistory.entries.count) {
						view->consoleHistory.pos = ~0U;
					}
				}
			}
			if(prevHistoryPos != view->consoleHistory.pos) {
				const char *command = (view->consoleHistory.pos == ~0U) ? "" : sb_get(&view->consoleHistory.entries.data[view->consoleHistory.pos].command);
				int len = (int)bb_strncpy(CallbackData->Buf, command, (size_t)CallbackData->BufSize);
				CallbackData->CursorPos = len;
				CallbackData->SelectionStart = len;
				CallbackData->SelectionEnd = len;
				CallbackData->BufTextLen = len;
				CallbackData->BufDirty = true;
			}
			break;
		}

	default:
		break;
	}
	return 0;
}

static void view_console_history_entry_add(view_t *view, const char *command)
{
	view->consoleHistory.pos = ~0U;
	for(u32 i = 0; i < view->consoleHistory.entries.count; ++i) {
		view_console_history_entry_t *entry = view->consoleHistory.entries.data + i;
		const char *entryCommand = sb_get(&entry->command);
		if(!bb_stricmp(entryCommand, command)) {
			view_console_history_entry_reset(entry);
			bba_erase(view->consoleHistory.entries, i);
			break;
		}
	}
	view_console_history_entry_t newEntry = {};
	sb_append(&newEntry.command, command);
	bba_push(view->consoleHistory.entries, newEntry);
	sb_reset(&view->consoleInput);
	view->consoleRequestActive = true;
}

static b32 view_console_send(view_t *view, const char *command)
{
	return mq_queue(view->session->outgoingMqId, kBBPacketType_ConsoleCommand, "%s", command);
}

enum class iniCategory {
	Unknown,
	CoreLog,
	ConsoleVariables,
};

static void view_console_command_exec(view_t *view, const char *filepath)
{
	fileData_t fileData = fileData_read(filepath);
	if(fileData.bufferSize) {
		iniCategory category = iniCategory::Unknown;
		span_t linesCursor = span_from_string((const char *)fileData.buffer);
		for(span_t line = tokenizeLine(&linesCursor); line.start; line = tokenizeLine(&linesCursor)) {
			if (line.start == line.end)
				continue;

			if(span_starts_with(line, "[", kSpanCaseSensitive) && span_ends_with(line, "]", kSpanCaseSensitive)) {
				if(!span_stricmp(line, span_from_string("[Core.Log]"))) {
					category = iniCategory::CoreLog;
				} else if(!span_stricmp(line, span_from_string("[ConsoleVariables]"))) {
					category = iniCategory::ConsoleVariables;
				} else {
					category = iniCategory::Unknown;
				}
				continue;
			}

			if(category == iniCategory::Unknown || span_starts_with(line, ";", kSpanCaseSensitive))
				continue;

			const char *lineCursor = line.start;
			span_t key = tokenize(&lineCursor, "= \r\n");
			span_t val = tokenize(&lineCursor, "= \r\n");
			if(key.start && val.start) {
				if(category == iniCategory::CoreLog) {
					view_console_send(view, va("log %.*s %.*s", (int)(key.end - key.start), key.start, (int)(val.end - val.start), val.start));
				} else {
					view_console_send(view, va("%.*s %.*s", (int)(key.end - key.start), key.start, (int)(val.end - val.start), val.start));
				}
			}
		}
	}
	fileData_reset(&fileData);
}

static void UIRecordedView_Console_Dispatch(view_t *view)
{
	if(view->session->outgoingMqId != mq_invalid_id() && (view->session->appInfo.packet.appInfo.initFlags & kBBInitFlag_ConsoleCommands) != 0) {
		const char *command = sb_get(&view->consoleInput);
		if(*command == '/') {
			if(!bb_strnicmp(command, "/exec ", 6)) {
				view_console_command_exec(view, command + 6);
				view_console_history_entry_add(view, command);
			}
		} else {
			if(view_console_send(view, command)) {
				view_console_history_entry_add(view, command);
			}
		}
	}
}

void UIRecordedView_Console(view_t *view, bool bHasFocus)
{
	bool consoleVisible = view->session->outgoingMqId != mq_invalid_id();
	if(consoleVisible) {
		ImGui::TextUnformatted("]");
		ImGui::SameLine();
		if(bHasFocus && key_is_pressed_this_frame(Key_Tilde)) {
			if(view->consoleInputActive) {
				// TODO: remove input focus here
			} else {
				view->consoleRequestActive = true;
			}
		}
		if(bHasFocus && view->consoleRequestActive) {
			ImGui::SetKeyboardFocusHere();
		}
		view->consoleRequestActive = false;
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
		if(ImGui::InputText("###ConsoleInput", &view->consoleInput, sizeof(bb_packet_text_t), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCharFilter, &UIRecordedView_ConsoleInputCallback, view)) {
			UIRecordedView_Console_Dispatch(view);
		}
		ImGui::PopItemWidth();
		Fonts_CacheGlyphs(sb_get(&view->consoleInput));
		if(ImGui::IsItemActive() && Imgui_Core_HasFocus()) {
			Imgui_Core_RequestRender();
		}
		view->consoleInputFocused = ImGui::IsWindowFocused();
		view->consoleInputActive = ImGui::IsItemActive();
	} else {
		view->consoleInputFocused = false;
		view->consoleRequestActive = false;
	}
}
