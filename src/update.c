// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#include "update.h"
#include "appdata.h"
#include "bb_json_generated.h"
#include "bb_structs_generated.h"
#include "bb_wrap_dirent.h"
#include "bbclient/bb_array.h"
#include "bbclient/bb_file.h"
#include "bbclient/bb_string.h"
#include "bbclient/bb_time.h"
#include "cmdline.h"
#include "config.h"
#include "env_utils.h"
#include "file_utils.h"
#include "globals.h"
#include "imgui_core.h"
#include "message_box.h"
#include "parson/parson.h"
#include "path_utils.h"
#include "process_utils.h"
#include "site_config.h"
#include "update_utils.h"
#include "va.h"
#include <stdlib.h>

static UINT g_updateAvailableMessage;
static UINT g_updateIgnoredMessage;
static UINT g_updateShutdownMessage;
u32 g_updateIgnoredVersion;
static b32 g_bUpdateRestartRequested;
static b32 g_bUpdateRequested;
static u32 s_updateMessageToSend;
static const char *s_updateMessageToSendName;
static u32 s_updateMessageCount;
static b32 s_updateMessageHostOnly;
static updateVersionName_t s_currentVersionName;
static updateVersionName_t s_desiredVersionName;
static updateManifest_t s_updateManifest;
static u64 s_lastUpdateCheckMs;

static BOOL CALLBACK Update_EnumWindowsCallback(HWND hWnd, LPARAM version)
{
	char classname[256] = { BB_EMPTY_INITIALIZER };
	if(GetClassNameA(hWnd, classname, sizeof(classname)) > 0) {
		if(!bb_stricmp(classname, "BlackboxHost") || (!s_updateMessageHostOnly && !bb_stricmp(classname, "BlackboxViewer"))) {
			BB_LOG("Update::EnumWindows", "Broadcast %s %d to %s hwnd %p", s_updateMessageToSendName, version, classname, hWnd);
			SendMessageA(hWnd, s_updateMessageToSend, 0, version);
			++s_updateMessageCount;
		}
	}
	return TRUE;
}

static u32 Update_RegisterMessage(const char *message)
{
	u32 val = 0;
	if(message && *message) {
		val = RegisterWindowMessageA(message);
		BB_LOG("Update::RegisterMessage", "%s registered as %u", message, val);
	}
	return val;
}

static void Update_BroadcastMessage(u32 message, u32 version, b32 bHostOnly)
{
	s_updateMessageHostOnly = bHostOnly;
	s_updateMessageCount = 0;
	s_updateMessageToSend = message;
	s_updateMessageToSendName = "(unknown)";
	if(s_updateMessageToSend == g_updateAvailableMessage) {
		s_updateMessageToSendName = g_site_config.updates.updateAvailableMessage.data;
	} else if(s_updateMessageToSend == g_updateIgnoredMessage) {
		s_updateMessageToSendName = g_site_config.updates.updateIgnoredMessage.data;
	} else if(s_updateMessageToSend == g_updateShutdownMessage) {
		s_updateMessageToSendName = g_site_config.updates.updateShutdownMessage.data;
	}
	EnumWindows(Update_EnumWindowsCallback, version);
}

b32 Update_Init(void)
{
	g_updateAvailableMessage = Update_RegisterMessage(g_site_config.updates.updateAvailableMessage.data);
	g_updateIgnoredMessage = Update_RegisterMessage(g_site_config.updates.updateIgnoredMessage.data);
	g_updateShutdownMessage = Update_RegisterMessage(g_site_config.updates.updateShutdownMessage.data);

	sb_t currentPath = appdata_get("bb");
	sb_append(&currentPath, "/bb_current_version.json");
	path_resolve_inplace(&currentPath);
	JSON_Value *val = json_parse_file(sb_get(&currentPath));
	sb_reset(&currentPath);
	if(val) {
		s_currentVersionName = json_deserialize_updateVersionName_t(val);
		json_value_free(val);
	}
	sb_t desiredPath = appdata_get("bb");
	sb_append(&desiredPath, "/bb_desired_version.json");
	path_resolve_inplace(&desiredPath);
	val = json_parse_file(sb_get(&desiredPath));
	sb_reset(&desiredPath);
	if(val) {
		s_desiredVersionName = json_deserialize_updateVersionName_t(val);
		json_value_free(val);
	}
	if(!s_desiredVersionName.name.count) {
		sb_append(&s_desiredVersionName.name, "stable");
	}
	Update_CheckForUpdates(!globals.viewer);

	const char *updateArg = cmdline_find_prefix("-update=");
	if(updateArg) {
		u32 updateVersionToAnnounce = strtoul(updateArg, NULL, 10);
		if(updateVersionToAnnounce) {
			Update_BroadcastMessage(g_updateAvailableMessage, updateVersionToAnnounce, true);
			BB_LOG("Update", "Broadcast UpdateAvailable message to %u main window(s)", s_updateMessageCount);
			if(!s_updateMessageCount) {
				Update_BroadcastMessage(g_updateAvailableMessage, updateVersionToAnnounce, false);
				BB_LOG("Update", "Broadcast UpdateAvailable message to %u viewer window(s)", s_updateMessageCount);
			}
		}
		if(!s_updateMessageCount) {
			BB_LOG("Update", "Requesting update since no windows are open");
			g_bUpdateRequested = true;
		}
		return false;
	}

	const char *updateResultDir = sb_get(&g_site_config.updates.updateResultDir);
	if(*updateResultDir) {
		sb_t dstDir = env_resolve(updateResultDir);
		if(path_mkdir(sb_get(&dstDir))) {
			sb_t srcDir = appdata_get("bb");
			sb_t src = { BB_EMPTY_INITIALIZER };
			sb_t dst = { BB_EMPTY_INITIALIZER };

			sb_va(&src, "%s\\bb_current_version.json", sb_get(&srcDir));
			sb_va(&dst, "%s\\bb_current_version.json", sb_get(&dstDir));
			CopyFileA(sb_get(&src), sb_get(&dst), false);
			sb_reset(&src);
			sb_reset(&dst);

			sb_va(&src, "%s\\bb_desired_version.json", sb_get(&srcDir));
			sb_va(&dst, "%s\\bb_desired_version.json", sb_get(&dstDir));
			CopyFileA(sb_get(&src), sb_get(&dst), false);
			sb_reset(&src);
			sb_reset(&dst);

			sb_reset(&srcDir);
		}
		sb_reset(&dstDir);
	}

	return true;
}

void Update_Shutdown(void)
{
	if(g_bUpdateRestartRequested && g_site_config.updates.updateManifestDir.count > 1) {
		fileData_t fileData = fileData_read("bbupdater.exe");
		if(fileData.buffer && fileData.bufferSize) {
			sb_t tempDir = env_resolve("%TEMP%");
			sb_t bbupdaterTempPath = env_resolve("%TEMP%\\bbupdater.exe");
			if(fileData_writeIfChanged(sb_get(&bbupdaterTempPath), NULL, fileData)) {
				//-source=D:\wsl\builds\bb\2\bb_2.zip -target=D:\bin\bb
				sb_t command = { BB_EMPTY_INITIALIZER };
				sb_t source = update_get_archive_name(g_site_config.updates.updateManifestDir.data, update_resolve_version(&s_updateManifest, s_desiredVersionName.name.data));
				char target[2048] = "";
				GetCurrentDirectoryA(sizeof(target), target);

				sb_va(&command, "\"%s\" \"-relaunch=%s\\bb.exe\" \"-version=%s\" \"-source=%s\" \"-target=%s\"",
				      sb_get(&bbupdaterTempPath),
				      target,
				      update_resolve_version(&s_updateManifest, sb_get(&s_desiredVersionName.name)),
				      sb_get(&source), target);
				if(g_config.updateWaitForDebugger) {
					sb_append(&command, " -debugger");
				}
				if(g_config.updatePauseAfterSuccessfulUpdate) {
					sb_append(&command, " -pauseSuccess");
				}
				if(g_config.updatePauseAfterFailedUpdate) {
					sb_append(&command, " -pauseFailed");
				}
				process_spawn(sb_get(&tempDir), sb_get(&command), kProcessSpawn_OneShot, kProcessLog_All);
				sb_reset(&command);
				sb_reset(&source);
			}
			sb_reset(&tempDir);
			sb_reset(&bbupdaterTempPath);
		}
		fileData_reset(&fileData);
	}
	updateManifest_reset(&s_updateManifest);
	updateVersionName_reset(&s_currentVersionName);
	updateVersionName_reset(&s_desiredVersionName);
}

void Update_RestartAndUpdate(u32 version)
{
	BB_LOG("Update", "Update and restart for version %u", version);
	Update_BroadcastMessage(g_updateShutdownMessage, version, false);
	g_bUpdateRestartRequested = true;
}

static void Update_IgnoreUpdate(u32 version)
{
	BB_LOG("Update", "Update %u ignore sent", version);
	Update_BroadcastMessage(g_updateIgnoredMessage, version, false);
}

static void update_available_messageBoxFunc(messageBox *mb, const char *action)
{
	u32 version = strtoul(sdict_find_safe(&mb->data, "version"), NULL, 10);
	if(!strcmp(action, "Update")) {
		Update_RestartAndUpdate(version);
	} else if(!strcmp(action, "Ignore")) {
		Update_IgnoreUpdate(version);
	}
}

static void Update_UpdateAvailableMessageBox(u32 version)
{
	if(version == g_updateIgnoredVersion)
		return;
	messageBox mb = { BB_EMPTY_INITIALIZER };
	mb.callback = update_available_messageBoxFunc;
	sdict_add_raw(&mb.data, "title", "Update Available");
	if(version) {
		sdict_add_raw(&mb.data, "text", va("An update to version %u is available.  Update and restart?", version));
	} else {
		sdict_add_raw(&mb.data, "text", "An update is available.  Update and restart?");
	}
	sdict_add_raw(&mb.data, "button1", "Update");
	sdict_add_raw(&mb.data, "button2", "Ignore");
	sdict_add_raw(&mb.data, "version", va("%u", version));
	mb_queue(mb);
	Imgui_Core_RequestRender();
}

static void Update_CheckVersions(b32 updateImmediately)
{
	const char *currentVersion = sb_get(&s_currentVersionName.name);
	const char *desiredVersion = update_resolve_version(&s_updateManifest, sb_get(&s_desiredVersionName.name));
	if(strcmp(currentVersion, desiredVersion)) {
		u32 version = strtoul(desiredVersion, NULL, 10);
		if(version) {
			if(updateImmediately) {
				Update_RestartAndUpdate(version);
			} else {
				Update_UpdateAvailableMessageBox(version);
			}
		}
	}
}

void Update_Tick(void)
{
	if(!globals.viewer && g_site_config.updates.updateCheckMs > 0 && s_lastUpdateCheckMs + g_site_config.updates.updateCheckMs < bb_current_time_ms()) {
		Update_CheckForUpdates(false);
	}
}

updateManifest_t *Update_GetManifest(void)
{
	return &s_updateManifest;
}

void Update_CheckForUpdates(b32 bUpdateImmediately)
{
	updateManifest_reset(&s_updateManifest);
	s_lastUpdateCheckMs = bb_current_time_ms();
	const char *updateManifestDir = sb_get(&g_site_config.updates.updateManifestDir);
	sb_t manifestPath = { BB_EMPTY_INITIALIZER };
	sb_va(&manifestPath, "%s/%s", updateManifestDir, "bb_build_manifest.json");
	path_resolve_inplace(&manifestPath);
	JSON_Value *val = json_parse_file(sb_get(&manifestPath));
	if(val) {
		s_updateManifest = json_deserialize_updateManifest_t(val);
		json_value_free(val);
	}
	sb_reset(&manifestPath);
	Update_CheckVersions(bUpdateImmediately);
}

const char *Update_GetCurrentVersion(void)
{
	return sb_get(&s_currentVersionName.name);
}

b32 Update_IsDesiredVersion(const char *versionName)
{
	const char *desiredVersion = sb_get(&s_desiredVersionName.name);
	return desiredVersion && versionName && !strcmp(desiredVersion, versionName);
}

void Update_SetDesiredVersion(const char *versionName)
{
	sb_reset(&s_desiredVersionName.name);
	sb_append(&s_desiredVersionName.name, versionName);
	JSON_Value *val = json_serialize_updateVersionName_t(&s_desiredVersionName);
	if(val) {
		sb_t desiredPath = appdata_get("bb");
		sb_append(&desiredPath, "/bb_desired_version.json");
		path_resolve_inplace(&desiredPath);
		FILE *fp = fopen(sb_get(&desiredPath), "wb");
		sb_reset(&desiredPath);
		if(fp) {
			char *serialized_string = json_serialize_to_string_pretty(val);
			fputs(serialized_string, fp);
			fclose(fp);
			json_free_serialized_string(serialized_string);
		}
	}
	json_value_free(val);
	Update_CheckVersions(true);
}

b32 Update_IsStableVersion(const char *versionName)
{
	const char *stableVersion = sb_get(&s_updateManifest.stable);
	return stableVersion && versionName && !strcmp(stableVersion, versionName);
}

static void update_promote_messageBoxFunc(messageBox *mb, const char *action)
{
	if(!strcmp(action, "Promote")) {
		const char *versionName = sdict_find_safe(&mb->data, "version");
		const char *updateManifestDir = sb_get(&g_site_config.updates.updateManifestDir);
		if(*updateManifestDir) {
			updateManifest_t manifest = updateManifest_build(updateManifestDir);
			sb_append(&manifest.stable, versionName);

			JSON_Value *val = json_serialize_updateManifest_t(&manifest);
			if(val) {
				sb_t manifestPath = { BB_EMPTY_INITIALIZER };
				sb_va(&manifestPath, "%s/%s", updateManifestDir, "bb_build_manifest.json");
				path_resolve_inplace(&manifestPath);
				FILE *fp = fopen(sb_get(&manifestPath), "wb");
				if(fp) {
					char *serialized_string = json_serialize_to_string_pretty(val);
					fputs(serialized_string, fp);
					fclose(fp);
					json_free_serialized_string(serialized_string);
				}
				sb_reset(&manifestPath);
			}
			json_value_free(val);
			updateManifest_reset(&manifest);
			Update_CheckForUpdates(false);
		}
	}
}

void Update_SetStableVersion(const char *versionName)
{
	messageBox mb = { BB_EMPTY_INITIALIZER };
	mb.callback = update_promote_messageBoxFunc;
	sdict_add_raw(&mb.data, "title", "Promote to Stable");
	sdict_add_raw(&mb.data, "text", va("Are you sure you want to promote %s to stable?", versionName));
	sdict_add_raw(&mb.data, "button1", "Promote");
	sdict_add_raw(&mb.data, "button2", "Cancel");
	sdict_add_raw(&mb.data, "version", versionName);
	mb_queue(mb);
	Imgui_Core_RequestRender();
}

LRESULT WINAPI Update_HandleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	BB_UNUSED(hWnd);
	BB_UNUSED(wParam);
	if(g_updateAvailableMessage && msg == g_updateAvailableMessage) {
		if(lParam != g_updateIgnoredVersion) {
			FLASHWINFO info = { BB_EMPTY_INITIALIZER };
			info.cbSize = sizeof(FLASHWINFO);
			info.hwnd = hWnd;
			info.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
			FlashWindowEx(&info);
			Update_UpdateAvailableMessageBox((u32)lParam);
		}
	} else if(g_updateIgnoredMessage && msg == g_updateIgnoredMessage) {
		BB_LOG("Update", "Update %u ignored", lParam);
		g_updateIgnoredVersion = (u32)lParam;
		FLASHWINFO info = { BB_EMPTY_INITIALIZER };
		info.cbSize = sizeof(FLASHWINFO);
		info.hwnd = hWnd;
		info.dwFlags = 0;
		FlashWindowEx(&info);
		Imgui_Core_RequestRender();
	} else if(g_updateShutdownMessage && msg == g_updateShutdownMessage) {
		BB_LOG("Update", "Update shutdown for version %u", lParam);
		Imgui_Core_RequestShutDown();
	}
	return 0;
}
