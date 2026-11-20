#pragma once

#include <windhawk_api.h>
#include <windhawk_utils.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

// Forward declarations
LRESULT CALLBACK ModWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void ScheduleOverWindows(bool autoHide);

// ---- Settings, as the user set them ----
inline int g_dockSize = 55;           // 0-100
inline float g_scale = 1.0f;          // Tam thoi tat magnification scale (mac dinh 1.6f)
inline int g_dockTint = 20;           // 0-100
inline int g_dockBlur = 35;           // 0-100
inline int g_magnificationRange = 55; // 0-100
inline int g_dockTheme = 0;           // 0 = System (follows Windows), 1 = dark, 2 = light
inline int g_animationMs = 100;
inline bool g_showTrash = true;
inline bool g_appsDivider = true;
inline bool g_magnifyOverWindows = true;
inline bool g_hideStartButton = true;
inline int g_dockPlateStyle = 0; // 0 = macOS Sequoia/Sonoma (Squircle + Inner Glow, Default), 1 = Classic Simple
inline bool g_explorerRightClickWinX = true; // Right click File Explorer opens Windows Start button context menu (Win+X)
inline int g_activeIndicatorColor = 0; // 0 = Adaptive (White on dark, Theme on light), 1 = Theme Accent, 2 = Bright White
inline int g_transparentTray = 1; // 0 = Normal, 1 = Always stealth (default), 2 = Show on hover
void ApplyAllTrayStealth();

// ---- Sizes derived from the settings (see ComputeSizes) ----
inline float g_iconSize = 39.0f;
inline float g_radius = 130.0f;       // effect radius
inline float g_buttonWidth = 47.0f;   // widened to fit the icon (see ApplyButtonWidth)
inline float g_buttonMargin = 2.5f;   // per side; negative pulls buttons together
inline float g_dockHeight = 65.0f;
inline float g_dockSidePadding = 15.0f;
inline float g_dockRadius = 20.0f;
inline float g_dockBottomGap = 7.0f;
inline int g_taskbarHeight = 85;      // 0 = leave the taskbar at its normal height
inline float g_iconLoadSize = 63;     // resolution icons are loaded at; 0 = Windows' own
inline float g_dividerSpace = 14.0f;  // room for a divider
inline float g_trashSlot = 48.0f;     // room for the Recycle Bin itself
inline float g_trashSpace = 62.0f;    // room reserved after the last icon: divider + Recycle Bin

constexpr float kStockIconSize = 24;      // Windows 11 taskbar icon size
constexpr float kStockButtonWidth = 44;   // Windows 11 taskbar button width
constexpr int kMinTaskbarHeight = 40;     // keeps the clock from being squashed
constexpr float kButtonIconPadding = 8;   // a button is at least its icon + this wide

// Fixed tuning, found by testing. Given for the stock 24px icon and scales
// with the dock.
constexpr float kIconSpacing = 4.0f;        // Spacing between app icons on the dock (default: 4)
constexpr float kTrashIconScale = 1.15f;

inline void ComputeSizes() {
    float icon = std::round(kStockIconSize * std::exp2((g_dockSize - 20) / 50.0f));
    g_iconSize = icon;

    g_dockHeight = std::round(icon * 5 / 3);
    g_dockSidePadding = std::round(icon * 0.385f);
    g_dockRadius = std::round(g_dockHeight * 0.3f);
    g_dockBottomGap = std::round(icon / 6);
    g_radius = icon * (1 + g_magnificationRange / 20.0f);

    g_buttonWidth = std::max(kStockButtonWidth, icon + kButtonIconPadding);
    float gap = kIconSpacing * icon / 12;
    g_buttonMargin = (icon + gap - g_buttonWidth) / 2;

    g_dividerSpace = std::round(icon * 0.35f);
    g_trashSlot = std::round(icon + gap - icon * 0.1f);
    g_trashSpace = g_dividerSpace + g_trashSlot;

    g_iconLoadSize = std::ceil(icon * g_scale);

    float dockTop = g_dockBottomGap + g_dockHeight;
    float iconTop = g_dockBottomGap + (g_dockHeight - icon) / 2 + g_scale * icon;
    float highest = std::max(dockTop, iconTop);

    g_taskbarHeight = std::max((int)std::ceil(highest + 2), kMinTaskbarHeight);
}

// ---------------------------------------------------------------------------
// Shell jobs
// ---------------------------------------------------------------------------

inline std::mutex g_shellJobsLock;
inline std::vector<std::function<void()>> g_shellJobs;
inline PTP_WORK g_shellJobWork = nullptr;  // created in Wh_ModInit

inline void CALLBACK RunShellJob(PTP_CALLBACK_INSTANCE, PVOID, PTP_WORK) {
    std::function<void()> job;
    {
        std::lock_guard lock(g_shellJobsLock);
        if (g_shellJobs.empty()) return;
        job = std::move(g_shellJobs.front());
        g_shellJobs.erase(g_shellJobs.begin());
    }
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    job();
    if (SUCCEEDED(co)) CoUninitialize();
}

inline void QueueShellJob(std::function<void()> job) {
    if (!g_shellJobWork) return;
    {
        std::lock_guard lock(g_shellJobsLock);
        g_shellJobs.push_back(std::move(job));
    }
    SubmitThreadpoolWork(g_shellJobWork);
}

// ---------------------------------------------------------------------------
// The mod's window
// ---------------------------------------------------------------------------

constexpr WCHAR kWindowClass[] = L"Windhawk_" WH_MOD_ID;
constexpr UINT kAppBarMessage = WM_APP;              // ABN_* from the shell
constexpr UINT kUpdateOverWindowsMessage = WM_APP + 1;
constexpr UINT kTrashChangedMessage = WM_APP + 2;    // from SHChangeNotifyRegister
constexpr UINT kTrashCheckedMessage = WM_APP + 3;    // from CheckRecycleBin

inline std::atomic<HWND> g_modWindow = nullptr;

inline HINSTANCE ModuleInstance() {
    HMODULE module = nullptr;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      (LPCWSTR)&ModuleInstance, &module);
    return module;
}

inline bool RegisterWindowClass() {
    static bool registered = [] {
        WNDCLASS wc{};
        wc.lpfnWndProc = ModWindowProc;
        wc.hInstance = ModuleInstance();
        wc.lpszClassName = kWindowClass;
        return RegisterClass(&wc) != 0;
    }();
    return registered;
}

inline HWND EnsureModWindow() {
    if (!g_modWindow && RegisterWindowClass()) {
        g_modWindow = CreateWindowEx(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                     ModuleInstance(), nullptr);
    }
    return g_modWindow;
}
