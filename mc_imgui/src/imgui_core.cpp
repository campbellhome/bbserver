// Copyright (c) 2012-2022 Matt Campbell
// MIT license (see License.txt)

#include "imgui_core.h"
#include "app_update.h"
#include "bb_string.h"
#include "bb_time.h"
#include "cmdline.h"
#include "common.h"
#include "fonts.h"
#include "imgui_core_windows.h"
#include "imgui_image.h"
#include "imgui_input_text.h"
#include "imgui_themes.h"
#include "imgui_utils.h"
#include "message_box.h"
#include "sb.h"
#include "system_error_utils.h"
#include "time_utils.h"
#include "ui_message_box.h"
#include "va.h"
#include "wrap_imgui.h"
#include "wrap_shellscalingapi.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "xinput9_1_0.lib")

static void Imgui_Core_InitD3D(void);

typedef struct tag_Imgui_Core_Window {
	HWND hwnd;
	LPDIRECT3DDEVICE9 pd3dDevice;
	HRESULT last3DResetResult;
	b32 b3dInitialized;
	b32 b3dValid;
	u8 pad[4];
} Imgui_Core_Window;

static LPDIRECT3D9 s_pD3D;
static D3DPRESENT_PARAMETERS g_d3dpp;
static WNDCLASSEX s_wc;
static Imgui_Core_Window s_wnd;
static bool g_hasFocus;
static bool g_trackingMouse;
static int g_dpi = USER_DEFAULT_SCREEN_DPI;
static float g_dpiScale;
static b32 g_bTextShadows;
static bool g_needUpdateDpiDependentResources;
static sb_t g_colorscheme;
static int g_appRequestRenderCount;
static bool g_shuttingDown;
static bool g_bDirtyWindowPlacement;
static bool g_setCmdline;
static bool g_bCloseHidesWindow;
static bool g_bDebugFocusChange;
static HWINEVENTHOOK s_hWinEventHook;

static void CALLBACK Imgui_Core_WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

static const char *D3DErrorString(HRESULT Hr)
{
#define D3D_CASE(x) \
	case x: return #x
	switch(Hr) {
		D3D_CASE(D3D_OK);

		D3D_CASE(D3DERR_WRONGTEXTUREFORMAT);
		D3D_CASE(D3DERR_UNSUPPORTEDCOLOROPERATION);
		D3D_CASE(D3DERR_UNSUPPORTEDCOLORARG);
		D3D_CASE(D3DERR_UNSUPPORTEDALPHAOPERATION);
		D3D_CASE(D3DERR_UNSUPPORTEDALPHAARG);
		D3D_CASE(D3DERR_TOOMANYOPERATIONS);
		D3D_CASE(D3DERR_CONFLICTINGTEXTUREFILTER);
		D3D_CASE(D3DERR_UNSUPPORTEDFACTORVALUE);
		D3D_CASE(D3DERR_CONFLICTINGRENDERSTATE);
		D3D_CASE(D3DERR_UNSUPPORTEDTEXTUREFILTER);
		D3D_CASE(D3DERR_CONFLICTINGTEXTUREPALETTE);
		D3D_CASE(D3DERR_DRIVERINTERNALERROR);

		D3D_CASE(D3DERR_NOTFOUND);
		D3D_CASE(D3DERR_MOREDATA);
		D3D_CASE(D3DERR_DEVICELOST);
		D3D_CASE(D3DERR_DEVICENOTRESET);
		D3D_CASE(D3DERR_NOTAVAILABLE);
		D3D_CASE(D3DERR_OUTOFVIDEOMEMORY);
		D3D_CASE(D3DERR_INVALIDDEVICE);
		D3D_CASE(D3DERR_INVALIDCALL);
		D3D_CASE(D3DERR_DRIVERINVALIDCALL);
		D3D_CASE(D3DERR_WASSTILLDRAWING);
		D3D_CASE(D3DOK_NOAUTOGEN);

#if !defined(D3D_DISABLE_9EX)
		D3D_CASE(D3DERR_DEVICEREMOVED);
		D3D_CASE(S_NOT_RESIDENT);
		D3D_CASE(S_RESIDENT_IN_SHARED_MEMORY);
		D3D_CASE(S_PRESENT_MODE_CHANGED);
		D3D_CASE(S_PRESENT_OCCLUDED);
		D3D_CASE(D3DERR_DEVICEHUNG);
		D3D_CASE(D3DERR_UNSUPPORTEDOVERLAY);
		D3D_CASE(D3DERR_UNSUPPORTEDOVERLAYFORMAT);
		D3D_CASE(D3DERR_CANNOTPROTECTCONTENT);
		D3D_CASE(D3DERR_UNSUPPORTEDCRYPTO);
		D3D_CASE(D3DERR_PRESENT_STATISTICS_DISJOINT);
#endif

	default:
		return va("Unknown (%8.8X)", Hr);
	}
#undef D3D_CASE
}

extern "C" b32 Imgui_Core_Init(const char *cmdline)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	Style_Init();

	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

	s_hWinEventHook = SetWinEventHook(
	    EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
	    NULL, Imgui_Core_WinEventProc, 0, 0,
	    WINEVENT_OUTOFCONTEXT);

	s_pD3D = Direct3DCreate9(D3D_SDK_VERSION);

	if(s_pD3D) {
		g_d3dpp.Windowed = TRUE;
		g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
		g_d3dpp.EnableAutoDepthStencil = TRUE;
		g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
		g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE; // Present with vsync
	}

	if(!cmdline_argc()) {
		g_setCmdline = true;
		cmdline_init_composite(cmdline);
	}

	Imgui_Core_Freetype_Init();
	Fonts_Init();

	return s_pD3D != nullptr;
}

void Imgui_Core_ResetD3D()
{
	if(!s_wnd.pd3dDevice)
		return;
	ImGui_ImplDX9_InvalidateDeviceObjects();
	ImGui_Image_InvalidateDeviceObjects();
	HRESULT hr = s_wnd.pd3dDevice->Reset(&g_d3dpp);
	s_wnd.b3dValid = (hr == D3D_OK);
	if(s_wnd.last3DResetResult != hr) {
		s_wnd.last3DResetResult = hr;
		if(s_wnd.b3dValid) {
			BB_LOG("ImguiCore", "D3D Reset HR: %s", D3DErrorString(hr));
		} else {
			BB_WARNING("ImguiCore", "D3D Reset HR: %s", D3DErrorString(hr));

			ImGuiPlatformIO &PlatformIO = ImGui::GetPlatformIO();
			if(PlatformIO.Platform_DestroyWindow) {
				ImGuiViewport *mainViewport = ImGui::GetMainViewport();
				if(mainViewport) {
					PlatformIO.Platform_DestroyWindow(mainViewport);
				}
			}

			ImGui_ImplDX9_Shutdown();
			ImGui_ImplWin32_Shutdown();
			s_wnd.pd3dDevice = nullptr;

			Imgui_Core_InitD3D();
		}
	}
	ImGui_ImplDX9_CreateDeviceObjects();
}

extern "C" void Imgui_Core_Shutdown(void)
{
	ImGui::InputTextShutdown();
	Fonts_Shutdown();
	Imgui_Core_Freetype_Shutdown();
	mb_shutdown(nullptr);

	if(s_hWinEventHook) {
		UnhookWinEvent(s_hWinEventHook);
	}

	if(g_setCmdline) {
		cmdline_shutdown();
	}

	sb_reset(&g_colorscheme);

	ImGui::DestroyContext();

	if(s_pD3D) {
		s_pD3D->Release();
		s_pD3D = nullptr;
	}
}

extern "C" b32 Imgui_Core_HasFocus(void)
{
	return g_hasFocus;
}

extern "C" float Imgui_Core_GetDpiScale(void)
{
	return g_dpiScale;
}

extern "C" void Imgui_Core_SetCloseHidesWindow(b32 bCloseHidesWindow)
{
	g_bCloseHidesWindow = bCloseHidesWindow != 0;
}

extern "C" void Imgui_Core_SetDebugFocusChange(b32 bDebugFocusChange)
{
	g_bDebugFocusChange = bDebugFocusChange != 0;
}

extern "C" void Imgui_Core_HideUnhideWindow(void)
{
	if(s_wnd.hwnd) {
		WINDOWPLACEMENT wp = { BB_EMPTY_INITIALIZER };
		wp.length = sizeof(wp);
		GetWindowPlacement(s_wnd.hwnd, &wp);
		if(wp.showCmd == SW_SHOWMINIMIZED) {
			ShowWindow(s_wnd.hwnd, SW_RESTORE);
			Imgui_Core_BringWindowToFront();
		} else if(!IsWindowVisible(s_wnd.hwnd)) {
			ShowWindow(s_wnd.hwnd, SW_SHOW);
			Imgui_Core_BringWindowToFront();
		} else {
			ShowWindow(s_wnd.hwnd, SW_HIDE);
		}
	}
}

extern "C" void Imgui_Core_HideWindow(void)
{
	if(s_wnd.hwnd) {
		ShowWindow(s_wnd.hwnd, SW_HIDE);
	}
}

extern "C" void Imgui_Core_UnhideWindow(void)
{
	if(s_wnd.hwnd) {
		if(IsIconic(s_wnd.hwnd)) {
			ShowWindow(s_wnd.hwnd, SW_RESTORE);
		} else {
			ShowWindow(s_wnd.hwnd, SW_SHOW);
		}
		Imgui_Core_RequestRender();
	}
}

extern "C" void Imgui_Core_MinimizeWindow(void)
{
	if(s_wnd.hwnd) {
		ShowWindow(s_wnd.hwnd, SW_MINIMIZE);
	}
}

extern "C" void Imgui_Core_BringWindowToFront(void)
{
	if(s_wnd.hwnd) {
		if(!BringWindowToTop(s_wnd.hwnd) && g_bDebugFocusChange) {
			system_error_to_log(GetLastError(), "Window", "BringWindowToTop");
		}
		if(!SetForegroundWindow(s_wnd.hwnd) && g_bDebugFocusChange) {
			system_error_to_log(GetLastError(), "Window", "SetForegroundWindow");
		}
		if(!SetFocus(s_wnd.hwnd) && g_bDebugFocusChange) {
			system_error_to_log(GetLastError(), "Window", "SetFocus");
		}
		Imgui_Core_RequestRender();
	}
}

extern "C" void Imgui_Core_FlashWindow(b32 bFlash)
{
	if(s_wnd.hwnd) {
		FLASHWINFO info = { BB_EMPTY_INITIALIZER };
		info.cbSize = sizeof(FLASHWINFO);
		info.hwnd = s_wnd.hwnd;
		info.dwFlags = bFlash ? (FLASHW_ALL | FLASHW_TIMERNOFG) : 0u;
		FlashWindowEx(&info);
	}
}

extern "C" void Imgui_Core_SetDpiScale(float dpiScale)
{
	if(g_dpiScale != dpiScale) {
		g_dpiScale = dpiScale;
		Imgui_Core_QueueUpdateDpiDependentResources();
	}
}

extern "C" void Imgui_Core_SetTextShadows(b32 bTextShadows)
{
	g_bTextShadows = bTextShadows;
}

extern "C" b32 Imgui_Core_GetTextShadows(void)
{
	return g_bTextShadows;
}

extern "C" void Imgui_Core_RequestRender(void)
{
	g_appRequestRenderCount = 3;
}

extern "C" b32 Imgui_Core_GetAndClearRequestRender(void)
{
	b32 ret = g_appRequestRenderCount > 0;
	g_appRequestRenderCount = BB_MAX(0, g_appRequestRenderCount - 1);
	return ret;
}

extern "C" void Imgui_Core_DirtyWindowPlacement(void)
{
	g_bDirtyWindowPlacement = true;
}

extern "C" b32 Imgui_Core_GetAndClearDirtyWindowPlacement(void)
{
	b32 ret = g_bDirtyWindowPlacement;
	g_bDirtyWindowPlacement = false;
	return ret;
}

extern "C" void Imgui_Core_RequestShutDown(void)
{
	g_shuttingDown = true;
}

extern "C" b32 Imgui_Core_IsShuttingDown(void)
{
	return g_shuttingDown;
}

extern "C" void Imgui_Core_SetColorScheme(const char *colorscheme)
{
	if(strcmp(sb_get(&g_colorscheme), colorscheme)) {
		sb_reset(&g_colorscheme);
		sb_append(&g_colorscheme, colorscheme);
		Imgui_Core_QueueUpdateDpiDependentResources();
	}
}

extern "C" const char *Imgui_Core_GetColorScheme(void)
{
	return sb_get(&g_colorscheme);
}

extern "C" void Update_Menu(void)
{
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
			if(ImGui::MenuItem("stable", nullptr, Update_IsDesiredVersion("stable") != 0)) {
				Update_SetDesiredVersion("stable");
			}
			if(ImGui::MenuItem("latest", nullptr, Update_IsDesiredVersion("latest") != 0)) {
				Update_SetDesiredVersion("latest");
			}
			for(u32 i = 0; i < (manifest ? manifest->versions.count : 0); ++i) {
				updateVersion_t *version = manifest->versions.data + i;
				const char *versionName = sb_get(&version->name);
				if(ImGui::MenuItem(AnnotateVersion(versionName), nullptr, Update_IsDesiredVersion(versionName) != 0)) {
					Update_SetDesiredVersion(versionName);
				}
			}
			ImGui::EndMenu();
		}
		if(Update_GetData()->showUpdateManagement && *currentVersion && !Update_IsStableVersion(currentVersion) != 0) {
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
}

void UpdateDpiDependentResources()
{
	Fonts_InitFonts();
	Imgui_Core_ResetD3D();
	Style_Apply(sb_get(&g_colorscheme));
}

extern "C" void Imgui_Core_QueueUpdateDpiDependentResources(void)
{
	g_needUpdateDpiDependentResources = true;
}

BOOL EnableNonClientDpiScalingShim(_In_ HWND hwnd)
{
	HMODULE hModule = GetModuleHandleA("User32.dll");
	if(hModule) {
		typedef BOOL(WINAPI * Proc)(_In_ HWND hwnd);
		Proc proc = (Proc)(void *)(GetProcAddress(hModule, "EnableNonClientDpiScaling"));
		if(proc) {
			return proc(hwnd);
		}
	}
	return FALSE;
}

UINT GetDpiForWindowShim(_In_ HWND hwnd)
{
	HMODULE hModule = GetModuleHandleA("User32.dll");
	if(hModule) {
		typedef UINT(WINAPI * Proc)(_In_ HWND hwnd);
		Proc proc = (Proc)(void *)(GetProcAddress(hModule, "GetDpiForWindow"));
		if(proc) {
			return proc(hwnd);
		}
	}
	return USER_DEFAULT_SCREEN_DPI;
}

static void CALLBACK Imgui_Core_WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
	// Idea to hook accessibility events comes from https://devblogs.microsoft.com/oldnewthing/20130930-00/?p=3083
	BB_UNUSED(hWinEventHook);
	BB_UNUSED(dwEventThread);
	BB_UNUSED(dwmsEventTime);
	if(hwnd &&
	   idObject == OBJID_WINDOW &&
	   idChild == CHILDID_SELF &&
	   event == EVENT_SYSTEM_FOREGROUND) {
		DWORD processId = 0;
		b32 bSameProcess = false;
		if(GetWindowThreadProcessId(hwnd, &processId)) {
			bSameProcess = processId == GetCurrentProcessId();
		}
		if(bSameProcess) {
			g_hasFocus = true;
		} else if(g_hasFocus) {
			g_hasFocus = false;
		}
	}
}

static Imgui_Core_UserWndProc *g_userWndProc;
void Imgui_Core_SetUserWndProc(Imgui_Core_UserWndProc *wndProc)
{
	g_userWndProc = wndProc;
}

LRESULT WINAPI Imgui_Core_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if(!s_pD3D) {
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	switch(msg) {
	case WM_NCCREATE:
		EnableNonClientDpiScalingShim(hWnd);
		g_dpi = (int)GetDpiForWindowShim(hWnd);
		g_dpiScale = (float)g_dpi / (float)USER_DEFAULT_SCREEN_DPI;
		Style_Apply(sb_get(&g_colorscheme));
		break;
	case WM_DPICHANGED: {
		g_dpi = HIWORD(wParam);
		g_dpiScale = (float)g_dpi / (float)USER_DEFAULT_SCREEN_DPI;
		UpdateDpiDependentResources();

		RECT *const prcNewWindow = (RECT *)lParam;
		SetWindowPos(hWnd,
		             NULL,
		             prcNewWindow->left,
		             prcNewWindow->top,
		             prcNewWindow->right - prcNewWindow->left,
		             prcNewWindow->bottom - prcNewWindow->top,
		             SWP_NOZORDER | SWP_NOACTIVATE);
		break;
	}
	case WM_MOUSEMOVE:
		Imgui_Core_RequestRender();
		if(!g_trackingMouse) {
			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof(tme);
			tme.hwndTrack = hWnd;
			tme.dwFlags = TME_LEAVE;
			tme.dwHoverTime = HOVER_DEFAULT;
			TrackMouseEvent(&tme);
			g_trackingMouse = true;
		}
		break;
	case WM_MOUSELEAVE:
		Imgui_Core_RequestRender();
		g_trackingMouse = false;
		ImGui::GetIO().MousePos = ImVec2(-1, -1);
		for(int i = 0; i < BB_ARRAYSIZE(ImGui::GetIO().MouseDown); ++i) {
			ImGui::GetIO().MouseDown[i] = false;
		}
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_CHAR:
		Imgui_Core_RequestRender();
		break;
	case WM_CLOSE:
		if(g_bCloseHidesWindow) {
			ShowWindow(hWnd, SW_HIDE);
			return 0;
		}
	default:
		break;
	}

	IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	if(ImGui::GetCurrentContext() && ImGui::GetIO().BackendPlatformUserData) {
		if(ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return true;
	} else {
		BB_WARNING("ImguiCore", "skipped ImGui_ImplWin32_WndProcHandler w/o BackendPlatformUserData");
	}

	if(g_userWndProc) {
		LRESULT userResult = (*g_userWndProc)(hWnd, msg, wParam, lParam);
		if(userResult) {
			return userResult;
		}
	}

	LRESULT result = Update_HandleWindowMessage(hWnd, msg, wParam, lParam);
	if(result) {
		return result;
	}

	switch(msg) {
	case WM_MOVE:
		Imgui_Core_RequestRender();
		Imgui_Core_DirtyWindowPlacement();
		break;
	case WM_SIZE:
		Imgui_Core_RequestRender();
		Imgui_Core_DirtyWindowPlacement();
		if(wParam != SIZE_MINIMIZED) {
			g_d3dpp.BackBufferWidth = LOWORD(lParam);
			g_d3dpp.BackBufferHeight = HIWORD(lParam);
			Imgui_Core_ResetD3D();
		}
		return 0;
	case WM_SYSCOMMAND:
		if((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

typedef struct D3DCreateInfo_s {
	D3DDEVTYPE deviceType;
	DWORD vertexProcessingType;
} D3DCreateInfo_t;

static void Imgui_Core_InitD3D(void)
{
	if(!s_wnd.pd3dDevice && s_wnd.hwnd && s_wnd.b3dInitialized) {
		if(s_pD3D) {
			s_pD3D->Release();
			s_pD3D = nullptr;
		}
		s_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
		if(!s_pD3D)
			return;
	}

	D3DCreateInfo_t d3dCreateInfo[] = {
		{ D3DDEVTYPE_HAL, D3DCREATE_MIXED_VERTEXPROCESSING },
		{ D3DDEVTYPE_HAL, D3DCREATE_SOFTWARE_VERTEXPROCESSING },
		{ D3DDEVTYPE_SW, D3DCREATE_MIXED_VERTEXPROCESSING },
		{ D3DDEVTYPE_SW, D3DCREATE_SOFTWARE_VERTEXPROCESSING },
	};
	b32 bOk = false;
	for(u32 i = 0; !bOk && i < BB_ARRAYSIZE(d3dCreateInfo); ++i) {
		bOk = s_pD3D->CreateDevice(D3DADAPTER_DEFAULT, d3dCreateInfo[i].deviceType, s_wnd.hwnd, d3dCreateInfo[i].vertexProcessingType, &g_d3dpp, &s_wnd.pd3dDevice) >= 0;
		if(bOk) {
			BB_LOG("ImguiCore", "DeviceType: %d VertexProcessingType: %u", d3dCreateInfo[i].deviceType, d3dCreateInfo[i].vertexProcessingType);
		}
	}
	if(bOk) {
		ImGui_ImplWin32_Init(s_wnd.hwnd);
		ImGui_Image_Init(s_wnd.pd3dDevice);
		ImGui_ImplDX9_Init(s_wnd.pd3dDevice);
		Fonts_InitFonts();
		s_wnd.b3dValid = true;
	} else {
		s_wnd.pd3dDevice = nullptr;
	}

	s_wnd.b3dInitialized = true;
}

extern "C" HWND Imgui_Core_InitWindow(const char *classname, const char *title, HICON icon, WINDOWPLACEMENT wp_)
{
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, Imgui_Core_WndProc, 0L, 0L, GetModuleHandle(NULL), icon, LoadCursor(NULL, IDC_ARROW), NULL, NULL, classname, NULL };
	WINDOWPLACEMENT wp = wp_;
	s_wc = wc;
	RegisterClassEx(&s_wc);

	int x = 100;
	int y = 100;
	int w = 1280;
	int h = 800;
	b32 startHidden = cmdline_find("-hide") > 0 && !g_bCloseHidesWindow;
	if(wp.rcNormalPosition.right > wp.rcNormalPosition.left) {
		x = wp.rcNormalPosition.left;
		y = wp.rcNormalPosition.top;
		w = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
		h = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
	}
	s_wnd.hwnd = CreateWindow(classname, title, WS_OVERLAPPEDWINDOW, x, y, w, h, NULL, NULL, s_wc.hInstance, NULL);
	if(wp.rcNormalPosition.right > wp.rcNormalPosition.left) {
		if(wp.showCmd == SW_SHOWMINIMIZED) {
			wp.showCmd = SW_SHOWNORMAL;
		}
		if(startHidden && wp.showCmd == SW_SHOWMAXIMIZED) {
			wp.showCmd = SW_SHOWMINIMIZED;
			wp.flags |= WPF_RESTORETOMAXIMIZED;
		} else if(!startHidden) {
			SetWindowPlacement(s_wnd.hwnd, &wp);
		}
		wp.showCmd = wp_.showCmd;
	}
	BB_LOG("ImguiCore", "hwnd: %p", s_wnd.hwnd);

	if(s_wnd.hwnd) {
		Imgui_Core_InitD3D();
		if(wp.showCmd == SW_HIDE && !g_bCloseHidesWindow) {
			ShowWindow(s_wnd.hwnd, SW_SHOWDEFAULT);
		} else {
			if(startHidden) {
				ShowWindow(s_wnd.hwnd, SW_HIDE);
			} else {
				ShowWindow(s_wnd.hwnd, (int)wp.showCmd);
			}
		}
		UpdateWindow(s_wnd.hwnd);
		Time_StartNewFrame();
	}

	return s_wnd.hwnd;
}

extern "C" void Imgui_Core_ShutdownWindow(void)
{
	if(s_wnd.pd3dDevice) {
		ImGuiPlatformIO &PlatformIO = ImGui::GetPlatformIO();
		if(PlatformIO.Platform_DestroyWindow) {
			ImGuiViewport *mainViewport = ImGui::GetMainViewport();
			if(mainViewport) {
				PlatformIO.Platform_DestroyWindow(mainViewport);
			}
		}

		ImGui_ImplDX9_Shutdown();
		ImGui_Image_Shutdown();
		ImGui_ImplWin32_Shutdown();
		s_wnd.pd3dDevice->Release();
	}
}

b32 Imgui_Core_BeginFrame(void)
{
	MSG msg = { BB_EMPTY_INITIALIZER };
	if(PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if(msg.message == WM_QUIT) {
			Imgui_Core_RequestShutDown();
		}
		return false;
	}

	if(s_wnd.hwnd && !s_wnd.pd3dDevice) {
		Imgui_Core_InitD3D();
		if(!s_wnd.b3dValid) {
			BB_TICK();
			return false;
		}
	}

	if(!s_wnd.b3dValid) {
		Imgui_Core_ResetD3D();
	}

	if(g_needUpdateDpiDependentResources) {
		g_needUpdateDpiDependentResources = false;
		UpdateDpiDependentResources();
	}
	if(Fonts_UpdateAtlas()) {
		Imgui_Core_ResetD3D();
	}

	ImGui_Image_NewFrame();
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	BB_TICK();

	return true;
}

void Imgui_Core_EndFrame(ImVec4 clear_col)
{
	messageBoxes *boxes = mb_get_queue();
	if(!boxes->manualUpdate) {
		UIMessageBox_Update(boxes);
	}
	ImGui::EndFrame();

	ImGuiIO &io = ImGui::GetIO();
	bool requestRender = Imgui_Core_GetAndClearRequestRender() ||
	                     ImGui::IsAnyKeyPressedOrReleasedThisFrame() ||
	                     io.MouseHoveredViewport != 0 ||
	                     io.MouseWheel != 0.0f ||
	                     io.MouseWheelH != 0.0f;
	for(bool mouseDown : io.MouseDown) {
		if(mouseDown) {
			requestRender = true;
			break;
		}
	}
	if(!requestRender) {
		for(const ImGuiKeyData &keyData : io.KeysData) {
			if(keyData.Down) {
				requestRender = true;
				break;
			}
		}
	}

	// ImGui Rendering
	if(requestRender && s_wnd.pd3dDevice && s_wnd.b3dValid) {
		s_wnd.pd3dDevice->SetRenderState(D3DRS_ZENABLE, false);
		s_wnd.pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
		s_wnd.pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
		D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_col.x * 255.0f), (int)(clear_col.y * 255.0f), (int)(clear_col.z * 255.0f), (int)(clear_col.w * 255.0f));
		s_wnd.pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
		if(s_wnd.pd3dDevice->BeginScene() >= 0) {
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			s_wnd.pd3dDevice->EndScene();
		} else {
			ImGui::EndFrame();
			Imgui_Core_RequestRender();
		}
		if(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
		HRESULT hr = s_wnd.pd3dDevice->Present(NULL, NULL, NULL, NULL);
		if(FAILED(hr)) {
			bb_sleep_ms(100);
			Imgui_Core_ResetD3D();
			Imgui_Core_RequestRender();
		}
	} else {
		if(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			ImGui::UpdatePlatformWindows();
		}
		ImGui::EndFrame();
		bb_sleep_ms(15);
	}
	Time_StartNewFrame();
}
