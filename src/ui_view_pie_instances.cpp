// Copyright (c) 2012-2020 Matt Campbell
// MIT license (see License.txt)

#include "ui_view_pie_instances.h"
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

static void CheckboxPIEInstanceVisiblity(view_t *view, u32 startIndex)
{
	view_pieInstances_t *pieInstances = &view->pieInstances;
	view_pieInstance_t *t = pieInstances->data + startIndex;
	PushID((int)startIndex);
	if(Checkbox("", &t->visible)) {
		view->visibleLogsDirty = true;
	}
	PopID();
}

static void CheckboxAllPIEInstanceVisiblity(view_t *view)
{
	view_pieInstances_t *pieInstances = &view->pieInstances;
	bool allChecked = true;
	for(u32 index = 0; index < pieInstances->count; ++index) {
		view_pieInstance_t *t = pieInstances->data + index;
		if(!t->visible) {
			allChecked = false;
			break;
		}
	}
	PushID(-1);
	if(Checkbox("", &allChecked)) {
		view_set_all_pieinstance_visibility(view, allChecked);
	}
	PopID();
}

void UIRecordedView_PIEInstanceTreeNode(view_t *view, u32 startIndex)
{
	view_pieInstance_t *vf = view->pieInstances.data + startIndex;
	if(vf) {
		recorded_pieInstance_t *rf = recorded_session_find_pieInstance(view->session, vf->pieInstance);
		CheckboxPIEInstanceVisiblity(view, startIndex);
		ImGui::SameLine();
		{
			LogLevelColorizer colorizer(rf ? GetLogLevelBasedOnCounts(rf->logCount) : kBBLogLevel_VeryVerbose, false);
			if(ImGui::TreeNodeEx((vf->primary != 0) ? "-" : va("%d##PIEInstance%u", vf->pieInstance, startIndex),
			                     DefaultOpenTreeNodeFlags | ImGuiTreeNodeFlags_Leaf, &vf->selected)) {
				ImGui::TreePop();
			}
		}
		if(rf) {
			if(IsTooltipActive()) {
				BeginTooltip();
				if(vf->primary != 0) {
					Text("PIEInstance -");
				} else {
					Text("PIEInstance %d", rf->pieInstance);
				}
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
	}
}

void UIViewPieInstances_Update(view_t *view)
{
	CheckboxAllPIEInstanceVisiblity(view);
	ImGui::SameLine();
	if(ImGui::TreeNodeEx("PIE Instances", DefaultOpenTreeNodeFlags)) {
		if(ImGui::BeginPopupContextItem("PIEInstanceContextMenu")) {
			if(ImGui::Selectable("Check All")) {
				view_set_all_pieinstance_visibility(view, true);
			}
			if(ImGui::Selectable("Uncheck All")) {
				view_set_all_pieinstance_visibility(view, false);
			}
			ImGui::EndPopup();
		}
		for(u32 index = 0; index < view->pieInstances.count; ++index) {
			UIRecordedView_PIEInstanceTreeNode(view, index);
		}
		ImGui::TreePop();
	}
}
