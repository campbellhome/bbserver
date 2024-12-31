// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "wrap_imgui.h"

typedef struct sb_s sb_t;

namespace ImGui
{

	void InputTextShutdown(void);
	bool InputTextMultilineScrolling(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
	bool InputTextMultilineScrolling(const char* label, sb_t* sb, u32 buf_size, const ImVec2& size, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
	bool InputTextScrolling(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);
	bool InputTextScrolling(const char* label, sb_t* sb, u32 buf_size, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None);

} // namespace ImGui
