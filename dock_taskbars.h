#pragma once

#include <windhawk_utils.h>
#include <windows.h>
#include <vector>
#include <list>
#include <optional>
#include <algorithm>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include "dock_utils.h"

// Forward declarations of external dependencies
struct Button;
extern std::optional<std::list<Button>> g_buttons;
Button* FindButton(wux::FrameworkElement const& el);
void ReleaseButton(Button& b);
void ApplySpacing(wux::FrameworkElement const& el);
void ApplyButtonWidth(Button& b, wux::FrameworkElement const& button);
void ApplyIconColumn(Button& b);
void ProcessButton(wux::FrameworkElement const& button);

struct IconImage;
extern std::optional<std::list<IconImage>> g_iconImages;
void ApplyIconSize(wux::FrameworkElement const& el);

struct Indicator;
extern std::optional<std::list<Indicator>> g_indicators;
void RestoreIndicators();

struct Plate;
extern std::optional<std::list<Plate>> g_plates;
void RemovePlates();
Plate* GetRowPlate(wux::DependencyObject const& row);

struct TrayPlate;
extern std::optional<std::list<TrayPlate>> g_trayPlates;
void RemoveTrayPlates();
void EnsureTrayPlate(wux::FrameworkElement const& row);
void UpdateAllTrayPlates();
void ApplyAllTrayPlateLooks();
void SetTrayPlateVisibility(wux::FrameworkElement const& row, wux::Visibility vis, float opacity);

void StopTrash();
void RestoreBackgrounds();
void HideBackground(wux::FrameworkElement const& el);
void RestoreHighlights();
void SetScale(wux::FrameworkElement const& el, float s, float tx);
void MarkPlatesDirty();
void ScheduleOverWindows(bool autoHide);
void BuildRow(wux::UIElement const& parent);
void StartFrames();
void StopFrames();
void RemoveAppBars();
void ClipTaskbars();

extern std::atomic<HWND> g_modWindow;
extern std::vector<RowItem> g_row;
extern winrt::weak_ref<wux::UIElement> g_rowParent;
extern float g_scale;
extern bool g_pointerOver;
extern float g_pointerX;
void RestoreLocalValue(wux::FrameworkElement const& el, wux::DependencyProperty const& dp,
                       wf::IInspectable const& value);

// ---- Taskbars ----

inline std::vector<winrt::weak_ref<wux::FrameworkElement>> g_frames;

struct FrameContextRevoker {
    winrt::weak_ref<wux::FrameworkElement> frame;
    wux::UIElement::ContextRequested_revoker contextRequested;
};
inline std::vector<FrameContextRevoker> g_frameContexts;

inline void NoteTaskbarFrame(wux::FrameworkElement const& frame) {
    if (!frame) return;
    for (auto& f : g_frames) {
        if (f.get() == frame) return;
    }
    g_frames.push_back(winrt::make_weak(frame));
    Wh_Log(L"Taskbar found");

    auto revoker = frame.ContextRequested(
        winrt::auto_revoke,
        [](wf::IInspectable const& sender, wuxi::ContextRequestedEventArgs const& e) {
            try {
                // 1. If an app button context menu was recently requested, ignore!
                if (GetTickCount64() - g_lastAppButtonContextTime < 600) return;

                // 2. Check original source: ignore interactive controls (app buttons, system tray icons, start button...)
                auto orig = e.OriginalSource();
                if (orig) {
                    auto obj = orig.try_as<wux::DependencyObject>();
                    while (obj) {
                        if (auto el = obj.try_as<wux::FrameworkElement>()) {
                            if (FindButton(el)) return;
                            std::wstring_view name = winrt::get_class_name(obj);
                            if (name.find(L"TaskListButton") != std::wstring_view::npos ||
                                name.find(L"TaskList") != std::wstring_view::npos ||
                                name.find(L"StartButton") != std::wstring_view::npos ||
                                name.find(L"SearchBox") != std::wstring_view::npos ||
                                name.find(L"TaskView") != std::wstring_view::npos ||
                                name.find(L"SystemTray") != std::wstring_view::npos ||
                                name.find(L"NotifyIcon") != std::wstring_view::npos ||
                                name.find(L"Clock") != std::wstring_view::npos ||
                                name.find(L"ExperienceToggleButton") != std::wstring_view::npos) {
                                return;
                            }
                        }
                        if (obj.try_as<wuxc::Primitives::ButtonBase>()) return;
                        obj = wuxm::VisualTreeHelper::GetParent(obj);
                    }
                }

                auto fe = sender.as<wux::FrameworkElement>();
                wf::Point pt{0, 0};
                if (e.TryGetPosition(fe, pt)) {
                    // 3. Check if coordinate falls inside any active dock button
                    if (g_buttons) {
                        for (auto& b : *g_buttons) {
                            auto btnEl = b.element.get();
                            if (!btnEl || btnEl.Visibility() != wux::Visibility::Visible || btnEl.ActualWidth() <= 0) continue;
                            try {
                                auto transform = btnEl.TransformToVisual(fe);
                                auto bounds = transform.TransformBounds(wf::Rect{0, 0, (float)btnEl.ActualWidth(), (float)btnEl.ActualHeight()});
                                if (pt.X >= bounds.X && pt.X <= bounds.X + bounds.Width &&
                                    pt.Y >= bounds.Y && pt.Y <= bounds.Y + bounds.Height) {
                                    return; // Over an app button!
                                }
                            } catch (...) {}
                        }
                    }
                }

                e.Handled(true);
                ShowDockMenu(fe, pt);
            } catch (...) {}
        });
    g_frameContexts.push_back({winrt::make_weak(frame), std::move(revoker)});

    // Proactively hide background & top border elements
    ForEachDescendant(frame, 12, [](wux::FrameworkElement const& el) {
        auto type = winrt::get_class_name(el);
        if (type == L"Taskbar.TaskbarBackground" ||
            type == L"Taskbar.StagingBar" ||
            el.Name() == L"BackgroundControl" ||
            el.Name() == L"TaskbarBackground") {
            HideBackground(el);
            ForEachDescendant(el, 6, [](wux::FrameworkElement const& child) {
                HideBackground(child);
                return true;
            });
        }
        return true;
    });

    MarkPlatesDirty();
    ScheduleOverWindows(true);
}

// ---- System tray ----

constexpr double kStockTaskbarHeight = 48;

struct AncestorState {
    winrt::weak_ref<wux::FrameworkElement> element;
    wf::IInspectable originalHeight{nullptr};
    wf::IInspectable originalMaxHeight{nullptr};
    wf::IInspectable originalAlignment{nullptr};
};

struct TrayRow {
    winrt::weak_ref<wux::FrameworkElement> element;
    wf::IInspectable originalHeight{nullptr};
    wf::IInspectable originalAlignment{nullptr};
    wf::IInspectable originalOpacity{nullptr};
    wf::IInspectable originalMargin{nullptr};
    int64_t opacityToken = 0;
    wux::UIElement::PointerEntered_revoker pointerEntered;
    wux::UIElement::PointerExited_revoker pointerExited;
    std::vector<AncestorState> ancestors;
};

[[clang::no_destroy]] inline std::optional<std::list<TrayRow>> g_trayRows{std::in_place};

inline void ApplyTrayStealth(wux::FrameworkElement const& row) {
    if (!g_trayRows) return;
    TrayRow* rowPtr = nullptr;
    for (auto& t : *g_trayRows) {
        if (t.element.get() == row) {
            rowPtr = &t;
            break;
        }
    }
    if (!rowPtr) return;

    if (g_transparentTray == 1) { // 1 = Always stealth (Ẩn hoàn toàn)
        rowPtr->pointerEntered.revoke();
        rowPtr->pointerExited.revoke();
        if (row.Opacity() != 0.0) row.Opacity(0.0);
        SetTrayPlateVisibility(row, wux::Visibility::Collapsed, 0.0f);
        if (!rowPtr->opacityToken) {
            rowPtr->opacityToken = row.RegisterPropertyChangedCallback(
                wux::UIElement::OpacityProperty(),
                [](wux::DependencyObject const& sender, wux::DependencyProperty const&) {
                    try {
                        if (g_transparentTray == 1) {
                            if (auto fe = sender.try_as<wux::FrameworkElement>()) {
                                if (fe.Opacity() != 0.0) fe.Opacity(0.0);
                            }
                        }
                    } catch (...) {}
                });
        }
    } else if (g_transparentTray == 2) { // 2 = Fade in on hover (Hiện khi rê chuột)
        if (rowPtr->opacityToken) {
            row.UnregisterPropertyChangedCallback(wux::UIElement::OpacityProperty(), rowPtr->opacityToken);
            rowPtr->opacityToken = 0;
        }
        if (row.Opacity() != 0.0) row.Opacity(0.0);
        SetTrayPlateVisibility(row, wux::Visibility::Collapsed, 0.0f);
        rowPtr->pointerEntered = row.PointerEntered(
            winrt::auto_revoke, [](wf::IInspectable const& sender, wuxi::PointerRoutedEventArgs const&) {
                try {
                    if (g_transparentTray == 2) {
                        if (auto fe = sender.try_as<wux::FrameworkElement>()) {
                            fe.Opacity(1.0);
                            SetTrayPlateVisibility(fe, wux::Visibility::Visible, 1.0f);
                        }
                    }
                } catch (...) {}
            });
        rowPtr->pointerExited = row.PointerExited(
            winrt::auto_revoke, [](wf::IInspectable const& sender, wuxi::PointerRoutedEventArgs const&) {
                try {
                    if (g_transparentTray == 2) {
                        if (auto fe = sender.try_as<wux::FrameworkElement>()) {
                            fe.Opacity(0.0);
                            SetTrayPlateVisibility(fe, wux::Visibility::Collapsed, 0.0f);
                        }
                    }
                } catch (...) {}
            });
    } else { // 0 = Normal visible (Dedicated Dock Plate)
        if (rowPtr->opacityToken) {
            row.UnregisterPropertyChangedCallback(wux::UIElement::OpacityProperty(), rowPtr->opacityToken);
            rowPtr->opacityToken = 0;
        }
        rowPtr->pointerEntered.revoke();
        rowPtr->pointerExited.revoke();
        RestoreLocalValue(row, wux::UIElement::OpacityProperty(), rowPtr->originalOpacity);
        SetTrayPlateVisibility(row, wux::Visibility::Visible, 1.0f);
    }
    UpdateAllTrayPlates();
}

inline void ApplyAllTrayStealth() {
    if (!g_trayRows) return;
    for (auto& t : *g_trayRows) {
        if (auto el = t.element.get()) {
            ApplyTrayStealth(el);
        }
    }
}

inline void FixTrayRow(wux::FrameworkElement const& row) {
    g_trayRows->remove_if([](TrayRow const& t) { return !t.element.get(); });
    TrayRow* rowPtr = nullptr;
    for (auto& t : *g_trayRows) {
        if (t.element.get() == row) {
            rowPtr = &t;
            break;
        }
    }
    if (!rowPtr) {
        auto& t = g_trayRows->emplace_back();
        t.element = winrt::make_weak(row);
        t.originalHeight = row.ReadLocalValue(wux::FrameworkElement::HeightProperty());
        t.originalAlignment = row.ReadLocalValue(wux::FrameworkElement::VerticalAlignmentProperty());
        t.originalOpacity = row.ReadLocalValue(wux::UIElement::OpacityProperty());
        t.originalMargin = row.ReadLocalValue(wux::FrameworkElement::MarginProperty());
        rowPtr = &t;
        Wh_Log(L"System tray found");

        // Record ancestors up to Taskbar.TaskbarFrame to unconstrain their height
        wux::DependencyObject cur = wuxm::VisualTreeHelper::GetParent(row);
        while (cur) {
            if (auto fe = cur.try_as<wux::FrameworkElement>()) {
                auto name = winrt::get_class_name(fe);
                if (name == L"Taskbar.TaskbarFrame") break;
                AncestorState as;
                as.element = winrt::make_weak(fe);
                as.originalHeight = fe.ReadLocalValue(wux::FrameworkElement::HeightProperty());
                as.originalMaxHeight = fe.ReadLocalValue(wux::FrameworkElement::MaxHeightProperty());
                as.originalAlignment = fe.ReadLocalValue(wux::FrameworkElement::VerticalAlignmentProperty());
                t.ancestors.push_back(as);
            }
            cur = wuxm::VisualTreeHelper::GetParent(cur);
        }
    }

    if (row.Height() != kStockTaskbarHeight) row.Height(kStockTaskbarHeight);
    if (row.VerticalAlignment() != wux::VerticalAlignment::Bottom) {
        row.VerticalAlignment(wux::VerticalAlignment::Bottom);
    }
    row.Clip(nullptr);

    // Unconstrain ancestors so row is never clipped when shifted upwards
    for (auto& as : rowPtr->ancestors) {
        if (auto fe = as.element.get()) {
            double h = (double)g_taskbarHeight;
            if (fe.Height() != h) fe.Height(h);
            if (fe.MaxHeight() != h) fe.MaxHeight(h);
            if (fe.VerticalAlignment() != wux::VerticalAlignment::Bottom) {
                fe.VerticalAlignment(wux::VerticalAlignment::Bottom);
            }
            fe.Clip(nullptr);
        }
    }

    EnsureTrayPlate(row);
    ApplyTrayStealth(row);
}

inline void RestoreTrayRows() {
    for (auto& t : *g_trayRows) {
        auto el = t.element.get();
        if (el) {
            try {
                if (t.opacityToken) {
                    el.UnregisterPropertyChangedCallback(wux::UIElement::OpacityProperty(), t.opacityToken);
                    t.opacityToken = 0;
                }
                t.pointerEntered.revoke();
                t.pointerExited.revoke();
                RestoreLocalValue(el, wux::FrameworkElement::HeightProperty(), t.originalHeight);
                RestoreLocalValue(el, wux::FrameworkElement::VerticalAlignmentProperty(),
                                  t.originalAlignment);
                RestoreLocalValue(el, wux::UIElement::OpacityProperty(), t.originalOpacity);
                RestoreLocalValue(el, wux::FrameworkElement::MarginProperty(), t.originalMargin);
            } catch (...) {}
        }
        for (auto& as : t.ancestors) {
            if (auto fe = as.element.get()) {
                try {
                    RestoreLocalValue(fe, wux::FrameworkElement::HeightProperty(), as.originalHeight);
                    RestoreLocalValue(fe, wux::FrameworkElement::MaxHeightProperty(), as.originalMaxHeight);
                    RestoreLocalValue(fe, wux::FrameworkElement::VerticalAlignmentProperty(), as.originalAlignment);
                } catch (...) {}
            }
        }
    }
    g_trayRows->clear();
    try { RemoveTrayPlates(); } catch (...) {}
}

inline void ScanTray(wux::FrameworkElement const& frame) {
    auto root = frame.XamlRoot();
    auto content = root ? root.Content() : nullptr;
    if (!content) return;
    auto visit = [](wux::FrameworkElement const& el) {
        auto type = winrt::get_class_name(el);
        if (type == L"Taskbar.TaskbarFrame") return false;
        if (type == L"SystemTray.SystemTrayFrame") {
            auto findRow = [](wux::FrameworkElement const& child) {
                if (child.Name() == L"SystemTrayFrameGrid") {
                    FixTrayRow(child);
                    return false;
                }
                return true;
            };
            ForEachDescendant(el, 3, findRow);
            return false;
        }
        return true;
    };
    ForEachDescendant(content, 10, visit);
}

inline void ScanTaskbars() {
    g_frames.erase(std::remove_if(g_frames.begin(), g_frames.end(),
                                  [](auto& f) { return !f.get(); }),
                   g_frames.end());
    g_buttons->remove_if([](Button const& b) { return !b.element.get(); });
    g_iconImages->remove_if([](IconImage const& i) { return !i.element.get(); });
    g_indicators->remove_if([](Indicator const& i) { return !i.element.get(); });

    auto visit = [](wux::FrameworkElement const& el) {
        auto type = winrt::get_class_name(el);
        if (type == L"Taskbar.TaskListButton" || type == L"Taskbar.ExperienceToggleButton") {
            ProcessButton(el);
            return false;
        }
        if (type == L"Taskbar.TaskbarBackground" ||
            type == L"Taskbar.StagingBar" ||
            el.Name() == L"BackgroundControl" ||
            el.Name() == L"TaskbarBackground") {
            HideBackground(el);
            ForEachDescendant(el, 6, [](wux::FrameworkElement const& child) {
                HideBackground(child);
                return true;
            });
            return false;
        }
        if (auto rect = el.try_as<wuxs::Rectangle>()) {
            if (rect.ActualHeight() <= 3.0f || rect.Height() <= 3.0f) {
                HideBackground(el);
            }
        }
        if (auto border = el.try_as<wuxc::Border>()) {
            auto bt = border.BorderThickness();
            if (bt.Top > 0 && bt.Left == 0 && bt.Right == 0) {
                HideBackground(el);
            }
        }
        return true;
    };
    for (auto& f : g_frames) {
        auto frame = f.get();
        if (!frame) continue;
        ForEachDescendant(frame, 12, visit);
        ScanTray(frame);
    }
}

inline void OnTaskbarPointerMoved(wux::FrameworkElement const& frame,
                           wuxi::PointerRoutedEventArgs const& e) {
    if (g_scale <= 1.0f) return;
    NoteTaskbarFrame(frame);
    Plate* plate = nullptr;
    for (auto& p : *g_plates) {
        if (p.frame.get() == frame) {
            plate = &p;
            break;
        }
    }
    wux::UIElement row{nullptr};
    bool over = false;
    if (plate && plate->width > 0 && plate->root.Visibility() == wux::Visibility::Visible) {
        row = plate->row.get();
        auto panel = plate->panel.get();
        if (row && panel) {
            auto point = e.GetCurrentPoint(panel).Position();
            over = point.X >= std::min(plate->left, plate->baseLeft) &&
                   point.X <= std::max(plate->left + plate->width,
                                       plate->baseLeft + plate->baseWidth) &&
                   (point.Y >= plate->top || (g_pointerOver && g_rowParent.get() == row));
        }
    }
    if (!over) {
        if (g_pointerOver) {
            g_pointerOver = false;
            StartFrames();
        }
        return;
    }
    if (g_rowParent.get() != row) BuildRow(row);
    g_pointerX = e.GetCurrentPoint(row).Position().X;
    g_pointerOver = true;
    StartFrames();
}

inline void OnTaskbarPointerExited() {
    g_pointerOver = false;
    StartFrames();
}

inline void ResetAll() {
    StopFrames();
    try { RemovePlates(); } catch (...) {}
    try { RemoveTrayPlates(); } catch (...) {}
    try { StopTrash(); } catch (...) {}
    RestoreBackgrounds();
    RestoreHighlights();
    RestoreIconImages();
    for (auto& b : *g_buttons) {
        if (auto el = b.element.get()) {
            try {
                ReleaseButton(b);
                SetScale(el, 1.0f, 0);
                wuxh::ElementCompositionPreview::SetIsTranslationEnabled(el, false);
            } catch (...) {}
        }
    }
    RestoreIndicators();
    RestoreTrayRows();
    RemoveAppBars();
    ClipTaskbars();
    if (HWND w = g_modWindow.exchange(nullptr)) DestroyWindow(w);
    g_row.clear();
    g_rowParent = nullptr;
    g_frames.clear();
    g_frameContexts.clear();
    g_buttons.reset();
    g_iconImages.reset();
    g_plates.reset();
    g_trayPlates.reset();
    g_indicators.reset();
    g_trayRows.reset();
}

using RunProc_t = void (*)(void*);

inline bool RunFromWindowThread(HWND hWnd, RunProc_t proc, void* param) {
    static const UINT msg = RegisterWindowMessage(L"Windhawk_RunFromWindowThread_" WH_MOD_ID);
    struct Param { RunProc_t proc; void* param; };

    DWORD tid = GetWindowThreadProcessId(hWnd, nullptr);
    if (!tid) return false;
    if (tid == GetCurrentThreadId()) { proc(param); return true; }

    HHOOK hook = SetWindowsHookEx(WH_CALLWNDPROC, [](int code, WPARAM w, LPARAM l) -> LRESULT {
        if (code == HC_ACTION) {
            auto* cwp = (const CWPSTRUCT*)l;
            if (cwp->message == msg) {
                auto* p = (Param*)cwp->lParam;
                p->proc(p->param);
            }
        }
        return CallNextHookEx(nullptr, code, w, l);
    }, nullptr, tid);
    if (!hook) return false;

    Param p{proc, param};
    SendMessage(hWnd, msg, 0, (LPARAM)&p);
    UnhookWindowsHookEx(hook);
    return true;
}

inline HWND FindTaskbar() {
    HWND result = nullptr;
    EnumWindows([](HWND hWnd, LPARAM l) -> BOOL {
        DWORD pid; WCHAR cls[32];
        if (GetWindowThreadProcessId(hWnd, &pid) && pid == GetCurrentProcessId() &&
            GetClassName(hWnd, cls, ARRAYSIZE(cls)) && _wcsicmp(cls, L"Shell_TrayWnd") == 0) {
            *(HWND*)l = hWnd;
            return FALSE;
        }
        return TRUE;
    }, (LPARAM)&result);
    return result;
}
