// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "ui_message_box.h"

#include "bb_string.h"
#include "imgui_core.h"
#include "imgui_utils.h"
#include "message_box.h"
#include "va.h"

// warning C4820 : 'StructName' : '4' bytes padding added after data member 'MemberName'
// warning C4365: '=': conversion from 'ImGuiTabItemFlags' to 'ImGuiID', signed/unsigned mismatch
BB_WARNING_PUSH(4820, 4365)
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
BB_WARNING_POP

static int s_activeFrames;

static bool UIMessageBox_DrawModal(messageBox* mb)
{
	sdict_t* sd = &mb->data;
	const char* title = sdict_find(sd, "title");
	if (!title)
		return false;

	const char* text = sdict_find(sd, "text");
	if (text)
	{
		ImGui::TextUnformatted(text);
	}

	sdictEntry_t* inputNumber = sdict_find_entry(sd, "inputNumber");
	if (inputNumber)
	{
		if (s_activeFrames < 3)
		{
			ImGui::SetKeyboardFocusHere();
		}
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
		bool entered = ImGui::InputText("##path", &inputNumber->value, 1024, flags);
		if (entered)
		{
			if (mb->callback)
			{
				mb->callback(mb, sb_get(&inputNumber->value));
			}
			return false;
		}
	}

	u32 buttonIndex = 0;
	const char* button;
	while ((button = sdict_find(sd, va("button%u", ++buttonIndex))) != nullptr)
	{
		if (buttonIndex == 1)
		{
			ImGui::Separator();
		}
		else
		{
			ImGui::SameLine();
		}
		if (ImGui::Button(button, ImVec2(120 * Imgui_Core_GetDpiScale(), 0.0f)))
		{
			if (mb->callback)
			{
				mb->callback(mb, button);
			}
			return false;
		}
	};

	if (ImGui::IsKeyPressed(ImGuiKey_Escape))
	{
		if (mb->callback)
		{
			mb->callback(mb, "escape");
		}
		return false;
	}

	return true;
}

static void UIMessageBox_UpdateModal(messageBoxes* boxes)
{
	messageBox* mb = mb_get_active(boxes);
	if (!mb)
		return;

	const char* title = sdict_find(&mb->data, "title");
	if (!title)
	{
		title = "Untitled";
	}

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos + viewport->Size * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowViewport(viewport->ID);

	int flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking;
	if (!ImGui::BeginPopupModal(title, nullptr, flags))
	{
		s_activeFrames = 0;
		ImGui::OpenPopup(title);
		if (!ImGui::BeginPopupModal(title, nullptr, flags))
		{
			if (mb->callback)
			{
				mb->callback(mb, "");
			}
			mb_remove_active(nullptr);
			return;
		}
	}

	++s_activeFrames;
	if (!UIMessageBox_DrawModal(mb))
	{
		ImGui::CloseCurrentPopup();
		mb_remove_active(nullptr);
	}

	ImGui::EndPopup();
	return;
}

float UIMessageBox_Update(messageBoxes* boxes)
{
	if (!boxes)
	{
		boxes = mb_get_queue();
	}

	messageBox* mb = mb_get_active(boxes);
	if (!mb)
		return 0.0f;

	if (boxes->modal)
	{
		UIMessageBox_UpdateModal(boxes);
		return 0.0f;
	}

	b32 bRemove = false;

	ImVec2 region = ImGui::GetContentRegionAvail();
	ImVec2 windowPos = ImGui::GetWindowPos();
	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImVec2 size = region;
	size.y = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	ImVec2 start = windowPos + cursorPos;
	ImVec2 end = start + size;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(start, end, 0x22222222);

	int prevSize = DrawList->VtxBuffer.size();

	ImGui::BeginGroup();
	ImGui::Spacing();
	sdict_t* sd = &mb->data;
	const char* title = sdict_find(&mb->data, "title");
	if (title)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::Text(" [ %s ] ", title);
		ImGui::SameLine();
	}

	const char* text = sdict_find(&mb->data, "text");
	if (text)
	{
		ImGui::TextWrapped(" %s ", text);
		ImGui::SameLine();
	}

	bool focused = false;

	ImGui::PushItemWidth(120.0f * Imgui_Core_GetDpiScale());
	sdictEntry_t* inputNumber = sdict_find_entry(sd, "inputNumber");
	if (inputNumber)
	{
		if (s_activeFrames < 3)
		{
			ImGui::SetKeyboardFocusHere();
		}
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
		bool entered = ImGui::InputText("##path", &inputNumber->value, 1024, flags);
		focused = ImGui::IsItemFocused();
		if (entered)
		{
			if (mb->callback)
			{
				mb->callback(mb, sb_get(&inputNumber->value));
			}
			bRemove = true;
		}
		ImGui::SameLine();
	}
	ImGui::PopItemWidth();

	ImVec2 curPos = ImGui::GetWindowPos() + ImGui::GetCursorPos();
	if (curPos.x - start.x >= region.x - 120 * Imgui_Core_GetDpiScale())
	{
		ImGui::NewLine();
	}

	u32 buttonIndex = 0;
	const char* button;
	while ((button = sdict_find(sd, va("button%u", ++buttonIndex))) != nullptr)
	{
		if (buttonIndex > 1)
		{
			ImGui::SameLine();
		}
		if (ImGui::Button(button, ImVec2(120 * Imgui_Core_GetDpiScale(), 0.0f)))
		{
			if (mb->callback)
			{
				mb->callback(mb, button);
			}
			bRemove = true;
			break;
		}
		focused = focused || ImGui::IsItemFocused();
	};

	ImGui::Spacing();
	ImGui::EndGroup();

	ImDrawVert* bottomLeft = &DrawList->VtxBuffer[prevSize - 1];
	ImDrawVert* bottomRight = bottomLeft - 1;
	ImDrawVert* topRight = bottomLeft - 2;
	ImDrawVert* topLeft = bottomLeft - 3;

	// colors are ABGR
	u32 colorLeft = boxes->bgColor[0];
	u32 colorRight = boxes->bgColor[1];
	if (colorLeft || colorRight)
	{
		bottomLeft->col = colorLeft;
		bottomRight->col = colorRight;
		topRight->col = colorRight;
		topLeft->col = colorLeft;
	}

	float endY = ImGui::GetWindowPos().y + ImGui::GetCursorPos().y;
	bottomLeft->pos.y = endY - ImGui::GetStyle().ItemSpacing.y;
	bottomRight->pos.y = endY - ImGui::GetStyle().ItemSpacing.y;

	if (focused)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			if (mb->callback)
			{
				mb->callback(mb, "escape");
			}
			bRemove = true;
		}
	}

	if (bRemove)
	{
		s_activeFrames = 0;
		mb_remove_active(boxes);
	}
	else
	{
		++s_activeFrames;
	}

	return endY - start.y;
}
