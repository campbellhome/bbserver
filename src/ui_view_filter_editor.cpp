// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "ui_view_filter_editor.h"
#include "bb_array.h"
#include "bb_colors.h"
#include "bb_string.h"
#include "config.h"
#include "fonts.h"
#include "imgui_core.h"
#include "imgui_tooltips.h"
#include "imgui_utils.h"
#include "site_config.h"
#include "ui_view.h"
#include "va.h"
#include "view.h"
#include "wrap_imgui_internal.h"

bool s_filterConfigOpen = false;
bool s_filterModified = false;
bool s_filterEverShown = false;

void UIFilterConfig_Open()
{
	s_filterConfigOpen = true;
}

void UIFilterConfig_Reset()
{
	s_filterConfigOpen = false;
	s_filterModified = false;
}

void SetNextDefaultWidth(float width)
{
	if (s_filterEverShown)
	{
		ImGui::SetNextItemWidth(-1.0f);
	}
	else
	{
		ImGui::SetNextItemWidth(width);
	}
}

void UIFilterConfig_UpdateFilterName(named_filter_t& filter)
{
	SetNextDefaultWidth(256.0f);
	if (ImGui::InputText("###InputTextFilterName", &filter.name, 2048, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		s_filterModified = true;
	}
}

void UIFilterConfig_UpdateFilterText(named_filter_t& filter)
{
	SetNextDefaultWidth(768.0f);
	if (ImGui::InputText("###InputTextFilterName", &filter.text, 2048, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		s_filterModified = true;
	}
}

void UIFilterConfig_UpdateFilterEnabled(named_filter_t& filter)
{
	if (ImGui::Checkbox("", &filter.filterEnabled))
	{
		s_filterModified = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("If disabled, filter cannot be selected in filter dropdown, nor can it colorize log lines");
	}
}

void UIFilterConfig_UpdateFilterSelectable(named_filter_t& filter)
{
	if (ImGui::Checkbox("", &filter.filterSelectable))
	{
		s_filterModified = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Can this filter be selected in the filter dropdown?");
	}
}

void UIFilterConfig_UpdateFilterCustomColors(named_filter_t& filter)
{
	if (ImGui::Checkbox("", &filter.customColorsEnabled))
	{
		s_filterModified = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Can this filter colorize logs?");
	}
}

void UIFilterConfig_UpdateFilterLogSelected(named_filter_t& filter)
{
	SetNextDefaultWidth(128.0f);
	const char* items[] = { "", "Selected", "Not Selected" };
	int index = 0;
	if (filter.testSelected)
	{
		if (filter.selected)
		{
			index = 1;
		}
		else
		{
			index = 2;
		}
	}
	if (ImGui::Combo("", &index, items, IM_COUNTOF(items)))
	{
		switch (index)
		{
		case 0:
			filter.testSelected = false;
			break;
		case 1:
			filter.testSelected = true;
			filter.selected = true;
			break;
		case 2:
			filter.testSelected = true;
			filter.selected = false;
			break;
		}
		s_filterModified = true;
	}
}

void UIFilterConfig_UpdateFilterLogBookmarked(named_filter_t& filter)
{
	SetNextDefaultWidth(128.0f);
	const char* items[] = { "", "Bookmarked", "Not Bookmarked" };
	int index = 0;
	if (filter.testBookmarked)
	{
		if (filter.bookmarked)
		{
			index = 1;
		}
		else
		{
			index = 2;
		}
	}
	if (ImGui::Combo("", &index, items, IM_COUNTOF(items)))
	{
		switch (index)
		{
		case 0:
			filter.testBookmarked = false;
			break;
		case 1:
			filter.testBookmarked = true;
			filter.bookmarked = true;
			break;
		case 2:
			filter.testBookmarked = true;
			filter.bookmarked = false;
			break;
		}
		s_filterModified = true;
	}
}

void UIFilterConfig_UpdateFilterAllowBG(named_filter_t& filter)
{
	if (ImGui::Checkbox("", &filter.allowBgColors))
	{
		s_filterModified = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Allow the application-provided background colors?");
	}
}

void UIFilterConfig_UpdateFilterAllowFG(named_filter_t& filter)
{
	if (ImGui::Checkbox("", &filter.allowFgColors))
	{
		s_filterModified = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Allow the application-provided forground colors?  This applies to whole-line colors as well as inline Quake-style color codes.");
	}
}

const char* s_colorComboNames[] = {
	"Default White",
	"Quake Black",
	"Quake Red",
	"Quake Green",
	"Quake Yellow",
	"Quake Blue",
	"Quake Cyan",
	"Quake Pink",
	"Quake White",
	"Quake Light Blue",
	"Quake Orange",
	"Quake Light Blue Alt",
	"Quake Orange Alt",
	"Quake Medium Blue",
	"Quake Amber",
	"Unreal Black",
	"Unreal Dark Red",
	"Unreal Dark Green",
	"Unreal Dark Blue",
	"Unreal Dark Yellow",
	"Unreal Dark Cyan",
	"Unreal Dark Purple",
	"Unreal Dark White",
	"Unreal Red",
	"Unreal Green",
	"Unreal Blue",
	"Unreal Yellow",
	"Unreal Cyan",
	"Unreal Purple",
	"Unreal White",
	"Active Session",
	"Inactive Session",
	"Log Level - VeryVerbose",
	"Log Level - Verbose",
	"Log Level - Log",
	"Log Level - Display",
	"Log Level - Warning",
	"Log Level - Error",
	"Log Level - Fatal",
	"Multiline",
	"Log Background Normal",
	"Log Background Normal Alternate 0",
	"Log Background Normal Alternate 1",
	"Log Background Bookmarked",
	"Log Background Bookmarked Alternate 0",
	"Log Background Bookmarked Alternate 1",
	"Resize Normal",
	"Resize Hovered",
	"Resize Active",
	"Message Box Background 0",
	"Message Box Background 1",
	"Text Shadow",
	"Normal Colors",
};
BB_CTASSERT(BB_ARRAYSIZE(s_colorComboNames) == kStyleColor_Count + 1);

void UIFilterConfig_UpdateFilterColor(named_filter_t& filter, bool bFG)
{
	SetNextDefaultWidth(256.0f);

	styleColor_e& color = bFG ? filter.fgStyle : filter.bgStyle;

	const char* combo_preview_value = s_colorComboNames[color];
	if (ImGui::BeginCombo("", combo_preview_value, ImGuiComboFlags_HeightLargest | ImGuiComboFlags_NoArrowButton))
	{
		for (int n = 1; n < (int)BB_ARRAYSIZE(s_colorComboNames); ++n)
		{
			if (n >= 30 && n != kStyleColor_Count)
			{
				continue;
			}

			const bool is_selected = (color == n);
			if (ImGui::Selectable(s_colorComboNames[n], is_selected))
			{
				color = (styleColor_e)n;
				s_filterModified = true;
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (is_selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
}

void UIFilterConfig_Update()
{
	if (!s_filterConfigOpen)
		return;

	ImVec2 viewportPos(0.0f, 0.0f);
	ImGuiViewport* viewport = ImGui::GetViewportForWindow("Config");
	if (viewport)
	{
		viewportPos.x += viewport->Pos.x;
		viewportPos.y += viewport->Pos.y;
	}

	float UIRecordings_Width();
	float startY = ImGui::GetFrameHeight();
	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - UIRecordings_Width(), io.DisplaySize.y - startY), ImGuiCond_Appearing);
	ImGui::SetNextWindowPos(ImVec2(viewportPos.x, viewportPos.y + startY), ImGuiCond_Appearing);
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_HorizontalScrollbar;
	if (ImGui::Begin("User Filters", &s_filterConfigOpen, windowFlags))
	{
		bool bClosed = !s_filterConfigOpen;

		ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Hideable | ImGuiTableFlags_Resizable | ImGuiTableFlags_HighlightHoveredColumn;

		ImGuiTableColumnFlags columnFlagsFilter = ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoResize;
		ImGuiTableColumnFlags columnFlagsName = ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoResize;
		ImGuiTableColumnFlags columnFlagsText = ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoResize;
		ImGuiTableColumnFlags columnFlagsNormal = ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize;
		ImGuiTableColumnFlags columnFlagsSmall = ImGuiTableColumnFlags_AngledHeader | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize;

		enum class EColumnType : int
		{
			Filter,
			Enabled,
			Selectable,
			CustomColors,
			AllowFG,
			FG,
			AllowBG,
			BG,
			Selected,
			Bookmarked,
			Name,
			Text,
		};

		struct column_data_t
		{
			EColumnType type;
			ImGuiTableColumnFlags flags;
			const char* name;
			float weight;
			u32 pad;
		};

		column_data_t columnData[] = {
			{ EColumnType::Filter, columnFlagsFilter, "", 0.0f },
			{ EColumnType::Name, columnFlagsName, "Name", 2.0f },
			{ EColumnType::Enabled, columnFlagsSmall, "Enabled", 0.0f },
			{ EColumnType::Selectable, columnFlagsSmall, "Filter Selectable", 0.0f },
			{ EColumnType::Selected, columnFlagsNormal, "Selected", 1.0f },
			{ EColumnType::Bookmarked, columnFlagsNormal, "Bookmarked", 1.0f },
			{ EColumnType::CustomColors, columnFlagsSmall, "Custom Colors", 0.0f },
			{ EColumnType::AllowBG, columnFlagsSmall, "Application Colors", 0.0f },
			{ EColumnType::BG, columnFlagsNormal, "Background", 1.0f },
			{ EColumnType::AllowFG, columnFlagsSmall, "Application Colors", 0.0f },
			{ EColumnType::FG, columnFlagsNormal, "Foreground", 1.0f },
			{ EColumnType::Text, columnFlagsText, "Text", 5.0f },
		};
		const int numColumns = IM_COUNTOF(columnData);

		int frozenColumns = 1;
		int frozenRows = 2;

		const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
		if (ImGui::BeginTable("table_angled_headers", numColumns, tableFlags, ImVec2(0.0f, TEXT_BASE_HEIGHT * 12)))
		{
			for (int n = 0; n < numColumns; ++n)
			{
				ImGui::TableSetupColumn(columnData[n].name, columnData[n].flags, columnData[n].weight);
			}
			ImGui::TableSetupScrollFreeze(frozenColumns, frozenRows);

			ImGui::TableAngledHeadersRow(); // Draw angled headers for all columns with the ImGuiTableColumnFlags_AngledHeader flag.
			ImGui::TableHeadersRow();       // Draw remaining headers and allow access to context-menu and other functions.

			// From ImGui "Drag to reorder items (simple)" demo:
			// FIXME: there is temporary (usually single-frame) ID Conflict during reordering as a same item may be submitting twice.
			// This code was always slightly faulty but in a way which was not easily noticeable.
			// Until we fix this, enable ImGuiItemFlags_AllowDuplicateId to disable detecting the issue.
			ImGui::PushItemFlag(ImGuiItemFlags_AllowDuplicateId, true);

			for (int row = 0; row < (int)g_user_named_filters.count; ++row)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				SetNextDefaultWidth(24.0f);
				ImGui::Selectable(va(ICON_FK_BARS "##RowId%u", g_user_named_filters.data[row].id));
				if (ImGui::IsItemHovered())
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
				}
				ImGui::PushID(row);

				if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
				{
					float dragDelta = ImGui::GetMouseDragDelta(0).y;
					if (fabsf(dragDelta) >= TEXT_BASE_HEIGHT)
					{
						int row_next = row + (dragDelta < 0.0f ? -1 : 1);
						if (row_next >= 0 && row_next < (int)g_user_named_filters.count)
						{
							BB_LOG("Debug", "Reorder g_user_named_filters: %d to %d (mouse %.1f -> %.1f)", row, row_next, GImGui->IO.MouseClickedPos[0].y, GImGui->IO.MousePos.y);
							named_filter_t tmp = g_user_named_filters.data[row];
							g_user_named_filters.data[row] = g_user_named_filters.data[row_next];
							g_user_named_filters.data[row_next] = tmp;
							ImGui::ResetMouseDragDelta(0);
							s_filterModified = true;
						}
					}
				}

				named_filter_t& filter = g_user_named_filters.data[row];
				for (int column = 1; column < numColumns; ++column)
					if (ImGui::TableSetColumnIndex(column))
					{
						ImGui::PushID(column);
						switch (columnData[column].type)
						{
						case EColumnType::Filter: break;
						case EColumnType::Name: UIFilterConfig_UpdateFilterName(filter); break;
						case EColumnType::Text: UIFilterConfig_UpdateFilterText(filter); break;
						case EColumnType::Enabled: UIFilterConfig_UpdateFilterEnabled(filter); break;
						case EColumnType::Selectable: UIFilterConfig_UpdateFilterSelectable(filter); break;
						case EColumnType::CustomColors: UIFilterConfig_UpdateFilterCustomColors(filter); break;
						case EColumnType::Selected: UIFilterConfig_UpdateFilterLogSelected(filter); break;
						case EColumnType::Bookmarked: UIFilterConfig_UpdateFilterLogBookmarked(filter); break;
						case EColumnType::AllowFG: UIFilterConfig_UpdateFilterAllowFG(filter); break;
						case EColumnType::AllowBG: UIFilterConfig_UpdateFilterAllowBG(filter); break;
						case EColumnType::FG: UIFilterConfig_UpdateFilterColor(filter, true); break;
						case EColumnType::BG: UIFilterConfig_UpdateFilterColor(filter, false); break;
						}
						ImGui::PopID();
					}
				ImGui::PopID();
			}
			ImGui::PopItemFlag(); // ImGuiItemFlags_AllowDuplicateId
			ImGui::EndTable();
		}

		if (s_filterModified)
		{
			named_filters_rebuild_nowrite();
			s_filterModified = false;
		}

		ImGui::Separator();
		bool ok = ImGui::Button("Ok");
		if (ok)
		{
			named_filters_write();
			UIFilterConfig_Reset();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel") || bClosed)
		{
			named_filters_read();
			UIFilterConfig_Reset();
		}

		s_filterEverShown = true;
	}
	ImGui::End();
}
