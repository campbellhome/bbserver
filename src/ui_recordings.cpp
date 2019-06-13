// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "ui_recordings.h"
#include "bb_colors.h"
#include "bbclient/bb_array.h"
#include "bbclient/bb_string.h"
#include "config.h"
#include "imgui_themes.h"
#include "imgui_tooltips.h"
#include "imgui_utils.h"
#include "recorded_session.h"
#include "recordings.h"
#include "ui_view.h"
#include "va.h"
#include "wrap_imgui.h"

using namespace ImGui;

void UIRecordings_Open()
{
	g_config.recordingsOpen = true;
}

void UIRecordings_Close()
{
	g_config.recordingsOpen = false;
}

float UIRecordings_Width()
{
	return g_config.recordingsOpen ? recordings_get_all()->width : 0.0f;
}

float UIRecordings_WidthWhenOpen()
{
	return recordings_get_all()->width;
}

static u32 UIRecordings_FindGroupEndIndex(grouped_recordings_t *groupedRecordings, u32 startIndex)
{
	grouped_recording_entry_t *e = groupedRecordings->data + startIndex;
	u32 groupId = e->groupId;
	u32 endIndex = startIndex;
	while(endIndex < groupedRecordings->count &&
	      groupId == groupedRecordings->data[endIndex].groupId) {
		++endIndex;
	}
	return endIndex;
}

static void UIRecordings_ClearSelection()
{
	grouped_recordings_t *groupedRecordings = grouped_recordings_get_all();
	u32 i;
	for(i = 0; i < groupedRecordings->count; ++i) {
		groupedRecordings->data[i].selected = false;
	}
}

static void UIRecordings_SetChildSelection(grouped_recording_entry_t *e)
{
	if(!e->recording) {
		grouped_recordings_t *groupedRecordings = grouped_recordings_get_all();
		u32 startIndex = (u32)(e - groupedRecordings->data);
		u32 endIndex = UIRecordings_FindGroupEndIndex(groupedRecordings, startIndex);
		u32 i;
		for(i = startIndex + 1; i < endIndex; ++i) {
			groupedRecordings->data[i].selected = e->selected;
		}
	}
}

static void UIRecordings_AddSelection(grouped_recording_entry_t *e)
{
	e->selected = true;
	UIRecordings_SetChildSelection(e);
	grouped_recordings_t *groupedRecordings = grouped_recordings_get_all();
	groupedRecordings->lastClickIndex = (u32)(e - groupedRecordings->data);
}

static void UIRecordings_ToggleSelection(grouped_recording_entry_t *e)
{
	e->selected = !e->selected;
	UIRecordings_SetChildSelection(e);
	grouped_recordings_t *groupedRecordings = grouped_recordings_get_all();
	groupedRecordings->lastClickIndex = e->selected ? (u32)(e - groupedRecordings->data) : ~0u;
}

static void UIRecordings_HandleClick(grouped_recording_entry_t *e)
{
	ImGuiIO &io = ImGui::GetIO();
	if(io.KeyAlt || (io.KeyCtrl && io.KeyShift))
		return;

	if(io.KeyCtrl) {
		UIRecordings_ToggleSelection(e);
	} else if(io.KeyShift) {
		grouped_recordings_t *groupedRecordings = grouped_recordings_get_all();
		if(groupedRecordings->lastClickIndex < groupedRecordings->count) {
			u32 startIndex = groupedRecordings->lastClickIndex;
			u32 endIndex = (u32)(e - groupedRecordings->data);
			groupedRecordings->lastClickIndex = endIndex;
			if(endIndex < startIndex) {
				u32 tmp = endIndex;
				endIndex = startIndex;
				startIndex = tmp;
			}
			for(u32 i = startIndex; i <= endIndex; ++i) {
				groupedRecordings->data[i].selected = true;
			}
		}
	} else {
		UIRecordings_ClearSelection();
		UIRecordings_AddSelection(e);
	}
}

static void UIRecordings_HandleDoubleClick(grouped_recording_entry_t *e)
{
	UIRecordings_ClearSelection();
	UIRecordings_AddSelection(e);
}

static void UIRecordings_DeleteSelection(recordingIds_t *pendingDeletions)
{
	grouped_recordings_t *groupedRecordings = grouped_recordings_get_all();
	for(u32 i = 0; i < groupedRecordings->count; ++i) {
		grouped_recording_entry_t *entry = groupedRecordings->data + i;
		if(!entry->selected || !entry->recording || entry->recording->active)
			continue;

		bool hasView = recorded_session_find(entry->recording->path) != nullptr;
		if(!hasView) {
			bba_push(*pendingDeletions, entry->recording->id);
		}
	}
}

static void UIRecordings_HandlePopupMenu(grouped_recording_entry_t *e, recordingIds_t *pendingDeletions)
{
	if(!e->selected) {
		UIRecordings_ClearSelection();
		UIRecordings_AddSelection(e);
	}

	u32 totalCount = 0;
	u32 deletableCount = 0;
	u32 activeCount = 0;
	u32 openableCount = 0;

	grouped_recordings_t *groupedRecordings = grouped_recordings_get_all();
	for(u32 i = 0; i < groupedRecordings->count; ++i) {
		grouped_recording_entry_t *entry = groupedRecordings->data + i;
		if(!entry->selected || !entry->recording)
			continue;

		++totalCount;
		bool hasView = recorded_session_find(entry->recording->path) != nullptr;
		if(!hasView) {
			++openableCount;
		}
		if(entry->recording->active) {
			++activeCount;
		} else if(!hasView) {
			++deletableCount;
		}
	}

	if(openableCount) {
		if(ImGui::Selectable(openableCount == 1 ? "Open view" : va("Open %u views", openableCount))) {
			for(u32 i = 0; i < groupedRecordings->count; ++i) {
				grouped_recording_entry_t *entry = groupedRecordings->data + i;
				if(!entry->selected || !entry->recording)
					continue;

				bool hasView = recorded_session_find(entry->recording->path) != nullptr;
				if(!hasView) {
					recorded_session_open(entry->recording->path, entry->recording->applicationFilename, false, entry->recording->active, entry->recording->outgoingMqId);
				}
			}
		}
	}

	// #todo: open containing folder?

	if(deletableCount) {
		const char *label = (totalCount > deletableCount) ? va("Delete %u/%u recordings", deletableCount, totalCount) : deletableCount == 1 ? "Delete recording" : va("Delete %u recordings", deletableCount);
		if(ImGui::Selectable(label)) {
			UIRecordings_DeleteSelection(pendingDeletions);
		}
	}

	if(!openableCount && !deletableCount) {
		Text("(No actions)");
	}
}

bool UIRecordings_ConfigMenu(recordings_t *recordings)
{
	bool ret = false;
	if(BeginMenu("Sort")) {
		if(RadioButton("Start Time", recordings->sort == kRecordingSort_StartTime)) {
			recordings->sort = kRecordingSort_StartTime;
			ret = true;
		}
		if(RadioButton("Application", recordings->sort == kRecordingSort_Application)) {
			recordings->sort = kRecordingSort_Application;
			ret = true;
		}
		ImGui::EndMenu();
	}
	if(BeginMenu("Group")) {
		if(RadioButton("None", recordings->group == kRecordingGroup_None)) {
			recordings->group = kRecordingGroup_None;
			ret = true;
		}
		if(RadioButton("Application", recordings->group == kRecordingGroup_Application)) {
			recordings->group = kRecordingGroup_Application;
			ret = true;
		}
		ImGui::EndMenu();
	}
	if(MenuItem("Show Date", nullptr, &recordings->showDate)) {
		ret = true;
	}
	if(MenuItem("Show Time", nullptr, &recordings->showTime)) {
		ret = true;
	}
	return ret;
}

static void UIRecordings_Recording(recordings_t *recordings, grouped_recording_entry_t *e, recordingIds_t *pendingDeletions)
{
	recording_t *recording = e->recording;
	FILETIME ft = { recording->filetimeLow, recording->filetimeHigh };
	SYSTEMTIME st;
	char timeBuffer[256] = "";
	char dateBuffer[256] = "";
	FileTimeToLocalFileTime(&ft, &ft);
	if(FileTimeToSystemTime(&ft, &st)) {
		GetTimeFormat(
		    LOCALE_USER_DEFAULT,
		    TIME_NOSECONDS,
		    &st,
		    NULL,
		    timeBuffer,
		    sizeof(timeBuffer));
		GetDateFormat(
		    LOCALE_USER_DEFAULT,
		    DATE_SHORTDATE,
		    &st,
		    NULL,
		    dateBuffer,
		    sizeof(dateBuffer));
	}

	PushID((int)recording->id);
	PushStyleColor(ImGuiCol_Text, recording->active ? MakeColor(kStyleColor_ActiveSession) : MakeColor(kStyleColor_InactiveSession));
	const char *label = nullptr;
	if(recordings->showDate && recordings->showTime) {
		label = va("%s %s: %s###Selectable", dateBuffer, timeBuffer, recording->applicationName);
	} else if(recordings->showDate) {
		label = va("%s: %s###Selectable", dateBuffer, recording->applicationName);
	} else if(recordings->showTime) {
		label = va("%s: %s###Selectable", timeBuffer, recording->applicationName);
	} else {
		label = va("%s###Selectable", recording->applicationName);
	}
	if(Selectable(label, e->selected != 0, ImGuiSelectableFlags_AllowDoubleClick)) {
		if(ImGui::IsMouseDoubleClicked(0)) {
			recorded_session_open(recording->path, recording->applicationFilename, false, recording->active, recording->outgoingMqId);
			UIRecordings_HandleDoubleClick(e);
		} else {
			UIRecordings_HandleClick(e);
		}
	}
	PopStyleColor();
	if(IsTooltipActive()) {
		if(recording->platform == kBBPlatform_Unknown) {
			SetTooltip("%s", recording->path);
		} else {
			SetTooltip("%s - %s", recording->path, bb_platform_name((bb_platform_e)recording->platform));
		}
	}
	if(ImGui::BeginPopupContextItem("RecordingContextMenu")) {
		UIRecordings_HandlePopupMenu(e, pendingDeletions);
		ImGui::EndPopup();
	}
	PopID();
}

void UIRecordings_Update(bool autoTileViews)
{
	if(!g_config.recordingsOpen)
		return;

	recordings_t *recordings = recordings_get_all();
	if(autoTileViews) {
		float startY = ImGui::GetFrameHeight();
		ImGuiIO &io = ImGui::GetIO();
		SetNextWindowSize(ImVec2(recordings->width, io.DisplaySize.y - startY), ImGuiSetCond_Always);
		SetNextWindowPos(ImVec2(io.DisplaySize.x - recordings->width, startY), ImGuiSetCond_Always);
	}

	int windowFlags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
	if(autoTileViews) {
		windowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	}
	if(Begin("Recordings", &g_config.recordingsOpen, windowFlags)) {
		bool scrollToBottom = false;
		bool windowSelected = ImGui::IsWindowFocused();
		grouped_recordings_t *groupedRecordings = grouped_recordings_get_all();
		if(BeginMenuBar()) {
			if(ImGui::BeginMenu("View")) {
				if(UIRecordings_ConfigMenu(recordings)) {
					scrollToBottom = true;
					recordings_sort();
					recordings_clear_dirty();
				}
				ImGui::EndMenu();
			}
			EndMenuBar();
		}

		const float normal = 0.4f;
		const float hovered = 0.8f;
		const float active = 0.8f;
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(normal, normal, normal, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(hovered, hovered, hovered, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(active, active, active, 1.0f));
		ImGui::Button("|", ImVec2(3 * g_config.dpiScale, ImGui::GetContentRegionAvail().y - 2));
		ImGui::PopStyleColor(3);
		if(ImGui::IsItemActive() || ImGui::IsItemHovered()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}
		if(ImGui::IsItemActive()) {
			recordings->width -= ImGui::GetIO().MouseDelta.x;
		}

		ImGui::SameLine();

		if(ImGui::BeginChild("Recordings")) {
			if(recordings_are_dirty()) {
				scrollToBottom = true;
				recordings_sort();
				recordings_clear_dirty();
			}

			recordingIds_t pendingDeletions;
			memset(&pendingDeletions, 0, sizeof(pendingDeletions));
			if(recordings->group == kRecordingGroup_None) {
				for(u32 i = 0; i < groupedRecordings->count; ++i) {
					grouped_recording_entry_t *e = groupedRecordings->data + i;
					if(e->recording) {
						UIRecordings_Recording(recordings, e, &pendingDeletions);
					}
				}
			} else {
				ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow |
				                                ImGuiTreeNodeFlags_OpenOnDoubleClick;
				u32 startIndex = 0;
				while(startIndex < groupedRecordings->count) {
					u32 endIndex = UIRecordings_FindGroupEndIndex(groupedRecordings, startIndex);
					grouped_recording_entry_t *startEntry = groupedRecordings->data + startIndex;
					recording_t *startRecording = startEntry[1].recording;
					if(startIndex + 2 == endIndex) {
						UIRecordings_Recording(recordings, startEntry + 1, &pendingDeletions);
					} else {
						u32 activeCount = 0;
						u32 inactiveCount = 0;
						u32 deletableCount = 0;
						for(u32 i = startIndex + 1; i < endIndex; ++i) {
							if(groupedRecordings->data[i].recording->active) {
								++activeCount;
							} else {
								++inactiveCount;
								if(!recorded_session_find(groupedRecordings->data[i].recording->path)) {
									++deletableCount;
								}
							}
						}
						PushStyleColor(ImGuiCol_Text, activeCount ? MakeColor(kStyleColor_ActiveSession) : MakeColor(kStyleColor_InactiveSession));
						bool open = ImGui::TreeNodeEx(va("%s###%s", startRecording->applicationName, startRecording->path),
						                              node_flags | (startEntry->selected ? ImGuiTreeNodeFlags_Selected : 0));
						PopStyleColor();
						if(ImGui::IsItemClicked()) {
							UIRecordings_HandleClick(startEntry);
						}
						if(ImGui::BeginPopupContextItem(va("RecordingGroupContextMenu%u", startEntry->groupId))) {
							UIRecordings_HandlePopupMenu(startEntry, &pendingDeletions);
							ImGui::EndPopup();
						}
						if(open) {
							for(u32 i = startIndex + 1; i < endIndex; ++i) {
								UIRecordings_Recording(recordings, groupedRecordings->data + i, &pendingDeletions);
							}
							TreePop();
						}
					}
					startIndex = endIndex;
				}
			}

			if(ImGui::IsKeyPressed(ImGuiKey_Delete)) {
				if(windowSelected) {
					UIRecordings_DeleteSelection(&pendingDeletions);
				}
			}

			if(pendingDeletions.count) {
				for(u32 i = 0; i < pendingDeletions.count; ++i) {
					recordings_delete_by_id(pendingDeletions.data[i], nullptr);
				}
				recordings_sort();
				bba_free(pendingDeletions);
			}

			if(scrollToBottom) {
				ImGui::SetScrollHere();
			}
		}
		ImGui::EndChild();
	}
	End();
	if(autoTileViews) {
		PopStyleVar(); // ImGuiStyleVar_WindowRounding
	}
}
