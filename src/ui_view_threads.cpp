// Copyright (c) 2012-2020 Matt Campbell
// MIT license (see License.txt)

#include "ui_view_threads.h"
#include "imgui_text_shadows.h"
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

static void CheckboxThreadVisiblity(view_t *view, u32 startIndex)
{
	view_threads_t *threads = &view->threads;
	view_thread_t *t = threads->data + startIndex;
	PushID((int)startIndex);
	if(Checkbox("", &t->visible)) {
		view->visibleLogsDirty = true;
	}
	PopID();
}

void UIRecordedView_ThreadTreeNode(view_t *view, u32 startIndex)
{
	recorded_threads_t *threads = &view->session->threads;
	view_threads_t *viewThreads = &view->threads;

	recorded_thread_t *t = threads->data + startIndex;
	view_thread_t *vt = viewThreads->data + startIndex;

	const char *threadName = t->threadName;
	CheckboxThreadVisiblity(view, startIndex);

	ImGui::SameLine();

	bb_log_level_e logLevel = GetLogLevelBasedOnCounts(t->logCount);
	LogLevelColorizer colorizer(logLevel);
	ScopedTextShadows shadows(logLevel);
	ImVec2 pos = ImGui::GetIconPosForText();
	ImGui::TextShadow(threadName);
	if(ImGui::Selectable(threadName, vt->selected != 0)) {
		// handle click
	}

	if(IsTooltipActive()) {
		BeginTooltip();
		UIRecordedView_TooltipLevelText("VeryVerbose: %u", t->logCount[kBBLogLevel_VeryVerbose], kBBLogLevel_VeryVerbose);
		UIRecordedView_TooltipLevelText("Verbose: %u", t->logCount[kBBLogLevel_Verbose], kBBLogLevel_Verbose);
		UIRecordedView_TooltipLevelText("Logs: %u", t->logCount[kBBLogLevel_Log], kBBLogLevel_Log);
		UIRecordedView_TooltipLevelText("Display: %u", t->logCount[kBBLogLevel_Display], kBBLogLevel_Display);
		UIRecordedView_TooltipLevelText("Warnings: %u", t->logCount[kBBLogLevel_Warning], kBBLogLevel_Warning);
		UIRecordedView_TooltipLevelText("Errors: %u", t->logCount[kBBLogLevel_Error], kBBLogLevel_Error);
		UIRecordedView_TooltipLevelText("Fatals: %u", t->logCount[kBBLogLevel_Fatal], kBBLogLevel_Fatal);
		EndTooltip();
	}
}

void UIViewThreads_Update(view_t *view)
{
	if(view->threads.count > 1) {
		if(ImGui::CollapsingHeader("Threads", ImGuiTreeNodeFlags_None)) {
			ImGui::PushID("ThreadsHeader");

			u32 checkedCount = 0;
			u32 uncheckedCount = 0;
			for(u32 index = 0; index < view->threads.count; ++index) {
				view_thread_t *vf = view->threads.data + index;
				if(vf->visible) {
					++checkedCount;
				} else {
					++uncheckedCount;
				}
			}

			ImGui::PushID(-1);
			bool allChecked = uncheckedCount == 0;
			if(checkedCount && uncheckedCount) {
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
			}
			if(ImGui::Checkbox("", &allChecked)) {
				view_set_all_thread_visibility(view, allChecked);
			}
			if(checkedCount && uncheckedCount) {
				ImGui::PopItemFlag();
			}
			ImGui::SameLine();
			ImGui::TextUnformatted("All Threads");
			ImGui::PopID();

			for(u32 index = 0; index < view->threads.count; ++index) {
				UIRecordedView_ThreadTreeNode(view, index);
			}
			ImGui::PopID();
		}
	}
}
