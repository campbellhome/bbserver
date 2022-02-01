// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#define _CRT_SECURE_NO_WARNINGS

#include "app_update.h"
#include "appdata.h"
#include "bb_array.h"
#include "bb_file.h"
#include "bb_string.h"
#include "bb_time.h"
#include "bb_wrap_dirent.h"
#include "cmdline.h"
#include "env_utils.h"
#include "file_utils.h"
#include "imgui_core.h"
#include "message_box.h"
#include "parson/parson.h"
#include "path_utils.h"
#include "process_utils.h"
#include "update_utils.h"
#include "va.h"
#include <stdlib.h>

static updateData s_updateData;
static UINT g_updateAvailableMessage;
static UINT g_updateIgnoredMessage;
static UINT g_updateShutdownMessage;
u32 g_updateIgnoredVersion;
static b32 g_bUpdateRestartRequested;
static b32 g_bUpdateRequested;
static u32 s_updateMessageToSend;
static u32 s_updateMessageCount;
static updateVersionName_t s_currentVersionName;
static updateVersionName_t s_desiredVersionName;
static updateManifest_t s_updateManifest;
static u64 s_lastUpdateCheckMs;

static BOOL CALLBACK Update_EnumWindowsCallback(HWND hWnd, LPARAM version)
{
	char classname[256] = { BB_EMPTY_INITIALIZER };
	if(GetClassNameA(hWnd, classname, sizeof(classname)) > 0) {
		if(!bb_stricmp(classname, s_updateData.windowClassname)) {
			BB_LOG("Update::EnumWindows", "Broadcast %u version %d to %s hwnd %p", s_updateMessageToSend, version, classname, hWnd);
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

static void Update_BroadcastMessage(u32 message, u32 version)
{
	s_updateMessageCount = 0;
	s_updateMessageToSend = message;
	EnumWindows(Update_EnumWindowsCallback, version);
}

b32 Update_Init(updateData *data)
{
	s_updateData = *data;
	g_updateAvailableMessage = Update_RegisterMessage(va("%s_updateAvailableMessage", s_updateData.appName));
	g_updateIgnoredMessage = Update_RegisterMessage(va("%s_updateIgnoredMessage", s_updateData.appName));
	g_updateShutdownMessage = Update_RegisterMessage(va("%s_updateShutdownMessage", s_updateData.appName));

	sb_t currentPath = appdata_get(s_updateData.appName);
	sb_va(&currentPath, "/%s_current_version.json", s_updateData.appName);
	path_resolve_inplace(&currentPath);
	JSON_Value *val = json_parse_file(sb_get(&currentPath));
	sb_reset(&currentPath);
	if(val) {
		s_currentVersionName = json_deserialize_updateVersionName_t(val);
		json_value_free(val);
	}
	sb_t desiredPath = appdata_get(s_updateData.appName);
	sb_va(&desiredPath, "/%s_desired_version.json", s_updateData.appName);
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
	Update_CheckForUpdates(true);

	const char *updateArg = cmdline_find_prefix("-update=");
	if(updateArg) {
		u32 updateVersionToAnnounce = strtoul(updateArg, NULL, 10);
		if(updateVersionToAnnounce) {
			Update_BroadcastMessage(g_updateAvailableMessage, updateVersionToAnnounce);
			BB_LOG("Update", "Broadcast UpdateAvailable message to %u main window(s)", s_updateMessageCount);
		}
		if(!s_updateMessageCount) {
			BB_LOG("Update", "Requesting update since no windows are open");
			g_bUpdateRequested = true;
		}
		Imgui_Core_RequestShutDown();
		return false;
	}

	if(s_updateData.resultDir && *s_updateData.resultDir) {
		sb_t dstDir = env_resolve(s_updateData.resultDir);
		if(path_mkdir(sb_get(&dstDir))) {
			sb_t srcDir = appdata_get(s_updateData.appName);
			sb_t src = { BB_EMPTY_INITIALIZER };
			sb_t dst = { BB_EMPTY_INITIALIZER };

			sb_va(&src, "%s\\%s_current_version.json", sb_get(&srcDir), s_updateData.appName);
			sb_va(&dst, "%s\\%s_current_version.json", sb_get(&dstDir), s_updateData.appName);
			CopyFileA(sb_get(&src), sb_get(&dst), false);
			sb_reset(&src);
			sb_reset(&dst);

			sb_va(&src, "%s\\%s_desired_version.json", sb_get(&srcDir), s_updateData.appName);
			sb_va(&dst, "%s\\%s_desired_version.json", sb_get(&dstDir), s_updateData.appName);
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
	if(g_bUpdateRestartRequested && s_updateData.manifestDir && *s_updateData.manifestDir) {
		fileData_t fileData = fileData_read(va("%supdater.exe", s_updateData.appName));
		if(fileData.buffer && fileData.bufferSize) {
			sb_t tempDir = env_resolve("%TEMP%");
			sb_t updaterTempPath = env_resolve(va("%%TEMP%%\\%supdater.exe", s_updateData.appName));
			if(fileData_writeIfChanged(sb_get(&updaterTempPath), NULL, fileData)) {
				//-source=D:\wsl\builds\appName\2\appName_2.zip -target=D:\bin\appName
				sb_t command = { BB_EMPTY_INITIALIZER };
				sb_t source = update_get_archive_name(s_updateData.manifestDir, s_updateData.appName, update_resolve_version(&s_updateManifest, s_desiredVersionName.name.data));
				char target[2048] = "";
				GetCurrentDirectoryA(sizeof(target), target);

				sb_va(&command, "\"%s\" \"-relaunch=%s\\%s\" \"-version=%s\" \"-source=%s\" \"-target=%s\"",
				      sb_get(&updaterTempPath),
				      target,
				      s_updateData.exeName,
				      update_resolve_version(&s_updateManifest, sb_get(&s_desiredVersionName.name)),
				      sb_get(&source), target);
				if(s_updateData.waitForDebugger) {
					sb_append(&command, " -debugger");
				}
				if(s_updateData.pauseAfterSuccess) {
					sb_append(&command, " -pauseSuccess");
				}
				if(s_updateData.pauseAfterFailure) {
					sb_append(&command, " -pauseFailed");
				}
				process_spawn(sb_get(&tempDir), sb_get(&command), kProcessSpawn_OneShot, kProcessLog_All);
				sb_reset(&command);
				sb_reset(&source);
			}
			sb_reset(&tempDir);
			sb_reset(&updaterTempPath);
		}
		fileData_reset(&fileData);
	}
	updateManifest_reset(&s_updateManifest);
	updateVersionName_reset(&s_currentVersionName);
	updateVersionName_reset(&s_desiredVersionName);
}

updateData *Update_GetData(void)
{
	return &s_updateData;
}

void Update_RestartAndUpdate(u32 version)
{
	BB_LOG("Update", "Update and restart for version %u", version);
	Update_BroadcastMessage(g_updateShutdownMessage, version);
	Imgui_Core_RequestShutDown();
	g_bUpdateRestartRequested = true;
}

static void Update_IgnoreUpdate(u32 version)
{
	BB_LOG("Update", "Update %u ignore sent", version);
	Update_BroadcastMessage(g_updateIgnoredMessage, version);
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
	mb_queue(mb, NULL);
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
	if(s_updateData.updateCheckMs > 0 && s_lastUpdateCheckMs + s_updateData.updateCheckMs < bb_current_time_ms() && mb_get_active(NULL) == NULL) {
		Update_CheckForUpdates(false);
	}
}

updateManifest_t *Update_GetManifest(void)
{
	return &s_updateManifest;
}

void Update_CheckForUpdates(b32 bUpdateImmediately)
{
	if(!s_updateData.manifestDir || !*s_updateData.manifestDir)
		return;

	updateManifest_reset(&s_updateManifest);
	s_lastUpdateCheckMs = bb_current_time_ms();
	sb_t manifestPath = { BB_EMPTY_INITIALIZER };
	sb_va(&manifestPath, "%s/%s_build_manifest.json", s_updateData.manifestDir, s_updateData.appName);
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
		sb_t desiredPath = appdata_get(s_updateData.appName);
		sb_va(&desiredPath, "/%s_desired_version.json", s_updateData.appName);
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
		if(s_updateData.manifestDir && *s_updateData.manifestDir) {
			updateManifest_t manifest = updateManifest_build(s_updateData.manifestDir, s_updateData.appName);
			sb_append(&manifest.stable, versionName);

			JSON_Value *val = json_serialize_updateManifest_t(&manifest);
			if(val) {
				sb_t manifestPath = { BB_EMPTY_INITIALIZER };
				sb_va(&manifestPath, "%s/%s_build_manifest.json", s_updateData.manifestDir, s_updateData.appName);
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
	mb_queue(mb, NULL);
	Imgui_Core_RequestRender();
}

LRESULT WINAPI Update_HandleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	BB_UNUSED(hWnd);
	BB_UNUSED(wParam);
	if(g_updateAvailableMessage && msg == g_updateAvailableMessage) {
		if(lParam != g_updateIgnoredVersion) {
			Imgui_Core_FlashWindow(true);
			Update_UpdateAvailableMessageBox((u32)lParam);
		}
	} else if(g_updateIgnoredMessage && msg == g_updateIgnoredMessage) {
		BB_LOG("Update", "Update %u ignored", lParam);
		g_updateIgnoredVersion = (u32)lParam;
		Imgui_Core_FlashWindow(false);
		Imgui_Core_RequestRender();
	} else if(g_updateShutdownMessage && msg == g_updateShutdownMessage) {
		BB_LOG("Update", "Update shutdown for version %u", lParam);
		Imgui_Core_RequestShutDown();
	}
	return 0;
}
