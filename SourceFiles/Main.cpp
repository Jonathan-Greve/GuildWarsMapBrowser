//
// Main.cpp
//

#include "pch.h"
#include "MapBrowser.h"
#include "InputManager.h"

using namespace DirectX;

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

namespace
{
std::unique_ptr<MapBrowser> g_map_browser;
std::unique_ptr<InputManager> g_input_manager;
}

LPCWSTR g_szAppName = L"GuildWarsMapBrowser";

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void ExitMapBrowser() noexcept;

// Indicates to hybrid graphics systems to prefer the discrete part by default
extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// Entry point
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (! XMVerifyCPUSupport())
        return 1;

    HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
    if (FAILED(hr))
        return 1;

    g_input_manager = std::make_unique<InputManager>();
    g_map_browser = std::make_unique<MapBrowser>(g_input_manager.get());

    // Register class and create window
    {
        // Register class
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIconW(hInstance, L"IDI_ICON");
        wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wcex.lpszClassName = L"GuildWarsMapBrowserWindowClass";
        wcex.hIconSm = LoadIconW(wcex.hInstance, L"IDI_ICON");
        if (! RegisterClassExW(&wcex))
            return 1;

        // Create window
        int w, h;
        g_map_browser->GetDefaultSize(w, h);

        RECT rc = {0, 0, static_cast<LONG>(w), static_cast<LONG>(h)};

        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        HWND hwnd = CreateWindowExW(0, L"GuildWarsMapBrowserWindowClass", g_szAppName, WS_OVERLAPPEDWINDOW,
                                    CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
                                    nullptr, nullptr, hInstance, nullptr);
        // TODO: Change to CreateWindowExW(WS_EX_TOPMOST, L"GuildWarsMapBrowserWindowClass", g_szAppName, WS_POPUP,
        // to default to fullscreen.

        if (! hwnd)
            return 1;

        ShowWindow(hwnd, nCmdShow);
        // TODO: Change nCmdShow to SW_SHOWMAXIMIZED to default to fullscreen.

        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(g_map_browser.get()));

        GetClientRect(hwnd, &rc);

        g_map_browser->Initialize(hwnd, rc.right - rc.left, rc.bottom - rc.top);
    }

    // Main message loop
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            g_map_browser->Tick();
        }
    }

    g_map_browser.reset();

    CoUninitialize();

    return static_cast<int>(msg.wParam);
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

// Windows procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    static bool s_in_sizemove = false;
    static bool s_in_suspend = false;
    static bool s_minimized = false;
    static bool s_fullscreen = false;
    // TODO: Set s_fullscreen to true if defaulting to fullscreen.

    auto map_browser = reinterpret_cast<MapBrowser*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_KEYDOWN:
        g_input_manager->OnKeyDown(static_cast<UINT>(wParam), hWnd);
        break;

    case WM_KEYUP:
        g_input_manager->OnKeyUp(static_cast<UINT>(wParam), hWnd);
        break;

    case WM_MOUSEMOVE:
        g_input_manager->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam, hWnd);
        break;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        g_input_manager->OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam, hWnd, message);
        break;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        g_input_manager->OnMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam, hWnd, message);
        break;
    case WM_MOUSEWHEEL:
        g_input_manager->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam), hWnd);
        break;
    case WM_PAINT:
        if (s_in_sizemove && map_browser)
        {
            map_browser->Tick();
        }
        else
        {
            PAINTSTRUCT ps;
            std::ignore = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_DISPLAYCHANGE:
        if (map_browser)
        {
            map_browser->OnDisplayChange();
        }
        break;

    case WM_MOVE:
        if (map_browser)
        {
            map_browser->OnWindowMoved();
        }
        break;

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            if (! s_minimized)
            {
                s_minimized = true;
                if (! s_in_suspend && map_browser)
                    map_browser->OnSuspending();
                s_in_suspend = true;
            }
        }
        else if (s_minimized)
        {
            s_minimized = false;
            if (s_in_suspend && map_browser)
                map_browser->OnResuming();
            s_in_suspend = false;
        }
        else if (! s_in_sizemove && map_browser)
        {
            map_browser->OnWindowSizeChanged(LOWORD(lParam), HIWORD(lParam));
        }
        break;

    case WM_ENTERSIZEMOVE:
        s_in_sizemove = true;
        break;

    case WM_EXITSIZEMOVE:
        s_in_sizemove = false;
        if (map_browser)
        {
            RECT rc;
            GetClientRect(hWnd, &rc);

            map_browser->OnWindowSizeChanged(rc.right - rc.left, rc.bottom - rc.top);
        }
        break;

    case WM_GETMINMAXINFO:
        if (lParam)
        {
            auto info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 320;
            info->ptMinTrackSize.y = 200;
        }
        break;

    case WM_ACTIVATEAPP:
        if (map_browser)
        {
            if (wParam)
            {
                map_browser->OnActivated();
            }
            else
            {
                map_browser->OnDeactivated();
            }
        }
        break;

    case WM_POWERBROADCAST:
        switch (wParam)
        {
        case PBT_APMQUERYSUSPEND:
            if (! s_in_suspend && map_browser)
                map_browser->OnSuspending();
            s_in_suspend = true;
            return TRUE;

        case PBT_APMRESUMESUSPEND:
            if (! s_minimized)
            {
                if (s_in_suspend && map_browser)
                    map_browser->OnResuming();
                s_in_suspend = false;
            }
            return TRUE;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_SYSKEYDOWN:
        if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000)
        {
            // Implements the classic ALT+ENTER fullscreen toggle
            if (s_fullscreen)
            {
                SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, 0);

                int width = 800;
                int height = 600;
                if (map_browser)
                    map_browser->GetDefaultSize(width, height);

                ShowWindow(hWnd, SW_SHOWNORMAL);

                SetWindowPos(hWnd, HWND_TOP, 0, 0, width, height,
                             SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }
            else
            {
                SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);

                SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

                ShowWindow(hWnd, SW_SHOWMAXIMIZED);
            }

            s_fullscreen = ! s_fullscreen;
        }
        break;

    case WM_MENUCHAR:
        // A menu is active and the user presses a key that does not correspond
        // to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
        return MAKELRESULT(0, MNC_CLOSE);
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Exit helper
void ExitMapBrowser() noexcept { PostQuitMessage(0); }
