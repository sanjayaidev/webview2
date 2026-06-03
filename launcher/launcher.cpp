//
// launcher.cpp — Win32 + WebView2 host for the Node.js automation app
// Starts backend.exe as a child process, opens a WebView2 window,
// and tears everything down cleanly when the window is closed.
//
// Build requirements:
//   - Microsoft.Web.WebView2 NuGet package (headers + WebView2LoaderStatic.lib)
//   - Microsoft.Windows.ImplementationLibrary NuGet package (wil/com.h)
//   - MSVC with /std:c++17, linked against:
//       WebView2LoaderStatic.lib user32.lib ole32.lib shell32.lib shlwapi.lib
//

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include <string>
#include <thread>
#include <chrono>
#include <shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

using namespace Microsoft::WRL;

// ── Configuration ─────────────────────────────────────────────────────────────
static const wchar_t* APP_TITLE      = L"Automation App";
static const int      WINDOW_W       = 1100;
static const int      WINDOW_H       = 720;
static const int      BACKEND_PORT   = 3721;
static const wchar_t* BACKEND_EXE    = L"backend.exe";

// ── Globals ───────────────────────────────────────────────────────────────────
static HWND                              g_hwnd        = nullptr;
static wil::com_ptr<ICoreWebView2>       g_webview;
static wil::com_ptr<ICoreWebView2Controller> g_controller;
static PROCESS_INFORMATION               g_backendProc = {};
static bool                              g_backendStarted = false;

// ── Forward declarations ──────────────────────────────────────────────────────
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void StartBackend();
void StopBackend();
void InitWebView(HWND hwnd);
std::wstring GetExeDir();

// ── Entry point ───────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // Initialize COM
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        MessageBoxW(nullptr, L"Failed to initialize COM.", APP_TITLE, MB_ICONERROR);
        return 1;
    }

    // Register window class
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"AutomationAppClass";
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    // Center window on primary monitor
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - WINDOW_W) / 2;
    int y = (screenH - WINDOW_H) / 2;

    g_hwnd = CreateWindowExW(
        0, L"AutomationAppClass", APP_TITLE,
        WS_OVERLAPPEDWINDOW,
        x, y, WINDOW_W, WINDOW_H,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hwnd) {
        MessageBoxW(nullptr, L"Failed to create window.", APP_TITLE, MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    // Start Node.js backend
    StartBackend();

    // Show window
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    // Init WebView2 (async, posts WM_USER when ready)
    InitWebView(g_hwnd);

    // Message loop
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    StopBackend();
    CoUninitialize();
    return (int)msg.wParam;
}

// ── Window procedure ──────────────────────────────────────────────────────────
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_SIZE:
            if (g_controller) {
                RECT bounds;
                GetClientRect(hwnd, &bounds);
                g_controller->put_Bounds(bounds);
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        // WM_USER+1: WebView2 environment ready — navigate to backend
        case WM_USER + 1: {
            if (g_controller) {
                RECT bounds;
                GetClientRect(hwnd, &bounds);
                g_controller->put_Bounds(bounds);
                std::wstring url = L"http://localhost:" + std::to_wstring(BACKEND_PORT);
                g_webview->Navigate(url.c_str());
            }
            return 0;
        }

        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

// ── Start backend.exe as a child process ─────────────────────────────────────
void StartBackend()
{
    std::wstring exeDir = GetExeDir();
    std::wstring backendPath = exeDir + L"\\" + BACKEND_EXE;

    // Check backend exists
    if (!PathFileExistsW(backendPath.c_str())) {
        MessageBoxW(g_hwnd,
            (L"Cannot find backend:\n" + backendPath).c_str(),
            APP_TITLE, MB_ICONERROR);
        return;
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // hide the console window

    std::wstring cmdLine = L"\"" + backendPath + L"\"";

    if (!CreateProcessW(
            nullptr,
            &cmdLine[0],
            nullptr, nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            exeDir.c_str(),
            &si,
            &g_backendProc))
    {
        MessageBoxW(g_hwnd, L"Failed to start backend.exe.", APP_TITLE, MB_ICONERROR);
        return;
    }

    g_backendStarted = true;

    // Give backend a moment to bind its port before WebView navigates
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
}

// ── Stop backend process ──────────────────────────────────────────────────────
void StopBackend()
{
    if (g_backendStarted) {
        TerminateProcess(g_backendProc.hProcess, 0);
        CloseHandle(g_backendProc.hProcess);
        CloseHandle(g_backendProc.hThread);
        g_backendStarted = false;
    }
}

// ── Initialize WebView2 environment ──────────────────────────────────────────
void InitWebView(HWND hwnd)
{
    std::wstring userDataFolder = GetExeDir() + L"\\webview2_data";

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr,                    // use default Edge/WebView2 runtime
        userDataFolder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
            {
                if (FAILED(result) || !env) {
                    MessageBoxW(hwnd,
                        L"WebView2 runtime not found.\n\n"
                        L"Please install the Microsoft Edge WebView2 Runtime:\n"
                        L"https://developer.microsoft.com/microsoft-edge/webview2/",
                        APP_TITLE, MB_ICONERROR);
                    PostQuitMessage(1);
                    return result;
                }

                env->CreateCoreWebView2Controller(hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            if (FAILED(result) || !controller) {
                                MessageBoxW(hwnd, L"Failed to create WebView2 controller.",
                                    APP_TITLE, MB_ICONERROR);
                                PostQuitMessage(1);
                                return result;
                            }

                            g_controller = controller;
                            g_controller->get_CoreWebView2(&g_webview);

                            // Disable context menu and status bar for cleaner look
                            wil::com_ptr<ICoreWebView2Settings> settings;
                            g_webview->get_Settings(&settings);
                            if (settings) {
                                settings->put_AreDefaultContextMenusEnabled(FALSE);
                                settings->put_IsStatusBarEnabled(FALSE);
                                settings->put_AreDevToolsEnabled(TRUE); // allow F12 for debugging
                            }

                            // Signal window to resize + navigate
                            PostMessageW(hwnd, WM_USER + 1, 0, 0);
                            return S_OK;
                        }
                    ).Get()
                );
                return S_OK;
            }
        ).Get()
    );
}

// ── Get directory of the running .exe ────────────────────────────────────────
std::wstring GetExeDir()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return std::wstring(path);
}
