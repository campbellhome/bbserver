// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "imgui_tooltips.h"
#include "config.h"
#include "imgui_core.h"
#include "wrap_imgui.h"
#include "wrap_imgui_internal.h"

namespace ImGui
{

	bool IsTooltipActive(tooltipConfig* tooltips)
	{
		if (!tooltips)
		{
			tooltips = &g_config.tooltips;
		}
		if (IsItemHovered() && tooltips->enabled)
		{
			if (GImGui->HoveredIdTimer >= tooltips->delay)
			{
				return true;
			}
			else
			{
				Imgui_Core_RequestRender();
			}
		}
		return false;
	}

} // namespace ImGui
