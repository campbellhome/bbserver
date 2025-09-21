// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "app_update.h"
#include "appdata.h"
#include "bb_array.h"
#include "bb_assert.h"
#include "bb_colors.h"
#include "bb_common.h"
#include "bb_log.h"
#include "bb_malloc.h"
#include "bb_sockets.h"
#include "bb_string.h"
#include "bb_thread.h"
#include "bbserver_asserts.h"
#include "bbserver_fileopendialog.h"
#include "bbserver_update.h"
#include "cmdline.h"
#include "common.h"
#include "config.h"
#include "crt_leak_check.h"
#include "device_codes.h"
#include "devkit_autodetect.h"
#include "discovery_thread.h"
#include "dragdrop.h"
#include "fonts.h"
#include "imgui_core.h"
#include "imgui_core_windows.h"
#include "imgui_themes.h"
#include "imgui_utils.h"
#include "message_box.h"
#include "message_queue.h"
#include "path_utils.h"
#include "process_utils.h"
#include "recorded_session.h"
#include "recordings.h"
#include "site_config.h"
#include "system_tray.h"
#include "tags.h"
#include "tasks.h"
#include "theme_config.h"
#include "ui_config.h"
#include "ui_message_box.h"
#include "ui_tags.h"
#include "ui_tags_import.h"
#include "ui_view.h"
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
static updateData s_updateData;
static b32 s_bMaximizeOnShow;
static u32 s_windowShowState;

extern "C" unsigned int BBServer_GetMainWindowShowState(void)
{
	return s_windowShowState;
}

static void BBServer_SetImguiPath(void)
{
	sb_t path = appdata_get("bb");
	sb_append(&path, "/imgui.ini");
	path_resolve_inplace(&path);
	path_mkdir(path_get_dir(sb_get(&path)));
	bb_strncpy(s_imguiPath, sb_get(&path), sizeof(s_imguiPath));
	sb_reset(&path);

	ImGuiIO& io = ImGui::GetIO();
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

static b32 BBServer_Init(const char* commandLineRecording)
{
	BB_UNUSED(commandLineRecording);
	site_config_init();
	uuid_init(&uuid_read_state, &uuid_write_state);
	BBServer_SetImguiPath();
	BBServer_SetLogPath();

	const char* applicationName = u8"Blackbox";
	BB_INIT_WITH_FLAGS(applicationName, kBBInitFlag_NoOpenView | kBBInitFlag_NoDiscovery);
	BB_THREAD_SET_NAME("main");
	BB_LOG("Startup", "%s starting...\nPath: %s\n", applicationName, s_bbLogPath);
	BB_LOG("Startup", "Arguments: %s", cmdline_get_full());
	bbthread_set_name("main");

	BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Registering assert handler");
	BBServer_InitAsserts(s_bbLogPath);

	BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Reading config");
	config_read(&g_config);

	BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Reading tags");
	tags_init();

	s_updateData.appName = "bb";
	s_updateData.exeName = "bb.exe";
	s_updateData.windowClassname = "BlackboxHost";
	s_updateData.resultDir = sb_get(&g_site_config.updates.updateResultDir);
	s_updateData.manifestDir = sb_get(&g_site_config.updates.updateManifestDir);
	s_updateData.waitForDebugger = g_config.updateWaitForDebugger;
	s_updateData.pauseAfterSuccess = g_config.updatePauseAfterSuccessfulUpdate;
	s_updateData.pauseAfterFailure = g_config.updatePauseAfterFailedUpdate;
	s_updateData.updateCheckMs = g_site_config.updates.updateCheckMs;
	s_updateData.showUpdateManagement = g_config.updateManagement;
	BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Initializing updates");
	if (!Update_Init(&s_updateData))
	{
		return false;
	}

	BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Initializing process system");
	process_init();
	BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Initializing task system");
	tasks_startup();
	BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Initializing message queue");
	mq_init();
	BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Validating config");
	config_validate_whitelist(&g_config.whitelist);
	config_validate_open_targets(&g_config.openTargets);
	BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Initializing styles");
	Style_Init();
	BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Initializing color scheme");
	Imgui_Core_SetColorScheme(sb_get(&g_config.colorscheme));
	BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Initializing file open dialog");
	FileOpenDialog_Init();
	mb_get_queue()->manualUpdate = true;
	BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Initializing network");
	if (bbnet_init())
	{
		BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Initializing discovery thread");
		if (discovery_thread_init(AF_UNSPEC) != 0)
		{
			new_recording_t recording;
			config_push_whitelist(&g_config.whitelist);
			GetSystemTimeAsFileTime(&recording.filetime);
			recording.applicationName = sb_from_c_string(applicationName);
			recording.applicationFilename = sb_from_c_string("bb");
			recording.path = sb_from_c_string(s_bbLogPath);
#ifdef _DEBUG
			recording.openView = !commandLineRecording || !*commandLineRecording;
#else
			recording.openView = false;
#endif
			recording.recordingType = kRecordingType_MainLog;
			recording.mqId = mq_invalid_id();
			recording.platform = bb_platform();
			to_ui(kToUI_RecordingStart, "%s", recording_build_start_identifier(recording));
			new_recording_reset(&recording);

			BB_INTERNAL_LOG(kBBLogLevel_Verbose, "Startup", "Initializing recordings");
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
	mb_shutdown(nullptr);
	tasks_shutdown();
	process_shutdown();
	devkit_autodetect_shutdown();
	UITags_Shutdown();
	UITagsImport_Shutdown();
	UIConfig_Reset();
	UIRecordedView_Shutdown();
	recordings_shutdown();
	discovery_thread_shutdown();
	recorded_session_shutdown();
	if (g_config.version != 0)
	{
		config_write(&g_config);
	}
	config_reset(&g_config);
	tags_shutdown();
	site_config_shutdown();
	bbnet_shutdown();
	message_queue_message_t message;
	while (mq_consume_to_ui(&message))
	{
		bb_log("(shutdown) to_ui type:%d msg:%s", message.command, message.text);
	}
	mq_shutdown();
	BBServer_ShutdownAsserts();
	uuid_shutdown();
	BB_SHUTDOWN();
}

static void BBServer_BringWindowToFront(HWND hwnd)
{
	DWORD processId;
	if (GetWindowThreadProcessId(hwnd, &processId))
	{
		AllowSetForegroundWindow(processId);
	}
	SendMessageA(hwnd, s_bringToFrontMessage, 0, 0);
}

static sb_t BBServer_GetCommandLineRecording(const char* cmdline)
{
	sb_t result = { BB_EMPTY_INITIALIZER };
	char viewerPath[kBBSize_MaxPath];
	if (cmdline && *cmdline && *cmdline != '-')
	{
		if (*cmdline == '\"')
		{
			++cmdline;
		}
		size_t pathLen = bb_strncpy(viewerPath, cmdline, sizeof(viewerPath));
		if (pathLen)
		{
			if (viewerPath[pathLen - 1] == '\"')
			{
				viewerPath[pathLen - 1] = '\0';
			}
			result = sb_from_c_string(viewerPath);
		}
	}
	return result;
}

static b32 BBServer_SingleInstanceCheck(const char* classname, const char* cmdline)
{
	HWND hExisting = FindWindowA(classname, nullptr);
	if (hExisting)
	{
		if (cmdline_find("-hide") <= 0)
		{
			BBServer_BringWindowToFront(hExisting);
		}

		sb_t viewerPath = BBServer_GetCommandLineRecording(cmdline);
		if (viewerPath.data)
		{
			COPYDATASTRUCT copyData = { BB_EMPTY_INITIALIZER };
			copyData.lpData = viewerPath.data;
			copyData.cbData = viewerPath.count;
			copyData.dwData = COPYDATA_MAGIC;
			SendMessageA(hExisting, WM_COPYDATA, 0, (LPARAM)&copyData);
		}
		sb_reset(&viewerPath);
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

#pragma comment(lib, "Advapi32.lib")

static void SetRegistryKeyValue(HKEY hkey, const char* subkey, const char* key, const char* value)
{
	LONG result = RegSetKeyValueA(hkey, subkey, key, REG_SZ, value, 0);
	if (result == ERROR_SUCCESS)
	{
		BB_LOG("Registry", "RegSetKeyValueA %s %s = %s", subkey, key, value);
	}
	else
	{
		BB_ERROR("Registry", "failed to RegSetKeyValueA %s %s = %s", subkey, key, value);
	}
}

static void SetRegistryDefaultValue(HKEY hkey, const char* subkey, const char* value)
{
	LONG result = RegSetValueA(hkey, subkey, REG_SZ, value, 0);
	if (result == ERROR_SUCCESS)
	{
		BB_INTERNAL_LOG(kBBLogLevel_Display, "Registry", "RegSetValueA %s (Default) = %s", subkey, value);
	}
	else
	{
		BB_ERROR("Registry", "failed to RegSetValueA %s (Default) = %s", subkey, value);
	}
}

static void BBServer_InitRegistry()
{
	char path[4096];
	u32 pathLen = GetModuleFileNameA(NULL, path, BB_ARRAYSIZE(path));
	if (!pathLen || pathLen >= BB_ARRAYSIZE(path) - 1)
		return;

	HKEY hkeyClassesRoot;
	LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Classes", 0, KEY_ALL_ACCESS, &hkeyClassesRoot);
	if (result == ERROR_SUCCESS)
	{
		const char* progId = "Blackbox.bbox.1";

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
	BB_UNUSED(hWnd);
	if (msg && msg == s_bringToFrontMessage)
	{
		BB_LOG("IPC", "blackbox_bring_to_front received");
		Imgui_Core_UnhideWindow();
		Imgui_Core_BringWindowToFront();
		return 0;
	}
	if (msg == WM_COPYDATA)
	{
		COPYDATASTRUCT* copyData = (COPYDATASTRUCT*)lParam;
		if (copyData && copyData->dwData == COPYDATA_MAGIC && copyData->cbData > 0)
		{
			const char* copyText = (const char*)copyData->lpData;
			if (copyText[copyData->cbData - 1] == '\0')
			{
				BB_LOG("IPC", "WM_COPYDATA received %s", copyText);
				DragDrop_ProcessPath(copyText);
			}
			else
			{
				BB_WARNING("IPC", "WM_COPYDATA ignored - %u bytes malformed data received", copyData->cbData);
			}
		}
	}
	if (msg == WM_DROPFILES)
	{
		return DragDrop_OnDropFiles(wParam);
	}
	if (msg == WM_SHOWWINDOW && s_bMaximizeOnShow)
	{
		s_bMaximizeOnShow = false;
		ShowWindow(hWnd, SW_MAXIMIZE);
	}
	LRESULT ret = deviceCodes_HandleWindowMessage(hWnd, msg, wParam, lParam);
	if (ret)
	{
		return ret;
	}
	return 0;
}

struct ImGuiAlloc
{
	void* addr;
	size_t bytes;
};

struct ImGuiAllocs
{
	u32 count;
	u32 allocated;
	ImGuiAlloc* data;
};

static ImGuiAllocs s_imguiAllocs;
s64 g_imgui_allocatedCount;
s64 g_imgui_allocatedBytes;

static void trackImGuiAlloc(void* addr, size_t bytes)
{
	ImGuiAlloc alloc;
	alloc.addr = addr;
	alloc.bytes = bytes;
	bba_push(s_imguiAllocs, alloc);
	++g_imgui_allocatedCount;
	g_imgui_allocatedBytes += bytes;
}

static void untrackImGuiAlloc(void* addr)
{
	for (u32 i = 0; i < s_imguiAllocs.count; ++i)
	{
		ImGuiAlloc* alloc = s_imguiAllocs.data + i;
		if (alloc->addr == addr)
		{
			--g_imgui_allocatedCount;
			g_imgui_allocatedBytes -= alloc->bytes;
			bba_erase(s_imguiAllocs, i);
			return;
		}
	}
	BB_ASSERT(addr == nullptr || Imgui_Core_IsShuttingDown());
}

static void* LoggingMallocWrapper(size_t size, void* user_data)
{
	IM_UNUSED(user_data);

	void* out = malloc(size);
	// char buf[256];
	// if(bb_snprintf(buf, sizeof(buf), "imMalloc(0x%p) %zu bytes\n", out, size) < 0) {
	//	buf[sizeof(buf) - 1] = '\0';
	// }
	// OutputDebugStringA(buf);
	trackImGuiAlloc(out, size);
	return out;
}
static void LoggingFreeWrapper(void* ptr, void* user_data)
{
	IM_UNUSED(user_data);

	// char buf[256];
	// if(bb_snprintf(buf, sizeof(buf), "imFree(0x%p)\n", ptr) < 0) {
	//	buf[sizeof(buf) - 1] = '\0';
	// }
	// OutputDebugStringA(buf);
	untrackImGuiAlloc(ptr);
	free(ptr);
}

int CALLBACK WinMain(_In_ HINSTANCE Instance, _In_opt_ HINSTANCE /*PrevInstance*/, _In_ LPSTR CommandLine, _In_ int /*ShowCode*/)
{
	crt_leak_check_init();
#ifdef _DEBUG
	bb_tracked_malloc_enable(true);
	bba_set_logging(true, true);
#endif

	cmdline_init_composite(CommandLine);
	s_bringToFrontMessage = RegisterWindowMessageA("blackbox_bring_to_front");
	const char* classname = "BlackboxHost";
	if (BBServer_SingleInstanceCheck(classname, CommandLine))
	{
		ImGui::SetAllocatorFunctions(&LoggingMallocWrapper, &LoggingFreeWrapper);
		if (Imgui_Core_Init(CommandLine))
		{
			ImGuiIO& IO = ImGui::GetIO();
			IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
			IO.ConfigViewportsNoDecoration = false;
			IO.ConfigWindowsMoveFromTitleBarOnly = true;
			sb_t viewerPath = BBServer_GetCommandLineRecording(CommandLine);
			if (BBServer_Init(sb_get(&viewerPath)))
			{
				Imgui_Core_SetTextShadows(g_config.textShadows);
				if (SystemTray_Init(Instance))
				{
					Fonts_ClearFonts();
					const char* title = "Blackbox";
					HWND hwnd = Imgui_Core_InitWindow(classname, title, LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAINICON)), *(WINDOWPLACEMENT*)&g_config.wp);
					if (hwnd)
					{
						HWND hExisting = FindWindowA(classname, nullptr);
						if (hExisting != hwnd)
						{
							// there was a race condition with two instances starting at the same time, and
							// we weren't the first to create our main window.
							Imgui_Core_RequestShutDown();
						}
						Imgui_Core_SetCloseHidesWindow(true);
						DragDrop_Init(hwnd);
						BBServer_InitRegistry();
						Fonts_ClearFonts();
						Fonts_AddFont(*(fontConfig_t*)&g_config.uiFontConfig);
						Fonts_AddFont(*(fontConfig_t*)&g_config.logFontConfig);
						Style_ReadConfig(Imgui_Core_GetColorScheme());
						ImGui::SetTextShadowColor(MakeColor(kStyleColor_TextShadow));
						Imgui_Core_SetUserWndProc(&BBServer_HandleWindowMessage);
						if (cmdline_find("-hide") > 0)
						{
							s_bMaximizeOnShow = (g_config.wp.showCmd == SW_SHOWMAXIMIZED);
							Imgui_Core_HideWindow();
						}
						if (viewerPath.data)
						{
							DragDrop_ProcessPath(viewerPath.data);
						}
						while (!Imgui_Core_IsShuttingDown())
						{
							if (Imgui_Core_GetAndClearDirtyWindowPlacement())
							{
								s_windowShowState = config_getwindowplacement(hwnd);
							}
							if (Imgui_Core_BeginFrame())
							{
								BBServer_Update();
								ImVec4 clear_col = ImColor(34, 35, 34);
								Imgui_Core_EndFrame(clear_col);
							}
							else
							{
								BBServer_UpdateMinimal();
							}
						}
						DragDrop_Shutdown();
						Imgui_Core_ShutdownWindow();
					}
				}
				SystemTray_Shutdown();
			}
			BBServer_Shutdown();
			sb_reset(&viewerPath);
		}
		Imgui_Core_Shutdown();
	}

	bba_free(s_imguiAllocs);
	cmdline_shutdown();
	FileOpenDialog_Shutdown();

	return 0;
}
