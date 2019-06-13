// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "imgui_tooltips.h"
#include "config.h"
#include "imgui_core.h"
#include "wrap_imgui.h"

// warning C4820 : 'StructName' : '4' bytes padding added after data member 'MemberName'
// warning C4365: '=': conversion from 'ImGuiTabItemFlags' to 'ImGuiID', signed/unsigned mismatch
BB_WARNING_PUSH(4820, 4365)
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
BB_WARNING_POP

namespace ImGui
{

	bool IsTooltipActive(tooltipConfig *tooltips)
	{
		if(!tooltips) {
			tooltips = &g_config.tooltips;
		}
		if(IsItemHovered() && tooltips->enabled) {
			if(GImGui->HoveredIdTimer >= tooltips->delay) {
				return true;
			} else {
				Imgui_Core_RequestRender();
			}
		}
		return false;
	}

} // namespace ImGui
