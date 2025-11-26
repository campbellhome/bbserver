// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "common.h"

#if defined(__cplusplus)
#include "wrap_imgui.h"
b32 Imgui_Core_BeginFrame(void);
void Imgui_Core_EndFrame(ImVec4 clear_col);
extern "C" {
#endif

b32 Imgui_Core_Init(const char* cmdline);
void Imgui_Core_Shutdown(void);

void Imgui_Core_ShutdownWindow(void);

void Imgui_Core_SetCloseHidesWindow(b32 bCloseHidesWindow);
void Imgui_Core_HideUnhideWindow(void);
void Imgui_Core_HideWindow(void);
void Imgui_Core_UnhideWindow(void);
void Imgui_Core_MinimizeWindow(void);
void Imgui_Core_BringWindowToFront(void);
void Imgui_Core_FlashWindow(b32 bFlash);
void Imgui_Core_SetWindowText(const char* text);
void Imgui_Core_SetDebugFocusChange(b32 bDebugFocusChange);

void Imgui_Core_SetDpiScale(float dpiScale);
float Imgui_Core_GetDpiScale(void);

void Imgui_Core_SetTextShadows(b32 bTextShadows);
b32 Imgui_Core_GetTextShadows(void);

b32 Imgui_Core_HasFocus(void);

void Imgui_Core_RequestRender(void);

void Imgui_Core_RequestShutDown(void);
b32 Imgui_Core_IsShuttingDown(void);

void Imgui_Core_SetColorScheme(const char* colorscheme);
const char* Imgui_Core_GetColorScheme(void);

b32 Imgui_Core_GetAndClearDirtyWindowPlacement(void);

void Imgui_Core_QueueUpdateDpiDependentResources(void);
b32 Imgui_Core_IsUpdateDpiDependentResourcesQueued(void);

#if defined(__cplusplus)
}
#endif
