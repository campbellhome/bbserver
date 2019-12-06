// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "ui_bb_messagebox.h"
#include "bb_colors.h"
#include "imgui_core.h"
#include "message_box.h"
#include "theme_config.h"
#include "va.h"

extern "C" {
messageBoxes g_messageboxes;
float g_messageboxHeight;
}

static inline ImVec2 operator+(const ImVec2 &lhs, const ImVec2 &rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
static inline ImVec2 operator-(const ImVec2 &lhs, const ImVec2 &rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }

float UIBlackboxMessageBox_Update(messageBoxes *boxes)
{
	messageBox *mb = mb_get_active(boxes);
	if(!mb)
		return 0.0f;

	b32 bRemove = false;

	ImVec2 region = ImGui::GetContentRegionAvail();
	ImVec2 windowPos = ImGui::GetWindowPos();
	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImVec2 size = region;
	size.y = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	ImVec2 start = windowPos + cursorPos;
	ImVec2 end = start + size;
	ImDrawList *DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(start, end, 0xffffffff);

	ImDrawVert *bottomLeft = &DrawList->VtxBuffer.back();
	ImDrawVert *bottomRight = bottomLeft - 1;
	ImDrawVert *topRight = bottomLeft - 2;
	ImDrawVert *topLeft = bottomLeft - 3;

	ImGui::BeginGroup();
	ImGui::Spacing();
	sdict_t *sd = &mb->data;
	const char *title = sdict_find(&mb->data, "title");
	if(title) {
		ImGui::AlignFirstTextHeightToWidgets();
		ImGui::Text(" [ %s ] ", title);
		ImGui::SameLine();
	}

	const char *text = sdict_find(&mb->data, "text");
	if(text) {
		ImGui::TextWrapped(" %s ", text);
		ImGui::SameLine();
	}

	ImVec2 curPos = ImGui::GetWindowPos() + ImGui::GetCursorPos();
	if(curPos.x - start.x >= region.x - 120 * Imgui_Core_GetDpiScale()) {
		ImGui::NewLine();
	}

	u32 buttonIndex = 0;
	const char *button;
	while((button = sdict_find(sd, va("button%u", ++buttonIndex))) != nullptr) {
		if(buttonIndex > 1) {
			ImGui::SameLine();
		}
		if(ImGui::Button(button, ImVec2(120 * Imgui_Core_GetDpiScale(), 0.0f))) {
			if(mb->callback) {
				mb->callback(mb, button);
			}
			bRemove = true;
			break;
		}
	};

	ImGui::Spacing();
	ImGui::EndGroup();

	// colors are ABGR
	u32 colorLeft = ImColor(MakeColor(kStyleColor_MessageBoxBackground0));
	u32 colorRight = ImColor(MakeColor(kStyleColor_MessageBoxBackground1));
	bottomLeft->col = colorLeft;
	bottomRight->col = colorRight;
	topRight->col = colorRight;
	topLeft->col = colorLeft;

	float endY = ImGui::GetWindowPos().y + ImGui::GetCursorPos().y;
	bottomLeft->pos.y = endY;
	bottomRight->pos.y = endY - ImGui::GetStyle().ItemSpacing.y;

	if(bRemove) {
		mb_remove_active(boxes);
	}

	return endY - start.y;
}
