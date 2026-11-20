#pragma once

// ============================================================================
// macOS Style Dock - Running Indicators Module
// ============================================================================
// The little pill/dot under each running app. It's part of its button, so it
// moves and grows with it; PlaceIndicator undoes that, keeping it at its
// normal size, centered under the icon, just above the bottom of the dock.

#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <psapi.h>

namespace wuxs = winrt::Windows::UI::Xaml::Shapes;
namespace wuxa = winrt::Windows::UI::Xaml::Automation;

// Normalize string by keeping only lowercase alphanumeric characters
inline std::wstring CleanLowerAlnum(std::wstring_view s) {
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s) {
        if (::iswalnum(c)) {
            out.push_back(::towlower(c));
        }
    }
    return out;
}

// Retrieve current Windows Accent Color from registry
inline winrt::Windows::UI::Color GetWindowsAccentColor() {
    DWORD accent = 0;
    DWORD size = sizeof(accent);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor",
                     RRF_RT_REG_DWORD, nullptr, &accent, &size) == ERROR_SUCCESS) {
        uint8_t a = (accent >> 24) & 0xFF;
        uint8_t b = (accent >> 16) & 0xFF;
        uint8_t g = (accent >> 8) & 0xFF;
        uint8_t r = accent & 0xFF;
        if (a == 0) a = 255;
        return winrt::Windows::UI::Color{a, r, g, b};
    }
    return winrt::Windows::UI::Color{255, 0, 120, 215};
}

struct Indicator {
    winrt::weak_ref<wux::FrameworkElement> element;
    winrt::weak_ref<wux::FrameworkElement> button;
    int64_t fillToken = 0;
    int64_t widthToken = 0;
    int64_t heightToken = 0;
    int64_t radiusXToken = 0;
    int64_t radiusYToken = 0;
    bool isActive = false;
    bool updating = false;
};

[[clang::no_destroy]] inline std::optional<std::list<Indicator>> g_indicators{std::in_place};

inline void ApplyDotShape(Indicator& ind) {
    auto el = ind.element.get();
    if (!el) return;

    float targetWidth = ind.isActive ? 14.0f : 4.0f;
    float targetHeight = 4.0f;
    float rad = 2.0f;

    ind.updating = true;

    // Rigidly lock min/max dimensions to prevent Windows 11 Storyboards from stretching
    el.MinWidth(targetWidth);
    el.MaxWidth(targetWidth);
    el.MinHeight(targetHeight);
    el.MaxHeight(targetHeight);

    if (el.Width() != targetWidth) el.Width(targetWidth);
    if (el.Height() != targetHeight) el.Height(targetHeight);

    if (auto r = el.try_as<wuxs::Rectangle>()) {
        if (r.RadiusX() != rad) r.RadiusX(rad);
        if (r.RadiusY() != rad) r.RadiusY(rad);

        winrt::Windows::UI::Color fillColor;
        if (ind.isActive) {
            if (g_activeIndicatorColor == 1) {
                fillColor = GetWindowsAccentColor();
            } else {
                fillColor = winrt::Windows::UI::Color{255, 255, 255, 255};
            }
        } else {
            // Clean Silver-White for inactive running apps
            fillColor = winrt::Windows::UI::Color{220, 230, 235, 240};
        }

        bool needNewBrush = true;
        if (auto cur = r.Fill().try_as<wuxm::SolidColorBrush>()) {
            auto cc = cur.Color();
            if (cc.A == fillColor.A && cc.R == fillColor.R && cc.G == fillColor.G && cc.B == fillColor.B) {
                needNewBrush = false;
            }
        }
        if (needNewBrush) {
            r.Fill(wuxm::SolidColorBrush(fillColor));
        }
    }

    ind.updating = false;
}

inline void ApplyDotShape(wux::FrameworkElement const& el) {
    if (!g_indicators) return;
    for (auto& ind : *g_indicators) {
        if (ind.element.get() == el) {
            ApplyDotShape(ind);
            return;
        }
    }
    el.MinWidth(4.0f);
    el.MaxWidth(4.0f);
    el.MinHeight(4.0f);
    el.MaxHeight(4.0f);
    if (el.Width() != 4.0f) el.Width(4.0f);
    if (el.Height() != 4.0f) el.Height(4.0f);
    if (auto r = el.try_as<wuxs::Rectangle>()) {
        r.RadiusX(2.0f);
        r.RadiusY(2.0f);
        r.Fill(wuxm::SolidColorBrush(winrt::Windows::UI::Color{220, 230, 235, 240}));
    }
}

// Complete foreground window matcher with zero-delay native support
inline bool IsForegroundWindowMatchingButton(wux::FrameworkElement const& button, HWND hwndFg) {
    if (!button || !hwndFg) return false;

    wchar_t cls[64] = {0};
    GetClassNameW(hwndFg, cls, 63);

    // 1. File Explorer
    if (wcscmp(cls, L"CabinetWClass") == 0) {
        if (IsFileExplorer(button)) return true;
    }

    // 2. UWP / Modern apps (Microsoft Store, Settings, Calculator, etc.)
    if (wcscmp(cls, L"ApplicationFrameWindow") == 0) {
        wchar_t winTitle[256] = {0};
        GetWindowTextW(hwndFg, winTitle, 255);
        std::wstring cleanWin = CleanLowerAlnum(winTitle);
        std::wstring cleanBtn = CleanLowerAlnum(GetAppIdentifier(button));
        if (cleanWin.length() >= 3 && cleanBtn.length() >= 3) {
            if (cleanBtn.find(cleanWin) != std::wstring::npos ||
                cleanWin.find(cleanBtn) != std::wstring::npos) {
                return true;
            }
        }
        if (cleanWin.find(L"store") != std::wstring::npos && cleanBtn.find(L"store") != std::wstring::npos) {
            return true;
        }
    }

    // 3. Desktop / Win32 apps: query process name
    DWORD pid = 0;
    GetWindowThreadProcessId(hwndFg, &pid);
    if (pid != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess) {
            wchar_t path[MAX_PATH] = {0};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
                wchar_t* fn = wcsrchr(path, L'\\');
                if (fn) fn++; else fn = path;
                std::wstring proc = fn;
                size_t dot = proc.rfind(L'.');
                if (dot != std::wstring::npos) proc = proc.substr(0, dot);

                std::wstring cleanProc = CleanLowerAlnum(proc);
                std::wstring cleanBtn = CleanLowerAlnum(GetAppIdentifier(button));

                CloseHandle(hProcess);

                if (cleanProc.empty() || cleanBtn.empty()) return false;

                if (cleanProc == cleanBtn) return true;

                if (cleanProc.length() >= 3 && cleanBtn.length() >= 3) {
                    if (cleanBtn.find(cleanProc) != std::wstring::npos ||
                        cleanProc.find(cleanBtn) != std::wstring::npos) {
                        return true;
                    }
                }

                // Known aliases
                if ((cleanProc == L"windowsterminal" || cleanProc == L"cmd" || cleanProc == L"powershell") &&
                    (cleanBtn.find(L"terminal") != std::wstring::npos ||
                     cleanBtn.find(L"commandprompt") != std::wstring::npos ||
                     cleanBtn.find(L"powershell") != std::wstring::npos)) {
                    return true;
                }
                if ((cleanProc == L"msedge" || cleanProc == L"edge") && cleanBtn.find(L"edge") != std::wstring::npos) {
                    return true;
                }
                if (cleanProc == L"code" && (cleanBtn.find(L"vscode") != std::wstring::npos ||
                                             cleanBtn.find(L"visualstudiocode") != std::wstring::npos)) {
                    return true;
                }
                return false;
            }
            CloseHandle(hProcess);
        }
    }

    return false;
}

// Exactly one app is active (trắng dài 14x4), and all other running apps are inactive (chấm tròn 4x4)
inline void UpdateActiveIndicator() {
    if (!g_indicators || g_indicators->empty()) return;

    HWND hwndFg = GetForegroundWindow();
    Indicator* activeInd = nullptr;

    if (hwndFg) {
        wchar_t cls[64] = {0};
        GetClassNameW(hwndFg, cls, 63);
        // Ignore desktop or taskbar
        if (wcscmp(cls, L"Shell_TrayWnd") != 0 && wcscmp(cls, L"Progman") != 0 && wcscmp(cls, L"WorkerW") != 0) {
            for (auto& ind : *g_indicators) {
                if (auto btn = ind.button.get()) {
                    if (IsForegroundWindowMatchingButton(btn, hwndFg)) {
                        activeInd = &ind;
                        break;
                    }
                }
            }
        }
    }

    for (auto& ind : *g_indicators) {
        auto el = ind.element.get();
        if (!el) continue;

        bool shouldBeActive = (activeInd != nullptr && &ind == activeInd);
        float targetW = shouldBeActive ? 14.0f : 4.0f;

        bool stateChanged = (ind.isActive != shouldBeActive);
        bool sizeDrifted = (std::abs((float)el.Width() - targetW) > 0.5f) ||
                           (std::abs((float)el.Height() - 4.0f) > 0.5f);

        if (stateChanged || sizeDrifted) {
            ind.isActive = shouldBeActive;
            ApplyDotShape(ind);
            if (auto b = ind.button.get()) ReapplyButton(b);
        }
    }
}

// Active window tracking via WinEventHook + lightweight 120ms polling timer
inline HWINEVENTHOOK g_foregroundEventHook = nullptr;
inline UINT_PTR g_activeTimerId = 0;

inline void CALLBACK ForegroundEventProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
    UpdateActiveIndicator();
    ScanAllBadges();
}

inline void CALLBACK ActiveTimerProc(HWND, UINT, UINT_PTR, DWORD) {
    UpdateActiveIndicator();
    ScanAllBadges();
}

inline void StartForegroundTracking() {
    if (!g_foregroundEventHook) {
        g_foregroundEventHook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            nullptr, ForegroundEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    }
    if (!g_activeTimerId) {
        g_activeTimerId = SetTimer(nullptr, 0, 120, ActiveTimerProc);
    }
}

inline void StopForegroundTracking() {
    if (g_foregroundEventHook) {
        UnhookWinEvent(g_foregroundEventHook);
        g_foregroundEventHook = nullptr;
    }
    if (g_activeTimerId) {
        KillTimer(nullptr, g_activeTimerId);
        g_activeTimerId = 0;
    }
}

inline void PlaceIndicator(wux::FrameworkElement const& button, float s) {
    Indicator* indPtr = nullptr;
    if (g_indicators) {
        for (auto& i : *g_indicators) {
            if (i.button.get() == button) {
                indPtr = &i;
                break;
            }
        }
    }
    if (!indPtr) return;
    auto el = indPtr->element.get();
    if (!el) return;

    // Self-correct dimensions if Windows Storyboard secretly altered them
    float targetW = indPtr->isActive ? 14.0f : 4.0f;
    if (std::abs((float)el.Width() - targetW) > 0.5f || std::abs((float)el.Height() - 4.0f) > 0.5f) {
        ApplyDotShape(*indPtr);
    }

    auto visual = wuxh::ElementCompositionPreview::GetElementVisual(el);
    wuxh::ElementCompositionPreview::SetIsTranslationEnabled(el, true);

    auto reset = [&] {
        visual.Scale({1, 1, 1});
        visual.Properties().InsertVector3(L"Translation", {0, 0, 0});
    };
    Plate* p = GetRowPlate(wuxm::VisualTreeHelper::GetParent(button));
    if (!p || s <= 0) return reset();

    float w = (float)el.Width();
    if (std::isnan(w) || w <= 0) w = (float)el.ActualWidth();
    float h = (float)el.Height();
    if (std::isnan(h) || h <= 0) h = (float)el.ActualHeight();

    float centerY = h / 2;
    wux::DependencyObject current = el;
    while (current && current != button) {
        if (auto ui = current.try_as<wux::UIElement>()) centerY += ui.ActualOffset().y;
        current = wuxm::VisualTreeHelper::GetParent(current);
    }
    if (!current) return reset();

    auto slot = wuxc::Primitives::LayoutInformation::GetLayoutSlot(button);
    float buttonTop = slot.Y + (float)button.Margin().Top;
    float inset = std::max(2.0f, std::round(g_iconSize / 10));
    float targetY = p->dockBottom - buttonTop - inset - h / 2;

    float pivotY = ButtonPivotY(button);
    float ty = (targetY - p->offsetY - pivotY) / s + pivotY - centerY;
    visual.CenterPoint({w / 2, h / 2, 0});
    visual.Scale({1 / s, 1 / s, 1});
    visual.Properties().InsertVector3(L"Translation", {0, ty, 0});
}

inline void TrackIndicator(wux::FrameworkElement const& el, wux::FrameworkElement const& button) {
    StartForegroundTracking();
    for (auto& i : *g_indicators) {
        if (i.element.get() == el) return;
    }
    auto& ind = g_indicators->emplace_back();
    ind.element = winrt::make_weak(el);
    ind.button = winrt::make_weak(button);

    HWND hwndFg = GetForegroundWindow();
    ind.isActive = IsForegroundWindowMatchingButton(button, hwndFg);
    ApplyDotShape(ind);

    // Guard all properties: if Windows 11 Storyboard tries to stretch width/height or change color/radius, snap back instantly!
    if (auto r = el.try_as<wuxs::Rectangle>()) {
        auto guardCb = [&ind](wux::DependencyObject const&, wux::DependencyProperty const&) {
            try {
                if (ind.updating) return;
                ApplyDotShape(ind);
            } catch (...) {}
        };

        try {
            ind.fillToken = r.RegisterPropertyChangedCallback(wuxs::Shape::FillProperty(), guardCb);
        } catch (...) {}
        try {
            ind.widthToken = r.RegisterPropertyChangedCallback(wux::FrameworkElement::WidthProperty(), guardCb);
        } catch (...) {}
        try {
            ind.heightToken = r.RegisterPropertyChangedCallback(wux::FrameworkElement::HeightProperty(), guardCb);
        } catch (...) {}
        try {
            ind.radiusXToken = r.RegisterPropertyChangedCallback(wuxs::Rectangle::RadiusXProperty(), guardCb);
        } catch (...) {}
        try {
            ind.radiusYToken = r.RegisterPropertyChangedCallback(wuxs::Rectangle::RadiusYProperty(), guardCb);
        } catch (...) {}
    }
}

inline void RestoreIndicators() {
    StopForegroundTracking();
    for (auto& i : *g_indicators) {
        if (auto el = i.element.get()) {
            if (auto r = el.try_as<wuxs::Rectangle>()) {
                try {
                    if (i.fillToken) r.UnregisterPropertyChangedCallback(wuxs::Shape::FillProperty(), i.fillToken);
                    if (i.widthToken) r.UnregisterPropertyChangedCallback(wux::FrameworkElement::WidthProperty(), i.widthToken);
                    if (i.heightToken) r.UnregisterPropertyChangedCallback(wux::FrameworkElement::HeightProperty(), i.heightToken);
                    if (i.radiusXToken) r.UnregisterPropertyChangedCallback(wuxs::Rectangle::RadiusXProperty(), i.radiusXToken);
                    if (i.radiusYToken) r.UnregisterPropertyChangedCallback(wuxs::Rectangle::RadiusYProperty(), i.radiusYToken);
                } catch (...) {}
            }
            i.fillToken = i.widthToken = i.heightToken = i.radiusXToken = i.radiusYToken = 0;

            try {
                auto visual = wuxh::ElementCompositionPreview::GetElementVisual(el);
                visual.Scale({1, 1, 1});
                visual.Properties().InsertVector3(L"Translation", {0, 0, 0});
                wuxh::ElementCompositionPreview::SetIsTranslationEnabled(el, false);
            } catch (...) {}
        }
    }
    g_indicators->clear();
    RestoreBadges();
}
