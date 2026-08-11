// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "ui_view_log_table.h"
#include "bb_assert.h"
#include "bb_colors.h"
#include "bbserver_utils.h"
#include "imgui_core.h"
#include "imgui_text_shadows.h"
#include "imgui_utils.h"
#include "recorded_session.h"
#include "time_utils.h"
#include "ui_loglevel_colorizer.h"
#include "ui_view.h"
#include "va.h"
#include "view.h"
#include "wrap_imgui.h"
#include "wrap_imgui_internal.h"

static sb_t s_textSpan;

const char* BuildLogColumnText(view_t* view, view_log_t* viewLog, view_column_e column);
void UIRecordedView_Logs_HandleClick(view_t* view, view_log_t* log);

#define EDITABLE_TABLE_OPTIONS 0

#if EDITABLE_TABLE_OPTIONS
// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
static void HelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip())
	{
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

// Make the UI compact because there are so many fields
static void PushStyleCompact()
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, (float)(int)(style.FramePadding.y * 0.60f));
	ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, (float)(int)(style.ItemSpacing.y * 0.60f));
}

static void PopStyleCompact()
{
	ImGui::PopStyleVar(2);
}

// Show a combo box with a choice of sizing policies
static void EditTableSizingFlags(ImGuiTableFlags* p_flags)
{
	BB_WARNING_PUSH(4820)
	struct EnumDesc
	{
		ImGuiTableFlags Value;
		const char* Name;
		const char* Tooltip;
	};
	BB_WARNING_POP
	static const EnumDesc policies[] = {
		{ ImGuiTableFlags_None, "Default", "Use default sizing policy:\n- ImGuiTableFlags_SizingFixedFit if ScrollX is on or if host window has ImGuiWindowFlags_AlwaysAutoResize.\n- ImGuiTableFlags_SizingStretchSame otherwise." },
		{ ImGuiTableFlags_SizingFixedFit, "ImGuiTableFlags_SizingFixedFit", "Columns default to _WidthFixed (if resizable) or _WidthAuto (if not resizable), matching contents width." },
		{ ImGuiTableFlags_SizingFixedSame, "ImGuiTableFlags_SizingFixedSame", "Columns are all the same width, matching the maximum contents width.\nImplicitly disable ImGuiTableFlags_Resizable and enable ImGuiTableFlags_NoKeepColumnsVisible." },
		{ ImGuiTableFlags_SizingStretchProp, "ImGuiTableFlags_SizingStretchProp", "Columns default to _WidthStretch with weights proportional to their widths." },
		{ ImGuiTableFlags_SizingStretchSame, "ImGuiTableFlags_SizingStretchSame", "Columns default to _WidthStretch with same weights." }
	};
	int idx;
	for (idx = 0; idx < IM_COUNTOF(policies); idx++)
		if (policies[idx].Value == (*p_flags & ImGuiTableFlags_SizingMask_))
			break;
	const char* preview_text = (idx < IM_COUNTOF(policies)) ? policies[idx].Name + (idx > 0 ? strlen("ImGuiTableFlags") : 0) : "";
	if (ImGui::BeginCombo("Sizing Policy", preview_text))
	{
		for (int n = 0; n < IM_COUNTOF(policies); n++)
			if (ImGui::Selectable(policies[n].Name, idx == n))
				*p_flags = (*p_flags & ~ImGuiTableFlags_SizingMask_) | policies[n].Value;
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip())
	{
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
		for (int m = 0; m < IM_COUNTOF(policies); m++)
		{
			ImGui::Separator();
			ImGui::Text("%s:", policies[m].Name);
			ImGui::Separator();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().IndentSpacing * 0.5f);
			ImGui::TextUnformatted(policies[m].Tooltip);
		}
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}
#endif // EDITABLE_TABLE_OPTIONS

namespace ImGui
{
	float TableGetColumnWidth(int column)
	{
		ImGuiTable* table = ImGui::GetCurrentTable();
		if (table && column >= 0 && column < table->ColumnsCount)
		{
			return table->Columns[column].WidthGiven;
		}
		return 0.0f;
	}
}

void LogTable_SetupColumns(view_t* view, ImGuiTableColumnFlags columns_base_flags, int freeze_cols, int freeze_rows)
{
	// Declare columns

	for (u32 i = 0; i < BB_ARRAYSIZE(view->columns); ++i)
	{
		ImGuiTableColumnFlags flags = columns_base_flags;
		if (!view->columns[i].visible)
		{
			flags |= ImGuiTableColumnFlags_DefaultHide;
		}
		ImGui::TableSetupColumn(g_view_column_long_display_names[i], flags, view->columns[i].width);

		if (view->columns[i].visible)
		{
			float columnWidth = ImGui::TableGetColumnWidth((int)i);
			if (columnWidth > 0.0f && columnWidth != view->columns[i].width)
			{
				view->columns[i].width = columnWidth;
			}
		}
	}
	ImGuiTableColumnFlags textFlags = columns_base_flags & (~ImGuiTableColumnFlags_WidthMask_);
	textFlags |= ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize;
	textFlags |= ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder;
	ImGui::TableSetupColumn("Text", textFlags, view->textWidth);

	// We use the "user_id" parameter of TableSetupColumn() to specify a user id that will be stored in the sort specifications.
	// This is so our sort function can identify a column given our own identifier. We could also identify them based on their index!
	// ImGui::TableSetupColumn("ID",           columns_base_flags | ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.0f, MyItemColumnID_ID);
	// ImGui::TableSetupColumn("Name",         columns_base_flags | ImGuiTableColumnFlags_WidthFixed, 0.0f, MyItemColumnID_Name);
	// ImGui::TableSetupColumn("Action",       columns_base_flags | ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 0.0f, MyItemColumnID_Action);
	// ImGui::TableSetupColumn("Quantity",     columns_base_flags | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, MyItemColumnID_Quantity);
	// ImGui::TableSetupColumn("Description",  columns_base_flags | ((flags & ImGuiTableFlags_NoHostExtendX) ? 0 : ImGuiTableColumnFlags_WidthStretch), 0.0f, MyItemColumnID_Description);
	// ImGui::TableSetupColumn("Hidden",       columns_base_flags | ImGuiTableColumnFlags_DefaultHide | ImGuiTableColumnFlags_NoSort);

	ImGui::TableSetupScrollFreeze(freeze_cols, freeze_rows);
}

typedef struct colored_text_s
{
	const char* start;
	const char* end;
	const char* next;
	int len;
	ImColor color;
	styleColor_e styleColor;
	b32 blink;
	b32 categoryNoColors;
} colored_text_t;

static colored_text_t UIRecordedView_GetColoredTextInternal(colored_text_t prev)
{
	const char* start = prev.next;
	const char* marker = start;
	colored_text_t ret = { BB_EMPTY_INITIALIZER };
	ret.end = prev.end;
	ret.categoryNoColors = prev.categoryNoColors;
	ret.styleColor = prev.styleColor;
	ret.color = prev.color;
	ret.blink = prev.blink;
	if (!start || !*start)
	{
		return ret;
	}

	if (*marker == kColorKeyPrefix && marker[1] >= kFirstColorKey && marker[1] <= kLastColorKey)
	{
		int colorIndex = marker[1] - kFirstColorKey;
		ret.start = start + 2;
		ret.next = marker + 2;
		if (!ret.categoryNoColors)
		{
			ret.styleColor = (styleColor_e)(colorIndex + kColorKeyOffset);
			ret.color = MakeColor(ret.styleColor);
		}
		marker += 2;
	}
	else if (*marker == '^' && marker[1] == 'F')
	{
		ret.start = start + 2;
		ret.next = marker + 2;
		if (!ret.categoryNoColors)
		{
			ret.blink = !prev.blink;
		}
		marker += 2;
	}
	else
	{
		ret.start = start;
	}

	while (*marker)
	{
		if (*marker == kColorKeyPrefix && marker[1] >= kFirstColorKey && marker[1] <= kLastColorKey ||
		    *marker == '^' && marker[1] == 'F')
		{
			ret.next = marker;
			ret.len = (int)(marker - ret.start);
			return ret;
		}
		else
		{
			// char c = *marker;
			++marker;
			/*
			    if(c == '\n') {
			    ret.len = (int)(marker - ret.start);
			    ret.next = nullptr;
			    return ret;
			    }
			    */
			if (marker > ret.start + 8 * 1024)
			{
				break;
			}
		}
	}
	if (marker > ret.start)
	{
		ret.len = (int)(marker - ret.start);
		ret.next = marker;
	}
	return ret;
}

static colored_text_t UIRecordedView_GetColoredText(colored_text_t prev)
{
	colored_text_t ret = UIRecordedView_GetColoredTextInternal(prev);
	if (ret.start && ret.start + ret.len > ret.end)
	{
		ret.len = (int)(ret.end - ret.start);
		ret.next = nullptr;
	}
	return ret;
}

static float LogTable_EmitLogText(view_t* view, view_log_t* viewLog, named_filter_t* log_color_entry)
{
	u32 logIndex = viewLog->sessionLogIndex;
	recorded_session_t* session = view->session;
	recorded_log_t* sessionLog = session->logs.data[logIndex];
	bb_decoded_packet_t* decoded = &sessionLog->packet;
	// recorded_category_t* recordedCategory = recorded_session_find_category(session, decoded->packet.logText.categoryId);
	view_category_t* viewCategory = view_find_category(view, decoded->packet.logText.categoryId);

	const configColorUsage colorUsage = g_config.logColorUsage;

	b32 categoryNoColors = viewCategory->noColor;
	if (log_color_entry)
	{
		if (!log_color_entry->allowBgColors)
		{
			categoryNoColors = true;
		}
	}

	bool bNeedText = true;

	BB_ASSERT(sessionLog->lines.count > viewLog->subLine);
	recorded_log_line_t recordedLogLine = sessionLog->lines.data[viewLog->subLine];
	span_t subLineSpan = { decoded->packet.logText.text + recordedLogLine.offset, decoded->packet.logText.text + recordedLogLine.offset + recordedLogLine.len };

	bool first = true;

	colored_text_t span = { BB_EMPTY_INITIALIZER };
	span.styleColor = GetStyleColorForLogLevel((bb_log_level_e)decoded->packet.logText.level);
	if (colorUsage != kConfigColors_None && !categoryNoColors)
	{
		span.styleColor = (styleColor_e)(decoded->packet.logText.colors.fg);
		if (colorUsage == kConfigColors_BgAsFg)
		{
			if (decoded->packet.logText.colors.bg != kBBColor_Default)
			{
				span.styleColor = (styleColor_e)(decoded->packet.logText.colors.bg);
			}
		}
	}
	if (decoded->packet.logText.colors.fg == kBBColor_Default ||
	    decoded->packet.logText.level == kBBLogLevel_Warning ||
	    decoded->packet.logText.level == kBBLogLevel_Error ||
	    decoded->packet.logText.level == kBBLogLevel_Fatal)
	{
		span.styleColor = GetStyleColorForLogLevel((bb_log_level_e)decoded->packet.logText.level);
	}
	span.color = MakeColor(span.styleColor);
	ImColor fgColor = span.color;

	if (log_color_entry &&
	    decoded->packet.logText.level != kBBLogLevel_Warning &&
	    decoded->packet.logText.level != kBBLogLevel_Error &&
	    decoded->packet.logText.level != kBBLogLevel_Fatal)
	{
		if (log_color_entry->fgStyle != kStyleColor_Count)
		{
			span.color = fgColor = MakeColor(log_color_entry->fgStyle);
		}
		else if (log_color_entry->fgColor[3] > 0.0f)
		{
			span.color = fgColor = ImColor(log_color_entry->fgColor[0], log_color_entry->fgColor[1], log_color_entry->fgColor[2], log_color_entry->fgColor[3]);
		}
	}

	if (viewLog->subLine && subLineSpan.start)
	{
		colored_text_t other = { BB_EMPTY_INITIALIZER };
		other.color = fgColor;
		other.next = decoded->packet.logText.text;
		other.end = subLineSpan.start;
		other.categoryNoColors = categoryNoColors;
		do
		{
			other = UIRecordedView_GetColoredText(other);
			if (other.len && other.start)
			{
				span.color = other.color;
				span.blink = other.blink;
			}
		} while (other.next);
	}

	ImFont* font = ImGui::GetFont();
	float totalTextSizeX = 0.0f;
	u32 totalLineLen = 0;
	b32 oldShadows = false;

	span.next = subLineSpan.start;
	span.end = subLineSpan.end;
	span.categoryNoColors = categoryNoColors;
	do
	{
		span = UIRecordedView_GetColoredText(span);
		if (g_config.logColorUsage == kConfigColors_None)
		{
			span.color = fgColor;
		}
		if (span.len && span.start)
		{
			if (first)
			{
				first = false;
			}
			else
			{
				ImGui::SameLine(0.0f, 0.0f);
				bNeedText = true;
			}
			ImColor color = span.color;
			if (span.blink)
			{
				// we explicitly want the double version of sin() because if a machine has been up
				// for a few weeks, we lose all fractional precision in float32, making sinf()
				// stairstep.
				const double rate = 2.5;
				float s = (float)sin(Time_GetCurrentFrameStartTime() * rate);
				float scale = fabsf(s);
				color.Value.w *= scale;
				Imgui_Core_RequestRender();
			}
			if (g_config.logColorUsage != kConfigColors_None)
			{
				oldShadows = PushTextShadows(span.styleColor);
			}
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			sb_clear(&s_textSpan);
			sb_va(&s_textSpan, "%.*s", span.len, span.start);
			ImGui::TextShadowed(sb_get(&s_textSpan));
			ImGui::PopStyleColor();
			if (g_config.logColorUsage != kConfigColors_None)
			{
				PopTextShadows(oldShadows);
			}
			bNeedText = false;

			ImVec2 textSize = font->CalcTextSizeA(GImGui->FontSize, FLT_MAX, 0.0f, sb_get(&s_textSpan), sb_get(&s_textSpan) + sb_len(&s_textSpan));
			totalTextSizeX += textSize.x;
			totalLineLen += span.len;
		}
	} while (span.next && totalLineLen < 8 * 1024);

	if (span.next && totalLineLen >= 8 * 1024)
	{
		if (g_config.logColorUsage != kConfigColors_None)
		{
			oldShadows = PushTextShadows(span.styleColor);
		}
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextShadowed("...");
		if (g_config.logColorUsage != kConfigColors_None)
		{
			PopTextShadows(oldShadows);
		}
		bNeedText = false;

		ImVec2 textSize = font->CalcTextSizeA(GImGui->FontSize, FLT_MAX, 0.0f, "...");
		totalTextSizeX += textSize.x;
	}

	if (bNeedText)
	{
		ImGui::TextUnformatted("");
	}

	ImVec2 blankSize = font->CalcTextSizeA(GImGui->FontSize, FLT_MAX, 0.0f, "            ");
	float requiredWidth = totalTextSizeX + blankSize.x;
	return requiredWidth;
}

static void LogTable_EmitRows(view_t* view, float row_min_height, b32 otherControlFocused)
{
	ImGui::verticalScrollDir_e verticalScrollDir = ImGui::kVerticalScroll_None;
	bool logsHovered = ImGui::IsWindowHovered();
	PushLogFont();
	ImGuiListClipper clipper;
	clipper.Begin((int)view->visibleLogs.count);
	u32 numVisibleLines = 0;
	float lineHeight = 1.0f;
	while (clipper.Step())
	{
		for (int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; row_n++)
		{
			++numVisibleLines;
			view_log_t* viewLog = view->visibleLogs.data + row_n;
			ImGui::PushID(va("%u.%u", viewLog->persistentLogIndex, viewLog->subLine));

			float startY = ImGui::GetCursorScreenPos().y;
			ImGui::TableNextRow(ImGuiTableRowFlags_None, row_min_height);

			u32 logIndex = viewLog->sessionLogIndex;
			recorded_session_t* session = view->session;
			recorded_log_t* sessionLog = session->logs.data[logIndex];
			bb_decoded_packet_t* decoded = &sessionLog->packet;
			recorded_category_t* recordedCategory = recorded_session_find_category(session, decoded->packet.logText.categoryId);
			view_category_t* viewCategory = view_find_category(view, decoded->packet.logText.categoryId);
			named_filter_t* log_color_entry = named_filters_resolve(view, viewLog, sessionLog, true);
			view_log_colors_t viewLogColors = UIRecordedView_InitLogColors(decoded, viewLog, viewCategory, log_color_entry);

			b32 oldShadows = false;
			b32 firstColumn = true;
			if (viewLog->subLine != 0)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, MakeColor(kStyleColor_Multiline));
				oldShadows = PushTextShadows(kStyleColor_Multiline);
			}

			for (int i = 0; i < kColumn_Count; ++i)
			{
				view->columns[i].visible = ImGui::TableSetColumnIndex(i);
				if (view->columns[i].visible)
				{
					view_column_e column = (view_column_e)i;
					const char* columnText = BuildLogColumnText(view, viewLog, column);
					if (firstColumn)
					{
						firstColumn = false;
						ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
						UIRecordedView_PushLogStyleColors(viewLogColors);
						ImGui::SelectableWithBackground(va("%s###%u_%u", columnText, viewLog->sessionLogIndex, viewLog->subLine), viewLog->selected != 0, viewLogColors.bgColor, selectable_flags, ImVec2(0, row_min_height));
						UIRecordedView_PopLogStyleColors(viewLogColors);

						if (ImGui::IsItemHovered())
						{
							if (ImGui::IsItemClicked())
							{
								UIRecordedView_Logs_HandleClick(view, viewLog);
							}
							verticalScrollDir = ImGui::GetVerticalScrollDir();
						}

						if (!g_config.tooltips.onlyOverSelected || viewLog->selected)
						{
							ImGui::PushStyleColor(ImGuiCol_Text, MakeColor(kStyleColor_kBBColor_Default));
							if (ImGui::GetMousePos().x >= ImGui::GetWindowPos().x + view->textStartX - ImGui::GetScrollX())
							{
								if (g_config.tooltips.overText)
								{
									UIRecordedView_SetLogTooltip(decoded, recordedCategory, session, view, sessionLog);
								}
							}
							else
							{
								if (g_config.tooltips.overMisc)
								{
									UIRecordedView_SetLogTooltip(decoded, recordedCategory, session, view, sessionLog);
								}
							}
							ImGui::PopStyleColor(1);
						}

						if (ImGui::BeginPopupContextItem(va("RecordedEntry_%u_%u_ContextMenu", logIndex, viewLog->subLine)))
						{
							if (!viewLog->selected)
							{
								UIRecordedView_Logs_ClearSelection(view);
								UIRecordedView_Logs_AddSelection(view, viewLog);
							}
							UIRecordedView_LogPopup(view, viewLog);
							ImGui::EndPopup();
						}
					}
					else
					{
						ImGui::TextShadowed(columnText);
					}
				}
			}

			if (viewLog->subLine != 0)
			{
				PopTextShadows(oldShadows);
				ImGui::PopStyleColor();
			}

			if (ImGui::TableSetColumnIndex(kColumn_Count))
			{
				view->textStartX = ImGui::GetCursorPosX();
				float textWidth = LogTable_EmitLogText(view, viewLog, log_color_entry);
				if (textWidth > view->textWidth)
				{
					view->textWidth = textWidth;
				}
			}

			float endY = ImGui::GetCursorScreenPos().y;
			lineHeight = endY - startY;

			ImGui::PopID();
		}
	}
	PopLogFont();

	view->numVisibleLines = numVisibleLines;

	UIRecordedView_UpdateScrolling(view, logsHovered, otherControlFocused, lineHeight, verticalScrollDir);
}

bool LogTable_Update(view_t* view, b32 otherControlFocused)
{
	if (!view)
		return false;

	// Using those as a base value to create width/height that are factor of the size of our font
#if EDITABLE_TABLE_OPTIONS
	PushLogFont();
	const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
	const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
	PopLogFont();
#else
	const float TEXT_BASE_HEIGHT = 0.0f;
#endif // EDITABLE_TABLE_OPTIONS

	static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
	static ImGuiTableColumnFlags columns_base_flags = ImGuiTableColumnFlags_None;

	enum ContentsType
	{
		CT_Text,
		CT_Button,
		CT_SmallButton,
		CT_FillButton,
		CT_Selectable,
		CT_SelectableSpanRow
	};
	static int contents_type = CT_SelectableSpanRow;
	const char* contents_type_names[] = { "Text", "Button", "SmallButton", "FillButton", "Selectable", "Selectable (span row)" };
	static int freeze_cols = 1;
	static int freeze_rows = 1;
	static ImVec2 outer_size_value = ImVec2(0.0f, -2.0f * TEXT_BASE_HEIGHT);
	static float row_min_height = 0.0f;          // Auto
	static float inner_width_with_scroll = 0.0f; // Auto-extend
	static bool outer_size_enabled = true;
	static bool show_headers = true;
	static bool show_wrapped_text = false;
	// static ImGuiTextFilter filter;
	// ImGui::SetNextItemOpen(true, ImGuiCond_Once); // FIXME-TABLE: Enabling this results in initial clipped first pass on table which tend to affect column sizing
#if EDITABLE_TABLE_OPTIONS
	if (ImGui::TreeNode("Options"))
	{
		// Make the UI compact because there are so many fields
		PushStyleCompact();
		ImGui::PushItemWidth(TEXT_BASE_WIDTH * 28.0f);

		if (ImGui::TreeNodeEx("Features:", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::CheckboxFlags("ImGuiTableFlags_Resizable", &flags, ImGuiTableFlags_Resizable);
			ImGui::CheckboxFlags("ImGuiTableFlags_Reorderable", &flags, ImGuiTableFlags_Reorderable);
			ImGui::CheckboxFlags("ImGuiTableFlags_Hideable", &flags, ImGuiTableFlags_Hideable);
			ImGui::CheckboxFlags("ImGuiTableFlags_Sortable", &flags, ImGuiTableFlags_Sortable);
			ImGui::CheckboxFlags("ImGuiTableFlags_NoSavedSettings", &flags, ImGuiTableFlags_NoSavedSettings);
			ImGui::CheckboxFlags("ImGuiTableFlags_ContextMenuInBody", &flags, ImGuiTableFlags_ContextMenuInBody);
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Decorations:", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::CheckboxFlags("ImGuiTableFlags_RowBg", &flags, ImGuiTableFlags_RowBg);
			ImGui::CheckboxFlags("ImGuiTableFlags_BordersV", &flags, ImGuiTableFlags_BordersV);
			ImGui::CheckboxFlags("ImGuiTableFlags_BordersOuterV", &flags, ImGuiTableFlags_BordersOuterV);
			ImGui::CheckboxFlags("ImGuiTableFlags_BordersInnerV", &flags, ImGuiTableFlags_BordersInnerV);
			ImGui::CheckboxFlags("ImGuiTableFlags_BordersH", &flags, ImGuiTableFlags_BordersH);
			ImGui::CheckboxFlags("ImGuiTableFlags_BordersOuterH", &flags, ImGuiTableFlags_BordersOuterH);
			ImGui::CheckboxFlags("ImGuiTableFlags_BordersInnerH", &flags, ImGuiTableFlags_BordersInnerH);
			ImGui::CheckboxFlags("ImGuiTableFlags_NoBordersInBody", &flags, ImGuiTableFlags_NoBordersInBody);
			ImGui::SameLine();
			HelpMarker("Disable vertical borders in columns Body (borders will always appear in Headers)");
			ImGui::CheckboxFlags("ImGuiTableFlags_NoBordersInBodyUntilResize", &flags, ImGuiTableFlags_NoBordersInBodyUntilResize);
			ImGui::SameLine();
			HelpMarker("Disable vertical borders in columns Body until hovered for resize (borders will always appear in Headers)");
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Sizing:", ImGuiTreeNodeFlags_DefaultOpen))
		{
			EditTableSizingFlags(&flags);
			ImGui::SameLine();
			HelpMarker("In the Advanced demo we override the policy of each column so those table-wide settings have less effect that typical.");
			ImGui::CheckboxFlags("ImGuiTableFlags_NoHostExtendX", &flags, ImGuiTableFlags_NoHostExtendX);
			ImGui::SameLine();
			HelpMarker("Make outer width auto-fit to columns, overriding outer_size.x value.\n\nOnly available when ScrollX/ScrollY are disabled and Stretch columns are not used.");
			ImGui::CheckboxFlags("ImGuiTableFlags_NoHostExtendY", &flags, ImGuiTableFlags_NoHostExtendY);
			ImGui::SameLine();
			HelpMarker("Make outer height stop exactly at outer_size.y (prevent auto-extending table past the limit).\n\nOnly available when ScrollX/ScrollY are disabled. Data below the limit will be clipped and not visible.");
			ImGui::CheckboxFlags("ImGuiTableFlags_NoKeepColumnsVisible", &flags, ImGuiTableFlags_NoKeepColumnsVisible);
			ImGui::SameLine();
			HelpMarker("Only available if ScrollX is disabled.");
			ImGui::CheckboxFlags("ImGuiTableFlags_PreciseWidths", &flags, ImGuiTableFlags_PreciseWidths);
			ImGui::SameLine();
			HelpMarker("Disable distributing remainder width to stretched columns (width allocation on a 100-wide table with 3 columns: Without this flag: 33,33,34. With this flag: 33,33,33). With larger number of columns, resizing will appear to be less smooth.");
			ImGui::CheckboxFlags("ImGuiTableFlags_NoClip", &flags, ImGuiTableFlags_NoClip);
			ImGui::SameLine();
			HelpMarker("Disable clipping rectangle for every individual columns (reduce draw command count, items will be able to overflow into other columns). Generally incompatible with ScrollFreeze options.");
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Padding:", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::CheckboxFlags("ImGuiTableFlags_PadOuterX", &flags, ImGuiTableFlags_PadOuterX);
			ImGui::CheckboxFlags("ImGuiTableFlags_NoPadOuterX", &flags, ImGuiTableFlags_NoPadOuterX);
			ImGui::CheckboxFlags("ImGuiTableFlags_NoPadInnerX", &flags, ImGuiTableFlags_NoPadInnerX);
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Scrolling:", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::CheckboxFlags("ImGuiTableFlags_ScrollX", &flags, ImGuiTableFlags_ScrollX);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetFrameHeight());
			ImGui::DragInt("freeze_cols", &freeze_cols, 0.2f, 0, 9, NULL, ImGuiSliderFlags_NoInput);
			ImGui::CheckboxFlags("ImGuiTableFlags_ScrollY", &flags, ImGuiTableFlags_ScrollY);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetFrameHeight());
			ImGui::DragInt("freeze_rows", &freeze_rows, 0.2f, 0, 9, NULL, ImGuiSliderFlags_NoInput);
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Sorting:", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::CheckboxFlags("ImGuiTableFlags_SortMulti", &flags, ImGuiTableFlags_SortMulti);
			ImGui::SameLine();
			HelpMarker("When sorting is enabled: hold shift when clicking headers to sort on multiple column. TableGetSortSpecs() may return specs where (SpecsCount > 1).");
			ImGui::CheckboxFlags("ImGuiTableFlags_SortTristate", &flags, ImGuiTableFlags_SortTristate);
			ImGui::SameLine();
			HelpMarker("When sorting is enabled: allow no sorting, disable default sorting. TableGetSortSpecs() may return specs where (SpecsCount == 0).");
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Headers:", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("show_headers", &show_headers);
			ImGui::CheckboxFlags("ImGuiTableFlags_HighlightHoveredColumn", &flags, ImGuiTableFlags_HighlightHoveredColumn);
			ImGui::CheckboxFlags("ImGuiTableColumnFlags_AngledHeader", &columns_base_flags, ImGuiTableColumnFlags_AngledHeader);
			ImGui::SameLine();
			HelpMarker("Enable AngledHeader on all columns. Best enabled on selected narrow columns (see \"Angled headers\" section of the demo).");
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Other:", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("show_wrapped_text", &show_wrapped_text);

			ImGui::DragFloat2("##OuterSize", &outer_size_value.x);
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::Checkbox("outer_size", &outer_size_enabled);
			ImGui::SameLine();
			HelpMarker("If scrolling is disabled (ScrollX and ScrollY not set):\n"
			           "- The table is output directly in the parent window.\n"
			           "- OuterSize.x < 0.0f will right-align the table.\n"
			           "- OuterSize.x = 0.0f will narrow fit the table unless there are any Stretch columns.\n"
			           "- OuterSize.y then becomes the minimum size for the table, which will extend vertically if there are more rows (unless NoHostExtendY is set).");

			// From a user point of view we will tend to use 'inner_width' differently depending on whether our table is embedding scrolling.
			// To facilitate toying with this demo we will actually pass 0.0f to the BeginTable() when ScrollX is disabled.
			ImGui::DragFloat("inner_width (when ScrollX active)", &inner_width_with_scroll, 1.0f, 0.0f, FLT_MAX);

			ImGui::DragFloat("row_min_height", &row_min_height, 1.0f, 0.0f, FLT_MAX);
			ImGui::SameLine();
			HelpMarker("Specify height of the Selectable item.");

			ImGui::Combo("items_type (first column)", &contents_type, contents_type_names, IM_COUNTOF(contents_type_names));
			// filter.Draw("filter");
			ImGui::TreePop();
		}

		ImGui::PopItemWidth();
		PopStyleCompact();
		ImGui::Spacing();
		ImGui::TreePop();
	}

	const ImDrawList* parent_draw_list = ImGui::GetWindowDrawList();
	const int parent_draw_list_draw_cmd_count = parent_draw_list->CmdBuffer.Size;
	ImVec2 table_scroll_cur, table_scroll_max; // For debug display
	const ImDrawList* table_draw_list = NULL;  // "
#endif // EDITABLE_TABLE_OPTIONS

	// Submit table
	const float inner_width_to_use = (flags & ImGuiTableFlags_ScrollX) ? inner_width_with_scroll : 0.0f;
	if (ImGui::BeginTable("table_advanced", kColumn_Count + 1, flags, outer_size_enabled ? outer_size_value : ImVec2(0, 0), inner_width_to_use))
	{
		LogTable_SetupColumns(view, columns_base_flags, freeze_cols, freeze_rows);

		// Show headers
		if (show_headers && (columns_base_flags & ImGuiTableColumnFlags_AngledHeader) != 0)
			ImGui::TableAngledHeadersRow();
		if (show_headers)
			ImGui::TableHeadersRow();

		// rows
		LogTable_EmitRows(view, row_min_height, otherControlFocused);

#if EDITABLE_TABLE_OPTIONS
		// Store some info to display debug details below
		table_scroll_cur = ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
		table_scroll_max = ImVec2(ImGui::GetScrollMaxX(), ImGui::GetScrollMaxY());
		table_draw_list = ImGui::GetWindowDrawList();
#endif // EDITABLE_TABLE_OPTIONS

		ImGui::EndTable();
	}

#if EDITABLE_TABLE_OPTIONS
	static bool show_debug_details = false;
	ImGui::Checkbox("Debug details", &show_debug_details);
	if (show_debug_details && table_draw_list)
	{
		ImGui::SameLine(0.0f, 0.0f);
		const int table_draw_list_draw_cmd_count = table_draw_list->CmdBuffer.Size;
		if (table_draw_list == parent_draw_list)
			ImGui::Text(": DrawCmd: +%d (in same window)",
			            table_draw_list_draw_cmd_count - parent_draw_list_draw_cmd_count);
		else
			ImGui::Text(": DrawCmd: +%d (in child window), Scroll: (%.f/%.f) (%.f/%.f)",
			            table_draw_list_draw_cmd_count - 1, table_scroll_cur.x, table_scroll_max.x, table_scroll_cur.y, table_scroll_max.y);
	}
#endif // EDITABLE_TABLE_OPTIONS

	return true;
}

void LogTable_Shutdown(void)
{
	sb_reset(&s_textSpan);
}
