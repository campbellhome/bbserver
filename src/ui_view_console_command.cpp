// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "ui_view_console_command.h"
#include "bb_array.h"
#include "bb_string.h"
#include "bb_time.h"
#include "file_utils.h"
#include "fonts.h"
#include "imgui_core.h"
#include "imgui_themes.h"
#include "imgui_tooltips.h"
#include "imgui_utils.h"
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
#include "wrap_imgui_internal.h"

extern "C" void view_console_history_entry_reset(view_console_history_entry_t* val);
static void UIRecordedView_ConsoleAutocomplete(view_t* view);

static int UIRecordedView_ConsoleInputCallback(ImGuiInputTextCallbackData* CallbackData)
{
	view_t* view = (view_t*)CallbackData->UserData;
	if (CallbackData->EventFlag & ImGuiInputTextFlags_CallbackCharFilter)
	{
		view->consoleInputTime = bb_current_time_ms();
		if (CallbackData->EventChar == '`')
			return 1;

		view->consolePopupOpen = true;
		view->consoleMode = kConsoleMode_Autocomplete;
	}

	if (CallbackData->EventFlag & ImGuiInputTextFlags_CallbackCompletion)
	{
		view->consoleInputTime = bb_current_time_ms();
		const char* command = nullptr;
		if (view->consoleMode == kConsoleMode_History)
		{
			if (view->consoleHistory.pos != ~0u)
			{
				command = sb_get(&view->consoleHistory.entries.data[view->consoleHistory.pos].command);
			}
		}
		else
		{
			if (view->consoleAutocompleteIndex != ~0u && view->consoleAutocompleteIndex < view->session->consoleAutocomplete.count)
			{
				command = view->session->consoleAutocomplete.data[view->consoleAutocompleteIndex].text;
			}
		}
		if (command && *command)
		{
			int len = (int)bb_strncpy(CallbackData->Buf, command, (size_t)CallbackData->BufSize);
			CallbackData->CursorPos = len;
			CallbackData->SelectionStart = len;
			CallbackData->SelectionEnd = len;
			CallbackData->BufTextLen = len;
			CallbackData->BufDirty = true;
		}
	}

	if (CallbackData->EventFlag & ImGuiInputTextFlags_CallbackHistory)
	{
		view->consolePopupOpen = true;
		if (view->consoleMode == kConsoleMode_History)
		{
			if (view->consoleHistory.entries.count > 0)
			{
				view->consoleHistoryTime = bb_current_time_ms();
				if (CallbackData->EventKey == ImGuiKey_UpArrow)
				{
					if (view->consoleHistory.pos == ~0U || view->consoleHistory.pos == 0)
					{
						view->consoleHistory.pos = view->consoleHistory.entries.count - 1;
					}
					else
					{
						--view->consoleHistory.pos;
					}
				}
				else if (CallbackData->EventKey == ImGuiKey_DownArrow)
				{
					if (view->consoleHistory.pos == ~0U)
					{
						view->consoleHistory.pos = 0u;
					}
					else
					{
						++view->consoleHistory.pos;
						if (view->consoleHistory.pos >= view->consoleHistory.entries.count)
						{
							view->consoleHistory.pos = 0U;
						}
					}
				}
			}
		}
		else if (view->session->consoleAutocomplete.count > 0)
		{
			if (CallbackData->EventKey == ImGuiKey_UpArrow)
			{
				if (view->consoleAutocompleteIndex == ~0U || view->consoleAutocompleteIndex == 0)
				{
					view->consoleAutocompleteIndex = view->session->consoleAutocomplete.count - 1;
				}
				else
				{
					--view->consoleAutocompleteIndex;
				}
			}
			else if (CallbackData->EventKey == ImGuiKey_DownArrow)
			{
				if (view->consoleAutocompleteIndex == ~0U)
				{
					view->consoleAutocompleteIndex = 0u;
				}
				else
				{
					++view->consoleAutocompleteIndex;
					if (view->consoleAutocompleteIndex >= view->session->consoleAutocomplete.count)
					{
						view->consoleAutocompleteIndex = 0u;
					}
				}
			}
		}
	}

	if (CallbackData->EventFlag & ImGuiInputTextFlags_CallbackAlways)
	{
		if (view->consoleRequestActive != view->consoleRequestActiveOld)
		{
			CallbackData->CursorPos = CallbackData->BufTextLen;
			view->consoleRequestActiveOld = view->consoleRequestActive;
		}
	}

	return 0;
}

static void view_console_history_entry_add(view_t* view, const char* command)
{
	view->consoleHistory.pos = ~0U;
	for (u32 i = 0; i < view->consoleHistory.entries.count; ++i)
	{
		view_console_history_entry_t* entry = view->consoleHistory.entries.data + i;
		const char* entryCommand = sb_get(&entry->command);
		if (!bb_stricmp(entryCommand, command))
		{
			view_console_history_entry_reset(entry);
			bba_erase(view->consoleHistory.entries, i);
			break;
		}
	}
	view_console_history_entry_t newEntry = {};
	sb_append(&newEntry.command, command);
	bba_push(view->consoleHistory.entries, newEntry);
	sb_reset(&view->consoleInput);
	++view->consoleRequestActive;
}

static b32 view_console_send(view_t* view, const char* command)
{
	return mq_queue(view->session->outgoingMqId, kBBPacketType_ConsoleCommand, "%s", command);
}

enum class iniCategory
{
	Unknown,
	CoreLog,
	ConsoleVariables,
};

static void view_console_command_exec(view_t* view, const char* filepath)
{
	fileData_t fileData = fileData_read(filepath);
	if (fileData.bufferSize)
	{
		iniCategory category = iniCategory::Unknown;
		span_t linesCursor = span_from_string((const char*)fileData.buffer);
		for (span_t line = tokenizeLine(&linesCursor); line.start; line = tokenizeLine(&linesCursor))
		{
			if (line.start == line.end)
				continue;

			if (span_starts_with(line, "[", kSpanCaseSensitive) && span_ends_with(line, "]", kSpanCaseSensitive))
			{
				if (!span_stricmp(line, span_from_string("[Core.Log]")))
				{
					category = iniCategory::CoreLog;
				}
				else if (!span_stricmp(line, span_from_string("[ConsoleVariables]")))
				{
					category = iniCategory::ConsoleVariables;
				}
				else
				{
					category = iniCategory::Unknown;
				}
				continue;
			}

			if (category == iniCategory::Unknown || span_starts_with(line, ";", kSpanCaseSensitive))
				continue;

			const char* lineCursor = line.start;
			span_t key = tokenize(&lineCursor, "= \r\n");
			span_t val = tokenize(&lineCursor, "= \r\n");
			if (key.start && val.start)
			{
				if (category == iniCategory::CoreLog)
				{
					view_console_send(view, va("log %.*s %.*s", (int)(key.end - key.start), key.start, (int)(val.end - val.start), val.start));
				}
				else
				{
					view_console_send(view, va("%.*s %.*s", (int)(key.end - key.start), key.start, (int)(val.end - val.start), val.start));
				}
			}
		}
	}
	fileData_reset(&fileData);
}

void UIRecordedView_Console_ClosePopup(view_t *view)
{
	view->consolePopupOpen = false;
	view->consoleMode = kConsoleMode_History;
	view->consoleAutocompleteIndex = ~0u;
	view->consoleAutocompleteId = 0u;
}

static void UIRecordedView_Console_Dispatch(view_t* view)
{
	bool consoleAvailable = (view->session->appInfo.packet.appInfo.initFlags & kBBInitFlag_ConsoleCommands) != 0;
	if (consoleAvailable && view->session->outgoingMqId != mq_invalid_id())
	{
		const char* command = sb_get(&view->consoleInput);
		if (*command == '/')
		{
			if (!bb_strnicmp(command, "/exec ", 6))
			{
				view_console_command_exec(view, command + 6);
				view_console_history_entry_add(view, command);
			}
		}
		else
		{
			if (view_console_send(view, command))
			{
				view_console_history_entry_add(view, command);
			}
		}
	}
	UIRecordedView_Console_ClosePopup(view);
}

void UIRecordedView_Console(view_t* view, bool bHasFocus)
{
	bool consoleAvailable = (view->session->appInfo.packet.appInfo.initFlags & kBBInitFlag_ConsoleCommands) != 0;
	bool consoleVisible = consoleAvailable && view->session->outgoingMqId != mq_invalid_id();
	if (consoleVisible)
	{
		ImGui::TextUnformatted("]");
		ImGui::SameLine();
		if (bHasFocus && ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
		{
			UIRecordedView_Console_ClosePopup(view);
			if (view->consoleInputActive)
			{
				// TODO: remove input focus here
			}
			else
			{
				++view->consoleRequestActive;
			}
		}
		if (bHasFocus && view->consoleRequestActive != view->consoleRequestActiveOld)
		{
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
		ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
		                                 ImGuiInputTextFlags_CallbackAlways |
		                                 ImGuiInputTextFlags_CallbackCompletion |
		                                 ImGuiInputTextFlags_CallbackHistory |
		                                 ImGuiInputTextFlags_CallbackCharFilter;
		if (ImGui::InputText("###ConsoleInput", &view->consoleInput, sizeof(bb_packet_text_t), inputFlags, &UIRecordedView_ConsoleInputCallback, view))
		{
			UIRecordedView_Console_Dispatch(view);
		}
		if (strcmp(sb_get(&view->consoleInput), sb_get(&view->lastConsoleInput)) != 0)
		{
			sb_reset(&view->lastConsoleInput);
			view->lastConsoleInput = sb_clone(&view->consoleInput);
		}
		ImGui::PopItemWidth();
		Fonts_CacheGlyphs(sb_get(&view->consoleInput));
		if (ImGui::IsItemActive() && Imgui_Core_HasFocus())
		{
			Imgui_Core_RequestRender();
		}
		view->consoleInputFocused = ImGui::IsWindowFocused();
		view->consoleInputActive = ImGui::IsItemActive();
		UIRecordedView_ConsoleAutocomplete(view);
	}
	else
	{
		view->consoleInputFocused = false;
	}
}

// #TODO: move this somewhere else and share with ui_view.cpp:
static void PushUIFont()
{
	ImFontAtlas* fonts = ImGui::GetIO().Fonts;
	ImGui::PushFont(fonts->Fonts[0]);
}

// #TODO: move this somewhere else and share with ui_view.cpp:
static void PopUIFont()
{
	ImGui::PopFont();
}

static void UIRecordedView_ConsoleAutocomplete(view_t* view)
{
	if ((view->session->appInfo.packet.appInfo.initFlags & kBBInitFlag_ConsoleAutocomplete) == 0)
	{
		return;
	}

	if (view->consoleInputFocused && view->consoleInputActive && view->consolePopupOpen)
	{
		ImGui::OpenPopup("###ConsolePopup", ImGuiPopupFlags_None);

		if (view->consoleInputTime >= view->consoleHistoryTime)
		{
			if (view->session->consoleAutocomplete.id == 0 || strcmp(sb_get(&view->session->consoleAutocomplete.request), sb_get(&view->consoleInput)) != 0)
			{
				u64 now = bb_current_time_ms();
				if (now > view->session->consoleAutocomplete.lastRequestTime + 200 && now > view->consoleInputTime + 100)
				{
					view->session->consoleAutocomplete.lastRequestTime = now;
					++view->session->consoleAutocomplete.id;
					sb_reset(&view->session->consoleAutocomplete.request);
					view->session->consoleAutocomplete.request = sb_clone(&view->consoleInput);
					mq_queue_userData(view->session->outgoingMqId, kBBPacketType_ConsoleAutocompleteRequest, view->session->consoleAutocomplete.id, sb_get(&view->consoleInput), view->consoleInput.count);
				}
			}
		}
	}

	if (view->consoleAutocompleteId != view->session->consoleAutocomplete.id)
	{
		view->consoleAutocompleteId = view->session->consoleAutocomplete.id;
		view->consoleAutocompleteIndex = ~0u;
	}

	u32 numLines = 0;
	if (view->consoleMode == kConsoleMode_History)
	{
		numLines = view->consoleHistory.entries.count;
	}
	else
	{
		if (sb_len(&view->consoleInput) > 0)
		{
			for (u32 i = 0; i < view->session->consoleAutocomplete.count; ++i)
			{
				const bb_packet_console_autocomplete_response_entry_t* entry = view->session->consoleAutocomplete.data + i;
				if (bb_strnicmp(entry->text, sb_get(&view->consoleInput), sb_len(&view->consoleInput)) == 0)
				{
					++numLines;
				}
			}
		}
	}
	if (!numLines)
	{
		return;
	}
	if (numLines > 20)
	{
		numLines = 20;
	}

	const ImVec2 cursorPos = ImGui::GetCursorPos();
	const ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();

	const ImGuiStyle& style = ImGui::GetStyle();

	// reserve space for an extra line (Mode: History etc)
	float popupHeight = ImGui::GetTextLineHeightWithSpacing() * (float)(numLines + 1) + style.ItemSpacing.y * 3;
	ImVec2 contentRegionAvail;
	contentRegionAvail.x = ImGui::GetContentRegionAvail().x - cursorPos.x - 10.0f;
	contentRegionAvail.y = cursorPos.y - ImGui::GetFrameHeightWithSpacing();
	ImVec2 popupSize(0.0f, contentRegionAvail.y < popupHeight ? contentRegionAvail.y : popupHeight);
	ImGui::SetNextWindowSize(popupSize);

	ImGui::SetNextWindowPos(ImVec2(cursorScreenPos.x, cursorScreenPos.y - ImGui::GetFrameHeightWithSpacing() - popupSize.y), ImGuiCond_Always);
	ImGuiCond filterPopupFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
	if (view->consolePopupOpen && ImGui::BeginPopup("###ConsolePopup", filterPopupFlags))
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			ImGui::CloseCurrentPopup();
			UIRecordedView_Console_ClosePopup(view);
		}
		bool isPopupFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (!isPopupFocused && !view->consoleInputFocused)
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::TextDisabled("Mode: %s", view->consoleMode == kConsoleMode_History ? "History" : "Autocomplete");

		const char* selected = nullptr;
		if (view->consoleMode == kConsoleMode_History)
		{
			for (u32 i = 0; i < view->consoleHistory.entries.count; ++i)
			{
				view_console_history_entry_t* entry = view->consoleHistory.entries.data + i;
				const char* entryCommand = sb_get(&entry->command);
				if (ImGui::Selectable(entryCommand, view->consoleHistory.pos == i))
				{
					selected = entryCommand;
				}
			}
		}
		else
		{
			for (u32 i = 0; i < view->session->consoleAutocomplete.count; ++i)
			{
				const bb_packet_console_autocomplete_response_entry_t* entry = view->session->consoleAutocomplete.data + i;
				if (bb_strnicmp(entry->text, sb_get(&view->consoleInput), sb_len(&view->consoleInput)) == 0)
				{
					if (ImGui::Selectable(entry->text, view->consoleAutocompleteIndex == i))
					{
						selected = entry->text;
					}
					if (ImGui::IsTooltipActive())
					{
						ImGui::BeginTooltip();
						PushUIFont();
						ImGui::TextShadowed(entry->description);
						PopUIFont();
						ImGui::EndTooltip();
					}
				}
			}
		}

		if (selected)
		{
			sb_reset(&view->consoleInput);
			sb_reset(&view->lastConsoleInput);
			view->consoleInput = sb_from_c_string(selected);
			view->lastConsoleInput = sb_clone(&view->consoleInput);
			++view->consoleRequestActive;
		}

		ImGui::EndPopup();
	}
}
