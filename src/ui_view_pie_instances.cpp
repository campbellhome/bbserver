// Copyright (c) 2012-2020 Matt Campbell
// MIT license (see License.txt)

#include "ui_view_pie_instances.h"
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

static void CheckboxPIEInstanceVisiblity(view_t* view, u32 startIndex)
{
	view_pieInstances_t* pieInstances = &view->pieInstances;
	view_pieInstance_t* t = pieInstances->data + startIndex;
	PushID((int)startIndex);
	if (Checkbox("", &t->visible))
	{
		view->visibleLogsDirty = true;
	}
	PopID();
}

void UIRecordedView_PIEInstanceTreeNode(view_t* view, u32 startIndex)
{
	view_pieInstance_t* vf = view->pieInstances.data + startIndex;
	if (vf)
	{
		recorded_pieInstance_t* rf = recorded_session_find_pieInstance(view->session, vf->pieInstance);
		CheckboxPIEInstanceVisiblity(view, startIndex);

		ImGui::SameLine();

		bb_log_level_e logLevel = GetLogLevelBasedOnCounts(rf->logCount);
		LogLevelColorizer colorizer(logLevel);
		ScopedTextShadows shadows(logLevel);
		ImVec2 pos = ImGui::GetIconPosForText();
		const char* pieInstanceName = (vf->primary != 0) ? "-" : va("%d##PIEInstance%u", vf->pieInstance, startIndex);
		ImGui::TextShadow(pieInstanceName);
		if (ImGui::Selectable(pieInstanceName, vf->selected != 0))
		{
			// handle click
		}

		if (rf)
		{
			if (IsTooltipActive())
			{
				BeginTooltip();
				if (vf->primary != 0)
				{
					Text("PIEInstance -");
				}
				else
				{
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

void UIViewPieInstances_Update(view_t* view)
{
	if (view->pieInstances.count > 1)
	{
		if (ImGui::CollapsingHeader("PIE Instances", ImGuiTreeNodeFlags_None))
		{
			ImGui::PushID("PIEInstancesHeader");

			u32 checkedCount = 0;
			u32 uncheckedCount = 0;
			for (u32 index = 0; index < view->pieInstances.count; ++index)
			{
				view_pieInstance_t* vf = view->pieInstances.data + index;
				if (vf->visible)
				{
					++checkedCount;
				}
				else
				{
					++uncheckedCount;
				}
			}

			ImGui::PushID(-1);
			bool allChecked = uncheckedCount == 0;
			if (checkedCount && uncheckedCount)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
			}
			if (ImGui::Checkbox("", &allChecked))
			{
				view_set_all_pieinstance_visibility(view, allChecked);
			}
			if (checkedCount && uncheckedCount)
			{
				ImGui::PopItemFlag();
			}
			ImGui::SameLine();
			ImGui::TextUnformatted("All PIE Instances");
			ImGui::PopID();

			for (u32 index = 0; index < view->pieInstances.count; ++index)
			{
				UIRecordedView_PIEInstanceTreeNode(view, index);
			}
			ImGui::PopID();
		}
	}
}
