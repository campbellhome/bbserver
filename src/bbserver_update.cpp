// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "bbserver_update.h"
#include "bbclient/bb_log.h"
#include "devkit_autodetect.h"
#include "fonts.h"
#include "globals.h"
#include "imgui_core.h"
#include "imgui_themes.h"
#include "imgui_utils.h"
#include "mc_callstack/callstack_utils.h"
#include "message_box.h"
#include "message_queue.h"
#include "recorded_session.h"
#include "recordings.h"
#include "tasks.h"
#include "theme_config.h"
#include "ui_config.h"
#include "ui_recordings.h"
#include "ui_view.h"
#include "update.h"
#include "view.h"
#include "wrap_imgui.h"

static bool s_showImguiDemo;
static bool s_showImguiAbout;
static bool s_showImguiMetrics;
static bool s_showImguiUserGuide;
static bool s_showImguiStyleEditor;
static bool s_failedDiscoveryStartup;
static int s_failedDiscoveryCount;

static const char *s_colorschemes[] = {
	"ImGui Dark",
	"Light",
	"Classic",
	"Visual Studio Dark",
	"Windows",
};

void BBServer_DebugCallstack()
{
	BB_LOG("UI::Menu::Debug", "User-initiated callstack log");
	sb_t callstack = callstack_generate_sb(0);
	BB_LOG("Debug::Callstack", "%s", sb_get(&callstack));
	sb_reset(&callstack);
}

void BBServer_DebugAssert()
{
	BB_LOG("UI::Menu::Debug", "User-initiated assert");
	BB_ASSERT(false);
}
void BBServer_DebugCrash()
{
	BB_LOG("UI::Menu::Debug", "User-initiated access violation");
	char *badAddress = (char *)1;
	*badAddress = 0;
}
static int infinite_recursion(int i)
{
	if(i < 0)
		return 0;
	return i + infinite_recursion(i + 1);
}
void BBServer_DebugInfiniteRecursion()
{
	BB_LOG("UI::Menu::Debug", "User-initiated infinite recursion");
	infinite_recursion(0);
}

void BBServer_MainMenuBar(void)
{
	if(ImGui::BeginMainMenuBar()) {
		if(ImGui::BeginMenu("File")) {
			if(ImGui::MenuItem("Exit")) {
				Imgui_Core_RequestShutDown();
			}
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Edit")) {
			if(ImGui::MenuItem("Recordings", NULL, &g_config.recordingsOpen)) {
				BB_LOG("UI::Menu::Recordings", "UIRecordings_ToggleOpen");
			}
			if(ImGui::MenuItem("Config")) {
				BB_LOG("UI::Menu::Config", "UIConfig_Open");
				UIConfig_Open(&g_config);
			}
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Debug")) {
			if(ImGui::MenuItem("DEBUG Reload style colors")) {
				Style_ReadConfig(Imgui_Core_GetColorScheme());
			}
			if(ImGui::BeginMenu("Color schemes")) {
				const char *colorscheme = Imgui_Core_GetColorScheme();
				for(int i = 0; i < BB_ARRAYSIZE(s_colorschemes); ++i) {
					bool bSelected = !strcmp(colorscheme, s_colorschemes[i]);
					if(ImGui::MenuItem(s_colorschemes[i], nullptr, &bSelected)) {
						Imgui_Core_SetColorScheme(s_colorschemes[i]);
						Style_ReadConfig(Imgui_Core_GetColorScheme());
					}
				}
				ImGui::EndMenu();
			}
			if(ImGui::BeginMenu("DEBUG Scale")) {
				float dpiScale = Imgui_Core_GetDpiScale();
				if(ImGui::MenuItem("1", nullptr, dpiScale == 1.0f)) {
					Imgui_Core_SetDpiScale(1.0f);
				}
				if(ImGui::MenuItem("1.25", nullptr, dpiScale == 1.25f)) {
					Imgui_Core_SetDpiScale(1.25f);
				}
				if(ImGui::MenuItem("1.5", nullptr, dpiScale == 1.5f)) {
					Imgui_Core_SetDpiScale(1.5f);
				}
				if(ImGui::MenuItem("1.75", nullptr, dpiScale == 1.75f)) {
					Imgui_Core_SetDpiScale(1.75f);
				}
				if(ImGui::MenuItem("2", nullptr, dpiScale == 2.0f)) {
					Imgui_Core_SetDpiScale(2.0f);
				}
				ImGui::EndMenu();
			}
			Fonts_Menu();
			if(ImGui::BeginMenu("Asserts and Crashes")) {
				if(ImGui::Checkbox("Assert MessageBox", &g_config.assertMessageBox)) {
					BB_LOG("UI::Menu::Debug", "Assert MessageBox: %d", g_config.assertMessageBox);
				}
				if(ImGui::MenuItem("Log Callstack")) {
					BBServer_DebugCallstack();
				}
				if(ImGui::MenuItem("Assert")) {
					BBServer_DebugAssert();
				}
				if(ImGui::MenuItem("Crash - access violation")) {
					BBServer_DebugCrash();
				}
				if(ImGui::MenuItem("Crash - infinite recursion")) {
					BBServer_DebugInfiniteRecursion();
				}
				ImGui::EndMenu();
			}
			if(ImGui::BeginMenu("DEBUG Updates")) {
				ImGui::MenuItem("Wait for debugger", nullptr, &g_config.updateWaitForDebugger);
				ImGui::MenuItem("Pause after successful update", nullptr, &g_config.updatePauseAfterSuccessfulUpdate);
				ImGui::MenuItem("Pause after failed update", nullptr, &g_config.updatePauseAfterFailedUpdate);
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Imgui Help")) {
			ImGui::MenuItem("Demo", nullptr, &s_showImguiDemo);
			ImGui::MenuItem("About", nullptr, &s_showImguiAbout);
			ImGui::MenuItem("Metrics", nullptr, &s_showImguiMetrics);
			ImGui::MenuItem("User Guide", nullptr, &s_showImguiUserGuide);
			ImGui::MenuItem("Style Editor", nullptr, &s_showImguiStyleEditor);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

static void BBServer_DispatchToUIMessageQueue()
{
	message_queue_message_t message;
	while(mq_consume_to_ui(&message)) {
		Imgui_Core_RequestRender();
		switch(message.command) {
		case kToUI_AddExistingFile:
			recording_add_existing(message.text, true);
			break;
		case kToUI_AddInvalidExistingFile:
			recording_add_existing(message.text, false);
			break;
		case kToUI_RecordingStart:
			recording_started(message.text);
			break;
		case kToUI_RecordingStop:
			recording_stopped(message.text);
			break;
		case kToUI_DiscoveryStatus:
			if(!s_failedDiscoveryStartup && !strcmp(message.text, "Retrying") && !globals.viewer) {
				++s_failedDiscoveryCount;
				if(s_failedDiscoveryCount > 10) {
					s_failedDiscoveryStartup = true;
					recording_t *recording = recordings_find_main_log();
					if(recording) {
						recorded_session_t *session = recorded_session_find(recording->path);
						if(!session) {
							recorded_session_open(recording->path, recording->applicationFilename, true, recording->active != 0, recording->outgoingMqId);
						}
					}

					messageBox mb = {};
					sdict_add_raw(&mb.data, "title", "Discovery error");
					sdict_add_raw(&mb.data, "text", "Failed to start listening for incoming connections.\nSee log for details.");
					sdict_add_raw(&mb.data, "button1", "Ok");
					mb_queue(mb);
				}
			}
			break;
		default:
			bb_log("to_ui type:%d msg:%s", message.command, message.text);
			break;
		}
	}
}

extern "C" void BBServer_Update(void)
{
	devkit_autodetect_tick();
	tasks_tick();

	if(!globals.viewer) {
		BBServer_MainMenuBar();
	}

	BBServer_DispatchToUIMessageQueue();
	recordings_autodelete_old_recordings();

	if(s_showImguiDemo) {
		ImGui::ShowDemoWindow(&s_showImguiDemo);
	}
	if(s_showImguiAbout) {
		ImGui::ShowAboutWindow(&s_showImguiAbout);
	}
	if(s_showImguiMetrics) {
		ImGui::ShowMetricsWindow(&s_showImguiMetrics);
	}
	if(s_showImguiUserGuide) {
		ImGui::ShowUserGuide();
	}
	if(s_showImguiStyleEditor) {
		ImGui::ShowStyleEditor();
	}

	Update_Tick();
	UIConfig_Update(&g_config);
	UIRecordings_Update(g_config.autoTileViews != 0);
	UIRecordedView_UpdateAll(g_config.autoTileViews != 0);
}
