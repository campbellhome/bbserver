// Copyright (c) Matt Campbell
// MIT license (see License.txt)

#include "system_tray.h"
#include "bb_common.h"
#include "bb_string.h"
#include "imgui_core.h"
#include "win32_resource.h"

#include "bb_wrap_windows.h"
#include <shellapi.h>

int UISystemTray_Open(void);

#define WM_USER_NOTIFYICON WM_USER + 1
static UINT WM_TASKBAR_CREATED;
static WNDCLASSEX g_sysTrayHiddenWindowClass;
static HWND g_sysTrayWnd;
static NOTIFYICONDATA g_sysTrayNotifyIconData;

static LRESULT CALLBACK SystemTray_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_TASKBAR_CREATED)
	{
		if (!Shell_NotifyIconA(NIM_ADD, &g_sysTrayNotifyIconData))
		{
		}
		return 0;
	}

	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_CLOSE:
	case WM_QUIT:
		Imgui_Core_RequestShutDown();
		return 0;
	case WM_USER_NOTIFYICON:
	{
		switch (LOWORD(lParam))
		{
		case WM_LBUTTONDOWN:
		{
			BB_LOG("SysTray", "Left click");
			Imgui_Core_HideUnhideWindow();
			break;
		}
		case WM_LBUTTONDBLCLK:
		{
			BB_LOG("SysTray", "Double click");
			Imgui_Core_UnhideWindow();
			Imgui_Core_BringWindowToFront();
			break;
		}
		case WM_RBUTTONUP:
		{
			BB_LOG("SysTray", "Right click");
			Imgui_Core_BringWindowToFront();
			POINT point;
			GetCursorPos(&point);

			if (!UISystemTray_Open())
			{
				HMENU hMenu = LoadMenu(g_sysTrayHiddenWindowClass.hInstance, MAKEINTRESOURCE(IDR_SYSTRAY_MENU));
				if (!hMenu)
					break;

				HMENU hSubMenu = GetSubMenu(hMenu, 0);
				if (!hSubMenu)
				{
					DestroyMenu(hMenu);
					break;
				}

				SetForegroundWindow(hwnd);
				// Blocking call :(
				TrackPopupMenu(hSubMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN, point.x, point.y, 0, hwnd, NULL);
				SendMessage(hwnd, WM_NULL, 0, 0);
				DestroyMenu(hMenu);
			}
			break;
		}
		default:
			break;
		}
		return 0;
	}
	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case ID_SYSTRAY_EXIT:
			Imgui_Core_RequestShutDown();
			break;
		default:
			break;
		}
		return 0;
	}
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
}

b32 SystemTray_Init(void* hInstance)
{
	WM_TASKBAR_CREATED = RegisterWindowMessageA("TaskbarCreated");

	g_sysTrayHiddenWindowClass.cbSize = sizeof(g_sysTrayHiddenWindowClass);
	g_sysTrayHiddenWindowClass.style = CS_HREDRAW | CS_VREDRAW;
	g_sysTrayHiddenWindowClass.lpfnWndProc = SystemTray_WndProc;
	g_sysTrayHiddenWindowClass.cbClsExtra = 0;
	g_sysTrayHiddenWindowClass.cbWndExtra = 0;
	g_sysTrayHiddenWindowClass.hInstance = hInstance;
	g_sysTrayHiddenWindowClass.hIcon = LoadIconA(hInstance, (LPCTSTR)MAKEINTRESOURCE(IDI_MAINICON));
	g_sysTrayHiddenWindowClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
	g_sysTrayHiddenWindowClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	g_sysTrayHiddenWindowClass.lpszMenuName = NULL;
	g_sysTrayHiddenWindowClass.lpszClassName = "Blackbox SystemTray";
	g_sysTrayHiddenWindowClass.hIconSm = LoadIconA(hInstance, (LPCTSTR)MAKEINTRESOURCE(IDI_MAINICON));
	if (!RegisterClassEx(&g_sysTrayHiddenWindowClass))
		return false;

	g_sysTrayWnd = CreateWindowExA(
	    WS_EX_CLIENTEDGE,
	    g_sysTrayHiddenWindowClass.lpszClassName,
	    "Blackbox SystemTray Window",
	    WS_OVERLAPPEDWINDOW,
	    CW_USEDEFAULT,
	    CW_USEDEFAULT,
	    CW_USEDEFAULT,
	    CW_USEDEFAULT,
	    NULL,
	    NULL,
	    hInstance,
	    NULL);
	if (!g_sysTrayWnd)
		return false;

	g_sysTrayNotifyIconData.cbSize = sizeof(g_sysTrayNotifyIconData);
	g_sysTrayNotifyIconData.hWnd = g_sysTrayWnd;
	g_sysTrayNotifyIconData.uID = IDI_MAINICON;
	g_sysTrayNotifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	bb_strncpy(g_sysTrayNotifyIconData.szTip, "Blackbox", sizeof(g_sysTrayNotifyIconData.szTip));
	g_sysTrayNotifyIconData.hIcon = LoadIconA(hInstance, (LPCTSTR)MAKEINTRESOURCE(IDI_MAINICON));
	g_sysTrayNotifyIconData.uCallbackMessage = WM_USER_NOTIFYICON;

	if (!Shell_NotifyIconA(NIM_ADD, &g_sysTrayNotifyIconData))
		return false;

	return true;
}

void SystemTray_Shutdown(void)
{
	if (g_sysTrayNotifyIconData.hWnd)
	{
		Shell_NotifyIconA(NIM_DELETE, &g_sysTrayNotifyIconData);
	}
	if (g_sysTrayWnd)
	{
		DestroyWindow(g_sysTrayWnd);
	}
	if (g_sysTrayHiddenWindowClass.hInstance)
	{
		UnregisterClassA(g_sysTrayHiddenWindowClass.lpszClassName, g_sysTrayHiddenWindowClass.hInstance);
	}
}
