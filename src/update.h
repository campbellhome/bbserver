// Copyright (c) 2012-2019 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"
#include "sb.h"
#include "update_utils.h"

#if defined(__cplusplus)
extern "C" {
#endif

b32 Update_Init(void);
void Update_Shutdown(void);
void Update_Tick(void);
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
