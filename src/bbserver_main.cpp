// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "appdata.h"
#include "bb_array.h"
#include "bb_common.h"
#include "bb_log.h"
#include "bb_sockets.h"
#include "bb_string.h"
#include "bb_thread.h"
#include "bbserver_asserts.h"
#include "bbserver_update.h"
#include "cmdline.h"
#include "common.h"
#include "config.h"
#include "crt_leak_check.h"
#include "devkit_autodetect.h"
#include "discovery_thread.h"
#include "dragdrop.h"
#include "globals.h"
#include "imgui_core.h"
#include "imgui_themes.h"
#include "message_box.h"
#include "message_queue.h"
#include "path_utils.h"
#include "process_utils.h"
#include "recorded_session.h"
#include "recordings.h"
#include "site_config.h"
#include "system_tray.h"
#include "tasks.h"
#include "theme_config.h"
#include "ui_config.h"
#include "ui_view.h"
#include "update.h"
#include "uuid_config.h"
#include "uuid_rfc4122/uuid.h"
#include "va.h"
#include "view.h"
#include "win32_resource.h"

#include "bb_structs_generated.h"
#include "bb_wrap_stdio.h"

#define COPYDATA_MAGIC 0x1234567890abcdefu
static char s_imguiPath[kBBSize_MaxPath];
static char s_bbLogPath[kBBSize_MaxPath];
static UINT s_bringToFrontMessage;

static bool BBServer_InitViewer(const char *cmdline)
{
	if(cmdline && *cmdline && *cmdline != '-') {
		globals.viewer = true;
		if(*cmdline == '\"') {
			++cmdline;
		}
		size_t pathLen = bb_strncpy(globals.viewerPath, cmdline, sizeof(globals.viewerPath));
		if(pathLen) {
			if(globals.viewerPath[pathLen - 1] == '\"') {
				globals.viewerPath[pathLen - 1] = '\0';
			}
			FILE *fp = fopen(globals.viewerPath, "rb");
			if(fp) {
				fclose(fp);

				const char *srcName = strrchr(globals.viewerPath, '\\');
				if(srcName) {
					++srcName;
				} else {
					srcName = globals.viewerPath;
				}
				const char *closeBrace = strrchr(srcName, '}');
				if(closeBrace) {
					srcName = closeBrace + 1;
				}

				bb_strncpy(globals.viewerName, srcName, sizeof(globals.viewerName));
				char *ext = strrchr(globals.viewerName, '.');
				if(ext) {
					*ext = '\0';
				}

				return true;
			}
		}
	}
	return false;
}

static void BBServer_SetImguiPath(void)
{
	sb_t path = appdata_get("bb");
	sb_append(&path, "/imgui.ini");
	path_resolve_inplace(&path);
	path_mkdir(sb_get(&path));
	bb_strncpy(s_imguiPath, sb_get(&path), sizeof(s_imguiPath));
	sb_reset(&path);

	ImGuiIO &io = ImGui::GetIO();
	io.IniFilename = s_imguiPath;
}

static void BBServer_SetLogPath(void)
{
	rfc_uuid uuid;
	char uuidBuffer[64];
	uuid_create(&uuid);
	format_uuid(&uuid, uuidBuffer, sizeof(uuidBuffer));
	sb_t path = appdata_get("bb");
	sb_va(&path, "/{%s}bb.bbox", uuidBuffer);
	path_resolve_inplace(&path);
	bb_strncpy(s_bbLogPath, sb_get(&path), sizeof(s_bbLogPath));
	sb_reset(&path);
	bb_init_file(s_bbLogPath);
}

static b32 BBServer_Init(void)
{
	site_config_init();
	uuid_init(&uuid_read_state, &uuid_write_state);
	BBServer_SetImguiPath();
	BBServer_SetLogPath();

	const char *applicationName = globals.viewer ? u8"Blackbox Viewer" : u8"Blackbox";

	BB_INIT_WITH_FLAGS(applicationName, kBBInitFlag_NoOpenView);
	BB_THREAD_SET_NAME("main");
	BB_LOG("Startup", "%s starting...\nPath: %s\n", applicationName, s_bbLogPath);
	BB_LOG("Startup", "Arguments: %s", cmdline_get_full());
	bbthread_set_name("main");

	BBServer_InitAsserts(s_bbLogPath);

	if(!Update_Init()) {
		return false;
	}

	process_init();
	tasks_startup();
	mq_init();
	config_read(&g_config);
	config_validate_whitelist(&g_config.whitelist);
	config_validate_open_targets(&g_config.openTargets);
	Style_Init();
	Imgui_Core_SetColorScheme(sb_get(&g_config.colorscheme));
	if(globals.viewer) {
		new_recording_t cmdlineRecording;
		GetSystemTimeAsFileTime(&cmdlineRecording.filetime);
		cmdlineRecording.applicationName = sb_from_c_string(globals.viewerName);
		cmdlineRecording.applicationFilename = sb_from_c_string(globals.viewerName);
		cmdlineRecording.path = sb_from_c_string(globals.viewerPath);
		cmdlineRecording.openView = true;
		cmdlineRecording.recordingType = kRecordingType_ExternalFile;
		cmdlineRecording.mqId = mq_invalid_id();
		cmdlineRecording.platform = kBBPlatform_Unknown;
		to_ui(kToUI_RecordingStart, "%s", recording_build_start_identifier(cmdlineRecording));
		new_recording_reset(&cmdlineRecording);

		g_config.recordingsOpen = false;
		g_config.autoTileViews = 1;
		g_config.autoDeleteAfterDays = 0;
		return true;
	} else if(bbnet_init()) {
		if(discovery_thread_init() != 0) {
			new_recording_t recording;
			config_push_whitelist(&g_config.whitelist);
			GetSystemTimeAsFileTime(&recording.filetime);
			recording.applicationName = sb_from_c_string(applicationName);
			recording.applicationFilename = sb_from_c_string("bb");
			recording.path = sb_from_c_string(s_bbLogPath);
#ifdef _DEBUG
			recording.openView = true;
#else
			recording.openView = false;
#endif
			recording.recordingType = kRecordingType_MainLog;
			recording.mqId = mq_invalid_id();
			recording.platform = bb_platform();
			to_ui(kToUI_RecordingStart, "%s", recording_build_start_identifier(recording));
			new_recording_reset(&recording);

			recordings_init();
			return true;
		}
	}
	return false;
}

static void BBServer_Shutdown(void)
{
	mq_pre_shutdown();
	Update_Shutdown();
	Style_ResetConfig();
	mb_shutdown();
	tasks_shutdown();
	devkit_autodetect_shutdown();
	UIConfig_Reset();
	UIRecordedView_Shutdown();
	recordings_shutdown();
	discovery_thread_shutdown();
	while(recorded_session_t *session = recorded_session_get(0)) {
		while(session->views.count) {
			u32 viewIndex = session->views.count - 1;
			view_t *view = session->views.data + viewIndex;
			view_reset(view);
			bba_erase(session->views, viewIndex);
		}
		recorded_session_close(session);
	}
	if(!globals.viewer && g_config.version != 0) {
		config_write(&g_config);
	}
	config_reset(&g_config);
	site_config_shutdown();
	bbnet_shutdown();
	message_queue_message_t message;
	while(mq_consume_to_ui(&message)) {
		bb_log("(shutdown) to_ui type:%d msg:%s", message.command, message.text);
	}
	mq_shutdown();
	BBServer_ShutdownAsserts();
	uuid_shutdown();
	BB_SHUTDOWN();
}

static b32 BBServer_SingleInstanceCheck(const char *classname)
{
	if(globals.viewer) {
		HWND hExisting = FindWindowA("BlackboxHost", nullptr);
		if(hExisting) {
			SendMessageA(hExisting, s_bringToFrontMessage, 0, 0);

			COPYDATASTRUCT copyData = { BB_EMPTY_INITIALIZER };
			copyData.lpData = globals.viewerPath;
			copyData.cbData = BB_ARRAYSIZE(globals.viewerPath);
			copyData.dwData = COPYDATA_MAGIC;
			SendMessageA(hExisting, WM_COPYDATA, 0, (LPARAM)&copyData);
			return false;
		}
	} else {
		HWND hExisting = FindWindowA(classname, nullptr);
		if(hExisting) {
			SendMessageA(hExisting, s_bringToFrontMessage, 0, 0);
			return false;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

#pragma comment(lib, "Advapi32.lib")

static void SetRegistryKeyValue(HKEY hkey, const char *subkey, const char *key, const char *value)
{
	LONG result = RegSetKeyValueA(hkey, subkey, key, REG_SZ, value, 0);
	if(result == ERROR_SUCCESS) {
		BB_LOG("Registry", "RegSetKeyValueA %s %s = %s", subkey, key, value);
	} else {
		BB_ERROR("Registry", "failed to RegSetKeyValueA %s %s = %s", subkey, key, value);
	}
}

static void SetRegistryDefaultValue(HKEY hkey, const char *subkey, const char *value)
{
	LONG result = RegSetValueA(hkey, subkey, REG_SZ, value, 0);
	if(result == ERROR_SUCCESS) {
		BB_LOG("Registry", "RegSetValueA %s (Default) = %s", subkey, value);
	} else {
		BB_ERROR("Registry", "failed to RegSetValueA %s (Default) = %s", subkey, value);
	}
}

static void BBServer_InitRegistry()
{
	char path[4096];
	u32 pathLen = GetModuleFileNameA(NULL, path, BB_ARRAYSIZE(path));
	if(!pathLen || pathLen >= BB_ARRAYSIZE(path) - 1)
		return;

	HKEY hkeyClassesRoot;
	LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Classes", 0, KEY_ALL_ACCESS, &hkeyClassesRoot);
	if(result == ERROR_SUCCESS) {
		const char *progId = "Blackbox.bbox.1";

		SetRegistryKeyValue(hkeyClassesRoot, ".bbox\\OpenWithProgIds", progId, "");
		SetRegistryDefaultValue(hkeyClassesRoot, ".bbox", progId);
		SetRegistryDefaultValue(hkeyClassesRoot, va("%s\\DefaultIcon", progId), va("%s,0", path));
		SetRegistryDefaultValue(hkeyClassesRoot, va("%s\\shell\\Open\\Command", progId), va("\"%s\" \"%%1\"", path));

		RegCloseKey(hkeyClassesRoot);
	}
}

//////////////////////////////////////////////////////////////////////////

LRESULT WINAPI BBServer_HandleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if(msg && msg == s_bringToFrontMessage) {
		BB_LOG("IPC", "blackbox_bring_to_front received");
		Imgui_Core_UnhideWindow();
		Imgui_Core_BringWindowToFront();
		return 0;
	}
	if(msg == WM_COPYDATA && !globals.viewer) {
		COPYDATASTRUCT *copyData = (COPYDATASTRUCT *)lParam;
		if(copyData && copyData->dwData == COPYDATA_MAGIC && copyData->cbData == sizeof(globals.viewerPath)) {
			const char *copyText = (const char *)copyData->lpData;
			BB_LOG("IPC", "WM_COPYDATA received %s", copyText);
			DragDrop_ProcessPath(copyText);
		}
	}
	LRESULT result = Update_HandleWindowMessage(hWnd, msg, wParam, lParam);
	if(result) {
		return result;
	}
	if(msg == WM_DROPFILES) {
		return DragDrop_OnDropFiles(wParam);
	}
	return 0;
}

struct ImGuiAlloc {
	void *addr;
	size_t bytes;
};

struct ImGuiAllocs {
	u32 count;
	u32 allocated;
	ImGuiAlloc *data;
};

static ImGuiAllocs s_imguiAllocs;
s64 g_imgui_allocatedCount;
s64 g_imgui_allocatedBytes;

static void trackImGuiAlloc(void *addr, size_t bytes)
{
	ImGuiAlloc alloc;
	alloc.addr = addr;
	alloc.bytes = bytes;
	bba_push(s_imguiAllocs, alloc);
	++g_imgui_allocatedCount;
	g_imgui_allocatedBytes += bytes;
}

static void untrackImGuiAlloc(void *addr)
{
	for(u32 i = 0; i < s_imguiAllocs.count; ++i) {
		ImGuiAlloc *alloc = s_imguiAllocs.data + i;
		if(alloc->addr == addr) {
			--g_imgui_allocatedCount;
			g_imgui_allocatedBytes -= alloc->bytes;
			bba_erase(s_imguiAllocs, i);
			return;
		}
	}
	BB_ASSERT(addr == nullptr || Imgui_Core_IsShuttingDown());
}

static void *LoggingMallocWrapper(size_t size, void *user_data)
{
	IM_UNUSED(user_data);

	void *out = malloc(size);
	//char buf[256];
	//if(bb_snprintf(buf, sizeof(buf), "imMalloc(0x%p) %zu bytes\n", out, size) < 0) {
	//	buf[sizeof(buf) - 1] = '\0';
	//}
	//OutputDebugStringA(buf);
	trackImGuiAlloc(out, size);
	return out;
}
static void LoggingFreeWrapper(void *ptr, void *user_data)
{
	IM_UNUSED(user_data);

	//char buf[256];
	//if(bb_snprintf(buf, sizeof(buf), "imFree(0x%p)\n", ptr) < 0) {
	//	buf[sizeof(buf) - 1] = '\0';
	//}
	//OutputDebugStringA(buf);
	untrackImGuiAlloc(ptr);
	free(ptr);
}

int CALLBACK WinMain(_In_ HINSTANCE Instance, _In_opt_ HINSTANCE /*PrevInstance*/, _In_ LPSTR CommandLine, _In_ int /*ShowCode*/)
{
	crt_leak_check_init();
	//bba_set_logging(true, true);
	ImGui::SetAllocatorFunctions(&LoggingMallocWrapper, &LoggingFreeWrapper);

	cmdline_init_composite(CommandLine);
	s_bringToFrontMessage = RegisterWindowMessageA("blackbox_bring_to_front");
	BBServer_InitViewer(CommandLine);
	const char *classname = (globals.viewer) ? "BlackboxViewer" : "BlackboxHost";
	if(BBServer_SingleInstanceCheck("BlackboxHost")) {
		if(Imgui_Core_Init(CommandLine)) {
			ImGuiIO &IO = ImGui::GetIO();
			IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
			IO.ConfigViewportsNoDecoration = false;
			IO.ConfigWindowsMoveFromTitleBarOnly = true;
			if(BBServer_Init()) {
				Imgui_Core_SetTextShadows(g_config.textShadows);
				if(globals.viewer || SystemTray_Init(Instance)) {
					Fonts_ClearFonts();
					const char *title = (globals.viewer) ? va("%s.bbox - Blackbox", globals.viewerName) : "Blackbox";
					HWND hwnd = Imgui_Core_InitWindow(classname, title, LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAINICON)), g_config.wp);
					if(hwnd) {
						if(!globals.viewer) {
							Imgui_Core_SetCloseHidesWindow(true);
						}
						DragDrop_Init(hwnd);
						BBServer_InitRegistry();
						Fonts_ClearFonts();
						Fonts_AddFont(g_config.uiFontConfig);
						Fonts_AddFont(g_config.logFontConfig);
						Style_ReadConfig(Imgui_Core_GetColorScheme());
						Imgui_Core_SetUserWndProc(&BBServer_HandleWindowMessage);
						if(!globals.viewer && cmdline_find("-hide") > 0) {
							Imgui_Core_HideWindow();
						}
						while(!Imgui_Core_IsShuttingDown()) {
							if(Imgui_Core_GetAndClearDirtyWindowPlacement()) {
								config_getwindowplacement(hwnd);
							}
							if(Imgui_Core_BeginFrame()) {
								BBServer_Update();
								ImVec4 clear_col = ImColor(34, 35, 34);
								Imgui_Core_EndFrame(clear_col);
							}
						}
						DragDrop_Shutdown();
						Imgui_Core_ShutdownWindow();
					}
				}
				SystemTray_Shutdown();
			}
			BBServer_Shutdown();
		}
		Imgui_Core_Shutdown();
	}

	bba_free(s_imguiAllocs);
	cmdline_shutdown();

	return 0;
}
