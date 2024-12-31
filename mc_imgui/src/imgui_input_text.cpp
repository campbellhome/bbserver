// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "imgui_input_text.h"
#include "bb_array.h"
#include "imgui_core.h"
#include "sb.h"
#include "wrap_imgui_internal.h"

// InputTextMultilineScrolling and supporting functions adapted from https://github.com/ocornut/imgui/issues/383

struct MultilineScrollState
{
	const char* externalBuf = nullptr;

	float scrollRegionX = 0.0f;
	float scrollX = 0.0f;
	float scrollRegionY = 0.0f;
	float scrollY = 0.0f;

	int oldCursorPos = 0;

	b32 bHasScrollTargetX = false;
	float scrollTargetX = 0.0f;
	b32 bHasScrollTargetY = false;
	float scrollTargetY = 0.0f;

	u8 pad[4];
};

typedef struct MultilineScrollStates_s
{
	u32 count;
	u32 allocated;
	MultilineScrollState** data;
} MultilineScrollStates_t;

static MultilineScrollStates_t s_multilineScrollStates;

namespace ImGui
{
	static bool InputTextScrollingEx(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags);
}

void ImGui::InputTextShutdown(void)
{
	for (u32 i = 0; i < s_multilineScrollStates.count; ++i)
	{
		delete s_multilineScrollStates.data[i];
	}
	bba_free(s_multilineScrollStates);
}

static int Imgui_Core_FindLineStart(const char* buf, int cursorPos)
{
	while ((cursorPos > 0) && (buf[cursorPos - 1] != '\n'))
	{
		--cursorPos;
	}
	return cursorPos;
}

static int Imgui_Core_CountLines(const char* buf, int cursorPos)
{
	int lines = 1;
	int pos = 0;
	while (buf[pos])
	{
		if (buf[pos] == '\n')
		{
			++lines;
		}
		if (pos == cursorPos)
			break;
		++pos;
	}
	return lines;
}

static int Imgui_Core_InputTextMultilineScrollingCallback(ImGuiInputTextCallbackData* data)
{
	MultilineScrollState* scrollState = (MultilineScrollState*)data->UserData;
	if (scrollState->oldCursorPos != data->CursorPos)
	{
		const char* buf = data->Buf ? data->Buf : scrollState->externalBuf;
		int lineStartPos = Imgui_Core_FindLineStart(buf, data->CursorPos);
		int lines = Imgui_Core_CountLines(buf, data->CursorPos);

		ImVec2 lineSize = ImGui::CalcTextSize(buf + lineStartPos, buf + data->CursorPos);
		float cursorX = lineSize.x;
		float cursorY = (float)lines * lineSize.y;
		float scrollAmountX = scrollState->scrollRegionX * 0.25f;
		float scrollAmountY = scrollState->scrollRegionY * 0.25f;

		if (cursorX < scrollState->scrollX)
		{
			scrollState->bHasScrollTargetX = true;
			scrollState->scrollTargetX = ImMax(0.0f, cursorX - scrollAmountX);
		}
		else if ((cursorX - scrollState->scrollRegionX) >= scrollState->scrollX)
		{
			scrollState->bHasScrollTargetX = true;
			if ((cursorX - scrollState->scrollRegionX) > scrollAmountX)
			{
				scrollState->scrollTargetX = cursorX - scrollAmountX;
			}
			else
			{
				scrollState->scrollTargetX = cursorX - scrollState->scrollRegionX + scrollAmountX;
			}
		}

		if (cursorY < scrollState->scrollY)
		{
			scrollState->bHasScrollTargetY = true;
			scrollState->scrollTargetY = ImMax(0.0f, cursorY - scrollAmountY);
		}
		else if ((cursorY - scrollState->scrollRegionY) >= scrollState->scrollY)
		{
			scrollState->bHasScrollTargetY = true;
			if ((cursorY - scrollState->scrollRegionY) > scrollAmountY)
			{
				scrollState->scrollTargetY = cursorY - scrollAmountY;
			}
			else
			{
				scrollState->scrollTargetY = cursorY - scrollState->scrollRegionY + scrollAmountY;
			}
		}

		scrollState->oldCursorPos = data->CursorPos;
	}

	return 0;
}

static bool ImGui::InputTextScrollingEx(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags)
{
	const char* labelVisibleEnd = FindRenderedTextEnd(label);
	bool bMultiline = (flags & ImGuiInputTextFlags_Multiline) != 0;

	ImVec2 textSize = CalcTextSize(buf);
	float scrollbarSize = GetStyle().ScrollbarSize;
	float labelWidth = CalcTextSize(label, labelVisibleEnd).x;

	ImVec2 availSize = GetContentRegionAvail();
	if (size.x > 0.0f)
	{
		availSize.x = size.x;
	}
	else if (size.x < 0.0f)
	{
		availSize.x += size.x;
	}
	else
	{
		availSize.x = CalcItemWidth();
	}

	if (size.y > 0.0f)
	{
		availSize.y = size.y;
	}
	else if (size.y < 0.0f)
	{
		availSize.y += size.y;
	}
	else if (size.y == 0.0f)
	{
		if (bMultiline)
		{
			availSize.y = 4.0f * GetTextLineHeightWithSpacing() + scrollbarSize;
		}
		else
		{
			availSize.y = GetTextLineHeightWithSpacing();
		}
	}
	float childWidth = availSize.x;
	if (labelWidth > 0.0f)
	{
		childWidth = -labelWidth;
	}
	float childHeight = availSize.y;

	float textBoxWidth = availSize.x - (bMultiline ? scrollbarSize : 0.0f);
	if (textSize.x > textBoxWidth * 0.8f)
	{
		textBoxWidth = textSize.x + textBoxWidth * 0.8f;
	}
	float textBoxHeight = 0.0f;
	if (childHeight < 0)
	{
		textBoxHeight = availSize.y + childHeight - GetStyle().FramePadding.y - scrollbarSize;
	}
	else if (childHeight > 0.0f)
	{
		textBoxHeight = childHeight - GetStyle().FramePadding.y - scrollbarSize;
	}
	float minTextBoxHeight = textSize.y + 2.0f * GetStyle().FramePadding.y;
	if (bMultiline)
	{
		minTextBoxHeight += 2.0f * GetTextLineHeightWithSpacing();
	}
	else
	{
		childHeight = minTextBoxHeight + scrollbarSize;
	}
	if (textBoxHeight < minTextBoxHeight)
	{
		textBoxHeight = minTextBoxHeight;
	}

	if (bMultiline && textBoxHeight < childHeight && textBoxWidth <= childWidth)
	{
		textBoxHeight = childHeight;
	}
	if (textBoxWidth < childWidth && textBoxHeight <= childHeight)
	{
		textBoxWidth = childWidth;
	}

	ImGuiID scrollStateKey = GetID("textInputScrollState");
	ImGuiStorage* storage = GetStateStorage();
	MultilineScrollState* scrollState = (MultilineScrollState*)storage->GetVoidPtr(scrollStateKey);
	if (!scrollState)
	{
		scrollState = new MultilineScrollState;
		if (scrollState)
		{
			bba_push(s_multilineScrollStates, scrollState);
			storage->SetVoidPtr(scrollStateKey, scrollState);
		}
	}
	if (!scrollState)
		return false;

	ImGuiWindowFlags childFlags = ImGuiWindowFlags_HorizontalScrollbar;
	BeginChild(label, ImVec2(childWidth, childHeight), false, childFlags);
	scrollState->scrollRegionX = ImMax(0.0f, GetWindowWidth() - scrollbarSize);
	scrollState->scrollX = GetScrollX();
	scrollState->scrollRegionY = ImMax(0.0f, GetWindowHeight() - scrollbarSize);
	scrollState->scrollY = GetScrollY();
	scrollState->externalBuf = buf;
	int oldCursorPos = scrollState->oldCursorPos;

	PushItemWidth(textBoxWidth);
	bool changed = InputTextEx(label, nullptr, buf, (int)buf_size, ImVec2(textBoxWidth, textBoxHeight),
	                           flags | ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_NoHorizontalScroll, Imgui_Core_InputTextMultilineScrollingCallback, scrollState);
	PopItemWidth();
	if (IsItemActive() && Imgui_Core_HasFocus())
	{
		Imgui_Core_RequestRender();
	}

	if (scrollState->bHasScrollTargetX)
	{
		float realMaxScrollX = GetScrollMaxX();
		float maxScrollX = textBoxWidth;
		float maxUserScrollX = maxScrollX - childWidth * 0.5f;
		if ((realMaxScrollX == 0.0f && scrollState->scrollTargetX > 0.0f) || scrollState->scrollTargetX > textSize.x || (scrollState->scrollTargetX > scrollState->scrollX && maxScrollX < textSize.x))
		{
			scrollState->oldCursorPos = oldCursorPos;
			SetScrollX(maxUserScrollX);
		}
		else
		{
			scrollState->bHasScrollTargetX = false;
			if (scrollState->scrollTargetX <= maxScrollX)
			{
				SetScrollX(scrollState->scrollTargetX);
			}
			else
			{
				SetScrollX(maxUserScrollX);
			}
		}
	}

	if (scrollState->bHasScrollTargetY)
	{
		float realMaxScrollY = GetScrollMaxY();
		float maxScrollY = textBoxHeight;
		float maxUserScrollY = maxScrollY - textBoxHeight * 0.5f;
		if ((realMaxScrollY == 0.0f && scrollState->scrollTargetY > 0.0f) || scrollState->scrollTargetY > textSize.y || (scrollState->scrollTargetY > scrollState->scrollY && maxScrollY < textSize.y))
		{
			scrollState->oldCursorPos = oldCursorPos;
			SetScrollY(maxUserScrollY);
		}
		else
		{
			scrollState->bHasScrollTargetY = false;
			if (scrollState->scrollTargetY <= maxScrollY)
			{
				SetScrollY(scrollState->scrollTargetY);
			}
			else
			{
				SetScrollY(maxUserScrollY);
			}
		}
	}

	EndChild();
	SameLine();
	TextUnformatted(label, labelVisibleEnd);

	return changed;
}

bool ImGui::InputTextMultilineScrolling(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags)
{
	return InputTextScrollingEx(label, buf, buf_size, size, flags | ImGuiInputTextFlags_Multiline);
}

bool ImGui::InputTextMultilineScrolling(const char* label, sb_t* sb, u32 buf_size, const ImVec2& size, ImGuiInputTextFlags flags)
{
	if (sb->allocated < buf_size)
	{
		sb_reserve(sb, buf_size);
	}
	bool ret = InputTextScrollingEx(label, (char*)sb->data, sb->allocated, size, flags | ImGuiInputTextFlags_Multiline);
	sb->count = sb->data ? (u32)strlen(sb->data) + 1 : 0;
	return ret;
}

bool ImGui::InputTextScrolling(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags)
{
	return InputTextScrollingEx(label, buf, buf_size, ImVec2(0.0f, 0.0f), flags);
}

bool ImGui::InputTextScrolling(const char* label, sb_t* sb, u32 buf_size, ImGuiInputTextFlags flags)
{
	if (sb->allocated < buf_size)
	{
		sb_reserve(sb, buf_size);
	}
	bool ret = InputTextScrollingEx(label, (char*)sb->data, sb->allocated, ImVec2(0.0f, 0.0f), flags);
	sb->count = sb->data ? (u32)strlen(sb->data) + 1 : 0;
	return ret;
}
