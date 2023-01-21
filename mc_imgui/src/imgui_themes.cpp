// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "imgui_themes.h"
#include "imgui_core.h"
#include "wrap_imgui.h"

static ImGuiStyle s_defaultStyle;

void Style_Init()
{
	s_defaultStyle = ImGui::GetStyle();
}

void Style_Apply(const char* colorscheme)
{
	float dpiScale = Imgui_Core_GetDpiScale();
	ImGuiStyle& s = ImGui::GetStyle();
	s = s_defaultStyle;
	s.WindowPadding.x *= dpiScale;
	s.WindowPadding.y *= dpiScale;
	s.WindowMinSize.x *= dpiScale;
	s.WindowMinSize.y *= dpiScale;
	s.ChildRounding *= dpiScale;
	s.PopupRounding *= dpiScale;
	s.FramePadding.x *= dpiScale;
	s.FramePadding.y *= dpiScale;
	s.FrameRounding *= dpiScale;
	s.ItemSpacing.x *= dpiScale;
	s.ItemSpacing.y *= dpiScale;
	s.ItemInnerSpacing.x *= dpiScale;
	s.ItemInnerSpacing.y *= dpiScale;
	s.TouchExtraPadding.x *= dpiScale;
	s.TouchExtraPadding.y *= dpiScale;
	s.IndentSpacing *= dpiScale;
	s.ColumnsMinSpacing *= dpiScale;
	s.ScrollbarSize *= dpiScale;
	s.ScrollbarRounding *= dpiScale;
	s.GrabMinSize = 40 * dpiScale;
	s.GrabRounding *= dpiScale;
	s.DisplayWindowPadding.x *= dpiScale;
	s.DisplayWindowPadding.y *= dpiScale;
	s.DisplaySafeAreaPadding.x *= dpiScale;
	s.DisplaySafeAreaPadding.y *= dpiScale;

	if (!strcmp(colorscheme, "Visual Studio Dark"))
	{
		StyleColorsVSDark();
	}
	else if (!strcmp(colorscheme, "Classic"))
	{
		ImGui::StyleColorsClassic();
	}
	else if (!strcmp(colorscheme, "Light"))
	{
		ImGui::StyleColorsLight();
		s.ScrollbarSize = 18.0f * dpiScale;
		s.ScrollbarRounding = 4.0f * dpiScale;
		s.Colors[ImGuiCol_TitleBgActive] = s.Colors[ImGuiCol_TabActive];
		s.Colors[ImGuiCol_PopupBg] = ImVec4(0.8f, 0.8f, 0.8f, 1.f);
	}
	else if (!strcmp(colorscheme, "Windows"))
	{
		StyleColorsWindows();
	}
	else /*if(!strcmp(colorscheme, "Dark"))*/
	{
		ImGui::StyleColorsDark();
		// modal background for default Dark theme looks like a non-responding Window
		ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
	}
}

void StyleColorsVSDark()
{
	ImGui::StyleColorsClassic();
	ImGuiStyle* style = &ImGui::GetStyle();
	ImVec4* colors = style->Colors;
	colors[ImGuiCol_TitleBgActive] = ImColor(63, 63, 70, 255); // VS Dark Active Tab
	colors[ImGuiCol_TitleBg] = ImColor(45, 45, 48, 255);       // VS Dark Inactive Tab
	colors[ImGuiCol_WindowBg] = ImColor(42, 42, 44, 255);      // VS Dark Output Window
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
	colors[ImGuiCol_Button] = ImColor(76, 76, 76);
	colors[ImGuiCol_ButtonHovered] = ImColor(128, 128, 128);
	colors[ImGuiCol_ButtonActive] = ImColor(112, 112, 112);
}

// taken from https://github.com/ocornut/imgui/issues/707
void StyleColorsWindows()
{
	ImGui::StyleColorsLight();
	ImGuiStyle* style = &ImGui::GetStyle();
	ImVec4* colors = style->Colors;

	float dpiScale = Imgui_Core_GetDpiScale();
	float hspacing = 8 * dpiScale;
	float vspacing = 6 * dpiScale;
	style->DisplaySafeAreaPadding = ImVec2(0, 0);
	style->WindowPadding = ImVec2(hspacing / 2, vspacing);
	style->FramePadding = ImVec2(hspacing, vspacing);
	style->ItemSpacing = ImVec2(hspacing, vspacing);
	style->ItemInnerSpacing = ImVec2(hspacing, vspacing);
	style->IndentSpacing = 20.0f * dpiScale;

	style->WindowRounding = 0.0f;
	style->FrameRounding = 0.0f;

	style->WindowBorderSize = 0.0f;
	style->FrameBorderSize = 1.0f * dpiScale;
	style->PopupBorderSize = 1.0f * dpiScale;

	style->ScrollbarSize = 20.0f * dpiScale;
	style->ScrollbarRounding = 0.0f;
	style->GrabRounding = 0.0f;

	ImVec4 white = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	ImVec4 transparent = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	ImVec4 dark = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
	ImVec4 darker = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);

	ImVec4 background = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
	ImVec4 text = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	ImVec4 border = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	ImVec4 grab = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
	ImVec4 header = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
	ImVec4 active = ImVec4(0.00f, 0.47f, 0.84f, 1.00f);
	ImVec4 hover = ImVec4(0.00f, 0.47f, 0.84f, 0.20f);

	ImVec4 headerBlue = ImVec4(124 / 255.f, 188 / 255.f, 234 / 255.f, 1.00f);

	colors[ImGuiCol_Text] = text;
	colors[ImGuiCol_WindowBg] = background;
	colors[ImGuiCol_ChildBg] = background;
	colors[ImGuiCol_PopupBg] = white;

	colors[ImGuiCol_Border] = border;
	colors[ImGuiCol_BorderShadow] = transparent;

	colors[ImGuiCol_Button] = header;
	colors[ImGuiCol_ButtonHovered] = hover;
	colors[ImGuiCol_ButtonActive] = active;

	colors[ImGuiCol_FrameBg] = white;
	colors[ImGuiCol_FrameBgHovered] = hover;
	colors[ImGuiCol_FrameBgActive] = active;

	colors[ImGuiCol_MenuBarBg] = header;
	colors[ImGuiCol_Header] = headerBlue;
	colors[ImGuiCol_HeaderHovered] = hover;
	colors[ImGuiCol_HeaderActive] = active;

	colors[ImGuiCol_CheckMark] = text;
	colors[ImGuiCol_SliderGrab] = grab;
	colors[ImGuiCol_SliderGrabActive] = darker;

	colors[ImGuiCol_ScrollbarBg] = header;
	colors[ImGuiCol_ScrollbarGrab] = grab;
	colors[ImGuiCol_ScrollbarGrabHovered] = dark;
	colors[ImGuiCol_ScrollbarGrabActive] = darker;
}
