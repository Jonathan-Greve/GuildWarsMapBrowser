//
// Main.cpp
//
#include "pch.h"
#include "MapBrowser.h"
#include "GuiGlobalConstants.h"
#include "InputManager.h"
#include "Extract_BASS_DLL_resource.h"
#include <filesystem>
#include <DbgHelp.h>

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* pExceptionPointers) {
    // Create mini dump file
    HANDLE hDumpFile = CreateFile(L"CrashDump.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
    dumpInfo.ExceptionPointers = pExceptionPointers;
    dumpInfo.ThreadId = GetCurrentThreadId();
    dumpInfo.ClientPointers = TRUE;

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, 
        static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs), 
        &dumpInfo, NULL, NULL);

    CloseHandle(hDumpFile);

    // Show error info to user
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    CONTEXT context = *pExceptionPointers->ContextRecord;

    STACKFRAME64 stackFrame;
    memset(&stackFrame, 0, sizeof(STACKFRAME64));

#ifdef _M_IX86 // Check whether the build is 32 or 64 bits
    int machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
    int machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rsp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#endif

    SymInitialize(process, NULL, TRUE);

    std::stringstream ss;
    ss << "Sorry! Guild Wars Map Browser just crashed unexpectedly.\n";
    ss << "A dump file has been created: \"CrashDump.dmp\".\n";
    ss << "Please contact the developers or create an issue on Github with the dump file attached if possible.\n\n";
    ss << "-------------------------------------------------------------------------------\n";
    ss << "Unhandled exception occurred.\nException Code: " << std::hex << pExceptionPointers->ExceptionRecord->ExceptionCode << std::endl;
    ss << "Call Stack:\n";

    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    DWORD displacement;

    while (StackWalk64(
        machineType, process, thread, &stackFrame, &context, NULL,
        SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {

        // Obtain the symbol for the address
        if (SymFromAddr(process, stackFrame.AddrPC.Offset, 0, symbol)) {
            ss << "Function: " << symbol->Name << " - Address: 0x" << std::hex << symbol->Address << std::endl;
        }

        // Try to obtain the file and line number for the address
        if (SymGetLineFromAddr64(process, stackFrame.AddrPC.Offset, &displacement, &line)) {
            ss << "File: " << line.FileName << " - Line: 0x" << std::hex << line.LineNumber << std::endl;
        }

        // Check for end of stack or invalid frame
        if (stackFrame.AddrPC.Offset == 0) {
            break;
        }
    }

    free(symbol);
    SymCleanup(process);

    MessageBoxA(NULL, ss.str().c_str(), "Critical Error", MB_ICONERROR | MB_OK);

    ExitProcess(pExceptionPointers->ExceptionRecord->ExceptionCode);

    return EXCEPTION_EXECUTE_HANDLER;
}

// BASS
extern LPFNBASSSTREAMCREATEFILE lpfnBassStreamCreateFile = nullptr;
extern LPFNBASSCHANNELPLAY lpfnBassChannelPlay = nullptr;
extern LPFNBASSCHANNELPAUSE lpfnBassChannelPause = nullptr;
extern LPFNBASSCHANNELSTOP lpfnBassChannelStop = nullptr;
extern LPFNBASSCHANNELBYTES2SECONDS lpfnBassChannelBytes2Seconds = nullptr;
extern LPFNBASSCHANNELGETLENGTH lpfnBassChannelGetLength = nullptr;
extern LPFNBASSSTREAMGETFILEPOSITION lpfnBassStreamGetFilePosition = nullptr;
extern LPFNBASSCHANNELGETINFO lpfnBassChannelGetInfo = nullptr;
extern LPFNBASSCHANNELFLAGS lpfnBassChannelFlags = nullptr;
extern LPFNBASSSTREAMFREE lpfnBassStreamFree = nullptr;
extern LPFNBASSCHANNELSETPOSITION lpfnBassChannelSetPosition = nullptr;
extern LPFNBASSCHANNELGETPOSITION lpfnBassChannelGetPosition = nullptr;
extern LPFNBASSCHANNELSECONDS2BYTES lpfnBassChannelSeconds2Bytes = nullptr;
extern LPFNBASSCHANNELSETATTRIBUTE lpfnBassChannelSetAttribute = nullptr;

// BASS_FX
extern LPFNBASSFXTMPOCREATE lpfnBassFxTempoCreate = nullptr;

using namespace DirectX;

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

extern bool is_bass_working = false;
extern HMODULE hBassDll = 0;
extern HMODULE hBassFxDll = 0;

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
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (! XMVerifyCPUSupport())
        return 1;

    HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
    if (FAILED(hr))
        return 1;


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

        // Load settings
        GuiGlobalConstants::LoadSettings();

        int x = CW_USEDEFAULT;
        int y = CW_USEDEFAULT;
        int w, h;

        if (GuiGlobalConstants::window_width != -1) {
            w = GuiGlobalConstants::window_width;
            h = GuiGlobalConstants::window_height;
            x = GuiGlobalConstants::window_pos_x;
            y = GuiGlobalConstants::window_pos_y;
        } else {
            g_map_browser->GetDefaultSize(w, h);
            RECT rc = { 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };
            AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
            w = rc.right - rc.left;
            h = rc.bottom - rc.top;
        }

        // Create window
        HWND hwnd = CreateWindowExW(0, L"GuildWarsMapBrowserWindowClass", g_szAppName, WS_OVERLAPPEDWINDOW,
            x, y, w, h,
            nullptr, nullptr, hInstance, nullptr);
        // TODO: Change to CreateWindowExW(WS_EX_TOPMOST, L"GuildWarsMapBrowserWindowClass", g_szAppName, WS_POPUP,
        // to default to fullscreen.

        g_input_manager = std::make_unique<InputManager>(hwnd);
        g_map_browser = std::make_unique<MapBrowser>(g_input_manager.get());

        if (! hwnd)
            return 1;

        int showCmd = nCmdShow;
        if (GuiGlobalConstants::window_width != -1 && GuiGlobalConstants::window_maximized) {
            showCmd = SW_SHOWMAXIMIZED;
        }
        ShowWindow(hwnd, showCmd);
        // TODO: Change nCmdShow to SW_SHOWMAXIMIZED to default to fullscreen.

        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(g_map_browser.get()));

        RECT rc;
        GetClientRect(hwnd, &rc);

        std::filesystem::path exePath = std::filesystem::current_path();
        std::filesystem::path bass_dllPath = exePath / "bass.dll";
        if (std::filesystem::exists(bass_dllPath) || extract_bass_dll_resource())
        {
            std::filesystem::path bass_fx_dllPath = exePath / "bass_fx.dll";
            if (std::filesystem::exists(bass_fx_dllPath) || extract_bass_fx_dll_resource())
            {

                hBassDll = LoadLibrary(TEXT("bass.dll"));
                hBassFxDll = LoadLibrary(TEXT("bass_fx.dll"));

                // Load the DLL
                if (hBassDll != NULL && hBassFxDll != NULL)
                {
                    // Get a pointer to the BASS_Init function
                    LPFNBASSINIT lpfnBassInit = (LPFNBASSINIT)GetProcAddress(hBassDll, "BASS_Init");
                    if (lpfnBassInit != NULL)
                    {
                        // Call BASS_Init through the function pointer
                        is_bass_working = lpfnBassInit(-1, 44100, 0, hwnd, NULL);
                    }

                    if (is_bass_working)
                    {
                        lpfnBassStreamCreateFile =
                            (LPFNBASSSTREAMCREATEFILE)GetProcAddress(hBassDll, "BASS_StreamCreateFile");
                        lpfnBassChannelPlay =
                            (LPFNBASSCHANNELPLAY)GetProcAddress(hBassDll, "BASS_ChannelPlay");
                        lpfnBassChannelPause =
                            (LPFNBASSCHANNELPAUSE)GetProcAddress(hBassDll, "BASS_ChannelPause");
                        lpfnBassChannelStop =
                            (LPFNBASSCHANNELSTOP)GetProcAddress(hBassDll, "BASS_ChannelStop");
                        lpfnBassChannelBytes2Seconds =
                            (LPFNBASSCHANNELBYTES2SECONDS)GetProcAddress(hBassDll, "BASS_ChannelBytes2Seconds");
                        lpfnBassChannelGetLength =
                            (LPFNBASSCHANNELGETLENGTH)GetProcAddress(hBassDll, "BASS_ChannelGetLength");
                        lpfnBassStreamGetFilePosition = (LPFNBASSSTREAMGETFILEPOSITION)GetProcAddress(
                            hBassDll, "BASS_StreamGetFilePosition");
                        lpfnBassChannelGetInfo =
                            (LPFNBASSCHANNELGETINFO)GetProcAddress(hBassDll, "BASS_ChannelGetInfo");
                        lpfnBassChannelFlags =
                            (LPFNBASSCHANNELFLAGS)GetProcAddress(hBassDll, "BASS_ChannelFlags");
                        lpfnBassStreamFree = (LPFNBASSSTREAMFREE)GetProcAddress(hBassDll, "BASS_StreamFree");
                        lpfnBassChannelSetPosition =
                            (LPFNBASSCHANNELSETPOSITION)GetProcAddress(hBassDll, "BASS_ChannelSetPosition");
                        lpfnBassChannelGetPosition =
                            (LPFNBASSCHANNELGETPOSITION)GetProcAddress(hBassDll, "BASS_ChannelGetPosition");
                        lpfnBassChannelSeconds2Bytes =
                            (LPFNBASSCHANNELSECONDS2BYTES)GetProcAddress(hBassDll, "BASS_ChannelSeconds2Bytes");
                        lpfnBassChannelSetAttribute =
                            (LPFNBASSCHANNELSETATTRIBUTE)GetProcAddress(hBassDll, "BASS_ChannelSetAttribute");
                        lpfnBassFxTempoCreate =
                            (LPFNBASSFXTMPOCREATE)GetProcAddress(hBassFxDll, "BASS_FX_TempoCreate");
                    }
                }
            }
        }

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

static void UpdateWindowSettings(HWND hWnd)
{
    WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
    if (GetWindowPlacement(hWnd, &wp)) {
        // Only update if we have valid data (not completely zero)
        // Check if window is visible? No, GetWindowPlacement works.
        
        // Don't save if minimized, keep last known restored pos
        if (wp.showCmd != SW_SHOWMINIMIZED) {
            GuiGlobalConstants::window_maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
            
            // rcNormalPosition gives the restored position even if maximized
            GuiGlobalConstants::window_width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
            GuiGlobalConstants::window_height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
            GuiGlobalConstants::window_pos_x = wp.rcNormalPosition.left;
            GuiGlobalConstants::window_pos_y = wp.rcNormalPosition.top;
        }
    }
}

// Windows procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

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

        //case WM_MOUSEMOVE:
        //    g_input_manager->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam, hWnd);
        //    break;
    case WM_INPUT:
    {
        g_input_manager->ProcessRawInput(lParam);
        break;
    }
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
    case WM_MOUSELEAVE:
        g_input_manager->OnMouseLeave(hWnd);
        break;
    case WM_PAINT:
        if (s_in_sizemove && map_browser)
        {
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
        UpdateWindowSettings(hWnd);
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
        UpdateWindowSettings(hWnd);
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
        UpdateWindowSettings(hWnd);
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
        UpdateWindowSettings(hWnd);
        GuiGlobalConstants::SaveSettings();
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

    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Exit helper
void ExitMapBrowser() noexcept { PostQuitMessage(0); }
