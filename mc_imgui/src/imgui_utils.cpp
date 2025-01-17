// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "imgui_utils.h"
#include "imgui_core.h"
#include "sb.h"
#include "va.h"
#include "wrap_imgui_internal.h"
#include <math.h>

namespace ImGui
{
	int s_tabCount;
	ImColor s_shadowColor(0, 0, 0);

	void PushStyleColor(ImGuiCol idx, const ImColor& col)
	{
		PushStyleColor(idx, (ImVec4)col);
	}

	bool Checkbox(const char* label, b8* v)
	{
		bool b = *v != 0;
		bool ret = Checkbox(label, &b);
		if (ret)
		{
			*v = b;
		}
		return ret;
	}

	bool Checkbox(const char* label, b32* v)
	{
		bool b = *v != 0;
		bool ret = Checkbox(label, &b);
		if (ret)
		{
			*v = b;
		}
		return ret;
	}

	bool Selectable(const char* label, b32* p_selected, ImGuiSelectableFlags flags, const ImVec2& size_arg)
	{
		if (Selectable(label, *p_selected != 0, flags, size_arg))
		{
			*p_selected = !*p_selected;
			return true;
		}
		return false;
	}

	bool TreeNodeEx(const char* label, ImGuiTreeNodeFlags flags, b32* v, void* ptr_id)
	{
		bool b = *v != 0;
		bool open;
		if (ptr_id)
		{
			open = TreeNodeEx(ptr_id, flags | (b ? ImGuiTreeNodeFlags_Selected : 0), "%s", label);
		}
		else
		{
			open = TreeNodeEx(label, flags | (b ? ImGuiTreeNodeFlags_Selected : 0));
		}
		if (IsItemClicked())
		{
			*v = !*v;
		}
		return open;
	}

	bool MenuItem(const char* label, const char* shortcut, b32* p_selected, bool enabled)
	{
		if (!p_selected)
		{
			return MenuItem(label, shortcut, (bool*)nullptr, enabled);
		}
		bool b = *p_selected != 0;
		bool ret = MenuItem(label, shortcut, &b, enabled);
		*p_selected = b;
		return ret;
	}

	bool Begin(const char* name, b32* p_open, ImGuiWindowFlags flags)
	{
		if (!p_open)
		{
			return Begin(name, (bool*)nullptr, flags);
		}
		bool b = *p_open != 0;
		bool ret = Begin(name, &b, flags);
		*p_open = b;
		return ret;
	}

	bool Begin(const char* name, b8* p_open, ImGuiWindowFlags flags)
	{
		if (!p_open)
		{
			return Begin(name, (bool*)nullptr, flags);
		}
		bool b = *p_open != 0;
		bool ret = Begin(name, &b, flags);
		*p_open = b;
		return ret;
	}

	bool IsAnyKeyPressedOrReleasedThisFrame()
	{
		const ImGuiIO& io = ImGui::GetIO();
		for (const ImGuiKeyData& keyData : io.KeysData)
		{
			if (keyData.Down || keyData.DownDurationPrev > 0.0f)
			{
				return true;
			}
		}
		return false;
	}

	bool IsCurrentWindowNavWindowRoot(void)
	{
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = GetCurrentWindow();
		return (g.NavWindow && window && (window == g.NavWindow || window == g.NavWindow->ParentWindow));
	}

	bool IsCurrentWindowNavWindow(void)
	{
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = GetCurrentWindow();
		return (window && window == g.NavWindow);
	}

	bool IsCurrentWindowMoving(void)
	{
		return GetCurrentContext()->MovingWindow == GetCurrentWindow();
	}

	bool IsAnyWindowMoving(void)
	{
		return GetCurrentContext()->MovingWindow != nullptr;
	}

	// InputText resize logic adapted from imgui_stdlib.cpp
	struct InputTextResizeCallbackUserData
	{
		sb_t* sb;
		ImGuiInputTextCallback userCallback;
		void* userCallbackUserData;
	};

	static int InputTextResizeCallback(ImGuiInputTextCallbackData* data)
	{
		InputTextResizeCallbackUserData* userData = (InputTextResizeCallbackUserData*)data->UserData;
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			// Resize string callback
			// If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
			sb_t* sb = userData->sb;
			IM_ASSERT(data->Buf == sb->data);
			if (data->BufSize > 0 && (u32)data->BufSize > sb->allocated)
			{
				u32 delta = (u32)data->BufSize - sb->allocated;
				sb_grow(sb, delta);

				data->Buf = sb->data;
				data->BufSize = (int)sb->allocated;
			}
		}
		else if (userData->userCallback)
		{
			// Forward to user callback, if any
			data->UserData = userData->userCallbackUserData;
			return userData->userCallback(data);
		}
		return 0;
	}

	bool InputText(const char* label, sb_t* sb, u32 buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
	{
		if (sb->allocated < buf_size)
		{
			sb_reserve(sb, buf_size);
		}

		bool ret;

		if ((flags & ImGuiInputTextFlags_CallbackResize) == 0)
		{
			ret = InputText(label, (char*)sb->data, sb->allocated, flags, callback, user_data);
		}
		else
		{
			InputTextResizeCallbackUserData resizeCallbackUserData;
			resizeCallbackUserData.sb = sb;
			resizeCallbackUserData.userCallback = callback;
			resizeCallbackUserData.userCallbackUserData = user_data;

			ret = InputText(label, (char*)sb->data, sb->allocated, flags, InputTextResizeCallback, &resizeCallbackUserData);
		}

		sb->count = sb->data ? (u32)strlen(sb->data) + 1 : 0;
		if (IsItemActive() && Imgui_Core_HasFocus())
		{
			Imgui_Core_RequestRender();
		}
		return ret;
	}

	bool InputTextMultiline(const char* label, sb_t* sb, u32 buf_size, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
	{
		if (sb->allocated < buf_size)
		{
			sb_reserve(sb, buf_size);
		}

		bool ret;

		if ((flags & ImGuiInputTextFlags_CallbackResize) == 0)
		{
			ret = InputTextMultiline(label, (char*)sb->data, sb->allocated, size, flags, callback, user_data);
		}
		else
		{
			InputTextResizeCallbackUserData resizeCallbackUserData;
			resizeCallbackUserData.sb = sb;
			resizeCallbackUserData.userCallback = callback;
			resizeCallbackUserData.userCallbackUserData = user_data;

			ret = InputTextMultiline(label, (char*)sb->data, sb->allocated, size, flags, InputTextResizeCallback, &resizeCallbackUserData);
		}

		sb->count = sb->data ? (u32)strlen(sb->data) + 1 : 0;
		if (IsItemActive() && Imgui_Core_HasFocus())
		{
			Imgui_Core_RequestRender();
		}
		return ret;
	}

	verticalScrollDir_e GetVerticalScrollDir()
	{
		if (IsKeyPressed(ImGuiKey_PageUp))
		{
			return kVerticalScroll_PageUp;
		}
		else if (IsKeyPressed(ImGuiKey_PageDown))
		{
			return kVerticalScroll_PageDown;
		}
		else if (IsKeyPressed(ImGuiKey_UpArrow))
		{
			return kVerticalScroll_Up;
		}
		else if (IsKeyPressed(ImGuiKey_DownArrow))
		{
			return kVerticalScroll_Down;
		}
		else if (IsKeyPressed(ImGuiKey_Home))
		{
			return kVerticalScroll_Start;
		}
		else if (IsKeyPressed(ImGuiKey_End))
		{
			return kVerticalScroll_End;
		}
		else
		{
			return kVerticalScroll_None;
		}
	}

	void SelectableTextUnformattedMultiline(const char* label, const char* text, ImVec2 size)
	{
		InputTextMultiline(label, const_cast<char*>(text), strlen(text) + 1, size, ImGuiInputTextFlags_ReadOnly);
		if (IsItemActive() && Imgui_Core_HasFocus())
		{
			Imgui_Core_RequestRender();
		}
	}

	void SelectableTextUnformatted(const char* label, const char* text)
	{
		InputText(label, const_cast<char*>(text), strlen(text) + 1, ImGuiInputTextFlags_ReadOnly);
		if (IsItemActive() && Imgui_Core_HasFocus())
		{
			Imgui_Core_RequestRender();
		}
	}

	void SelectableText(const char* label, const char* fmt, ...)
	{
		sb_t sb = { 0 };
		va_list args;
		va_start(args, fmt);
		sb_va_list(&sb, fmt, args);
		SelectableTextUnformatted(label, sb_get(&sb));
		va_end(args);
		sb_reset(&sb);
	}

	void PushButtonColors(buttonType_e colors)
	{
		const auto& styleColors = GetStyle().Colors;
		const ImGuiCol_ colorNormal = ImGuiCol_Button;
		const ImGuiCol_ colorHoverd = ImGuiCol_ButtonHovered;
		const ImGuiCol_ colorActive = ImGuiCol_ButtonActive;
		switch (colors)
		{
		case kButton_Normal:
			PushStyleColor(colorNormal, styleColors[colorNormal]);
			PushStyleColor(colorHoverd, styleColors[colorHoverd]);
			PushStyleColor(colorActive, styleColors[colorActive]);
			break;
		case kButton_Disabled:
			PushStyleColor(colorNormal, (ImVec4)ImColor(64, 64, 64));
			PushStyleColor(colorHoverd, (ImVec4)ImColor(64, 64, 64));
			PushStyleColor(colorActive, (ImVec4)ImColor(64, 64, 64));
			break;
		case kButton_TabActive:
			PushStyleColor(colorNormal, styleColors[ImGuiCol_Header]);
			PushStyleColor(colorHoverd, styleColors[ImGuiCol_HeaderHovered]);
			PushStyleColor(colorActive, styleColors[ImGuiCol_HeaderActive]);
			break;
		case kButton_TabInactive:
			PushStyleColor(colorNormal, (ImVec4)ImColor(255, 255, 255, 12));
			PushStyleColor(colorHoverd, styleColors[ImGuiCol_HeaderHovered]);
			PushStyleColor(colorActive, styleColors[ImGuiCol_HeaderActive]);
			break;
		case kButton_ColumnHeader:
			PushStyleColor(colorNormal, styleColors[colorNormal]);
			PushStyleColor(colorHoverd, styleColors[colorHoverd]);
			PushStyleColor(colorActive, styleColors[colorActive]);
			break;
		case kButton_ColumnHeaderNoSort:
			PushStyleColor(colorNormal, 0u);
			PushStyleColor(colorHoverd, 0u);
			PushStyleColor(colorActive, 0u);
			break;
		case kButton_ResizeBar:
			PushStyleColor(colorNormal, styleColors[colorNormal]);
			PushStyleColor(colorHoverd, styleColors[colorHoverd]);
			PushStyleColor(colorActive, styleColors[colorActive]);
			break;
		}
	}
	void PopButtonColors(void)
	{
		PopStyleColor(3);
	}
	bool Button(const char* label, buttonType_e colors, const ImVec2& size)
	{
		PushButtonColors(colors);
		bool ret = Button(label, size);
		PopButtonColors();
		return ret;
	}

	void BeginTabButtons(void)
	{
		s_tabCount = 0;
	}
	bool TabButton(const char* label, sb_t* active, const char* name, const ImVec2& size)
	{
		if (s_tabCount++)
		{
			SameLine();
		}
		bool isActive = !strcmp(sb_get(active), name);
		bool ret = Button(label, isActive ? kButton_TabActive : kButton_TabInactive, size);
		if (ret && !isActive)
		{
			sb_reset(active);
			sb_append(active, name);
		}
		return ret;
	}
	bool TabButton(const char* label, u32* active, u32 id, const ImVec2& size)
	{
		if (s_tabCount++)
		{
			SameLine();
		}
		bool isActive = *active == id;
		bool ret = Button(label, isActive ? kButton_TabActive : kButton_TabInactive, size);
		if (ret && !isActive)
		{
			*active = id;
		}
		return ret;
	}
	bool TabButtonIconColored(const char* icon, ImColor iconColor, const char* label, u32* active, u32 id, const ImVec2& size)
	{
		if (s_tabCount++)
		{
			SameLine();
		}
		bool isActive = *active == id;
		ImVec2 iconPos = GetIconPosForButton();
		bool ret = Button(label, isActive ? kButton_TabActive : kButton_TabInactive, size);
		if (ret && !isActive)
		{
			*active = id;
		}
		DrawIconAtPos(iconPos, icon, iconColor, true);
		return ret;
	}
	void EndTabButtons(void)
	{
	}

	bool BeginTabChild(sb_t* active, const char* str_id, const ImVec2& size, bool border, ImGuiWindowFlags extra_flags)
	{
		if (strcmp(sb_get(active), str_id))
		{
			return false;
		}
		SetCursorPosY(GetCursorPosY() - 4 * Imgui_Core_GetDpiScale());
		return BeginChild(str_id, size, border, extra_flags);
	}

	bool BeginTabChild(u32* active, u32 id, const char* str_id, const ImVec2& size, bool border, ImGuiWindowFlags extra_flags)
	{
		if (*active != id)
		{
			return false;
		}
		SetCursorPosY(GetCursorPosY() - 4 * Imgui_Core_GetDpiScale());
		return BeginChild(str_id, size, border, extra_flags);
	}

	void EndTabChild()
	{
		EndChild();
	}

	void PushColumnHeaderClipRect(float offset, float width)
	{
		float windowX = GetWindowPos().x;
		float x1 = floorf(0.5f + windowX + offset - 1.0f);
		float x2 = floorf(0.5f + windowX + offset + width - 1.0f);
		PushClipRect(ImVec2(x1, -FLT_MAX), ImVec2(x2, +FLT_MAX), true);
	}

	void DrawColumnText(columnDrawData h, u32 columnIndex, const char* text, const char* end)
	{
		float offset = h.columnOffsets[columnIndex];
		float width = h.columnWidths[columnIndex] * Imgui_Core_GetDpiScale();
		PushColumnHeaderClipRect(offset, width);
		if (columnIndex)
		{
			SameLine(offset);
		}
		TextUnformatted(text, end);
		PopClipRect();
	}

	void DrawColumnHeaderText(float offset, float width, const char* text, const char* end, const char* contextMenuName)
	{
		PushColumnHeaderClipRect(offset - GetScrollX(), width);
		SameLine(offset);
		TextUnformatted(text, end);
		PopClipRect();
		if (contextMenuName && IsMouseClicked(1) && IsItemHovered(ImGuiHoveredFlags_RectOnly))
		{
			OpenPopup(contextMenuName);
		}
	}

	columnDrawResult DrawColumnHeader(columnDrawData h, u32 columnIndex, const char* contextMenuName)
	{
#define ICON_SORT_UP ICON_FK_CARET_UP
#define ICON_SORT_DOWN ICON_FK_CARET_DOWN

		columnDrawResult res = {};
		b32 sortable = h.sortColumn != nullptr;
		const char* text = h.columnNames[columnIndex];
		float* width = h.columnWidths + columnIndex;
		bool last = columnIndex + 1 == h.numColumns;
		if (last)
		{
			*width = GetContentRegionAvail().x;
		}
		else if (*width <= 0.0f && sortable)
		{
			*width = CalcTextSize(text).x + CalcTextSize(" " ICON_SORT_UP).x + GetStyle().ItemSpacing.x * 2.0f;
		}
		float scale = (Imgui_Core_GetDpiScale() <= 0.0f) ? 1.0f : Imgui_Core_GetDpiScale();
		float startOffset = GetCursorPosX();

		if (Button(va("###%s", text), sortable ? kButton_ColumnHeader : kButton_ColumnHeaderNoSort, ImVec2(*width * scale, 0.0f)) && sortable)
		{
			res.sortChanged = true;
			if (*h.sortColumn == columnIndex)
			{
				*h.sortDescending = !*h.sortDescending;
			}
			else
			{
				*h.sortColumn = columnIndex;
				*h.sortDescending = false;
			}
		}
		if (contextMenuName && IsMouseClicked(1) && IsItemHovered(ImGuiHoveredFlags_RectOnly))
		{
			OpenPopup(contextMenuName);
		}

		res.active = res.active || IsItemActive();
		float endOffset = startOffset + *width * scale + GetStyle().ItemSpacing.x;
		const char* columnText = (sortable && *h.sortColumn == columnIndex) ? va("%s %s", *h.sortDescending ? ICON_SORT_DOWN : ICON_SORT_UP, text) : text;
		const float itemPad = GetStyle().ItemSpacing.x;
		DrawColumnHeaderText(startOffset + GetStyle().ItemInnerSpacing.x, *width * scale - itemPad, columnText);
		SameLine(endOffset);
		if (!last)
		{
			Button(va("|###sep%s", text), kButton_ResizeBar, ImVec2(3.0f * Imgui_Core_GetDpiScale(), 0.0f));
			if (ImGui::IsItemActive() || ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			}
			if (IsItemActive())
			{
				res.active = true;
				*width += GetIO().MouseDelta.x / scale;
				if (*width < 1.0f)
				{
					*width = 1.0f;
				}
			}
			SameLine();
		}

		h.columnOffsets[columnIndex + 1] = GetCursorPosX();
		return res;
	}

	void PushSelectableColors(b32 selected, b32 viewActive)
	{
		if (selected && !viewActive)
		{
			ImVec4 col = GetStyle().Colors[ImGuiCol_Header];
			col.x *= 0.5f;
			col.y *= 0.5f;
			col.z *= 0.5f;
			PushStyleColor(ImGuiCol_Header, col);
		}
	}

	void PopSelectableColors(b32 selected, b32 viewActive)
	{
		if (selected && !viewActive)
		{
			PopStyleColor();
		}
	}

	bool BeginContextMenu(const char* name)
	{
		if (IsMouseClicked(1) && IsItemHovered(ImGuiHoveredFlags_RectOnly))
		{
			OpenPopup(name);
		}
		return BeginPopup(name);
	}

	void EndContextMenu()
	{
		EndPopup();
	}

	void IconOverlayColored(ImColor iconColor, const char* icon, ImColor overlayColor, const char* overlay)
	{
		ImVec2 pos = GetIconPosForText();
		IconColored(iconColor, icon);
		float end = pos.x + CalcTextSize(icon).x;
		pos.x = pos.x + (end - pos.x) / 2;
		ImDrawList* drawList = GetWindowDrawList();
		ImVec2 size = CalcTextSize(overlay);
		float lineHeight = GetTextLineHeightWithSpacing();
		float deltaY = lineHeight - size.y + 2 * Imgui_Core_GetDpiScale();
		pos.y += deltaY;
		drawList->AddText(ImVec2(pos.x + 1, pos.y + 1), ImColor(0, 0, 0), overlay);
		drawList->AddText(ImVec2(pos.x + 0, pos.y + 0), overlayColor, overlay);
	}

	void IconOverlay(const char* icon, const char* overlay, ImColor overlayColor)
	{
		ImColor textColor = GetStyle().Colors[ImGuiCol_Text];
		IconOverlayColored(textColor, icon, overlayColor, overlay);
	}

	void IconColored(ImColor color, const char* icon)
	{
		ImVec2 pos = GetIconPosForText();
		TextColored(ImColor(0, 0, 0, 0), "%s", icon);
		ImDrawList* drawList = GetWindowDrawList();
		ImVec2 size = CalcTextSize(icon);
		float lineHeight = GetTextLineHeightWithSpacing();
		float deltaY = lineHeight - size.y;
		pos.y += deltaY;
		drawList->AddText(ImVec2(pos.x + 1, pos.y + 1), ImColor(0, 0, 0), icon);
		drawList->AddText(ImVec2(pos.x + 0, pos.y + 0), color, icon);
	}

	void Icon(const char* icon)
	{
		ImColor textColor = GetStyle().Colors[ImGuiCol_Text];
		IconColored(textColor, icon);
	}

	void DrawIconAtPos(ImVec2 pos, const char* icon, ImColor color, bool align, float scale)
	{
		ImFont* font = GetFont();
		float fontSize = GetFontSize();
		fontSize *= scale;

		if (align)
		{
			ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, icon);
			float lineHeight = GetTextLineHeightWithSpacing();
			float deltaY = lineHeight - size.y;
			pos.y += deltaY;
		}

		ImDrawList* drawList = GetWindowDrawList();
		if (color.Value.x > 0.0f || color.Value.y > 0.0f || color.Value.z > 0.0f)
		{
			drawList->AddText(font, fontSize, ImVec2(pos.x + 1, pos.y + 1), ImColor(0, 0, 0), icon);
		}
		drawList->AddText(font, fontSize, ImVec2(pos.x + 0, pos.y + 0), color, icon);
	}

	ImVec2 GetIconPosForButton()
	{
		ImVec2 windowPos = GetWindowPos();
		ImVec2 cursorPos = GetCursorPos();
		ImVec2 framePadding = GetStyle().FramePadding;
		ImVec2 textPos(windowPos.x + cursorPos.x + framePadding.x + GetScrollX(), windowPos.y + cursorPos.y - framePadding.y * 0.5f + 1 - GetScrollY());
		return textPos;
	}

	ImVec2 GetIconPosForText()
	{
		ImVec2 windowPos = GetWindowPos();
		ImVec2 cursorPos = GetCursorPos();
		float lineHeight = GetTextLineHeightWithSpacing();
		float fontHeight = CalcTextSize(nullptr).y;
		ImVec2 textPos(windowPos.x + cursorPos.x - GetScrollX(), windowPos.y + cursorPos.y - lineHeight + fontHeight - GetScrollY());
		return textPos;
	}

	void SetTextShadowColor(ImColor shadowColor)
	{
		s_shadowColor = shadowColor;
	}

	void TextShadow(const char* text, bool bWrapped, bool bHideLabel)
	{
		if (!Imgui_Core_GetTextShadows())
			return;

		const char* text_display_end = bHideLabel ? FindRenderedTextEnd(text, nullptr) : nullptr;

		ImVec2 pos = GetIconPosForText();

		ImFont* font = GetFont();
		float fontSize = GetFontSize();

		ImGuiWindow* window = GetCurrentWindow();
		const float wrap_pos_x = window->DC.TextWrapPos;
		const bool wrap_enabled = (wrap_pos_x >= 0.0f) && bWrapped;
		const float wrap_width = wrap_enabled ? CalcWrapWidthForPos(window->DC.CursorPos, wrap_pos_x) : 0.0f;

		ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
		float lineHeight = GetTextLineHeightWithSpacing();
		float deltaY = lineHeight - size.y;
		pos.y += deltaY;

		ImDrawList* drawList = GetWindowDrawList();
		drawList->AddText(font, fontSize, ImVec2(pos.x + 1, pos.y + 1), s_shadowColor, text, text_display_end, wrap_width);
	}

	void TextShadowed(const char* text)
	{
		TextShadow(text);
		TextUnformatted(text);
	}

	void TextWrappedShadowed(const char* text)
	{
		TextShadow(text, true, true);
		TextWrapped("%s", text);
	}

	void DrawStrikethrough(const char* text, ImColor color, ImVec2 pos)
	{
		ImFont* font = GetFont();
		float fontSize = GetFontSize();

		float barHeight = 2 * Imgui_Core_GetDpiScale();

		ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
		float lineHeight = GetTextLineHeightWithSpacing();
		float deltaY = lineHeight - size.y;
		pos.y += deltaY + 1 + 0.5f * (lineHeight - barHeight);

		ImVec2 start = pos;
		ImVec2 end = pos;
		end.x += size.x;

		ImDrawList* drawList = GetWindowDrawList();
		drawList->AddLine(start, end, color, barHeight);
	}

	// should be a copy/paste of ImGui::Selectable with added block rendering a bgColor background
	bool SelectableWithBackground(const char* label, bool selected, const ImColor bgColor, ImGuiSelectableFlags flags, const ImVec2& size_arg)
	{
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;

		// Submit label or explicit size to ItemSize(), whereas ItemAdd() will submit a larger/spanning rectangle.
		ImGuiID id = window->GetID(label);
		ImVec2 label_size = CalcTextSize(label, NULL, true);
		ImVec2 size(size_arg.x != 0.0f ? size_arg.x : label_size.x, size_arg.y != 0.0f ? size_arg.y : label_size.y);
		ImVec2 pos = window->DC.CursorPos;
		pos.y += window->DC.CurrLineTextBaseOffset;
		ItemSize(size, 0.0f);

		// Fill horizontal space
		// We don't support (size < 0.0f) in Selectable() because the ItemSpacing extension would make explicitly right-aligned sizes not visibly match other widgets.
		const bool span_all_columns = (flags & ImGuiSelectableFlags_SpanAllColumns) != 0;
		const float min_x = span_all_columns ? window->ParentWorkRect.Min.x : pos.x;
		const float max_x = span_all_columns ? window->ParentWorkRect.Max.x : window->WorkRect.Max.x;
		if (size_arg.x == 0.0f || (flags & ImGuiSelectableFlags_SpanAvailWidth))
			size.x = ImMax(label_size.x, max_x - min_x);

		// Text stays at the submission position, but bounding box may be extended on both sides
		const ImVec2 text_min = pos;
		const ImVec2 text_max(min_x + size.x, pos.y + size.y);

		// Selectables are meant to be tightly packed together with no click-gap, so we extend their box to cover spacing between selectable.
		ImRect bb(min_x, pos.y, text_max.x, text_max.y);
		if ((flags & ImGuiSelectableFlags_NoPadWithHalfSpacing) == 0)
		{
			const float spacing_x = span_all_columns ? 0.0f : style.ItemSpacing.x;
			const float spacing_y = style.ItemSpacing.y;
			const float spacing_L = IM_TRUNC(spacing_x * 0.50f);
			const float spacing_U = IM_TRUNC(spacing_y * 0.50f);
			bb.Min.x -= spacing_L;
			bb.Min.y -= spacing_U;
			bb.Max.x += (spacing_x - spacing_L);
			bb.Max.y += (spacing_y - spacing_U);
		}
		// if (g.IO.KeyCtrl) { GetForegroundDrawList()->AddRect(bb.Min, bb.Max, IM_COL32(0, 255, 0, 255)); }

		// Modify ClipRect for the ItemAdd(), faster than doing a PushColumnsBackground/PushTableBackgroundChannel for every Selectable..
		const float backup_clip_rect_min_x = window->ClipRect.Min.x;
		const float backup_clip_rect_max_x = window->ClipRect.Max.x;
		if (span_all_columns)
		{
			window->ClipRect.Min.x = window->ParentWorkRect.Min.x;
			window->ClipRect.Max.x = window->ParentWorkRect.Max.x;
		}

		const bool disabled_item = (flags & ImGuiSelectableFlags_Disabled) != 0;
		const bool item_add = ItemAdd(bb, id, NULL, disabled_item ? ImGuiItemFlags_Disabled : ImGuiItemFlags_None);
		if (span_all_columns)
		{
			window->ClipRect.Min.x = backup_clip_rect_min_x;
			window->ClipRect.Max.x = backup_clip_rect_max_x;
		}

		if (!item_add)
			return false;

		const bool disabled_global = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
		if (disabled_item && !disabled_global) // Only testing this as an optimization
			BeginDisabled();

		// FIXME: We can standardize the behavior of those two, we could also keep the fast path of override ClipRect + full push on render only,
		// which would be advantageous since most selectable are not selected.
		if (span_all_columns && window->DC.CurrentColumns)
			PushColumnsBackground();
		else if (span_all_columns && g.CurrentTable)
			TablePushBackgroundChannel();

		// We use NoHoldingActiveID on menus so user can click and _hold_ on a menu then drag to browse child entries
		ImGuiButtonFlags button_flags = 0;
		if (flags & ImGuiSelectableFlags_NoHoldingActiveID)
		{
			button_flags |= ImGuiButtonFlags_NoHoldingActiveId;
		}
		if (flags & ImGuiSelectableFlags_NoSetKeyOwner)
		{
			button_flags |= ImGuiButtonFlags_NoSetKeyOwner;
		}
		if (flags & ImGuiSelectableFlags_SelectOnClick)
		{
			button_flags |= ImGuiButtonFlags_PressedOnClick;
		}
		if (flags & ImGuiSelectableFlags_SelectOnRelease)
		{
			button_flags |= ImGuiButtonFlags_PressedOnRelease;
		}
		if (flags & ImGuiSelectableFlags_AllowDoubleClick)
		{
			button_flags |= ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_PressedOnDoubleClick;
		}
		if ((flags & ImGuiSelectableFlags_AllowOverlap) || (g.LastItemData.InFlags & ImGuiItemFlags_AllowOverlap))
		{
			button_flags |= ImGuiButtonFlags_AllowOverlap;
		}

		const bool was_selected = selected;
		bool hovered, held;
		bool pressed = ButtonBehavior(bb, id, &hovered, &held, button_flags);

		// Auto-select when moved into
		// - This will be more fully fleshed in the range-select branch
		// - This is not exposed as it won't nicely work with some user side handling of shift/control
		// - We cannot do 'if (g.NavJustMovedToId != id) { selected = false; pressed = was_selected; }' for two reasons
		//   - (1) it would require focus scope to be set, need exposing PushFocusScope() or equivalent (e.g. BeginSelection() calling PushFocusScope())
		//   - (2) usage will fail with clipped items
		//   The multi-select API aim to fix those issues, e.g. may be replaced with a BeginSelection() API.
		if ((flags & ImGuiSelectableFlags_SelectOnNav) && g.NavJustMovedToId != 0 && g.NavJustMovedToFocusScopeId == g.CurrentFocusScopeId)
			if (g.NavJustMovedToId == id)
				selected = pressed = true;

		// Update NavId when clicking or when Hovering (this doesn't happen on most widgets), so navigation can be resumed with gamepad/keyboard
		if (pressed || (hovered && (flags & ImGuiSelectableFlags_SetNavIdOnHover)))
		{
			if (!g.NavDisableMouseHover && g.NavWindow == window && g.NavLayer == window->DC.NavLayerCurrent)
			{
				SetNavID(id, window->DC.NavLayerCurrent, g.CurrentFocusScopeId, WindowRectAbsToRel(window, bb)); // (bb == NavRect)
				g.NavDisableHighlight = true;
			}
		}
		if (pressed)
			MarkItemEdited(id);

		// In this branch, Selectable() cannot toggle the selection so this will never trigger.
		if (selected != was_selected) //-V547
			g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

		// Render
		if (hovered || selected)
		{
			const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered
			                                                                                  : ImGuiCol_Header);
			RenderFrame(bb.Min, bb.Max, col, false, 0.0f);
		}

		//////////////////////////////////////////////////////////////////////////
		// Begin MC Change
		else
		{
			const ImU32 col = bgColor;
			if ((col & 0xff000000) != 0)
			{
				RenderFrame(bb.Min, bb.Max, col, false, 0.0f);
			}
		}
		// End MC Change
		//////////////////////////////////////////////////////////////////////////

		if (g.NavId == id)
			RenderNavHighlight(bb, id, ImGuiNavHighlightFlags_TypeThin | ImGuiNavHighlightFlags_NoRounding);

		if (span_all_columns && window->DC.CurrentColumns)
			PopColumnsBackground();
		else if (span_all_columns && g.CurrentTable)
			TablePopBackgroundChannel();

		RenderTextClipped(text_min, text_max, label, NULL, &label_size, style.SelectableTextAlign, &bb);

		// Automatically close popups
		if (pressed && (window->Flags & ImGuiWindowFlags_Popup) && !(flags & ImGuiSelectableFlags_DontClosePopups) && !(g.LastItemData.InFlags & ImGuiItemFlags_SelectableDontClosePopup))
			CloseCurrentPopup();

		if (disabled_item && !disabled_global)
			EndDisabled();

		IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
		return pressed; //-V1020
	}

	static const void* s_activeSelectables;
	void SetActiveSelectables(const void* data)
	{
		s_activeSelectables = data;
	}

	bool IsActiveSelectables(const void* data)
	{
		return s_activeSelectables == data;
	}

	ImGuiViewport* GetViewportForWindow(const char* windowName)
	{
		ImGuiWindow* window = FindWindowByName(windowName);
		if (window)
		{
			ImGuiViewport* viewport = FindViewportByID(window->ViewportId);
			if (viewport)
			{
				return viewport;
			}
		}
		return GetMainViewport();
	}

} // namespace ImGui
