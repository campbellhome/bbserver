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
} // namespace ImGui

void LogTable_SetupColumns(view_t* view)
{
	// Declare columns
	for (u32 i = 0; i < BB_ARRAYSIZE(view->columns); ++i)
	{
		ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None;
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
	ImGuiTableColumnFlags textFlags = ImGuiTableColumnFlags_None;
	textFlags |= ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize;
	textFlags |= ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder;
	ImGui::TableSetupColumn("Text", textFlags, view->textWidth);

	ImGui::TableSetupScrollFreeze(1, 1);
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

static void LogTable_EmitRows(view_t* view, b32 otherControlFocused)
{
	// reset visible region
	view->lastVisibleSessionIndexStart = ~0U;
	view->lastVisibleSessionIndexEnd = 0U;
	view->lastVisibleSelectedSessionIndexStart = ~0U;
	view->lastVisibleSelectedSessionIndexEnd = 0U;

	ImGui::verticalScrollDir_e verticalScrollDir = ImGui::kVerticalScroll_None;
	bool logsHovered = ImGui::IsWindowHovered();
	PushLogFont();
	ImGuiListClipper clipper;
	clipper.Begin((int)view->visibleLogs.count);
	u32 numVisibleLines = 0;
	float lineHeight = 1.0f;
	bool bFirst = true;
	while (clipper.Step())
	{
		for (int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; row_n++)
		{
			if (!bFirst)
			{
				++numVisibleLines;
			}
			view_log_t* viewLog = view->visibleLogs.data + row_n;
			ImGui::PushID(va("%u.%u", viewLog->persistentLogIndex, viewLog->subLine));

			float startY = ImGui::GetCursorScreenPos().y;
			ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.0f);

			u32 logIndex = viewLog->sessionLogIndex;
			recorded_session_t* session = view->session;
			recorded_log_t* sessionLog = session->logs.data[logIndex];
			bb_decoded_packet_t* decoded = &sessionLog->packet;
			recorded_category_t* recordedCategory = recorded_session_find_category(session, decoded->packet.logText.categoryId);
			view_category_t* viewCategory = view_find_category(view, decoded->packet.logText.categoryId);
			named_filter_t* log_color_entry = named_filters_resolve(view, viewLog, sessionLog, true);
			view_log_colors_t viewLogColors = UIRecordedView_InitLogColors(decoded, viewLog, viewCategory, log_color_entry);

			LogLevelColorizer colorizer((bb_log_level_e)decoded->packet.logText.level);

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
					ImGui::TextShadowed(columnText);
					if (firstColumn)
					{
						firstColumn = false;
						ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
						UIRecordedView_PushLogStyleColors(viewLogColors);
						ImGui::SameLine();
						ImGui::SelectableWithBackground(va("###%u_%u", viewLog->sessionLogIndex, viewLog->subLine), viewLog->selected != 0, viewLogColors.bgColor, selectable_flags, ImVec2(0, 0));
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
			lineHeight = clipper.ItemsHeight > 0.0f ? clipper.ItemsHeight : endY - startY;

			if (bFirst)
			{
				bFirst = false;
			}
			else
			{
				// track our visible view region so we can recenter when toggling categories on/off etc
				view->lastVisibleSessionIndexStart = BB_MIN(view->lastVisibleSessionIndexStart, viewLog->sessionLogIndex);
				view->lastVisibleSessionIndexEnd = BB_MAX(view->lastVisibleSessionIndexEnd, viewLog->sessionLogIndex);
				if (viewLog->selected)
				{
					view->lastVisibleSelectedSessionIndexStart = BB_MIN(view->lastVisibleSelectedSessionIndexStart, viewLog->sessionLogIndex);
					view->lastVisibleSelectedSessionIndexEnd = BB_MAX(view->lastVisibleSelectedSessionIndexEnd, viewLog->sessionLogIndex);
				}
			}
			ImGui::PopID();
		}
	}
	PopLogFont();

	view->numVisibleLines = numVisibleLines;

	UIRecordedView_UpdateScrolling(view, logsHovered, otherControlFocused, lineHeight, verticalScrollDir);
}

// Custom ImGui::TableHeadersRow() to open a CategoriesContextMenu popup
static void LogTable_HeadersRow()
{
	ImGuiContext& g = *GImGui;
	ImGuiTable* table = g.CurrentTable;
	IM_ASSERT_USER_ERROR_RET(table != NULL, "Call should only be done while in BeginTable() scope!");

	// Call layout if not already done. This is automatically done by TableNextRow: we do it here _only_ to make
	// it easier to debug-step in TableUpdateLayout(). Your own version of this function doesn't need this.
	if (!table->IsLayoutLocked)
		ImGui::TableUpdateLayout(table);

	// Open row
	const float row_height = ImGui::TableGetHeaderRowHeight();
	ImGui::TableNextRow(ImGuiTableRowFlags_Headers, row_height);
	const float row_y1 = ImGui::GetCursorScreenPos().y;
	if (table->HostSkipItems) // Merely an optimization, you may skip in your own code.
		return;

	const int columns_count = ImGui::TableGetColumnCount();
	for (int column_n = 0; column_n < columns_count; column_n++)
	{
		if (!ImGui::TableSetColumnIndex(column_n) && table->LastHeldHeaderColumn != column_n)
			continue;

		// Push an id to allow empty/unnamed headers. This is also idiomatic as it ensure there is a consistent ID path to access columns (for e.g. automation)
		const char* name = (ImGui::TableGetColumnFlags(column_n) & ImGuiTableColumnFlags_NoHeaderLabel) ? "" : ImGui::TableGetColumnName(column_n);
		ImGui::PushID(column_n);
		ImGui::TableHeader(name);
		ImGui::PopID();
		if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
		{
			ImGui::OpenPopup("CategoriesContextMenu");
		}
	}

	// Allow opening popup from the right-most section after the last column.
	ImVec2 mouse_pos = ImGui::GetMousePos();
	if (ImGui::IsMouseReleased(1) && ImGui::TableGetHoveredColumn() == columns_count)
	{
		if (mouse_pos.y >= row_y1 && mouse_pos.y < row_y1 + row_height)
		{
			ImGui::OpenPopup("CategoriesContextMenu");
		}
	}
}

bool LogTable_Update(view_t* view, b32 otherControlFocused)
{
	if (!view)
		return false;

	const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
	if (ImGui::BeginTable("logtable", kColumn_Count + 1, flags, ImVec2(0, 0), 0.0f))
	{
		LogTable_SetupColumns(view);

		// Show headers
		LogTable_HeadersRow();
		UIRecordedView_ColumnContextMenu(view, "CategoriesContextMenu");

		// rows
		LogTable_EmitRows(view, otherControlFocused);

		ImGui::EndTable();
	}

	return true;
}

void LogTable_Shutdown(void)
{
	sb_reset(&s_textSpan);
}
