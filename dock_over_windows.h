#pragma once

#include <windows.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <sddl.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <string>
#include <vector>

// Forward declarations of external dependencies
extern bool g_magnifyOverWindows;
extern std::atomic<bool> g_unloading;
extern bool g_dockExpanded;
extern float g_dockBottomGap;
extern float g_dockHeight;

extern std::atomic<HWND> g_modWindow;
HINSTANCE ModuleInstance();
bool RegisterWindowClass();
HWND EnsureModWindow();

HWND FindTaskbar();
using RunProc_t = void (*)(void*);
bool RunFromWindowThread(HWND hWnd, RunProc_t proc, void* param);
void CheckRecycleBinSoon();
void UpdateTrashIcons();

// ---- Magnify over windows ----

constexpr UINT kTraySettingMessage = WM_USER + 0x1CA;
constexpr WPARAM kTraySettingAutoHideGet = 3;
constexpr WPARAM kTraySettingAutoHideSet = 4;
constexpr UINT_PTR kTrayHideTimer = 2;

inline bool g_managingAutoHide = false;
inline bool g_autoHideBefore = false;

inline std::wstring UserValueName(PCWSTR name) {
    static std::wstring sid = [] {
        std::wstring result;
        HANDLE token;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return result;
        alignas(TOKEN_USER) BYTE buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
        DWORD size;
        PWSTR text;
        if (GetTokenInformation(token, TokenUser, buffer, sizeof(buffer), &size) &&
            ConvertSidToStringSidW(((TOKEN_USER*)buffer)->User.Sid, &text)) {
            result = text;
            LocalFree(text);
        }
        CloseHandle(token);
        return result;
    }();
    return std::wstring(name) + L"_" + sid;
}

inline bool KeepTaskbarShown() { return g_magnifyOverWindows && !g_unloading; }

inline void ApplyAutoHide() {
    HWND tb = FindTaskbar();
    if (!tb) return;
    if (KeepTaskbarShown()) {
        if (!g_managingAutoHide) {
            g_autoHideBefore = SendMessage(tb, kTraySettingMessage, kTraySettingAutoHideGet, 0);
            g_managingAutoHide = true;
        }
        SendMessage(tb, kTraySettingMessage, kTraySettingAutoHideSet, TRUE);
    } else if (g_managingAutoHide) {
        SendMessage(tb, kTraySettingMessage, kTraySettingAutoHideSet, g_autoHideBefore);
        g_managingAutoHide = false;
    }
    Wh_Log(L"[magnify over windows] %s", KeepTaskbarShown() ? L"on" : L"off");
    Wh_SetIntValue(UserValueName(L"managingAutoHide").c_str(), g_managingAutoHide);
    Wh_SetIntValue(UserValueName(L"autoHideBefore").c_str(), g_autoHideBefore);
}

using TrayUI__Hide_t = void(WINAPI*)(void* pThis);
inline TrayUI__Hide_t TrayUI__Hide_Original;
inline void WINAPI TrayUI__Hide_Hook(void* pThis) {
    if (KeepTaskbarShown()) {
        if (HWND tb = FindTaskbar()) KillTimer(tb, kTrayHideTimer);
        return;
    }
    TrayUI__Hide_Original(pThis);
}

using CSecondaryTray__AutoHide_t = void(WINAPI*)(void* pThis, bool param1);
inline CSecondaryTray__AutoHide_t CSecondaryTray__AutoHide_Original;
inline void WINAPI CSecondaryTray__AutoHide_Hook(void* pThis, bool param1) {
    if (KeepTaskbarShown()) return;
    CSecondaryTray__AutoHide_Original(pThis, param1);
}

template <typename F>
void ForEachTaskbarWindow(F f) {
    EnumWindows(
        [](HWND w, LPARAM l) -> BOOL {
            DWORD pid;
            WCHAR cls[32];
            if (GetWindowThreadProcessId(w, &pid) && pid == GetCurrentProcessId() &&
                GetClassName(w, cls, ARRAYSIZE(cls)) &&
                (!_wcsicmp(cls, L"Shell_TrayWnd") || !_wcsicmp(cls, L"Shell_SecondaryTrayWnd"))) {
                (*(F*)l)(w);
            }
            return TRUE;
        },
        (LPARAM)&f);
}

inline int StripHeight(UINT dpi) {
    float height = std::max(g_dockBottomGap + g_dockHeight, (float)kStockTaskbarHeight);
    return MulDiv((int)std::ceil(height), dpi, 96);
}

inline void ClipTaskbars() {
    ForEachTaskbarWindow([](HWND tb) {
        typedef HRESULT(WINAPI* DwmSetWindowAttribute_t)(HWND, DWORD, LPCVOID, DWORD);
        static auto pfnDwmSetWindowAttribute = (DwmSetWindowAttribute_t)GetProcAddress(
            GetModuleHandleW(L"dwmapi.dll"), "DwmSetWindowAttribute");
        if (pfnDwmSetWindowAttribute) {
            COLORREF noBorder = 0xFFFFFFFE; // DWMWA_COLOR_NONE
            pfnDwmSetWindowAttribute(tb, 34 /* DWMWA_BORDER_COLOR */, &noBorder, sizeof(noBorder));
        }

        RECT window, have;
        if (!GetWindowRect(tb, &window)) return;
        int type = GetWindowRgnBox(tb, &have);
        if (!KeepTaskbarShown() || g_dockExpanded) {
            if (type != ERROR) SetWindowRgn(tb, nullptr, TRUE);
            return;
        }
        RECT want{0, 0, window.right - window.left, window.bottom - window.top};
        want.top = std::max(0, (int)want.bottom - StripHeight(GetDpiForWindow(tb)));
        // Offset 1px downwards so any residual top edge stroke from taskbar is clipped away
        want.top = std::min(want.bottom, want.top + 1);
        if (type == SIMPLEREGION && EqualRect(&have, &want)) return;
        SetWindowRgn(tb, CreateRectRgnIndirect(&want), TRUE);
    });
}

struct AppBar {
    HWND hwnd;
    RECT rect;
};
inline std::vector<AppBar> g_appBars;

inline void RemoveAppBars() {
    for (auto& bar : g_appBars) {
        APPBARDATA abd{sizeof(abd)};
        abd.hWnd = bar.hwnd;
        SHAppBarMessage(ABM_REMOVE, &abd);
        DestroyWindow(bar.hwnd);
    }
    g_appBars.clear();
}

inline void AddAppBar(RECT const& rect) {
    HWND w = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kWindowClass, L"", WS_POPUP, 0, 0,
                            0, 0, nullptr, nullptr, ModuleInstance(), nullptr);
    if (!w) return;
    g_appBars.push_back({w, rect});

    APPBARDATA abd{sizeof(abd)};
    abd.hWnd = w;
    abd.uCallbackMessage = kAppBarMessage;
    SHAppBarMessage(ABM_NEW, &abd);
    abd.uEdge = ABE_BOTTOM;
    abd.rc = rect;
    SHAppBarMessage(ABM_QUERYPOS, &abd);
    abd.rc.top = abd.rc.bottom - (rect.bottom - rect.top);
    SHAppBarMessage(ABM_SETPOS, &abd);
    Wh_Log(L"[magnify over windows] keeping %d px free at the bottom of a screen",
           (int)(rect.bottom - rect.top));
}

inline void UpdateAppBars() {
    std::vector<RECT> want;
    if (KeepTaskbarShown()) {
        ForEachTaskbarWindow([&want](HWND tb) {
            HMONITOR monitor = MonitorFromWindow(tb, MONITOR_DEFAULTTONEAREST);
            MONITORINFO info{sizeof(info)};
            UINT dpiX, dpiY;
            if (!GetMonitorInfo(monitor, &info) ||
                FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
                return;
            }
            RECT rect = info.rcMonitor;
            rect.top = rect.bottom - StripHeight(dpiY);
            want.push_back(rect);
        });
    }
    bool same = want.size() == g_appBars.size();
    for (size_t i = 0; same && i < want.size(); i++) {
        same = EqualRect(&want[i], &g_appBars[i].rect);
    }
    if (same) return;
    RemoveAppBars();
    if (want.empty() || !RegisterWindowClass()) return;
    for (auto& rect : want) AddAppBar(rect);
}

inline int g_overWindowsPending = 0;

inline void ScheduleOverWindows(bool autoHide) {
    bool post = g_overWindowsPending == 0;
    g_overWindowsPending = std::max(g_overWindowsPending, autoHide ? 2 : 1);
    if (!post) return;
    HWND w = EnsureModWindow();
    if (!w || !PostMessage(w, kUpdateOverWindowsMessage, 0, 0)) g_overWindowsPending = 0;
}

inline void UpdateOverWindows() {
    int pending = std::exchange(g_overWindowsPending, 0);
    if (g_unloading || !pending) return;
    if (pending == 2) ApplyAutoHide();
    UpdateAppBars();
    ClipTaskbars();
}

inline void ApplyMagnifyOverWindows() {
    if (HWND tb = FindTaskbar()) {
        RunFromWindowThread(tb, [](void*) { ScheduleOverWindows(true); }, nullptr);
    }
}

inline LRESULT CALLBACK ModWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case kAppBarMessage:
            if (wParam == ABN_POSCHANGED) ScheduleOverWindows(false);
            return 0;
        case WM_DISPLAYCHANGE:
            ScheduleOverWindows(false);
            break;
        case kUpdateOverWindowsMessage:
            UpdateOverWindows();
            return 0;
        case kTrashChangedMessage:
            CheckRecycleBinSoon();
            return 0;
        case kTrashCheckedMessage:
            if (!g_unloading) {
                try { UpdateTrashIcons(); } catch (...) {}
            }
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
