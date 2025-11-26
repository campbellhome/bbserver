// Copyright (c) Matt Campbell
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
#include "wrap_dx11.h"
#include "wrap_imgui.h"
#include "wrap_shellscalingapi.h"

#include <locale.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

static bool ImGui_Core_CreateDeviceD3D(HWND hWnd);
static void ImGui_Core_CleanupDeviceD3D();
static void ImGui_Core_CreateRenderTarget();
static void ImGui_Core_CleanupRenderTarget();

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static WNDCLASSEX s_wc;
static HWND s_wnd;
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

extern "C" b32 Imgui_Core_Init(const char* cmdline)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	Style_Init();

	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE); // TODO: ImGui_ImplWin32_EnableDpiAwareness for V2

	s_hWinEventHook = SetWinEventHook(
	    EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
	    NULL, Imgui_Core_WinEventProc, 0, 0,
	    WINEVENT_OUTOFCONTEXT);

	if (!cmdline_argc())
	{
		g_setCmdline = true;
		cmdline_init_composite(cmdline);
	}

	Imgui_Core_Freetype_Init();
	Fonts_Init();

	return true;
}

extern "C" void Imgui_Core_ShutdownWindow(void)
{
	ImGuiPlatformIO& PlatformIO = ImGui::GetPlatformIO();
	if (PlatformIO.Platform_DestroyWindow)
	{
		ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		if (mainViewport)
		{
			PlatformIO.Platform_DestroyWindow(mainViewport);
		}
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_Image_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::InputTextShutdown();
	Fonts_Shutdown();

	ImGui_Core_CleanupDeviceD3D();
	DestroyWindow(s_wnd);
	UnregisterClassA(s_wc.lpszClassName, s_wc.hInstance);

	ImGui::DestroyContext();

	Imgui_Core_Freetype_Shutdown();
}

extern "C" void Imgui_Core_Shutdown(void)
{
	mb_shutdown(nullptr);

	if (s_hWinEventHook)
	{
		UnhookWinEvent(s_hWinEventHook);
	}

	if (g_setCmdline)
	{
		cmdline_shutdown();
	}

	sb_reset(&g_colorscheme);
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
	if (s_wnd)
	{
		WINDOWPLACEMENT wp = { BB_EMPTY_INITIALIZER };
		wp.length = sizeof(wp);
		GetWindowPlacement(s_wnd, &wp);
		if (wp.showCmd == SW_SHOWMINIMIZED)
		{
			ShowWindow(s_wnd, SW_RESTORE);
			Imgui_Core_BringWindowToFront();
		}
		else if (!IsWindowVisible(s_wnd))
		{
			ShowWindow(s_wnd, SW_SHOW);
			Imgui_Core_BringWindowToFront();
		}
		else
		{
			ShowWindow(s_wnd, SW_HIDE);
		}
	}
}

extern "C" void Imgui_Core_HideWindow(void)
{
	if (s_wnd)
	{
		ShowWindow(s_wnd, SW_HIDE);
	}
}

extern "C" void Imgui_Core_UnhideWindow(void)
{
	if (s_wnd)
	{
		if (IsIconic(s_wnd))
		{
			ShowWindow(s_wnd, SW_RESTORE);
		}
		else
		{
			ShowWindow(s_wnd, SW_SHOW);
		}
		Imgui_Core_RequestRender();
	}
}

extern "C" void Imgui_Core_MinimizeWindow(void)
{
	if (s_wnd)
	{
		ShowWindow(s_wnd, SW_MINIMIZE);
	}
}

extern "C" void Imgui_Core_BringWindowToFront(void)
{
	if (s_wnd)
	{
		if (!BringWindowToTop(s_wnd) && g_bDebugFocusChange)
		{
			system_error_to_log(GetLastError(), "Window", "BringWindowToTop");
		}
		if (!SetForegroundWindow(s_wnd) && g_bDebugFocusChange)
		{
			system_error_to_log(GetLastError(), "Window", "SetForegroundWindow");
		}
		if (!SetFocus(s_wnd) && g_bDebugFocusChange)
		{
			system_error_to_log(GetLastError(), "Window", "SetFocus");
		}
		Imgui_Core_RequestRender();
	}
}

extern "C" void Imgui_Core_FlashWindow(b32 bFlash)
{
	if (s_wnd)
	{
		FLASHWINFO info = { BB_EMPTY_INITIALIZER };
		info.cbSize = sizeof(FLASHWINFO);
		info.hwnd = s_wnd;
		info.dwFlags = bFlash ? (FLASHW_ALL | FLASHW_TIMERNOFG) : 0u;
		FlashWindowEx(&info);
	}
}

extern "C" void Imgui_Core_SetWindowText(const char* text)
{
	if (s_wnd)
	{
		SetWindowTextA(s_wnd, text);
	}
}

extern "C" void Imgui_Core_SetDpiScale(float dpiScale)
{
	if (g_dpiScale != dpiScale)
	{
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

extern "C" void Imgui_Core_SetColorScheme(const char* colorscheme)
{
	if (strcmp(sb_get(&g_colorscheme), colorscheme))
	{
		sb_reset(&g_colorscheme);
		sb_append(&g_colorscheme, colorscheme);
		Imgui_Core_QueueUpdateDpiDependentResources();
	}
}

extern "C" const char* Imgui_Core_GetColorScheme(void)
{
	return sb_get(&g_colorscheme);
}

static const char* Update_AnnotateVersion(updateManifest_t* manifest, const char* version)
{
	const char* annotated = version;
	if (version && !bb_stricmp(version, sb_get(&manifest->stable)))
	{
		annotated = va("%s (stable)", version);
	}
	else if (version && !bb_stricmp(version, sb_get(&manifest->latest)))
	{
		annotated = va("%s (latest)", version);
	}
	return annotated;
}

extern "C" void Update_Menu(void)
{
	if (ImGui::BeginMenu("Update"))
	{
		updateManifest_t* manifest = Update_GetManifest();
		const char* currentVersion = Update_GetCurrentVersion();
		const char* currentVersionAnnotated = Update_AnnotateVersion(manifest, currentVersion);
		ImGui::MenuItem(va("version %s", *currentVersionAnnotated ? currentVersionAnnotated : "unknown"), nullptr, false, false);
		if (ImGui::MenuItem("Check for updates"))
		{
			Update_CheckForUpdates(false);
		}
		if (ImGui::BeginMenu("Set desired version"))
		{
			if (ImGui::MenuItem("stable", nullptr, Update_IsDesiredVersion("stable") != 0))
			{
				Update_SetDesiredVersion("stable");
			}
			if (ImGui::MenuItem("latest", nullptr, Update_IsDesiredVersion("latest") != 0))
			{
				Update_SetDesiredVersion("latest");
			}
			for (u32 i = 0; i < (manifest ? manifest->versions.count : 0); ++i)
			{
				updateVersion_t* version = manifest->versions.data + i;
				const char* versionName = sb_get(&version->name);
				if (ImGui::MenuItem(Update_AnnotateVersion(manifest, versionName), nullptr, Update_IsDesiredVersion(versionName) != 0))
				{
					Update_SetDesiredVersion(versionName);
				}
			}
			ImGui::EndMenu();
		}
		if (Update_GetData()->showUpdateManagement && *currentVersion && !Update_IsStableVersion(currentVersion))
		{
			if (ImGui::MenuItem(va("Promote %s to stable version", currentVersion)))
			{
				Update_SetStableVersion(currentVersion);
			}
		}
		if (g_updateIgnoredVersion)
		{
			if (ImGui::MenuItem(va("Update to version %u and restart", g_updateIgnoredVersion)))
			{
				Update_RestartAndUpdate(g_updateIgnoredVersion);
			}
		}
		ImGui::EndMenu();
	}
}

void UpdateDpiDependentResources()
{
	Fonts_InitFonts();
	Style_Apply(sb_get(&g_colorscheme));
}

extern "C" void Imgui_Core_QueueUpdateDpiDependentResources(void)
{
	g_needUpdateDpiDependentResources = true;
}

extern "C" b32 Imgui_Core_IsUpdateDpiDependentResourcesQueued(void)
{
	return g_needUpdateDpiDependentResources;
}

BOOL EnableNonClientDpiScalingShim(_In_ HWND hwnd)
{
	HMODULE hModule = GetModuleHandleA("User32.dll");
	if (hModule)
	{
		typedef BOOL(WINAPI * Proc)(_In_ HWND hwnd);
		Proc proc = (Proc)(void*)(GetProcAddress(hModule, "EnableNonClientDpiScaling"));
		if (proc)
		{
			return proc(hwnd);
		}
	}
	return FALSE;
}

UINT GetDpiForWindowShim(_In_ HWND hwnd)
{
	HMODULE hModule = GetModuleHandleA("User32.dll");
	if (hModule)
	{
		typedef UINT(WINAPI * Proc)(_In_ HWND hwnd);
		Proc proc = (Proc)(void*)(GetProcAddress(hModule, "GetDpiForWindow"));
		if (proc)
		{
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
	if (hwnd &&
	    idObject == OBJID_WINDOW &&
	    idChild == CHILDID_SELF &&
	    event == EVENT_SYSTEM_FOREGROUND)
	{
		DWORD processId = 0;
		b32 bSameProcess = false;
		if (GetWindowThreadProcessId(hwnd, &processId))
		{
			bSameProcess = processId == GetCurrentProcessId();
		}
		if (bSameProcess)
		{
			g_hasFocus = true;
		}
		else if (g_hasFocus)
		{
			g_hasFocus = false;
		}
	}
}

static Imgui_Core_UserWndProc* g_userWndProc;
void Imgui_Core_SetUserWndProc(Imgui_Core_UserWndProc* wndProc)
{
	g_userWndProc = wndProc;
}

LRESULT WINAPI Imgui_Core_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_NCCREATE:
		EnableNonClientDpiScalingShim(hWnd);
		g_dpi = (int)GetDpiForWindowShim(hWnd);
		g_dpiScale = (float)g_dpi / (float)USER_DEFAULT_SCREEN_DPI;
		Style_Apply(sb_get(&g_colorscheme));
		break;
	case WM_DPICHANGED:
	{
		g_dpi = HIWORD(wParam);
		g_dpiScale = (float)g_dpi / (float)USER_DEFAULT_SCREEN_DPI;
		UpdateDpiDependentResources();

		RECT* const prcNewWindow = (RECT*)lParam;
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
		if (!g_trackingMouse)
		{
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
		for (size_t i = 0; i < BB_ARRAYSIZE(ImGui::GetIO().MouseDown); ++i)
		{
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
		if (g_bCloseHidesWindow)
		{
			ShowWindow(hWnd, SW_HIDE);
			return 0;
		}
	default:
		break;
	}

	IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendPlatformUserData)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		{
			return true;
		}
	}
	else if (s_wnd && ImGui::GetIO().BackendFlags != 0)
	{
		BB_WARNING("ImguiCore", "skipped ImGui_ImplWin32_WndProcHandler w/o BackendPlatformUserData");
	}

	if (g_userWndProc)
	{
		LRESULT userResult = (*g_userWndProc)(hWnd, msg, wParam, lParam);
		if (userResult)
		{
			return userResult;
		}
	}

	LRESULT result = Update_HandleWindowMessage(hWnd, msg, wParam, lParam);
	if (result)
	{
		return result;
	}

	switch (msg)
	{
	case WM_MOVE:
		Imgui_Core_RequestRender();
		Imgui_Core_DirtyWindowPlacement();
		break;
	case WM_SIZE:
		Imgui_Core_RequestRender();
		Imgui_Core_DirtyWindowPlacement();
		if (wParam != SIZE_MINIMIZED)
		{
			// Queue resize
			g_ResizeWidth = (UINT)LOWORD(lParam);
			g_ResizeHeight = (UINT)HIWORD(lParam);
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool ImGui_Core_CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	// This is a basic setup. Optimally could use e.g. DXGI_SWAP_EFFECT_FLIP_DISCARD and handle fullscreen mode differently. See #8979 for suggestions.
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	// createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0,
	};
	HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
		res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res != S_OK)
		return false;

	// Disable DXGI's default Alt+Enter fullscreen behavior.
	// - You are free to leave this enabled, but it will not work properly with multiple viewports.
	// - This must be done for all windows associated to the device. Our DX11 backend does this automatically for secondary viewports that it creates.
	IDXGIFactory* pSwapChainFactory;
	if (SUCCEEDED(g_pSwapChain->GetParent(IID_PPV_ARGS(&pSwapChainFactory))))
	{
		pSwapChainFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
		pSwapChainFactory->Release();
	}

	ImGui_Core_CreateRenderTarget();
	return true;
}

void ImGui_Core_CleanupDeviceD3D()
{
	ImGui_Core_CleanupRenderTarget();
	if (g_pSwapChain)
	{
		g_pSwapChain->Release();
		g_pSwapChain = nullptr;
	}
	if (g_pd3dDeviceContext)
	{
		g_pd3dDeviceContext->Release();
		g_pd3dDeviceContext = nullptr;
	}
	if (g_pd3dDevice)
	{
		g_pd3dDevice->Release();
		g_pd3dDevice = nullptr;
	}
}

void ImGui_Core_CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

void ImGui_Core_CleanupRenderTarget()
{
	if (g_mainRenderTargetView)
	{
		g_mainRenderTargetView->Release();
		g_mainRenderTargetView = nullptr;
	}
}

extern "C" HWND Imgui_Core_InitWindow(const char* classname, const char* title, HICON icon, WINDOWPLACEMENT wp_)
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
	if (wp.rcNormalPosition.right > wp.rcNormalPosition.left)
	{
		x = wp.rcNormalPosition.left;
		y = wp.rcNormalPosition.top;
		w = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
		h = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
	}
	s_wnd = CreateWindow(classname, title, WS_OVERLAPPEDWINDOW, x, y, w, h, NULL, NULL, s_wc.hInstance, NULL);
	if (wp.rcNormalPosition.right > wp.rcNormalPosition.left)
	{
		if (wp.showCmd == SW_SHOWMINIMIZED)
		{
			wp.showCmd = SW_SHOWNORMAL;
		}
		if (startHidden && wp.showCmd == SW_SHOWMAXIMIZED)
		{
			wp.showCmd = SW_SHOWMINIMIZED;
			wp.flags |= WPF_RESTORETOMAXIMIZED;
		}
		else if (!startHidden)
		{
			SetWindowPlacement(s_wnd, &wp);
		}
		wp.showCmd = wp_.showCmd;
	}
	BB_LOG("ImguiCore", "hwnd: %p", s_wnd);

	if (s_wnd)
	{
		if (!ImGui_Core_CreateDeviceD3D(s_wnd))
		{
			ImGui_Core_CleanupDeviceD3D();
			DestroyWindow(s_wnd);
			UnregisterClassA(wc.lpszClassName, wc.hInstance);
			return 0;
		}

		if (wp.showCmd == SW_HIDE && !g_bCloseHidesWindow)
		{
			ShowWindow(s_wnd, SW_SHOWDEFAULT);
		}
		else
		{
			if (startHidden)
			{
				ShowWindow(s_wnd, SW_HIDE);
			}
			else
			{
				ShowWindow(s_wnd, (int)wp.showCmd);
			}
		}
		UpdateWindow(s_wnd);
		Time_StartNewFrame();

		ImGui_ImplWin32_Init(s_wnd);
		ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
		ImGui_Image_Init(g_pd3dDevice);
		Fonts_InitFonts();
	}

	return s_wnd;
}

b32 Imgui_Core_BeginFrame(void)
{
	MSG msg = { BB_EMPTY_INITIALIZER };
	if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT)
		{
			Imgui_Core_RequestShutDown();
		}
		return false;
	}

	if (g_needUpdateDpiDependentResources)
	{
		g_needUpdateDpiDependentResources = false;
		UpdateDpiDependentResources();
	}
	if (Fonts_UpdateAtlas())
	{
		// Imgui_Core_ResetD3D();
	}

	ImGui_Image_NewFrame();
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	BB_TICK();

	return true;
}

void Imgui_Core_EndFrame(ImVec4 clear_color)
{
	messageBoxes* boxes = mb_get_queue();
	if (!boxes->manualUpdate)
	{
		UIMessageBox_Update(boxes);
	}
	ImGui::EndFrame();

	ImGuiIO& io = ImGui::GetIO();
	bool requestRender = Imgui_Core_GetAndClearRequestRender() ||
	                     ImGui::IsAnyKeyPressedOrReleasedThisFrame() ||
	                     io.MouseHoveredViewport != 0 ||
	                     io.MouseWheel != 0.0f ||
	                     io.MouseWheelH != 0.0f;
	for (bool mouseDown : io.MouseDown)
	{
		if (mouseDown)
		{
			requestRender = true;
			break;
		}
	}
	if (!requestRender)
	{
		for (const ImGuiKeyData& keyData : io.KeysData)
		{
			if (keyData.Down)
			{
				requestRender = true;
				break;
			}
		}
	}

	// ImGui Rendering
	if (requestRender)
	{
		ImGui::Render();
		const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
		g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
		HRESULT hr = g_pSwapChain->Present(1, 0); // Present with vsync
		// HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
		g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
	}
	else
	{
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
		}
		ImGui::EndFrame();
		bb_sleep_ms(15);
	}
	Time_StartNewFrame();
}

#if 0 // TODO
#endif
