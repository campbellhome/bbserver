// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "ui_config.h"
#include "bb_array.h"
#include "bb_colors.h"
#include "bb_string.h"
#include "device_codes.h"
#include "fonts.h"
#include "imgui_core.h"
#include "imgui_themes.h"
#include "imgui_tooltips.h"
#include "imgui_utils.h"
#include "theme_config.h"
#include "ui_recordings.h"

static config_t s_preferencesConfig;
static sb_t s_tmpStr;
bool s_preferencesValid;
bool s_preferencesOpen;
bool s_preferencesAdvanced;
bool s_preferencesNeedFocus;

using namespace ImGui;

extern "C" void configWhitelistEntry_reset(configWhitelistEntry_t* val);

static void Preferences_WhitelistTooltipAndContextMenu(const char* contextMenuName, bool& bRemove)
{
	if (ImGui::BeginPopupContextItem(contextMenuName))
	{
		if (ImGui::Selectable("Delete row"))
		{
			bRemove = true;
		}
		ImGui::EndPopup();
	}
}

static void Preferences_WhitelistEntry(u32 index, configWhitelist_t& whitelist)
{
	bool bRemove = false;

	configWhitelistEntry_t& entry = whitelist.data[index];
	PushID((int)index);

	Checkbox("allow", &entry.allow);
	Preferences_WhitelistTooltipAndContextMenu("whitelistEntryAllowContextMenu", bRemove);
	NextColumn();

	ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
	PushItemWidth(-1.0f);
	InputText("##addr", &entry.addressPlusMask, 128, flags);
	Fonts_CacheGlyphs(sb_get(&entry.addressPlusMask));
	if (IsItemActive() && Imgui_Core_HasFocus())
	{
		Imgui_Core_RequestRender();
	}
	Preferences_WhitelistTooltipAndContextMenu("whitelistEntryAddrContextMenu", bRemove);
	PopItemWidth();
	NextColumn();
	PushItemWidth(-1.0f);
	InputText("##applicationName", &entry.applicationName, kBBSize_ApplicationName, flags);
	Fonts_CacheGlyphs(sb_get(&entry.applicationName));
	if (IsItemActive() && Imgui_Core_HasFocus())
	{
		Imgui_Core_RequestRender();
	}
	Preferences_WhitelistTooltipAndContextMenu("whitelistEntryAppContextMenu", bRemove);
	PopItemWidth();
	NextColumn();
	PushItemWidth(-1.0f);
	InputText("##comment", &entry.comment, 128, flags);
	Fonts_CacheGlyphs(sb_get(&entry.comment));
	if (IsItemActive() && Imgui_Core_HasFocus())
	{
		Imgui_Core_RequestRender();
	}
	Preferences_WhitelistTooltipAndContextMenu("whitelistEntryCommentContextMenu", bRemove);
	PopItemWidth();
	NextColumn();

	if (Button(" ^ "))
	{
		if (index > 0)
		{
			whitelist_move_entry(&whitelist, index, index - 1);
		}
	}
	SameLine();
	if (Button(" v "))
	{
		if (index + 1 < whitelist.count)
		{
			whitelist_move_entry(&whitelist, index, index + 1);
		}
	}
	NextColumn();

	PopID();

	if (bRemove)
	{
		configWhitelistEntry_reset(whitelist.data + index);
		bba_erase(whitelist, index);
	}
}

static void Preferences_OpenTargetEntry(u32 index, openTargetList_t& openTargets)
{
	bool bRemove = false;

	openTargetEntry_t& entry = openTargets.data[index];
	PushID((int)index);

	ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
	PushItemWidth(-1.0f);
	InputText("##name", &entry.displayName, kBBSize_ApplicationName, flags);
	Fonts_CacheGlyphs(sb_get(&entry.displayName));
	if (IsItemActive() && Imgui_Core_HasFocus())
	{
		Imgui_Core_RequestRender();
	}
	if (IsTooltipActive(&s_preferencesConfig.tooltips) && (entry.displayName.data || entry.commandLine.data))
	{
		SetTooltip("%s\n%s", sb_get(&entry.displayName), sb_get(&entry.commandLine));
	}
	if (ImGui::BeginPopupContextItem("openTargetNameContextMenu"))
	{
		if (ImGui::Selectable("Delete row"))
		{
			bRemove = true;
		}
		ImGui::EndPopup();
	}
	PopItemWidth();
	NextColumn();
	PushItemWidth(-1.0f);
	InputText("##commandLine", &entry.commandLine, kBBSize_MaxPath, flags);
	Fonts_CacheGlyphs(sb_get(&entry.commandLine));
	if (IsItemActive() && Imgui_Core_HasFocus())
	{
		Imgui_Core_RequestRender();
	}
	if (IsTooltipActive(&s_preferencesConfig.tooltips) && (entry.displayName.data || entry.commandLine.data))
	{
		SetTooltip("%s\n%s", sb_get(&entry.displayName), sb_get(&entry.commandLine));
	}
	if (ImGui::BeginPopupContextItem("openTargetCommandLineContextMenu"))
	{
		if (ImGui::Selectable("Delete row"))
		{
			bRemove = true;
		}
		ImGui::EndPopup();
	}
	PopItemWidth();
	NextColumn();

	if (Button(" ^ "))
	{
		if (index > 0)
		{
			open_target_move_entry(&openTargets, index, index - 1);
		}
	}
	SameLine();
	if (Button(" v "))
	{
		if (index + 1 < openTargets.count)
		{
			open_target_move_entry(&openTargets, index, index + 1);
		}
	}
	NextColumn();

	PopID();

	if (bRemove)
	{
		bba_erase(openTargets, index);
	}
}

static void Preferences_PathFixupEntry(u32 index, pathFixupList_t& pathFixups)
{
	bool bRemove = false;

	pathFixupEntry_t& entry = pathFixups.data[index];
	PushID((int)index);

	ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
	PushItemWidth(-1.0f);
	InputText("##src", &entry.src, kBBSize_MaxPath, flags);
	Fonts_CacheGlyphs(sb_get(&entry.src));
	if (IsItemActive() && Imgui_Core_HasFocus())
	{
		Imgui_Core_RequestRender();
	}
	if (IsTooltipActive(&s_preferencesConfig.tooltips) && (entry.src.data || entry.dst.data))
	{
		SetTooltip("%s\n%s", sb_get(&entry.src), sb_get(&entry.dst));
	}
	if (ImGui::BeginPopupContextItem("pathFixupSrcContextMenu"))
	{
		if (ImGui::Selectable("Delete row"))
		{
			bRemove = true;
		}
		ImGui::EndPopup();
	}
	PopItemWidth();
	NextColumn();
	PushItemWidth(-1.0f);
	InputText("##dst", &entry.dst, kBBSize_MaxPath, flags);
	Fonts_CacheGlyphs(sb_get(&entry.dst));
	if (IsItemActive() && Imgui_Core_HasFocus())
	{
		Imgui_Core_RequestRender();
	}
	if (IsTooltipActive(&s_preferencesConfig.tooltips) && (entry.src.data || entry.dst.data))
	{
		SetTooltip("%s\n%s", sb_get(&entry.src), sb_get(&entry.dst));
	}
	if (ImGui::BeginPopupContextItem("pathFixupDstContextMenu"))
	{
		if (ImGui::Selectable("Delete row"))
		{
			bRemove = true;
		}
		ImGui::EndPopup();
	}
	PopItemWidth();
	NextColumn();

	if (Button(" ^ "))
	{
		if (index > 0)
		{
			path_fixup_move_entry(&pathFixups, index, index - 1);
		}
	}
	SameLine();
	if (Button(" v "))
	{
		if (index + 1 < pathFixups.count)
		{
			path_fixup_move_entry(&pathFixups, index, index + 1);
		}
	}
	NextColumn();

	PopID();

	if (bRemove)
	{
		bba_erase(pathFixups, index);
	}
}

static void Preferences_MaxRecordingsEntry(u32 index, config_max_recordings_t& pathFixups)
{
	bool bRemove = false;

	config_max_recordings_entry_t& entry = pathFixups.data[index];
	PushID((int)index);

	ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
	PushItemWidth(-1.0f);
	InputText("##filter", &entry.filter, kBBSize_MaxPath, flags);
	Fonts_CacheGlyphs(sb_get(&entry.filter));
	if (IsItemActive() && Imgui_Core_HasFocus())
	{
		Imgui_Core_RequestRender();
	}
	if (IsTooltipActive(&s_preferencesConfig.tooltips) && entry.filter.data)
	{
		SetTooltip("%s\nAllowed: %u", sb_get(&entry.filter), entry.allowed);
	}
	if (ImGui::BeginPopupContextItem("maxRecordingsFilterContextMenu"))
	{
		if (ImGui::Selectable("Delete row"))
		{
			bRemove = true;
		}
		ImGui::EndPopup();
	}
	PopItemWidth();
	NextColumn();
	int val = (int)entry.allowed;
	ImGui::Text("Allowed:");
	SameLine();
	PushItemWidth(100 * Imgui_Core_GetDpiScale());
	InputInt("##AllowedInput", &val, 0, 9999);
	PopItemWidth();
	val = BB_CLAMP(val, 0, 9999);
	entry.allowed = (u32)val;
	if (IsItemActive() && Imgui_Core_HasFocus())
	{
		Imgui_Core_RequestRender();
	}
	if (IsTooltipActive(&s_preferencesConfig.tooltips) && entry.filter.data)
	{
		SetTooltip("%s\nAllowed: %u", sb_get(&entry.filter), entry.allowed);
	}
	if (ImGui::BeginPopupContextItem("maxRecordingsAllowedContextMenu"))
	{
		if (ImGui::Selectable("Delete row"))
		{
			bRemove = true;
		}
		ImGui::EndPopup();
	}
	NextColumn();

	if (Button(" ^ "))
	{
		if (index > 0)
		{
			config_max_recordings_move_entry(&pathFixups, index, index - 1);
		}
	}
	SameLine();
	if (Button(" v "))
	{
		if (index + 1 < pathFixups.count)
		{
			config_max_recordings_move_entry(&pathFixups, index, index + 1);
		}
	}
	NextColumn();

	PopID();

	if (bRemove)
	{
		bba_erase(pathFixups, index);
	}
}

void UIConfig_Open(config_t* config)
{
	if (s_preferencesValid)
	{
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
	if (s_preferencesValid)
	{
		config_reset(&s_preferencesConfig);
		s_preferencesValid = false;
		s_preferencesOpen = false;
	}
	sb_reset(&s_tmpStr);
}

bool UIConfig_IsOpen()
{
	return s_preferencesOpen;
}

static const char* s_colorschemes[] = {
	"ImGui Dark",
	"Light",
	"Classic",
	"Visual Studio Dark",
	"Windows",
};

void UIConfig_ApplyColorscheme(config_t* config)
{
	if (!config)
	{
		config = &g_config;
	}
	const char* colorscheme = sb_get(&config->colorscheme);
	Style_Apply(colorscheme);
	Style_ReadConfig(colorscheme);
	ImGui::SetTextShadowColor(MakeColor(kStyleColor_TextShadow));
}

static const char* g_colorUsageNames[] = {
	"Full",
	"Background as Foreground",
	"Ignore Background",
	"Ignore All"
};
BB_CTASSERT(BB_ARRAYSIZE(g_colorUsageNames) == kConfigColors_Count);

static const char* g_viewTileModeNames[] = {
	"Auto",
	"Prefer Columns",
	"Prefer Rows",
	"Columns",
	"Rows",
	"None",
};
BB_CTASSERT(BB_ARRAYSIZE(g_viewTileModeNames) == kViewTileMode_Count);

static const char* g_listenProtocolNames[] = {
	"Unknown",
	"IPv4",
	"IPv6",
	"IPv4 & IPv6",
};
BB_CTASSERT(BB_ARRAYSIZE(g_listenProtocolNames) == kConfigListenProtocol_Count);

void UIConfig_Update(config_t* config)
{
	if (!s_preferencesOpen)
		return;

	ImVec2 viewportPos(0.0f, 0.0f);
	ImGuiViewport* viewport = ImGui::GetViewportForWindow("Config");
	if (viewport)
	{
		viewportPos.x += viewport->Pos.x;
		viewportPos.y += viewport->Pos.y;
	}

	float UIRecordings_Width();
	float startY = ImGui::GetFrameHeight();
	ImGuiIO& io = ImGui::GetIO();
	SetNextWindowSize(ImVec2(io.DisplaySize.x - UIRecordings_Width(), io.DisplaySize.y - startY), ImGuiCond_Appearing);
	SetNextWindowPos(ImVec2(viewportPos.x, viewportPos.y + startY), ImGuiCond_Appearing);
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_HorizontalScrollbar;
	if (Begin("Config", &s_preferencesOpen, windowFlags))
	{
		if (ImGui::CollapsingHeader("Interface", ImGuiTreeNodeFlags_DefaultOpen))
		{
			PushItemWidth(200 * Imgui_Core_GetDpiScale());
			ImGui::BeginGroup();
			ImGui::Combo("View tile mode", (s32*)&s_preferencesConfig.viewTileMode, g_viewTileModeNames, BB_ARRAYSIZE(g_viewTileModeNames));
			InputFloat("Double-click seconds", &s_preferencesConfig.doubleClickSeconds);

			Checkbox("Alternate row background colors", &s_preferencesConfig.alternateRowBackground);
			SameLine(0.0f, 40.0f * Imgui_Core_GetDpiScale());
			int colorUsageIndex = s_preferencesConfig.logColorUsage;
			if (ImGui::Combo("Log colors", &colorUsageIndex, g_colorUsageNames, BB_ARRAYSIZE(g_colorUsageNames)))
			{
				if (colorUsageIndex >= 0 && colorUsageIndex < BB_ARRAYSIZE(g_colorUsageNames))
				{
					s_preferencesConfig.logColorUsage = (configColorUsage)colorUsageIndex;
				}
			}

			int colorschemeIndex = -1;
			for (int i = 0; i < BB_ARRAYSIZE(s_colorschemes); ++i)
			{
				if (!strcmp(sb_get(&s_preferencesConfig.colorscheme), s_colorschemes[i]))
				{
					colorschemeIndex = i;
					break;
				}
			}
			if (ImGui::Combo("Colorscheme", &colorschemeIndex, s_colorschemes, BB_ARRAYSIZE(s_colorschemes)))
			{
				if (colorschemeIndex >= 0 && colorschemeIndex < BB_ARRAYSIZE(s_colorschemes))
				{
					sb_reset(&s_preferencesConfig.colorscheme);
					sb_append(&s_preferencesConfig.colorscheme, s_colorschemes[colorschemeIndex]);
					UIConfig_ApplyColorscheme(&s_preferencesConfig);
				}
			}
			ImGui::SameLine(0.0f, 40.0f * Imgui_Core_GetDpiScale());
			ImGui::Checkbox("Log text shadows", &s_preferencesConfig.textShadows);
			ImGui::Checkbox("DPI Aware", &s_preferencesConfig.dpiAware);
			if (IsTooltipActive(&s_preferencesConfig.tooltips))
			{
				ImGui::SetTooltip("Requires restart.  Default font is not recommended if DPI Aware.");
			}
			ImGui::Checkbox("Ctrl+Scrollwheel scales UI", &s_preferencesConfig.dpiScrollwheel);
			ImGui::EndGroup();
			PopItemWidth();
			ImGui::SameLine(0.0f, 20.0f * Imgui_Core_GetDpiScale());
			PushItemWidth(200 * Imgui_Core_GetDpiScale());
			ImGui::BeginGroup();
			Checkbox("Tooltips", &s_preferencesConfig.tooltips.enabled);
			if (IsTooltipActive(&s_preferencesConfig.tooltips))
			{
				ImGui::SetTooltip("Show tooltips");
			}
			Checkbox("Over log text", &s_preferencesConfig.tooltips.overText);
			if (IsTooltipActive(&s_preferencesConfig.tooltips))
			{
				ImGui::SetTooltip("Show tooltips over log text");
			}
			Checkbox("Over log misc columns", &s_preferencesConfig.tooltips.overMisc);
			if (IsTooltipActive(&s_preferencesConfig.tooltips))
			{
				ImGui::SetTooltip("Show tooltips over misc log columns");
			}
			Checkbox("Only over selected log lines", &s_preferencesConfig.tooltips.onlyOverSelected);
			if (IsTooltipActive(&s_preferencesConfig.tooltips))
			{
				ImGui::SetTooltip("Show tooltips only over selected log lines");
			}
			InputFloat("Delay seconds", &s_preferencesConfig.tooltips.delay);
			if (IsTooltipActive(&s_preferencesConfig.tooltips))
			{
				ImGui::SetTooltip("Delay before showing tooltips");
			}
			ImGui::EndGroup();
			PopItemWidth();
			PushItemWidth(400 * Imgui_Core_GetDpiScale());
			ImGui::SliderInt("Scrollbar Size", &s_preferencesConfig.sizes.scrollbarSize, 0, 20);
			if (IsTooltipActive(&s_preferencesConfig.tooltips))
			{
				ImGui::SetTooltip("Scrollbar width/height (0 for default)");
			}
			ImGui::SameLine();
			ImGui::SliderInt("Resize Bar Size", &s_preferencesConfig.sizes.resizeBarSize, 0, 10);
			if (IsTooltipActive(&s_preferencesConfig.tooltips))
			{
				ImGui::SetTooltip("Resize bar width/height (0 for default)");
			}
			PopItemWidth();
		}
		ImFont* uiFont = ImGui::GetFont();
		ImVec2 fontSizeDim = uiFont->CalcTextSizeA(uiFont->FontSize, FLT_MAX, 0.0f, "100 _+__-_ size");
		if (ImGui::CollapsingHeader("Font", ImGuiTreeNodeFlags_DefaultOpen))
		{
			BeginGroup();
			PushID("UIFont");
			Checkbox("Custom UI Font", &s_preferencesConfig.uiFontConfig.enabled);
			if (s_preferencesConfig.uiFontConfig.enabled)
			{
				int val = (int)s_preferencesConfig.uiFontConfig.size;
				PushItemWidth(fontSizeDim.x);
				InputInt("size", &val, 1, 10);
				PopItemWidth();
				val = BB_CLAMP(val, 1, 1024);
				s_preferencesConfig.uiFontConfig.size = (u32)val;
				Text("Path:");
				SameLine();
				PushItemWidth(300.0f * Imgui_Core_GetDpiScale());
				ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
				InputText("##path", &s_preferencesConfig.uiFontConfig.path, kBBSize_MaxPath, flags);
				Fonts_CacheGlyphs(sb_get(&s_preferencesConfig.uiFontConfig.path));
				if (IsItemActive() && Imgui_Core_HasFocus())
				{
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
			if (s_preferencesConfig.logFontConfig.enabled)
			{
				int val = (int)s_preferencesConfig.logFontConfig.size;
				PushItemWidth(fontSizeDim.x);
				InputInt("size", &val, 1, 10);
				PopItemWidth();
				val = BB_CLAMP(val, 1, 1024);
				s_preferencesConfig.logFontConfig.size = (u32)val;
				Text("Path:");
				SameLine();
				PushItemWidth(300.0f * Imgui_Core_GetDpiScale());
				ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue;
				InputText("##path", &s_preferencesConfig.logFontConfig.path, kBBSize_MaxPath, flags);
				Fonts_CacheGlyphs(sb_get(&s_preferencesConfig.logFontConfig.path));
				if (IsItemActive() && Imgui_Core_HasFocus())
				{
					Imgui_Core_RequestRender();
				}
				PopItemWidth();
			}
			PopID();
			EndGroup();
		}
		if (s_preferencesAdvanced && ImGui::CollapsingHeader("Open Targets", ImGuiTreeNodeFlags_DefaultOpen))
		{
			Columns(3, "opentargetscolumns");
			SetColumnOffset(1, 155.0f * Imgui_Core_GetDpiScale());
			SetColumnOffset(2, 480.0f * Imgui_Core_GetDpiScale());
			Separator();
			Text("Name");
			NextColumn();
			Text("Command Line");
			NextColumn();
			Text("Move");
			NextColumn();
			Separator();
			for (u32 i = 0; i < s_preferencesConfig.openTargets.count; ++i)
			{
				Preferences_OpenTargetEntry(i, s_preferencesConfig.openTargets);
			}
			Columns(1);
			Separator();
			if (Button("Add Entry###Add OpenTarget"))
			{
				bba_add(s_preferencesConfig.openTargets, 1);
			}
		}
		if (s_preferencesAdvanced && ImGui::CollapsingHeader("Path Fixups", ImGuiTreeNodeFlags_DefaultOpen))
		{
			Columns(3, "pathfixupscolumns");
			SetColumnOffset(1, 240.0f * Imgui_Core_GetDpiScale());
			SetColumnOffset(2, 480.0f * Imgui_Core_GetDpiScale());
			Separator();
			Text("Original Path");
			NextColumn();
			Text("Local Path");
			NextColumn();
			Text("Move");
			NextColumn();
			Separator();
			for (u32 i = 0; i < s_preferencesConfig.pathFixups.count; ++i)
			{
				Preferences_PathFixupEntry(i, s_preferencesConfig.pathFixups);
			}
			Columns(1);
			Separator();
			if (Button("Add Entry###Add PathFixup"))
			{
				bba_add(s_preferencesConfig.pathFixups, 1);
			}
		}
		if (s_preferencesAdvanced && ImGui::CollapsingHeader("Allowed Connections", ImGuiTreeNodeFlags_DefaultOpen))
		{
			sb_clear(&s_tmpStr);
			const sbs_t* deviceCodes = deviceCodes_lock();
			sb_va(&s_tmpStr, "%u Device Codes:", deviceCodes->count);
			for (u32 i = 0; i < deviceCodes->count; ++i)
			{
				if (i)
				{
					sb_append(&s_tmpStr, ",");
				}
				sb_va(&s_tmpStr, " %s", sb_get(deviceCodes->data + i));
			}
			deviceCodes_unlock();
			TextUnformatted(sb_get(&s_tmpStr));
			Columns(5, "whitelistcolumns");
			SetColumnOffset(1, 80.0f * Imgui_Core_GetDpiScale());
			SetColumnOffset(2, 280.0f * Imgui_Core_GetDpiScale());
			SetColumnOffset(3, 480.0f * Imgui_Core_GetDpiScale());
			SetColumnOffset(4, 680.0f * Imgui_Core_GetDpiScale());
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
			for (u32 i = 0; i < s_preferencesConfig.whitelist.count; ++i)
			{
				Preferences_WhitelistEntry(i, s_preferencesConfig.whitelist);
			}
			Columns(1);
			Separator();
			if (Button("Add Entry###Add Whitelist"))
			{
				configWhitelistEntry_t* entry = bba_add(s_preferencesConfig.whitelist, 1);
				if (entry)
				{
					entry->allow = true;
					sb_append(&entry->addressPlusMask, "localhost");
				}
			}
			PushItemWidth(120 * Imgui_Core_GetDpiScale());
			if (ImGui::BeginCombo("Allowed Protocols", g_listenProtocolNames[s_preferencesConfig.listenProtocol]))
			{
				for (s32 i = 1; i < BB_ARRAYSIZE(g_listenProtocolNames); ++i)
					if (ImGui::Selectable(g_listenProtocolNames[i], s_preferencesConfig.listenProtocol == i))
					{
						s_preferencesConfig.listenProtocol = (configListenProtocol_t)i;
					}
				ImGui::EndCombo();
			}
			PopItemWidth();
		}
		if (ImGui::CollapsingHeader("Miscellaneous", ImGuiTreeNodeFlags_DefaultOpen))
		{
			int val = (int)s_preferencesConfig.autoDeleteAfterDays;
			ImGui::Text("Auto-delete old sessions after");
			SameLine();
			PushItemWidth(100 * Imgui_Core_GetDpiScale());
			InputInt("days (0 disables)", &val, 1, 10);
			PopItemWidth();
			val = BB_CLAMP(val, 0, 9999);
			s_preferencesConfig.autoDeleteAfterDays = (u32)val;

			if (s_preferencesAdvanced || s_preferencesConfig.maxRecordings.count > 0)
			{
				BeginGroup();
				Columns(3, "maxrecordingscolumns");
				SetColumnOffset(1, 240.0f * Imgui_Core_GetDpiScale());
				SetColumnOffset(2, 480.0f * Imgui_Core_GetDpiScale());
				Separator();
				Text("Application name filter");
				NextColumn();
				Text("Max recordings");
				NextColumn();
				Text("Move");
				NextColumn();
				Separator();
				for (u32 i = 0; i < s_preferencesConfig.maxRecordings.count; ++i)
				{
					Preferences_MaxRecordingsEntry(i, s_preferencesConfig.maxRecordings);
				}
				Columns(1);
				Separator();
				if (Button("Add Entry###Add MaxRecordings"))
				{
					bba_add(s_preferencesConfig.maxRecordings, 1);
				}
				EndGroup();
			}

			Checkbox("Auto-close all applications instead of just the one starting up", &s_preferencesConfig.autoCloseAll);
			SameLine();
			Checkbox("Manually opened views are marked to auto close", &s_preferencesConfig.autoCloseManual);
			Checkbox("Show advanced config", &s_preferencesAdvanced);
		}
		Separator();
		bool ok = Button("Ok");
		SameLine();
		bool apply = Button("Apply");
		if (ok || apply)
		{
			WINDOWPLACEMENT wp = *(WINDOWPLACEMENT*)&config->wp;
			config_t tmp = *config;
			*config = s_preferencesConfig;
			config->wp = *(configWindowplacement_t*)&wp;
			s_preferencesConfig = tmp;
			s_preferencesOpen = false;
			config_push_whitelist(&config->whitelist);
			GetIO().MouseDoubleClickTime = config->doubleClickSeconds;
			Fonts_ClearFonts();
			Fonts_AddFont(*(fontConfig_t*)&config->uiFontConfig);
			Fonts_AddFont(*(fontConfig_t*)&config->logFontConfig);
			Imgui_Core_SetColorScheme(sb_get(&config->colorscheme));
			Imgui_Core_SetTextShadows(config->textShadows);
			if (apply)
			{
				UIConfig_Open(config);
			}
			config_write(config);
		}
		SameLine();
		if (Button("Cancel"))
		{
			s_preferencesOpen = false;
			Imgui_Core_QueueUpdateDpiDependentResources();
			Style_Apply(Imgui_Core_GetColorScheme());
			Style_ReadConfig(Imgui_Core_GetColorScheme());
			ImGui::SetTextShadowColor(MakeColor(kStyleColor_TextShadow));
		}
	}
	End();
	if (!s_preferencesOpen && s_preferencesValid)
	{
		config_reset(&s_preferencesConfig);
		s_preferencesValid = false;
	}
}
