// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "ui_view_filter.h"
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

using namespace ImGui;

static const char* g_viewFilterCategoryNames[]{
	"Input",
	"History",
	"Config",
	"SiteConfig",
};
BB_CTASSERT(BB_ARRAYSIZE(g_viewFilterCategoryNames) == (size_t)EViewFilterCategory::Count);

static sb_t s_textSpan;
static sb_t s_newFilterName;

void UIRecordedView_ShutdownFilter(void)
{
	sb_reset(&s_textSpan);
	sb_reset(&s_newFilterName);
}

void UIRecordedView_ShowFilterTooltip(view_t* view)
{
	if (view->vfilter.type == kVF_Standard)
	{
		if (view->vfilter.valid)
		{
			TextShadowed(sb_get(&view->vfilter.input));
		}
		else
		{
			PushLogFont();
			PushStyleColor(ImGuiCol_Text, MakeColor(kStyleColor_LogLevel_Error));
			TextShadowed(view_filter_get_error_string(&view->vfilter));
			PopStyleColor();
			PopLogFont();
		}
	}
	else if (view->vfilter.type == kVF_SQL)
	{
		TextShadowed(view_get_create_table_command());
		TextShadowed(sb_get(&view->vfilter.input));
		if (view->sqlSelectError.count)
		{
			Separator();
			PushStyleColor(ImGuiCol_Text, MakeColor(kStyleColor_LogLevel_Error));
			TextShadowed(view->sqlSelectError.data);
			PopStyleColor();
		}
	}
	else
	{
		TextUnformatted(sb_get(&view->config.filterInput));
	}

	if (view->config.showFilterHelp)
	{
		Separator();
		TextUnformatted("Filters have multiple modes:");
		Separator();
		TextUnformatted("[Simple]");
		TextUnformatted("Use one or more keywords to OR together.  Conditions:");
		BulletText("A log line must contain one or more keywords");
		BulletText("Quotes can be used to match phrases");
		BulletText("All keywords prefixed with + have to be present");
		BulletText("And all keywords prefixed with - cannot be present");
		Separator();
		TextUnformatted("[Expression]");
		TextUnformatted("Filter using a SQL-like expression.  Use AND, OR, NOT, (, and ) to group comparisons.");
		TextUnformatted("Comparison sources are Text, Category, Filename, etc.");
		TextUnformatted("Comparisons are Is, Matches, Contains, StartsWith, and EndsWith.");
		TextUnformatted("Example: category is \"LogConsoleResponse\" and (text startswith \"WorldName:\" or text startswith \"World is\" or text startswith \"GameModeType:\")");
		Separator();
		TextUnformatted("[SQL WHERE clause]");
		TextUnformatted("Use sqlite to filter log lines.  Filter is a WHERE clause for this table schema:");
		TextUnformatted(view_get_create_table_command());
		TextUnformatted("Filter starts with WHERE and is used with this SELECT statement:");
		TextUnformatted(view_get_select_statement_fmt());
	}
}

static void view_filter_history_entry_add(view_t* view, const char* command)
{
	view->config.filterHistory.pos = ~0U;
	while (view->config.filterHistory.entries.count > 9)
	{
		bba_erase(view->config.filterHistory.entries, 0);
	}
	for (u32 i = 0; i < view->config.filterHistory.entries.count; ++i)
	{
		view_console_history_entry_t* entry = view->config.filterHistory.entries.data + i;
		const char* entryCommand = sb_get(&entry->command);
		if (!bb_stricmp(entryCommand, command))
		{
			view_console_history_entry_reset(entry);
			bba_erase(view->config.filterHistory.entries, i);
			break;
		}
	}
	view_console_history_entry_t newEntry = {};
	sb_append(&newEntry.command, command);
	bba_push(view->config.filterHistory.entries, newEntry);
	sb_reset(&view->consoleInput);
	++view->consoleRequestActive;
}

static void UIRecordedView_ApplyFilter(view_t* view, EViewFilterCategory category)
{
	const char* filterText = sb_get(&view->config.filterInput);
	view->filterPopupOpen = 0;
	view->visibleLogsDirty = true;
	view->config.filterActive = true;
	if (category != EViewFilterCategory::Config && category != EViewFilterCategory::SiteConfig)
	{
		view_filter_history_entry_add(view, filterText);
	}
	BB_LOG("Debug", "Set filter to '%s'\n", filterText);
}

static ImVec2 UIRecordedView_SizeFilterItem(const char* filterName, const char* filterText, const char* category)
{
	BB_UNUSED(category);
	sb_clear((&s_textSpan));
	if (*filterName)
	{
		sb_va(&s_textSpan, "%s: %s", filterName, filterText);
	}
	else
	{
		sb_va(&s_textSpan, "%s", filterText);
	}
	const char* filter = sb_get(&s_textSpan);

	ImFont* font = GetFont();
	ImVec2 textSize = font->CalcTextSizeA(GImGui->FontSize, FLT_MAX, 0.0f, filter);
	return textSize;
}

static const char* view_filter_find_config_by_text(const char* searchText)
{
	for (u32 i = 0; i < g_user_named_filters.count; ++i)
	{
		const char* filterText = sb_get(&g_user_named_filters.data[i].text);
		if (!bb_stricmp(searchText, filterText))
		{
			const char* filterName = sb_get(&g_user_named_filters.data[i].name);
			return filterName;
		}
	}
	return NULL;
}

static const char* view_filter_find_site_config_by_text(const char* searchText)
{
	for (u32 i = 0; i < g_site_config.namedFilters.count; ++i)
	{
		const char* filterText = sb_get(&g_site_config.namedFilters.data[i].text);
		if (!bb_stricmp(searchText, filterText))
		{
			const char* filterName = sb_get(&g_site_config.namedFilters.data[i].name);
			return filterName;
		}
	}
	return NULL;
}

static void UIRecordedView_AddNamedFilterToConfig(const char* name, const char* text)
{
	named_filter_t filter = {};
	filter.name = sb_from_c_string(name);
	filter.text = sb_from_c_string(text);
	filter.filterEnabled = true;
	filter.filterSelectable = true;
	bba_push(g_user_named_filters, filter);
	named_filters_rebuild();
}

static void UIRecordedView_FilterItem(view_t* view, const char* filterName, const char* filterText, u32 categoryIndex, EViewFilterCategory category, bool filterContextPopupWasOpen)
{
	const char* categoryName = g_viewFilterCategoryNames[(size_t)category];
	sb_clear((&s_textSpan));
	if (*filterName)
	{
		sb_va(&s_textSpan, "%s: %s###%s_%s", filterName, filterText, categoryName, filterName);
	}
	else
	{
		sb_va(&s_textSpan, "%s###%s_%s", filterText, categoryName, filterText);
	}

	const char* CategoryPopupName = va("Filter%s%uContextMenu", categoryName, categoryIndex);
	bool bCategoryPopupOpen = ImGui::IsPopupOpen(CategoryPopupName);

	bool bMatchesInput = *filterText && (!strcmp(sb_get(&view->config.filterInput), filterText));
	bool selected = (filterContextPopupWasOpen) ? bCategoryPopupOpen : bMatchesInput;

	if (ImGui::Selectable(sb_get(&s_textSpan), &selected))
	{
		sb_clear(&view->config.filterInput);
		if (*filterName)
		{
			sb_va(&view->config.filterInput, "@%s", filterName);
		}
		else
		{
			sb_append(&view->config.filterInput, filterText);
		}
		UIRecordedView_ApplyFilter(view, category);
	}

	if (category == EViewFilterCategory::History)
	{
		const char* configName = view_filter_find_config_by_text(filterText);
		const char* siteConfigName = view_filter_find_site_config_by_text(filterText);
		if (ImGui::BeginPopupContextItem(CategoryPopupName))
		{
			view->filterContextPopupOpen = true;
			if (configName)
			{
				ImGui::Selectable(va("In config as %s", configName), false);
			}
			else if (siteConfigName)
			{
				ImGui::Selectable(va("In site config as %s", siteConfigName), false);
			}
			else
			{
				ImGui::TextUnformatted("New: ");
				ImGui::SameLine();
				if (ImGui::InputText("##NewFilter", &s_newFilterName, 64, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					UIRecordedView_AddNamedFilterToConfig(sb_get(&s_newFilterName), filterText);
					sb_reset(&s_newFilterName);
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}
	}
	else if (category == EViewFilterCategory::Config)
	{
		if (ImGui::BeginPopupContextItem(CategoryPopupName))
		{
			view->filterContextPopupOpen = true;
			if (ImGui::Selectable(va("Remove named filter %s", filterName), false))
			{
				named_filter_t* filter = g_user_named_filters.data + categoryIndex;
				named_filter_reset(filter);
				bba_erase(g_user_named_filters, categoryIndex);
				named_filters_rebuild();
			}
			ImGui::EndPopup();
		}
	}
}

static int UIRecordedView_FilterInputCallback(ImGuiInputTextCallbackData* CallbackData)
{
	view_t* view = (view_t*)CallbackData->UserData;
	switch (CallbackData->EventFlag)
	{
	case ImGuiInputTextFlags_CallbackCharFilter:
		if (CallbackData->EventChar == '`')
			return 1;
		break;

	case ImGuiInputTextFlags_CallbackCompletion:
		break;

	case ImGuiInputTextFlags_CallbackHistory:
		if (view->config.filterHistory.entries.count)
		{
			u32 prevHistoryPos = view->config.filterHistory.pos;
			if (CallbackData->EventKey == ImGuiKey_DownArrow)
			{
				if (view->config.filterHistory.pos == ~0U)
				{
					view->config.filterHistory.pos = view->config.filterHistory.entries.count - 1;
				}
				else if (view->config.filterHistory.pos)
				{
					--view->config.filterHistory.pos;
				}
			}
			else if (CallbackData->EventKey == ImGuiKey_UpArrow)
			{
				if (view->config.filterHistory.pos != ~0U)
				{
					++view->config.filterHistory.pos;
					if (view->config.filterHistory.pos >= view->config.filterHistory.entries.count)
					{
						view->config.filterHistory.pos = ~0U;
					}
				}
			}
			if (prevHistoryPos != view->config.filterHistory.pos)
			{
				const char* command = (view->config.filterHistory.pos == ~0U) ? "" : sb_get(&view->config.filterHistory.entries.data[view->config.filterHistory.pos].command);
				int len = (int)bb_strncpy(CallbackData->Buf, command, (size_t)CallbackData->BufSize);
				CallbackData->CursorPos = len;
				CallbackData->SelectionStart = len;
				CallbackData->SelectionEnd = len;
				CallbackData->BufTextLen = len;
				CallbackData->BufDirty = true;
			}
			break;
		}

	default:
		break;
	}
	return 0;
}

bool UIRecordedView_UpdateFilter(view_t* view)
{
	const ImVec2 cursorPos = ImGui::GetCursorPos();
	const ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
	if (ImGui::InputText("###Filter", &view->config.filterInput, 256, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackResize, &UIRecordedView_FilterInputCallback, view))
	{
		UIRecordedView_ApplyFilter(view, EViewFilterCategory::Input);
	}
	bool filterActive = ImGui::IsItemActive();
	bool filterWindowActive = false;
	bool filterPopupWasOpen = view->filterPopupOpen != 0;
	bool filterContextPopupWasOpen = view->filterContextPopupOpen != 0;
	if (filterActive || view->filterContextPopupOpen)
	{
		view->filterPopupOpen = true;
		ImGui::OpenPopup("###FilterPopup", ImGuiPopupFlags_None);
	}
	view->filterContextPopupOpen = false;
	if (view->filterPopupOpen && (view->config.filterHistory.entries.count > 0 || g_site_config.namedFilters.count > 0 || g_user_named_filters.count > 0))
	{
		ImVec2 totalSize;
		const float spacingHeight = ImGui::GetFrameHeightWithSpacing() - ImGui::GetFrameHeight();
		for (u32 i = 0; i < view->config.filterHistory.entries.count; ++i)
		{
			const char* text = sb_get(&view->config.filterHistory.entries.data[i].command);
			ImVec2 size = UIRecordedView_SizeFilterItem("", text, "History");
			totalSize.x = totalSize.x >= size.x ? totalSize.x : size.x;
			totalSize.y += ImGui::GetFrameHeight();
		}
		if (g_site_config.namedFilters.count > 0)
		{
			for (u32 i = 0; i < g_site_config.namedFilters.count; ++i)
			{
				const char* name = sb_get(&g_site_config.namedFilters.data[i].name);
				const char* text = sb_get(&g_site_config.namedFilters.data[i].text);
				ImVec2 size = UIRecordedView_SizeFilterItem(name, text, "Config");
				totalSize.x = totalSize.x >= size.x ? totalSize.x : size.x;
				totalSize.y += ImGui::GetFrameHeight();
			}
		}
		if (g_user_named_filters.count > 0)
		{
			for (u32 i = 0; i < g_user_named_filters.count; ++i)
			{
				const char* name = sb_get(&g_user_named_filters.data[i].name);
				const char* text = sb_get(&g_user_named_filters.data[i].text);
				ImVec2 size = UIRecordedView_SizeFilterItem(name, text, "Site");
				totalSize.x = totalSize.x >= size.x ? totalSize.x : size.x;
				totalSize.y += ImGui::GetFrameHeight();
			}
		}
		totalSize.x += 4.0f * spacingHeight;
		totalSize.y += 3.0f * spacingHeight;

		ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail() - cursorPos - ImVec2(10.0f, ImGui::GetFrameHeightWithSpacing());
		ImVec2 popupSize(contentRegionAvail.x < totalSize.x ? contentRegionAvail.x : totalSize.x, contentRegionAvail.y < totalSize.y ? contentRegionAvail.y : totalSize.y);
		SetNextWindowSize(popupSize);

		SetNextWindowPos(ImVec2(cursorScreenPos.x, cursorScreenPos.y + ImGui::GetFrameHeightWithSpacing()), ImGuiCond_Always);
		ImGuiCond filterPopupFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
		if (ImGui::BeginPopup("###FilterPopup", filterPopupFlags))
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
			{
				filterWindowActive = true;
			}
			for (u32 i = 0; i < view->config.filterHistory.entries.count; ++i)
			{
				u32 reverseIndex = view->config.filterHistory.entries.count - i - 1;
				const char* text = sb_get(&view->config.filterHistory.entries.data[reverseIndex].command);
				UIRecordedView_FilterItem(view, "", text, i, EViewFilterCategory::History, filterContextPopupWasOpen);
			}
			if (g_user_named_filters.count > 0)
			{
				ImGui::Separator();
				for (u32 i = 0; i < g_user_named_filters.count; ++i)
				{
					if (g_user_named_filters.data[i].filterEnabled && g_user_named_filters.data[i].filterSelectable)
					{
						const char* name = sb_get(&g_user_named_filters.data[i].name);
						const char* text = sb_get(&g_user_named_filters.data[i].text);
						UIRecordedView_FilterItem(view, name, text, i, EViewFilterCategory::Config, filterContextPopupWasOpen);
					}
				}
			}
			if (g_site_config.namedFilters.count > 0)
			{
				ImGui::Separator();
				for (u32 i = 0; i < g_site_config.namedFilters.count; ++i)
				{
					if (g_site_config.namedFilters.data[i].filterEnabled && g_site_config.namedFilters.data[i].filterSelectable)
					{
						const char* name = sb_get(&g_site_config.namedFilters.data[i].name);
						const char* text = sb_get(&g_site_config.namedFilters.data[i].text);
						UIRecordedView_FilterItem(view, name, text, i, EViewFilterCategory::SiteConfig, filterContextPopupWasOpen);
					}
				}
			}
			ImGui::EndPopup();
		}
	}
	if (!filterActive && !filterWindowActive && filterPopupWasOpen && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		view->filterPopupOpen = 0;
		view->config.filterHistory.pos = ~0U;
	}
	Fonts_CacheGlyphs(sb_get(&view->config.filterInput));
	const bool filterFocused = IsItemActive() && Imgui_Core_HasFocus();
	if (filterFocused)
	{
		Imgui_Core_RequestRender();
	}
	if (ImGui::IsTooltipActive())
	{
		BeginTooltip();
		PushUIFont();
		UIRecordedView_ShowFilterTooltip(view);
		PopUIFont();
		EndTooltip();
	}
	return filterFocused || view->filterPopupOpen;
}
