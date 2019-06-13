// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "ui_view_console_command.h"
#include "bbclient/bb_array.h"
#include "bbclient/bb_string.h"
#include "fonts.h"
#include "imgui_core.h"
#include "imgui_themes.h"
#include "imgui_utils.h"
#include "keys.h"
#include "message_queue.h"
#include "recorded_session.h"
#include "recordings.h"
#include "sb.h"
#include "time_utils.h"
#include "ui_config.h"
#include "ui_recordings.h"
#include "va.h"
#include "view.h"

extern "C" void view_console_history_entry_reset(view_console_history_entry_t *val);

static int UIRecordedView_ConsoleInputCallback(ImGuiTextEditCallbackData *CallbackData)
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
		BB_ASSERT(false);
		break;
	}
	return 0;
}

static void UIRecordedView_Console_Dispatch(view_t *view)
{
	if(view->session->outgoingMqId != mq_invalid_id()) {
		const char *command = sb_get(&view->consoleInput);
		if(mq_queue(view->session->outgoingMqId, kBBPacketType_ConsoleCommand, "%s", command)) {
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
			view_console_history_entry_t newEntry = { BB_EMPTY_INITIALIZER };
			sb_append(&newEntry.command, command);
			bba_push(view->consoleHistory.entries, newEntry);
			sb_reset(&view->consoleInput);
			view->consoleRequestActive = true;
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
		ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth());
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
