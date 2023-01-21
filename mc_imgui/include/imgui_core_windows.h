// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#pragma once

#include "bb_wrap_windows.h"

#if defined(__cplusplus)
extern "C" {
#endif

HWND Imgui_Core_InitWindow(const char* classname, const char* title, HICON icon, WINDOWPLACEMENT wp);
typedef LRESULT(Imgui_Core_UserWndProc)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void Imgui_Core_SetUserWndProc(Imgui_Core_UserWndProc* WndProc);

#if defined(__cplusplus)
}
#endif
