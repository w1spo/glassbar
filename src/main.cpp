#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")


// ============================================================
// Application
// ============================================================

constexpr wchar_t APP_NAME[] = L"TaskbarGlass";
constexpr wchar_t WINDOW_CLASS[] = L"TaskbarGlassWindow";
constexpr wchar_t MUTEX_NAME[] = L"TaskbarGlass_SingleInstance";

constexpr UINT WM_TRAY = WM_APP + 1;

constexpr UINT ID_TOGGLE = 1001;
constexpr UINT ID_AUTOSTART = 1002;
constexpr UINT ID_OPACITY_25 = 1003;
constexpr UINT ID_OPACITY_50 = 1004;
constexpr UINT ID_OPACITY_75 = 1005;
constexpr UINT ID_OPACITY_100 = 1006;
constexpr UINT ID_RESET = 1007;
constexpr UINT ID_EXIT = 1008;

// ============================================================
// Settings
// ============================================================

struct Settings
{
    bool transparent = true;
    bool autostart = true;

    // 0 = completely invisible
    // 255 = completely opaque
    BYTE opacity = 255;
};

Settings g_settings;

HWND g_window = nullptr;
HINSTANCE g_instance = nullptr;

NOTIFYICONDATAW g_tray{};

UINT g_taskbarCreatedMessage = 0;

// ============================================================
// Paths
// ============================================================

std::wstring GetAppDataDirectory()
{
    PWSTR rawPath = nullptr;

    if (SUCCEEDED(
        SHGetKnownFolderPath(
            FOLDERID_RoamingAppData,
            KF_FLAG_DEFAULT,
            nullptr,
            &rawPath)))
    {
        std::wstring path(rawPath);

        CoTaskMemFree(rawPath);

        path += L"\\TaskbarGlass";

        CreateDirectoryW(
            path.c_str(),
            nullptr);

        return path;
    }

    return L".";
}

std::wstring GetSettingsPath()
{
    return GetAppDataDirectory() + L"\\settings.ini";
}

// ============================================================
// Settings
// ============================================================

void LoadSettings()
{
    const std::wstring path = GetSettingsPath();

    std::wifstream file(path);

    if (!file)
        return;

    std::wstring line;

    while (std::getline(file, line))
    {
        if (line == L"transparent=1")
        {
            g_settings.transparent = true;
        }
        else if (line == L"transparent=0")
        {
            g_settings.transparent = false;
        }
        else if (line == L"autostart=1")
        {
            g_settings.autostart = true;
        }
        else if (line == L"autostart=0")
        {
            g_settings.autostart = false;
        }
        else if (line.rfind(L"opacity=", 0) == 0)
        {
            int value = _wtoi(
                line.substr(8).c_str());

            value = std::clamp(
                value,
                0,
                255);

            g_settings.opacity =
                static_cast<BYTE>(value);
        }
    }
}

void SaveSettings()
{
    std::wofstream file(GetSettingsPath());

    if (!file)
        return;

    file << L"transparent="
         << (g_settings.transparent ? 1 : 0)
         << L"\n";

    file << L"autostart="
         << (g_settings.autostart ? 1 : 0)
         << L"\n";

    file << L"opacity="
         << static_cast<int>(
                g_settings.opacity)
         << L"\n";
}

// ============================================================
// Autostart
// ============================================================

bool SetAutostart(bool enabled)
{
    HKEY key = nullptr;

    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_SET_VALUE,
        &key);

    if (result != ERROR_SUCCESS)
        return false;

    bool success = true;

    if (enabled)
    {
        wchar_t exePath[MAX_PATH]{};

        DWORD length = GetModuleFileNameW(
            nullptr,
            exePath,
            MAX_PATH);

        if (length == 0)
        {
            success = false;
        }
        else
        {
            result = RegSetValueExW(
                key,
                APP_NAME,
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(exePath),
                static_cast<DWORD>(
                    (wcslen(exePath) + 1) *
                    sizeof(wchar_t)));

            success =
                result == ERROR_SUCCESS;
        }
    }
    else
    {
        result = RegDeleteValueW(
            key,
            APP_NAME);

        success =
            result == ERROR_SUCCESS ||
            result == ERROR_FILE_NOT_FOUND;
    }

    RegCloseKey(key);

    return success;
}

// ============================================================
// Taskbar
// ============================================================

std::vector<HWND> GetTaskbars()
{
    std::vector<HWND> taskbars;

    HWND hwnd = nullptr;

    while ((hwnd = FindWindowExW(
        nullptr,
        hwnd,
        L"Shell_TrayWnd",
        nullptr)) != nullptr)
    {
        taskbars.push_back(hwnd);
    }

    hwnd = nullptr;

    while ((hwnd = FindWindowExW(
        nullptr,
        hwnd,
        L"Shell_SecondaryTrayWnd",
        nullptr)) != nullptr)
    {
        taskbars.push_back(hwnd);
    }

    return taskbars;
}

// ============================================================
// Taskbar transparency
// ============================================================

void ApplyTransparency(HWND hwnd)
{
    if (!IsWindow(hwnd))
        return;

    LONG_PTR exStyle =
        GetWindowLongPtrW(
            hwnd,
            GWL_EXSTYLE);

    if (g_settings.transparent)
    {
        exStyle |= WS_EX_LAYERED;

        SetWindowLongPtrW(
            hwnd,
            GWL_EXSTYLE,
            exStyle);

        SetLayeredWindowAttributes(
            hwnd,
            0,
            g_settings.opacity,
            LWA_ALPHA);
    }
    else
    {
        exStyle &= ~WS_EX_LAYERED;

        SetWindowLongPtrW(
            hwnd,
            GWL_EXSTYLE,
            exStyle);

        SetLayeredWindowAttributes(
            hwnd,
            0,
            255,
            LWA_ALPHA);
    }

    // Tell DWM that the frame should be recalculated.
    MARGINS margins{};
    margins.cxLeftWidth = -1;

    DwmExtendFrameIntoClientArea(
        hwnd,
        &margins);
}

void ApplyToTaskbars()
{
    const auto taskbars = GetTaskbars();

    for (HWND hwnd : taskbars)
    {
        ApplyTransparency(hwnd);
    }
}

// ============================================================
// Tray icon
// ============================================================

void CreateTrayIcon()
{
    ZeroMemory(
        &g_tray,
        sizeof(g_tray));

    g_tray.cbSize =
        sizeof(NOTIFYICONDATAW);

    g_tray.hWnd =
        g_window;

    g_tray.uID = 1;

    g_tray.uFlags =
        NIF_ICON |
        NIF_MESSAGE |
        NIF_TIP;

    g_tray.uCallbackMessage =
        WM_TRAY;

    g_tray.hIcon =
        LoadIconW(
            nullptr,
            IDI_APPLICATION);

    wcscpy_s(
        g_tray.szTip,
        APP_NAME);

    Shell_NotifyIconW(
        NIM_ADD,
        &g_tray);
}

void RemoveTrayIcon()
{
    Shell_NotifyIconW(
        NIM_DELETE,
        &g_tray);
}

// ============================================================
// Tray menu
// ============================================================

void ShowTrayMenu()
{
    HMENU menu =
        CreatePopupMenu();

    if (!menu)
        return;

    AppendMenuW(
        menu,
        MF_STRING |
        (g_settings.transparent
            ? MF_CHECKED
            : 0),
        ID_TOGGLE,
        L"Transparent Taskbar");

    AppendMenuW(
        menu,
        MF_STRING |
        (g_settings.autostart
            ? MF_CHECKED
            : 0),
        ID_AUTOSTART,
        L"Start with Windows");

    AppendMenuW(
        menu,
        MF_SEPARATOR,
        0,
        nullptr);

    HMENU opacityMenu =
        CreatePopupMenu();

    AppendMenuW(
        opacityMenu,
        MF_STRING |
        (g_settings.opacity == 64
            ? MF_CHECKED
            : 0),
        ID_OPACITY_25,
        L"25%");

    AppendMenuW(
        opacityMenu,
        MF_STRING |
        (g_settings.opacity == 128
            ? MF_CHECKED
            : 0),
        ID_OPACITY_50,
        L"50%");

    AppendMenuW(
        opacityMenu,
        MF_STRING |
        (g_settings.opacity == 192
            ? MF_CHECKED
            : 0),
        ID_OPACITY_75,
        L"75%");

    AppendMenuW(
        opacityMenu,
        MF_STRING |
        (g_settings.opacity == 255
            ? MF_CHECKED
            : 0),
        ID_OPACITY_100,
        L"100%");

    AppendMenuW(
        menu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(
            opacityMenu),
        L"Opacity");

    AppendMenuW(
        menu,
        MF_SEPARATOR,
        0,
        nullptr);

    AppendMenuW(
        menu,
        MF_STRING,
        ID_RESET,
        L"Reset");

    AppendMenuW(
        menu,
        MF_SEPARATOR,
        0,
        nullptr);

    AppendMenuW(
        menu,
        MF_STRING,
        ID_EXIT,
        L"Exit");

    POINT point{};

    GetCursorPos(&point);

    SetForegroundWindow(
        g_window);

    TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON |
        TPM_BOTTOMALIGN |
        TPM_LEFTALIGN,
        point.x,
        point.y,
        0,
        g_window,
        nullptr);

    PostMessageW(
        g_window,
        WM_NULL,
        0,
        0);

    DestroyMenu(menu);
}

// ============================================================
// Commands
// ============================================================

void ResetSettings()
{
    g_settings.transparent = true;
    g_settings.autostart = true;
    g_settings.opacity = 255;

    SetAutostart(true);

    SaveSettings();

    ApplyToTaskbars();
}

void HandleCommand(UINT command)
{
    switch (command)
    {
        case ID_TOGGLE:
        {
            g_settings.transparent =
                !g_settings.transparent;

            SaveSettings();

            ApplyToTaskbars();

            break;
        }

        case ID_AUTOSTART:
        {
            g_settings.autostart =
                !g_settings.autostart;

            SetAutostart(
                g_settings.autostart);

            SaveSettings();

            break;
        }

        case ID_OPACITY_25:
        {
            g_settings.opacity = 64;

            g_settings.transparent = true;

            SaveSettings();

            ApplyToTaskbars();

            break;
        }

        case ID_OPACITY_50:
        {
            g_settings.opacity = 128;

            g_settings.transparent = true;

            SaveSettings();

            ApplyToTaskbars();

            break;
        }

        case ID_OPACITY_75:
        {
            g_settings.opacity = 192;

            g_settings.transparent = true;

            SaveSettings();

            ApplyToTaskbars();

            break;
        }

        case ID_OPACITY_100:
        {
            g_settings.opacity = 255;

            g_settings.transparent = true;

            SaveSettings();

            ApplyToTaskbars();

            break;
        }

        case ID_RESET:
        {
            ResetSettings();
            break;
        }

        case ID_EXIT:
        {
            DestroyWindow(g_window);
            break;
        }
    }
}

// ============================================================
// Window procedure
// ============================================================

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    if (message ==
        g_taskbarCreatedMessage)
    {
        // Explorer has restarted.
        // Give it a moment to recreate the taskbar.

        SetTimer(
            hwnd,
            1,
            500,
            nullptr);

        return 0;
    }

    switch (message)
    {
        case WM_TIMER:
        {
            if (wParam == 1)
            {
                KillTimer(
                    hwnd,
                    1);

                ApplyToTaskbars();
            }

            return 0;
        }

        case WM_TRAY:
        {
            if (lParam == WM_RBUTTONUP ||
                lParam == WM_LBUTTONUP)
            {
                ShowTrayMenu();
            }

            return 0;
        }

        case WM_COMMAND:
        {
            HandleCommand(
                LOWORD(wParam));

            return 0;
        }

        case WM_DESTROY:
        {
            RemoveTrayIcon();

            PostQuitMessage(0);

            return 0;
        }
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

// ============================================================
// Entry point
// ============================================================

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int)
{
    g_instance = hInstance;

    // --------------------------------------------------------
    // Single instance
    // --------------------------------------------------------

    HANDLE mutex =
        CreateMutexW(
            nullptr,
            TRUE,
            MUTEX_NAME);

    if (!mutex)
        return 1;

    if (GetLastError() ==
        ERROR_ALREADY_EXISTS)
    {
        CloseHandle(mutex);
        return 0;
    }

    // --------------------------------------------------------
    // Settings
    // --------------------------------------------------------

    LoadSettings();

    SetAutostart(
        g_settings.autostart);

    // --------------------------------------------------------
    // TaskbarCreated
    // --------------------------------------------------------

    g_taskbarCreatedMessage =
        RegisterWindowMessageW(
            L"TaskbarCreated");

    // --------------------------------------------------------
    // Window class
    // --------------------------------------------------------

    WNDCLASSEXW wc{};

    wc.cbSize =
        sizeof(WNDCLASSEXW);

    wc.lpfnWndProc =
        WindowProc;

    wc.hInstance =
        hInstance;

    wc.lpszClassName =
        WINDOW_CLASS;

    wc.hIcon =
        LoadIconW(
            nullptr,
            IDI_APPLICATION);

    wc.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);

    RegisterClassExW(&wc);

    // --------------------------------------------------------
    // Hidden message window
    // --------------------------------------------------------

    g_window =
        CreateWindowExW(
            0,
            WINDOW_CLASS,
            APP_NAME,
            0,
            0,
            0,
            0,
            0,
            HWND_MESSAGE,
            nullptr,
            hInstance,
            nullptr);

    if (!g_window)
    {
        CloseHandle(mutex);
        return 1;
    }

    // --------------------------------------------------------
    // Tray
    // --------------------------------------------------------

    CreateTrayIcon();

    // --------------------------------------------------------
    // Initial taskbar state
    // --------------------------------------------------------

    ApplyToTaskbars();

    // --------------------------------------------------------
    // Message loop
    // --------------------------------------------------------

    MSG msg{};

    while (GetMessageW(
        &msg,
        nullptr,
        0,
        0) > 0)
    {
        TranslateMessage(&msg);

        DispatchMessageW(&msg);
    }

    // --------------------------------------------------------
    // Cleanup
    // --------------------------------------------------------

    RemoveTrayIcon();

    CloseHandle(mutex);

    return 0;
}
