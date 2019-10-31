// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "ui_recordings.h"
#include "bb_array.h"
#include "bb_colors.h"
#include "bb_string.h"
#include "bbserver_utils.h"
#include "config.h"
#include "imgui_themes.h"
#include "imgui_tooltips.h"
#include "imgui_utils.h"
#include "message_queue.h"
#include "path_utils.h"
#include "recorded_session.h"
#include "recordings.h"
#include "ui_view.h"
#include "va.h"
#include "wrap_imgui.h"

using namespace ImGui;

void UIRecordings_Open()
{
	recordings_get_config()->recordingsOpen = true;
}

void UIRecordings_Close()
{
	recordings_get_config()->recordingsOpen = false;
}

float UIRecordings_Width()
{
	return recordings_get_config()->recordingsOpen ? recordings_get_config()->width : 0.0f;
}

float UIRecordings_WidthWhenOpen()
{
	return recordings_get_config()->width;
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

static void UIRecordings_ClearSelection(recording_tab_t tab)
{
	grouped_recordings_t *groupedRecordings = grouped_recordings_get_all(tab);
	u32 i;
	for(i = 0; i < groupedRecordings->count; ++i) {
		groupedRecordings->data[i].selected = false;
	}
}

static void UIRecordings_SetChildSelection(recording_tab_t tab, grouped_recording_entry_t *e)
{
	if(!e->recording) {
		grouped_recordings_t *groupedRecordings = grouped_recordings_get_all(tab);
		u32 startIndex = (u32)(e - groupedRecordings->data);
		u32 endIndex = UIRecordings_FindGroupEndIndex(groupedRecordings, startIndex);
		u32 i;
		for(i = startIndex + 1; i < endIndex; ++i) {
			groupedRecordings->data[i].selected = e->selected;
		}
	}
}

static void UIRecordings_AddSelection(recording_tab_t tab, grouped_recording_entry_t *e)
{
	e->selected = true;
	UIRecordings_SetChildSelection(tab, e);
	grouped_recordings_t *groupedRecordings = grouped_recordings_get_all(tab);
	groupedRecordings->lastClickIndex = (u32)(e - groupedRecordings->data);
}

static void UIRecordings_ToggleSelection(recording_tab_t tab, grouped_recording_entry_t *e)
{
	e->selected = !e->selected;
	UIRecordings_SetChildSelection(tab, e);
	grouped_recordings_t *groupedRecordings = grouped_recordings_get_all(tab);
	groupedRecordings->lastClickIndex = e->selected ? (u32)(e - groupedRecordings->data) : ~0u;
}

static void UIRecordings_HandleClick(recording_tab_t tab, grouped_recording_entry_t *e)
{
	ImGuiIO &io = ImGui::GetIO();
	if(io.KeyAlt || (io.KeyCtrl && io.KeyShift))
		return;

	if(io.KeyCtrl) {
		UIRecordings_ToggleSelection(tab, e);
	} else if(io.KeyShift) {
		grouped_recordings_t *groupedRecordings = grouped_recordings_get_all(tab);
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
		UIRecordings_ClearSelection(tab);
		UIRecordings_AddSelection(tab, e);
	}
}

static void UIRecordings_HandleDoubleClick(recording_tab_t tab, grouped_recording_entry_t *e)
{
	UIRecordings_ClearSelection(tab);
	UIRecordings_AddSelection(tab, e);
}

static void UIRecordings_DeleteSelection(recording_tab_t tab, recordingIds_t *pendingDeletions)
{
	grouped_recordings_t *groupedRecordings = grouped_recordings_get_all(tab);
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

static void UIRecordings_HandlePopupMenu(recording_tab_t tab, grouped_recording_entry_t *e, recordingIds_t *pendingDeletions)
{
	if(!e->selected) {
		UIRecordings_ClearSelection(tab);
		UIRecordings_AddSelection(tab, e);
	}

	u32 totalCount = 0;
	u32 deletableCount = 0;
	u32 activeCount = 0;
	u32 openableCount = 0;

	grouped_recordings_t *groupedRecordings = grouped_recordings_get_all(tab);
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

	if(deletableCount) {
		const char *label = (totalCount > deletableCount) ? va("Delete %u/%u recordings", deletableCount, totalCount) : deletableCount == 1 ? "Delete recording" : va("Delete %u recordings", deletableCount);
		if(ImGui::Selectable(label)) {
			UIRecordings_DeleteSelection(tab, pendingDeletions);
		}
	}

	if(totalCount == 1 && e && e->recording) {
		if(ImGui::Selectable("Open containing folder")) {
			sb_t dir = sb_from_c_string(e->recording->path);
			path_remove_filename(&dir);
			BBServer_OpenDirInExplorer(sb_get(&dir));
			sb_reset(&dir);
		}
		if(e->recording->active) {
			if(e->recording->outgoingMqId != mq_invalid_id()) {
				if(ImGui::Selectable("Stop recording")) {
					mq_queue(e->recording->outgoingMqId, kBBPacketType_StopRecording, "stop");
				}
			}
		}
	} else if(!openableCount && !deletableCount) {
		Text("(No actions)");
	}
}

bool UIRecordings_ConfigMenu(recordings_tab_config_t *tabConfig)
{
	bool ret = false;
	int sort = tabConfig->sort;
	if(ImGui::Combo("Sort", &sort, "Time\0Application\0")) {
		tabConfig->sort = (recording_sort_t)sort;
		ret = true;
	}
	int group = tabConfig->group;
	if(ImGui::Combo("Group", &group, "None\0Application\0")) {
		tabConfig->group = (recording_group_t)group;
		ret = true;
	}
	if(ImGui::Checkbox("Date", &tabConfig->showDate)) {
		ret = true;
	}
	ImGui::SameLine();
	if(ImGui::Checkbox("Time", &tabConfig->showTime)) {
		ret = true;
	}
	ImGui::Separator();
	return ret;
}

static void UIRecordings_Recording(recording_tab_t tab, grouped_recording_entry_t *e, recordingIds_t *pendingDeletions)
{
	recordings_config_t *config = recordings_get_config();
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
	if(config->tabs[tab].showDate && config->tabs[tab].showTime) {
		label = va("%s %s: %s###Selectable", dateBuffer, timeBuffer, recording->applicationName);
	} else if(config->tabs[tab].showDate) {
		label = va("%s: %s###Selectable", dateBuffer, recording->applicationName);
	} else if(config->tabs[tab].showTime) {
		label = va("%s: %s###Selectable", timeBuffer, recording->applicationName);
	} else {
		label = va("%s###Selectable", recording->applicationName);
	}
	if(Selectable(label, e->selected != 0, ImGuiSelectableFlags_AllowDoubleClick)) {
		if(ImGui::IsMouseDoubleClicked(0)) {
			recorded_session_open(recording->path, recording->applicationFilename, false, recording->active, recording->outgoingMqId);
			UIRecordings_HandleDoubleClick(tab, e);
		} else {
			UIRecordings_HandleClick(tab, e);
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
		UIRecordings_HandlePopupMenu(tab, e, pendingDeletions);
		ImGui::EndPopup();
	}
	PopID();
}

void UIRecordings_UpdateTab(recording_tab_t tab)
{
	recordings_config_t *config = recordings_get_config();

	bool scrollToBottom = false;
	bool windowSelected = ImGui::IsWindowFocused();
	grouped_recordings_t *groupedRecordings = grouped_recordings_get_all(tab);
	//if(BeginMenuBar()) {
	//	if(ImGui::BeginMenu("View")) {
	if(UIRecordings_ConfigMenu(config->tabs + tab)) {
		scrollToBottom = true;
		recordings_sort(tab);
		recordings_clear_dirty(tab);
	}
	//		ImGui::EndMenu();
	//	}
	//	EndMenuBar();
	//}

	const float normal = 0.4f;
	const float hovered = 0.8f;
	const float active = 0.8f;
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(normal, normal, normal, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(hovered, hovered, hovered, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(active, active, active, 1.0f));
	ImGui::Button("|", ImVec2(config_get_resizeBarSize(&g_config), ImGui::GetContentRegionAvail().y - 2));
	ImGui::PopStyleColor(3);
	if(ImGui::IsItemActive() || ImGui::IsItemHovered()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	if(ImGui::IsItemActive()) {
		config->width -= ImGui::GetIO().MouseDelta.x;
	}

	ImGui::SameLine();

	if(ImGui::BeginChild(va("Recordings%d", tab))) {
		if(recordings_are_dirty(tab)) {
			scrollToBottom = true;
			recordings_sort(tab);
			recordings_clear_dirty(tab);
		}

		recordingIds_t pendingDeletions;
		memset(&pendingDeletions, 0, sizeof(pendingDeletions));
		if(config->tabs[tab].group == kRecordingGroup_None) {
			for(u32 i = 0; i < groupedRecordings->count; ++i) {
				grouped_recording_entry_t *e = groupedRecordings->data + i;
				if(e->recording) {
					UIRecordings_Recording(tab, e, &pendingDeletions);
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
					UIRecordings_Recording(tab, startEntry + 1, &pendingDeletions);
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
						UIRecordings_HandleClick(tab, startEntry);
					}
					if(ImGui::BeginPopupContextItem(va("RecordingGroupContextMenu%u", startEntry->groupId))) {
						UIRecordings_HandlePopupMenu(tab, startEntry, &pendingDeletions);
						ImGui::EndPopup();
					}
					if(open) {
						for(u32 i = startIndex + 1; i < endIndex; ++i) {
							UIRecordings_Recording(tab, groupedRecordings->data + i, &pendingDeletions);
						}
						TreePop();
					}
				}
				startIndex = endIndex;
			}
		}

		if(ImGui::IsKeyPressed(ImGuiKey_Delete)) {
			if(windowSelected) {
				UIRecordings_DeleteSelection(tab, &pendingDeletions);
			}
		}

		if(pendingDeletions.count) {
			for(u32 i = 0; i < pendingDeletions.count; ++i) {
				recordings_delete_by_id(pendingDeletions.data[i]);
			}
			recordings_sort(tab);
			bba_free(pendingDeletions);
		}

		if(scrollToBottom) {
			ImGui::SetScrollHere();
		}
	}
	ImGui::EndChild();
}

static const char *s_recordingTabNames[] = {
	"Internal",
	"External",
};
BB_CTASSERT(BB_ARRAYSIZE(s_recordingTabNames) == kRecordingTab_Count);

void UIRecordings_Update(bool autoTileViews)
{
	recordings_config_t *config = recordings_get_config();
	if(!config->recordingsOpen)
		return;

	if(autoTileViews) {
		ImVec2 viewportPos(0.0f, 0.0f);
		ImGuiViewport *viewport = ImGui::GetViewportForWindow("Recordings");
		if(viewport) {
			viewportPos.x += viewport->Pos.x;
			viewportPos.y += viewport->Pos.y;
		}
		float startY = ImGui::GetFrameHeight();
		ImGuiIO &io = ImGui::GetIO();
		SetNextWindowSize(ImVec2(config->width, io.DisplaySize.y - startY), ImGuiCond_Always);
		SetNextWindowPos(ImVec2(viewportPos.x + io.DisplaySize.x - config->width, viewportPos.y + startY), ImGuiCond_Always);
	}

	int windowFlags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDocking;
	if(autoTileViews) {
		windowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	}

	if(Begin("Recordings", &config->recordingsOpen, windowFlags)) {
		ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
		if(ImGui::BeginTabBar("RecordingsTab", tab_bar_flags)) {
			for(u32 tab = 0; tab < kRecordingTab_Count; ++tab) {
				if(ImGui::BeginTabItem(s_recordingTabNames[tab])) {
					UIRecordings_UpdateTab((recording_tab_t)tab);
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
	}

	End();
	if(autoTileViews) {
		PopStyleVar(); // ImGuiStyleVar_WindowRounding
	}
}
