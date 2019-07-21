// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "ui_config.h"
#include "bbclient/bb_array.h"
#include "bbclient/bb_string.h"
#include "fonts.h"
#include "imgui_core.h"
#include "imgui_themes.h"
#include "imgui_tooltips.h"
#include "imgui_utils.h"
#include "theme_config.h"
#include "ui_recordings.h"

static config_t s_preferencesConfig;
bool s_preferencesValid;
bool s_preferencesOpen;
bool s_preferencesAdvanced;
bool s_preferencesNeedFocus;

using namespace ImGui;

extern "C" void configWhitelistEntry_reset(configWhitelistEntry_t *val);

static void Preferences_WhitelistTooltipAndContextMenu(const char *contextMenuName, bool &bRemove)
{
	if(ImGui::BeginPopupContextItem(contextMenuName)) {
		if(ImGui::Selectable("Delete row")) {
			bRemove = true;
		}
		ImGui::EndPopup();
	}
}

static void Preferences_WhitelistEntry(u32 index, configWhitelist_t &whitelist)
{
	bool bRemove = false;

	configWhitelistEntry_t &entry = whitelist.data[index];
	PushID((int)index);

	Checkbox("allow", &entry.allow);
	Preferences_WhitelistTooltipAndContextMenu("whitelistEntryAllowContextMenu", bRemove);
	NextColumn();

	ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
	PushItemWidth(-1.0f);
	InputText("##addr", &entry.addressPlusMask, 128, flags);
	Fonts_CacheGlyphs(sb_get(&entry.addressPlusMask));
	if(IsItemActive() && Imgui_Core_HasFocus()) {
		Imgui_Core_RequestRender();
	}
	Preferences_WhitelistTooltipAndContextMenu("whitelistEntryAddrContextMenu", bRemove);
	PopItemWidth();
	NextColumn();
	PushItemWidth(-1.0f);
	InputText("##applicationName", &entry.applicationName, kBBSize_ApplicationName, flags);
	Fonts_CacheGlyphs(sb_get(&entry.applicationName));
	if(IsItemActive() && Imgui_Core_HasFocus()) {
		Imgui_Core_RequestRender();
	}
	Preferences_WhitelistTooltipAndContextMenu("whitelistEntryAppContextMenu", bRemove);
	PopItemWidth();
	NextColumn();
	PushItemWidth(-1.0f);
	InputText("##comment", &entry.comment, 128, flags);
	Fonts_CacheGlyphs(sb_get(&entry.comment));
	if(IsItemActive() && Imgui_Core_HasFocus()) {
		Imgui_Core_RequestRender();
	}
	Preferences_WhitelistTooltipAndContextMenu("whitelistEntryCommentContextMenu", bRemove);
	PopItemWidth();
	NextColumn();

	if(Button(" ^ ")) {
		if(index > 0) {
			whitelist_move_entry(&whitelist, index, index - 1);
		}
	}
	SameLine();
	if(Button(" v ")) {
		if(index + 1 < whitelist.count) {
			whitelist_move_entry(&whitelist, index, index + 1);
		}
	}
	NextColumn();

	PopID();

	if(bRemove) {
		configWhitelistEntry_reset(whitelist.data + index);
		bba_erase(whitelist, index);
	}
}

static void Preferences_OpenTargetEntry(u32 index, openTargetList_t &openTargets)
{
	bool bRemove = false;

	openTargetEntry_t &entry = openTargets.data[index];
	PushID((int)index);

	ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
	PushItemWidth(-1.0f);
	InputText("##name", &entry.displayName, kBBSize_ApplicationName, flags);
	Fonts_CacheGlyphs(sb_get(&entry.displayName));
	if(IsItemActive() && Imgui_Core_HasFocus()) {
		Imgui_Core_RequestRender();
	}
	if(IsTooltipActive(&s_preferencesConfig.tooltips) && (entry.displayName.data || entry.commandLine.data)) {
		SetTooltip("%s\n%s", sb_get(&entry.displayName), sb_get(&entry.commandLine));
	}
	if(ImGui::BeginPopupContextItem("openTargetNameContextMenu")) {
		if(ImGui::Selectable("Delete row")) {
			bRemove = true;
		}
		ImGui::EndPopup();
	}
	PopItemWidth();
	NextColumn();
	PushItemWidth(-1.0f);
	InputText("##commandLine", &entry.commandLine, kBBSize_MaxPath, flags);
	Fonts_CacheGlyphs(sb_get(&entry.commandLine));
	if(IsItemActive() && Imgui_Core_HasFocus()) {
		Imgui_Core_RequestRender();
	}
	if(IsTooltipActive(&s_preferencesConfig.tooltips) && (entry.displayName.data || entry.commandLine.data)) {
		SetTooltip("%s\n%s", sb_get(&entry.displayName), sb_get(&entry.commandLine));
	}
	if(ImGui::BeginPopupContextItem("openTargetCommandLineContextMenu")) {
		if(ImGui::Selectable("Delete row")) {
			bRemove = true;
		}
		ImGui::EndPopup();
	}
	PopItemWidth();
	NextColumn();

	if(Button(" ^ ")) {
		if(index > 0) {
			open_target_move_entry(&openTargets, index, index - 1);
		}
	}
	SameLine();
	if(Button(" v ")) {
		if(index + 1 < openTargets.count) {
			open_target_move_entry(&openTargets, index, index + 1);
		}
	}
	NextColumn();

	PopID();

	if(bRemove) {
		bba_erase(openTargets, index);
	}
}

static void Preferences_PathFixupEntry(u32 index, pathFixupList_t &pathFixups)
{
	bool bRemove = false;

	pathFixupEntry_t &entry = pathFixups.data[index];
	PushID((int)index);

	ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
	PushItemWidth(-1.0f);
	InputText("##src", &entry.src, kBBSize_MaxPath, flags);
	Fonts_CacheGlyphs(sb_get(&entry.src));
	if(IsItemActive() && Imgui_Core_HasFocus()) {
		Imgui_Core_RequestRender();
	}
	if(IsTooltipActive(&s_preferencesConfig.tooltips) && (entry.src.data || entry.dst.data)) {
		SetTooltip("%s\n%s", sb_get(&entry.src), sb_get(&entry.dst));
	}
	if(ImGui::BeginPopupContextItem("pathFixupSrcContextMenu")) {
		if(ImGui::Selectable("Delete row")) {
			bRemove = true;
		}
		ImGui::EndPopup();
	}
	PopItemWidth();
	NextColumn();
	PushItemWidth(-1.0f);
	InputText("##dst", &entry.dst, kBBSize_MaxPath, flags);
	Fonts_CacheGlyphs(sb_get(&entry.dst));
	if(IsItemActive() && Imgui_Core_HasFocus()) {
		Imgui_Core_RequestRender();
	}
	if(IsTooltipActive(&s_preferencesConfig.tooltips) && (entry.src.data || entry.dst.data)) {
		SetTooltip("%s\n%s", sb_get(&entry.src), sb_get(&entry.dst));
	}
	if(ImGui::BeginPopupContextItem("pathFixupDstContextMenu")) {
		if(ImGui::Selectable("Delete row")) {
			bRemove = true;
		}
		ImGui::EndPopup();
	}
	PopItemWidth();
	NextColumn();

	if(Button(" ^ ")) {
		if(index > 0) {
			path_fixup_move_entry(&pathFixups, index, index - 1);
		}
	}
	SameLine();
	if(Button(" v ")) {
		if(index + 1 < pathFixups.count) {
			path_fixup_move_entry(&pathFixups, index, index + 1);
		}
	}
	NextColumn();

	PopID();

	if(bRemove) {
		bba_erase(pathFixups, index);
	}
}

void UIConfig_Open(config_t *config)
{
	if(s_preferencesValid) {
		config_reset(&s_preferencesConfig);
		s_preferencesValid = false;
	}

	s_preferencesConfig = config_clone(config);
	s_preferencesValid = true;
	s_preferencesOpen = true;
	s_preferencesAdvanced = false;
	s_preferencesNeedFocus = true;
}

void UIConfig_Reset()
{
	if(s_preferencesValid) {
		config_reset(&s_preferencesConfig);
		s_preferencesValid = false;
		s_preferencesOpen = false;
	}
}

bool UIConfig_IsOpen()
{
	return s_preferencesOpen;
}

static const char *s_colorschemes[] = {
	"ImGui Dark",
	"Light",
	"Classic",
	"Visual Studio Dark",
	"Windows",
};

void UIConfig_ApplyColorscheme(config_t *config)
{
	if(!config) {
		config = &g_config;
	}
	const char *colorscheme = sb_get(&config->colorscheme);
	Style_Apply(colorscheme);
	Style_ReadConfig(colorscheme);
}

static const char *g_colorUsageNames[] = {
	"Full",
	"Background as Foreground",
	"Ignore Background",
	"Ignore All"
};
BB_CTASSERT(BB_ARRAYSIZE(g_colorUsageNames) == kConfigColors_Count);

void UIConfig_Update(config_t *config)
{
	if(!s_preferencesOpen)
		return;

	ImVec2 viewportPos(0.0f, 0.0f);
	ImGuiViewport *viewport = ImGui::GetViewportForWindow("Config");
	if(viewport) {
		viewportPos.x += viewport->Pos.x;
		viewportPos.y += viewport->Pos.y;
	}

	float UIRecordings_Width();
	float startY = ImGui::GetFrameHeight();
	ImGuiIO &io = ImGui::GetIO();
	SetNextWindowSize(ImVec2(io.DisplaySize.x - UIRecordings_Width(), io.DisplaySize.y - startY), ImGuiCond_Always);
	SetNextWindowPos(ImVec2(viewportPos.x, viewportPos.y + startY), ImGuiCond_Always);

	if(Begin("Config", &s_preferencesOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
		if(ImGui::CollapsingHeader("Interface", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::BeginGroup();
			Checkbox("Auto-tile views", &s_preferencesConfig.autoTileViews);
			InputFloat("Double-click seconds", &s_preferencesConfig.doubleClickSeconds);

			Checkbox("Alternate row background colors", &s_preferencesConfig.alternateRowBackground);
			SameLine(0.0f, 40.0f * g_config.dpiScale);
			int colorUsageIndex = s_preferencesConfig.logColorUsage;
			if(ImGui::Combo("Log colors", &colorUsageIndex, g_colorUsageNames, BB_ARRAYSIZE(g_colorUsageNames))) {
				if(colorUsageIndex >= 0 && colorUsageIndex < BB_ARRAYSIZE(g_colorUsageNames)) {
					s_preferencesConfig.logColorUsage = (configColorUsage)colorUsageIndex;
				}
			}

			int colorschemeIndex = -1;
			for(int i = 0; i < BB_ARRAYSIZE(s_colorschemes); ++i) {
				if(!strcmp(sb_get(&s_preferencesConfig.colorscheme), s_colorschemes[i])) {
					colorschemeIndex = i;
					break;
				}
			}
			if(ImGui::Combo("Colorscheme", &colorschemeIndex, s_colorschemes, BB_ARRAYSIZE(s_colorschemes))) {
				if(colorschemeIndex >= 0 && colorschemeIndex < BB_ARRAYSIZE(s_colorschemes)) {
					sb_reset(&s_preferencesConfig.colorscheme);
					sb_append(&s_preferencesConfig.colorscheme, s_colorschemes[colorschemeIndex]);
					UIConfig_ApplyColorscheme(&s_preferencesConfig);
				}
			}
			ImGui::SameLine(0.0f, 40.0f * g_config.dpiScale);
			ImGui::Checkbox("Log text shadows", &s_preferencesConfig.textShadows);
			ImGui::Checkbox("DPI Aware", &s_preferencesConfig.dpiAware);
			if(IsTooltipActive(&s_preferencesConfig.tooltips)) {
				ImGui::SetTooltip("Requires restart.  Default font is not recommended if DPI Aware.");
			}
			ImGui::EndGroup();
			ImGui::SameLine(0.0f, 20.0f * g_config.dpiScale);
			ImGui::BeginGroup();
			Checkbox("Tooltips", &s_preferencesConfig.tooltips.enabled);
			if(IsTooltipActive(&s_preferencesConfig.tooltips)) {
				ImGui::SetTooltip("Show tooltips");
			}
			Checkbox("Over log text", &s_preferencesConfig.tooltips.overText);
			if(IsTooltipActive(&s_preferencesConfig.tooltips)) {
				ImGui::SetTooltip("Show tooltips over log text");
			}
			Checkbox("Over log misc columns", &s_preferencesConfig.tooltips.overMisc);
			if(IsTooltipActive(&s_preferencesConfig.tooltips)) {
				ImGui::SetTooltip("Show tooltips over misc log columns");
			}
			Checkbox("Only over selected log lines", &s_preferencesConfig.tooltips.onlyOverSelected);
			if(IsTooltipActive(&s_preferencesConfig.tooltips)) {
				ImGui::SetTooltip("Show tooltips only over selected log lines");
			}
			InputFloat("Delay seconds", &s_preferencesConfig.tooltips.delay);
			if(IsTooltipActive(&s_preferencesConfig.tooltips)) {
				ImGui::SetTooltip("Delay before showing tooltips");
			}
			ImGui::EndGroup();
		}
		if(s_preferencesAdvanced && ImGui::CollapsingHeader("Open Targets", ImGuiTreeNodeFlags_DefaultOpen)) {
			Columns(3, "opentargetscolumns");
			SetColumnOffset(1, 155.0f * g_config.dpiScale);
			SetColumnOffset(2, 480.0f * g_config.dpiScale);
			Separator();
			Text("Name");
			NextColumn();
			Text("Command Line");
			NextColumn();
			Text("Move");
			NextColumn();
			Separator();
			for(u32 i = 0; i < s_preferencesConfig.openTargets.count; ++i) {
				Preferences_OpenTargetEntry(i, s_preferencesConfig.openTargets);
			}
			Columns(1);
			Separator();
			if(Button("Add Entry###Add OpenTarget")) {
				bba_add(s_preferencesConfig.openTargets, 1);
			}
		}
		if(s_preferencesAdvanced && ImGui::CollapsingHeader("Path Fixups", ImGuiTreeNodeFlags_DefaultOpen)) {
			Columns(3, "pathfixupscolumns");
			SetColumnOffset(1, 240.0f * g_config.dpiScale);
			SetColumnOffset(2, 480.0f * g_config.dpiScale);
			Separator();
			Text("Original Path");
			NextColumn();
			Text("Local Path");
			NextColumn();
			Text("Move");
			NextColumn();
			Separator();
			for(u32 i = 0; i < s_preferencesConfig.pathFixups.count; ++i) {
				Preferences_PathFixupEntry(i, s_preferencesConfig.pathFixups);
			}
			Columns(1);
			Separator();
			if(Button("Add Entry###Add PathFixup")) {
				bba_add(s_preferencesConfig.pathFixups, 1);
			}
		}
		if(ImGui::CollapsingHeader("Whitelist", ImGuiTreeNodeFlags_DefaultOpen)) {
			Columns(5, "whitelistcolumns");
			SetColumnOffset(1, 80.0f * g_config.dpiScale);
			SetColumnOffset(2, 280.0f * g_config.dpiScale);
			SetColumnOffset(3, 480.0f * g_config.dpiScale);
			SetColumnOffset(4, 680.0f * g_config.dpiScale);
			Separator();
			Text("Allow");
			NextColumn();
			Text("Address");
			NextColumn();
			Text("Application Name");
			NextColumn();
			Text("Comment");
			NextColumn();
			Text("Move");
			NextColumn();
			Separator();
			for(u32 i = 0; i < s_preferencesConfig.whitelist.count; ++i) {
				Preferences_WhitelistEntry(i, s_preferencesConfig.whitelist);
			}
			Columns(1);
			Separator();
			if(Button("Add Entry###Add Whitelist")) {
				configWhitelistEntry_t *entry = bba_add(s_preferencesConfig.whitelist, 1);
				if(entry) {
					entry->allow = true;
					sb_append(&entry->addressPlusMask, "localhost");
				}
			}
		}
		ImFont *uiFont = ImGui::GetFont();
		ImVec2 fontSizeDim = uiFont->CalcTextSizeA(uiFont->FontSize, FLT_MAX, 0.0f, "100 _+__-_ size");
		if(ImGui::CollapsingHeader("Font", ImGuiTreeNodeFlags_DefaultOpen)) {
			BeginGroup();
			PushID("UIFont");
			Checkbox("Custom UI Font", &s_preferencesConfig.uiFontConfig.enabled);
			if(s_preferencesConfig.uiFontConfig.enabled) {
				int val = (int)s_preferencesConfig.uiFontConfig.size;
				PushItemWidth(fontSizeDim.x);
				InputInt("size", &val, 1, 10);
				PopItemWidth();
				val = BB_CLAMP(val, 1, 1024);
				s_preferencesConfig.uiFontConfig.size = (u32)val;
				Text("Path:");
				SameLine();
				PushItemWidth(300.0f * g_config.dpiScale);
				ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
				InputText("##path", &s_preferencesConfig.uiFontConfig.path, kBBSize_MaxPath, flags);
				Fonts_CacheGlyphs(sb_get(&s_preferencesConfig.uiFontConfig.path));
				if(IsItemActive() && Imgui_Core_HasFocus()) {
					Imgui_Core_RequestRender();
				}
				PopItemWidth();
			}
			PopID();
			EndGroup();
			SameLine();
			BeginGroup();
			PushID("LogFont");
			Checkbox("Custom Log Font", &s_preferencesConfig.logFontConfig.enabled);
			if(s_preferencesConfig.logFontConfig.enabled) {
				int val = (int)s_preferencesConfig.logFontConfig.size;
				PushItemWidth(fontSizeDim.x);
				InputInt("size", &val, 1, 10);
				PopItemWidth();
				val = BB_CLAMP(val, 1, 1024);
				s_preferencesConfig.logFontConfig.size = (u32)val;
				Text("Path:");
				SameLine();
				PushItemWidth(300.0f * g_config.dpiScale);
				ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
				InputText("##path", &s_preferencesConfig.logFontConfig.path, kBBSize_MaxPath, flags);
				Fonts_CacheGlyphs(sb_get(&s_preferencesConfig.logFontConfig.path));
				if(IsItemActive() && Imgui_Core_HasFocus()) {
					Imgui_Core_RequestRender();
				}
				PopItemWidth();
			}
			PopID();
			EndGroup();
		}
		if(ImGui::CollapsingHeader("Miscellaneous", ImGuiTreeNodeFlags_DefaultOpen)) {
			int val = (int)s_preferencesConfig.autoDeleteAfterDays;
			ImGui::Text("Auto-delete old sessions after");
			SameLine();
			InputInt("days (0 disables)", &val, 1, 10);
			val = BB_CLAMP(val, 0, 9999);
			s_preferencesConfig.autoDeleteAfterDays = (u32)val;
			Checkbox("Auto-close all applications instead of just the one starting up", &s_preferencesConfig.autoCloseAll);
			Checkbox("Show advanced config", &s_preferencesAdvanced);
		}
		Separator();
		if(Button("Ok")) {
			WINDOWPLACEMENT wp = config->wp;
			config_t tmp = *config;
			*config = s_preferencesConfig;
			config->wp = wp;
			s_preferencesConfig = tmp;
			s_preferencesOpen = false;
			config_push_whitelist(&config->whitelist);
			GetIO().MouseDoubleClickTime = config->doubleClickSeconds;
			Fonts_ClearFonts();
			Fonts_AddFont(config->uiFontConfig);
			Fonts_AddFont(config->logFontConfig);
			Imgui_Core_SetColorScheme(sb_get(&config->colorscheme));
			Imgui_Core_SetTextShadows(g_config.textShadows);
		}
		SameLine();
		if(Button("Cancel")) {
			s_preferencesOpen = false;
			Imgui_Core_QueueUpdateDpiDependentResources();
			Style_Apply(Imgui_Core_GetColorScheme());
			Style_ReadConfig(Imgui_Core_GetColorScheme());
		}
	}
	End();
	if(!s_preferencesOpen && s_preferencesValid) {
		config_reset(&s_preferencesConfig);
		s_preferencesValid = false;
	}
}
