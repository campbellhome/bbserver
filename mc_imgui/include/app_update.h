// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "sb.h"
#include "update_utils.h"

#include "bb_wrap_windows.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct tag_updateData {
	const char *appName;
	const char *exeName;
	const char *windowClassname;
	const char *resultDir;
	const char *manifestDir;
	b32 waitForDebugger;
	b32 pauseAfterSuccess;
	b32 pauseAfterFailure;
	u32 updateCheckMs;
	b32 showUpdateManagement;
	u8 pad[4];
} updateData;

b32 Update_Init(updateData *data);
void Update_Shutdown(void);
void Update_Tick(void);
void Update_Menu(void);
updateData *Update_GetData(void);
updateManifest_t *Update_GetManifest(void);
void Update_CheckForUpdates(b32 bUpdateImmediately);
const char *Update_GetCurrentVersion(void);
b32 Update_IsDesiredVersion(const char *versionName);
void Update_SetDesiredVersion(const char *versionName);
b32 Update_IsStableVersion(const char *versionName);
void Update_SetStableVersion(const char *versionName);
void Update_RestartAndUpdate(u32 version);
LRESULT WINAPI Update_HandleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern u32 g_updateIgnoredVersion;

#if defined(__cplusplus)
}
#endif
