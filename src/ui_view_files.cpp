// Copyright (c) 2012-2020 Matt Campbell
// MIT license (see License.txt)

#include "ui_view_files.h"
#include "imgui_tooltips.h"
#include "imgui_utils.h"
#include "recorded_session.h"
#include "ui_loglevel_colorizer.h"
#include "ui_view.h"
#include "va.h"
#include "view.h"
#include "view_config.h"
#include "wrap_imgui.h"
#include "wrap_imgui_internal.h"

using namespace ImGui;

static void CheckboxFileVisiblity(view_t *view, u32 startIndex)
{
	view_files_t *files = &view->files;
	view_file_t *t = files->data + startIndex;
	PushID((int)startIndex);
	if(Checkbox("", &t->visible)) {
		view->visibleLogsDirty = true;
	}
	PopID();
}

static void CheckboxAllFileVisiblity(view_t *view)
{
	view_files_t *files = &view->files;
	bool allChecked = true;
	for(u32 index = 0; index < files->count; ++index) {
		view_file_t *t = files->data + index;
		if(!t->visible) {
			allChecked = false;
			break;
		}
	}
	PushID(-1);
	if(Checkbox("", &allChecked)) {
		view_set_all_file_visibility(view, allChecked);
	}
	PopID();
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

void UIRecordedView_FileTreeNode(view_t *view, u32 startIndex)
{
	view_file_t *vf = view->files.data + startIndex;
	recorded_filename_t *rf = recorded_session_find_filename(view->session, vf->id);
	const char *fileName = GetFilenameFromPath(vf->path);
	CheckboxFileVisiblity(view, startIndex);
	ImGui::SameLine();
	{
		LogLevelColorizer colorizer(GetLogLevelBasedOnCounts(rf->logCount), false);
		if(ImGui::TreeNodeEx(va("%s###File%u", fileName, startIndex),
		                     DefaultOpenTreeNodeFlags | ImGuiTreeNodeFlags_Leaf, &vf->selected)) {
			ImGui::TreePop();
		}
	}

	if(IsTooltipActive()) {
		BeginTooltip();
		Text("%s", rf->path);
		UIRecordedView_TooltipLevelText("VeryVerbose: %u", rf->logCount[kBBLogLevel_VeryVerbose], kBBLogLevel_VeryVerbose);
		UIRecordedView_TooltipLevelText("Verbose: %u", rf->logCount[kBBLogLevel_Verbose], kBBLogLevel_Verbose);
		UIRecordedView_TooltipLevelText("Logs: %u", rf->logCount[kBBLogLevel_Log], kBBLogLevel_Log);
		UIRecordedView_TooltipLevelText("Display: %u", rf->logCount[kBBLogLevel_Display], kBBLogLevel_Display);
		UIRecordedView_TooltipLevelText("Warnings: %u", rf->logCount[kBBLogLevel_Warning], kBBLogLevel_Warning);
		UIRecordedView_TooltipLevelText("Errors: %u", rf->logCount[kBBLogLevel_Error], kBBLogLevel_Error);
		UIRecordedView_TooltipLevelText("Fatals: %u", rf->logCount[kBBLogLevel_Fatal], kBBLogLevel_Fatal);
		EndTooltip();
	}
}

void UIViewFiles_Update(view_t *view)
{
	CheckboxAllFileVisiblity(view);
	ImGui::SameLine();
	if(ImGui::TreeNodeEx("Files", DefaultOpenTreeNodeFlags)) {
		if(ImGui::BeginPopupContextItem("FilesContextMenu")) {
			if(ImGui::Selectable("Check All")) {
				view_set_all_file_visibility(view, true);
			}
			if(ImGui::Selectable("Uncheck All")) {
				view_set_all_file_visibility(view, false);
			}
			ImGui::EndPopup();
		}
		for(u32 index = 0; index < view->files.count; ++index) {
			UIRecordedView_FileTreeNode(view, index);
		}
		ImGui::TreePop();
	}
}
