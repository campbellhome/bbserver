// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "bbserver_update.h"
#include "bb_array.h"
#include "bb_colors.h"
#include "bb_log.h"
#include "bb_string.h"
#include "bb_time.h"
#include "devkit_autodetect.h"
#include "fonts.h"
#include "globals.h"
#include "imgui_core.h"
#include "imgui_themes.h"
#include "imgui_tooltips.h"
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
#include "va.h"
#include "view.h"
#include "wrap_imgui.h"

extern s64 g_imgui_allocatedCount;
extern s64 g_imgui_allocatedBytes;

static bool s_showSystemTrayPopup;
extern "C" int UISystemTray_Open(void)
{
	if(0) {
		s_showSystemTrayPopup = true;
		return 1;
	} else {
		return 0;
	}
}
static void UISystemTray_Update(void)
{
#if 0
	if(s_showSystemTrayPopup) {
		//ImGui::SetNextWindowViewport(12345);
		ImGui::OpenPopup("SystemTrayMenu");
		s_showSystemTrayPopup = false;
	}
	b32 open = true;
	ImGui::SetNextWindowViewport(12345);
	//ImGui::SetNextWindowViewportPos(1673,1087);
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
	if(ImGui::Begin("SystemTrayMenu", &open, windowFlags)) {
		Imgui_Core_RequestRender();
		ImGui::TextUnformatted("[System Tray Menu]");
		if(ImGui::Selectable("Exit Blackbox")) {
			Imgui_Core_RequestShutDown();
		}
		ImGui::End();
	}
#endif
}

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

static void BBServer_OpenMainLog(b32 bNoDuplicates = true)
{
	recording_t *recording = recordings_find_main_log();
	if(recording) {
		recorded_session_t *session = recorded_session_find(recording->path);
		if(!session || !bNoDuplicates) {
			recorded_session_open(recording->path, recording->applicationFilename, true, recording->active != 0, recording->outgoingMqId);
		}
	}
}

static void BBServer_DebugCallstack()
{
	BB_LOG("UI::Menu::Debug", "User-initiated callstack log");
	sb_t callstack = callstack_generate_sb(0);
	BB_LOG("Debug::Callstack", "%s", sb_get(&callstack));
	sb_reset(&callstack);
}

static void BBServer_DebugAssert()
{
	BB_LOG("UI::Menu::Debug", "User-initiated assert");
	BB_ASSERT(false);
}
static void BBServer_DebugCrash()
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
static void BBServer_DebugInfiniteRecursion()
{
	BB_LOG("UI::Menu::Debug", "User-initiated infinite recursion");
	infinite_recursion(0);
}

static void App_GenerateColorTestLogs()
{
	BBServer_OpenMainLog();
	BB_LOG("Test::Blink", "^F"
	                      "This"
	                      "^F"
	                      " is a %s"
	                      "colored and ^Fblinking^F%s log\n",
	       warningColorStr, normalColorStr);

	BB_TRACE_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, kBBLogLevel_VeryVerbose, 0, "Test::Color::VeryVerbose", "This log is VeryVerbose");
	BB_TRACE_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, kBBLogLevel_Verbose, 0, "Test::Color::Verbose", "This log is Verbose");
	BB_TRACE_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, kBBLogLevel_Log, 0, "Test::Color::Log", "This log is Log");
	BB_TRACE_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, kBBLogLevel_Display, 0, "Test::Color::Display", "This log is Display");
	BB_TRACE_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, kBBLogLevel_Warning, 0, "Test::Color::Warning", "This log is Warning");
	BB_TRACE_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, kBBLogLevel_Error, 0, "Test::Color::Error", "This log is Error");
	BB_TRACE_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, kBBLogLevel_Fatal, 0, "Test::Color::Fatal", "This log is Fatal");

	for(int i = 0; i < kNumColorKeys; ++i) {
		char c = kFirstColorKey + (char)i;
		BB_LOG("Test::Color::Log", "This is a log with %c%cColor %c%c%c%c%s, which should be %s\n",
		       kColorKeyPrefix, c, kColorKeyPrefix, kColorKeyPrefix, c, c, normalColorStr, textColorNames[i + kColorKeyOffset]);
	}

	for(int i = 0; i < kNumColorKeys; ++i) {
		char c = kFirstColorKey + (char)i;
		BB_WARNING("Test::Color::Warning", "This is a warning with %c%cColor %c%c%c%c%s, which should be %s\n",
		           kColorKeyPrefix, c, kColorKeyPrefix, kColorKeyPrefix, c, c, warningColorStr, textColorNames[i + kColorKeyOffset]);
	}

	for(int i = 0; i < kNumColorKeys; ++i) {
		char c = kFirstColorKey + (char)i;
		BB_ERROR("Test::Color::Error", "This is an error with %c%cColor %c%c%c%c%s, which should be %s\n",
		         kColorKeyPrefix, c, kColorKeyPrefix, kColorKeyPrefix, c, c, errorColorStr, textColorNames[i + kColorKeyOffset]);
	}

	for(int i = kBBColor_UE4_Black; i < kBBColor_Count; ++i) {
		BB_SET_COLOR((bb_color_t)i, kBBColor_Default);
		BB_LOG("Test::Color::SetColor", "This log text should be %s", textColorNames[i]);
	}
	for(int i = kBBColor_UE4_Black; i < kBBColor_Count; ++i) {
		bb_color_t fgColor = (i == kBBColor_UE4_DarkBlue ||
		                      i == kBBColor_UE4_Blue ||
		                      i == kBBColor_UE4_Black)
		                         ? kBBColor_Default
		                         : kBBColor_UE4_Black;
		BB_SET_COLOR(fgColor, (bb_color_t)i);
		BB_LOG("Test::Color::SetColor", "This log text should be on %s", textColorNames[i]);
	}
	BB_SET_COLOR(kBBColor_Default, kBBColor_Default);
}

static void App_GenerateLineTestLogs()
{
	BBServer_OpenMainLog();
	BB_LOG("Test::Multiline", "FirstLine\nSecondLine\n");
	BB_LOG("Test::Multiline", "FirstWithTrailingBlank\n\n");
	BB_LOG("Test::Multiline", "\n\n");
	BB_LOG("Test::Multiline", "\n\nPrevious was 2 blank lines - this has 2 before and 3 after.\n\n\n");

	BB_LOG("Test::Long Line", "This is a reasonably long line, which one might expect to wrap in a tooltip, even though it does not in the log view.  It *should* wrap at 600px in the tooltip.  At least, it did when this was written.  That might have changed by now, but the point remains - this is a long log line.\n");

	BB_LOG_PARTIAL("Test::Partial", "this is a ");
	BB_LOG_PARTIAL("Test::Partial", "^0__partial__^7");
	BB_LOG_PARTIAL("Test::Partial", " test\n");

	BB_LOG_PARTIAL("Test::Partial", "this is a ");
	BB_LOG_PARTIAL("Test::Partial", "__partial__");
	BB_LOG_PARTIAL("Test::Partial::Subcategory", "(interrupting partial different category)");
	BB_LOG_PARTIAL("Test::Partial", " test\n");

	BB_LOG_PARTIAL("Test::Partial", "this is a ");
	BB_LOG_PARTIAL("Test::Partial", "__partial__");
	BB_WARNING_PARTIAL("Test::Partial", "(interrupting partial warning)");
	BB_LOG_PARTIAL("Test::Partial", " test\n");

	BB_LOG_PARTIAL("Test::Partial", "this is a ");
	BB_LOG_PARTIAL("Test::Partial", "__partial__");
	BB_LOG("Test::Partial", "(interrupting log)");
	BB_LOG_PARTIAL("Test::Partial", " test\n");

	BB_LOG("Test::Multiline", "^8Line terminated by \\r\\n\r\nLine terminated by \\n\nLine terminated by \\r\rLine unterminated");

	BB_LOG("Test::LongLine", "Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  --------------------------------------------------> ^<(2k packet limit here)^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length. ^F  ^0Really long log line to test exceeding normal blackbox packet length.  ^1Really long log line to test exceeding normal blackbox packet length.  ^2Really long log line to test exceeding normal blackbox packet length.  ^3Really long log line to test exceeding normal blackbox packet length.  ^4Really long log line to test exceeding normal blackbox packet length.  ^5Really long log line to test exceeding normal blackbox packet length.  ^6Really long log line to test exceeding normal blackbox packet length.  ^7Really long log line to test exceeding normal blackbox packet length.  ^8Really long log line to test exceeding normal blackbox packet length.  ^9Really long log line to test exceeding normal blackbox packet length.  ^:Really long log line to test exceeding normal blackbox packet length.  ^<Really long log line to test exceeding normal blackbox packet length.  End at 3596.");
}

static void App_GeneratePIEInstanceTestLogs()
{
	BBServer_OpenMainLog();
	BB_TRACE_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, kBBLogLevel_Log, 0, "Test::PIEInstance", "This log is from PIEInstance 0");
	BB_TRACE_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, kBBLogLevel_Log, 1, "Test::PIEInstance", "This log is from PIEInstance 1");
	BB_TRACE_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, kBBLogLevel_Log, 2, "Test::PIEInstance", "This log is from PIEInstance 2");
	BB_TRACE_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, kBBLogLevel_Log, 3, "Test::PIEInstance", "This log is from PIEInstance 3");
	BB_TRACE_DYNAMIC_PREFORMATTED(__FILE__, (u32)__LINE__, kBBLogLevel_Log, 4, "Test::PIEInstance", "This log is from PIEInstance 4");
}

static bb_thread_return_t app_test_log_thread(void *args)
{
	BBServer_OpenMainLog();
	u32 i = (u32)(u64)args;
	BB_THREAD_SET_NAME(va("app_test_log_thread %u", i));

	u64 count = 0;
	u64 start = bb_current_time_ms();
	u64 now = start;
	u64 end = now + 10000;
	while(now < end) {
		BB_TRACE(kBBLogLevel_Verbose, "Test::ThreadSpam", "app_test_log_thread %u %llu dt %llu ms", i, ++count, now - start);
		now = bb_current_time_ms();
	}

	BB_THREAD_END();
	return 0;
}

static void App_GenerateSpamTestLogs()
{
	BBServer_OpenMainLog();
	for(u64 i = 0; i < 10; ++i) {
		bbthread_create(app_test_log_thread, (void *)i);
	}
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
		if(ImGui::BeginMenu("Help")) {
			if(ImGui::BeginMenu("ImGui")) {
				ImGui::MenuItem("Demo", nullptr, &s_showImguiDemo);
				ImGui::MenuItem("About", nullptr, &s_showImguiAbout);
				ImGui::MenuItem("Metrics", nullptr, &s_showImguiMetrics);
				ImGui::MenuItem("User Guide", nullptr, &s_showImguiUserGuide);
				ImGui::MenuItem("Style Editor", nullptr, &s_showImguiStyleEditor);
				ImGui::EndMenu();
			}
			if(ImGui::BeginMenu("Logging Test")) {
				if(ImGui::MenuItem("Colors")) {
					App_GenerateColorTestLogs();
				}
				if(ImGui::MenuItem("Lines")) {
					App_GenerateLineTestLogs();
				}
				if(ImGui::MenuItem("PIEInstance")) {
					App_GeneratePIEInstanceTestLogs();
				}
				if(ImGui::MenuItem("Thread Spam")) {
					App_GenerateSpamTestLogs();
				}
				if(ImGui::MenuItem("Main Log")) {
					BBServer_OpenMainLog(false);
				}
				ImGui::EndMenu();
			}
#if 0
			recording_t *r = recordings_find_main_log();
			recorded_session_t *s = r ? recorded_session_find(r->path) : nullptr;
			if(s) {
				bool logReads = s->logReads != 0;
				if(ImGui::MenuItem("Log All Reads", NULL, &logReads)) {
					s->logReads = logReads;
					BB_LOG("Debug", "Toggled spammy for '%s'\n", s->appInfo.packet.appInfo.applicationName);
				}
			}
#endif
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Update")) {
			updateManifest_t *manifest = Update_GetManifest();
			auto AnnotateVersion = [manifest](const char *version) {
				const char *annotated = version;
				if(version && !bb_stricmp(version, sb_get(&manifest->stable))) {
					annotated = va("%s (stable)", version);
				} else if(version && !bb_stricmp(version, sb_get(&manifest->latest))) {
					annotated = va("%s (latest)", version);
				}
				return annotated;
			};
			const char *currentVersion = Update_GetCurrentVersion();
			const char *currentVersionAnnotated = AnnotateVersion(currentVersion);
			ImGui::MenuItem(va("version %s", *currentVersionAnnotated ? currentVersionAnnotated : "unknown"), nullptr, false, false);
			if(ImGui::MenuItem("Check for updates")) {
				Update_CheckForUpdates(false);
			}
			if(ImGui::BeginMenu("Set desired version")) {
				if(ImGui::MenuItem("stable", nullptr, Update_IsDesiredVersion("stable"))) {
					Update_SetDesiredVersion("stable");
				}
				if(ImGui::MenuItem("latest", nullptr, Update_IsDesiredVersion("latest"))) {
					Update_SetDesiredVersion("latest");
				}
				for(u32 i = 0; i < (manifest ? manifest->versions.count : 0); ++i) {
					updateVersion_t *version = manifest->versions.data + i;
					const char *versionName = sb_get(&version->name);
					if(ImGui::MenuItem(AnnotateVersion(versionName), nullptr, Update_IsDesiredVersion(versionName))) {
						Update_SetDesiredVersion(versionName);
					}
				}
				ImGui::EndMenu();
			}
			if(g_config.updateManagement && *currentVersion && !Update_IsStableVersion(currentVersion)) {
				if(ImGui::MenuItem(va("Promote %s to stable version", currentVersion))) {
					Update_SetStableVersion(currentVersion);
				}
			}
			if(g_updateIgnoredVersion) {
				if(ImGui::MenuItem(va("Update to version %u and restart", g_updateIgnoredVersion))) {
					Update_RestartAndUpdate(g_updateIgnoredVersion);
				}
			}
			ImGui::EndMenu();
		}

		if(s_failedDiscoveryStartup) {
			ImGui::PushStyleColor(ImGuiCol_Text, MakeColor(kStyleColor_LogLevel_Error));
			if(ImGui::MenuItem("Discovery Error")) {
				recording_t *recording = recordings_find_main_log();
				if(recording) {
					recorded_session_t *session = recorded_session_find(recording->path);
					if(!session) {
						recorded_session_open(recording->path, recording->applicationFilename, true, recording->active != 0, recording->outgoingMqId);
					}
				}
			}
			if(ImGui::IsTooltipActive()) {
				ImGui::BeginTooltip();
				ImGui::TextUnformatted("Failed to start listening for incoming connections.\nSee log for details.");
				ImGui::EndTooltip();
			}
			ImGui::PopStyleColor();
		}

		ImFont *font = ImGui::GetFont();
		ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, "Recordings");

		ImGuiStyle &style = ImGui::GetStyle();
		float checkWidth = style.FramePadding.x * 4 + style.ItemInnerSpacing.x + textSize.y;

		float width = BB_MAX((textSize.x + checkWidth + 10) * g_config.dpiScale, UIRecordings_WidthWhenOpen());
		ImGui::SameLine(ImGui::GetWindowWidth() - width);
		if(ImGui::Checkbox("Recordings", &g_config.recordingsOpen)) {
			BB_LOG("UI::Menu::Recordings", "UIRecordings_ToggleOpen");
		}
		if(ImGui::BeginPopupContextItem("RecordingsContext")) {
			if(ImGui::Selectable("Auto-close all")) {
				recorded_session_auto_close_all();
			}
			ImGui::EndPopup();
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
				}
			} else if(!strcmp(message.text, "Running")) {
				s_failedDiscoveryStartup = false;
				s_failedDiscoveryCount = 0;
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
		if(ImGui::Begin("Dear ImGui Metrics", &s_showImguiMetrics)) {
			ImGui::Separator();
			ImGui::Text("bba allocs: %lld", g_bba_allocatedCount);
			ImGui::Text("bba bytes: %lld", g_bba_allocatedBytes);
			ImGui::Text("ImGui allocs: %lld", g_imgui_allocatedCount);
			ImGui::Text("ImGui bytes: %lld", g_imgui_allocatedBytes);
		}
		ImGui::End();
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
	UISystemTray_Update();
}
