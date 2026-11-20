#pragma once

#include <windows.h>
#include <windhawk_api.h>
#include <windhawk_utils.h>

#include <algorithm>
#include <cmath>
#include <list>
#include <optional>
#include <string_view>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Automation.h>

#include "dock_utils.h"
#include "dock_badge.h"
#include "dock_icon_card.h"

namespace wf = winrt::Windows::Foundation;
namespace wux = winrt::Windows::UI::Xaml;
namespace wuxa = winrt::Windows::UI::Xaml::Automation;
namespace wuxc = winrt::Windows::UI::Xaml::Controls;
namespace wuxh = winrt::Windows::UI::Xaml::Hosting;
namespace wuxm = winrt::Windows::UI::Xaml::Media;

// Forward declarations
extern float g_iconSize;
extern float g_buttonWidth;
extern float g_buttonMargin;

float GetRowOffsetY(wux::DependencyObject const& row);
void PlaceIndicator(wux::FrameworkElement const& button, float s);
void TrackIndicator(wux::FrameworkElement const& el, wux::FrameworkElement const& button);
void MarkPlatesDirty();

// ---------------------------------------------------------------------------
// Button structure and tracking
// ---------------------------------------------------------------------------

struct Button {
    winrt::weak_ref<wux::FrameworkElement> element;
    wf::IInspectable originalMargin{nullptr};  // local value, may be UnsetValue
    int64_t marginToken = 0;
    wux::FrameworkElement::SizeChanged_revoker sizeChanged;
    // The button's template root, widened to fit the icon (ApplyButtonWidth).
    winrt::weak_ref<wux::FrameworkElement> root;
    wf::IInspectable originalRootMinWidth{nullptr};
    // Its icon column, for app buttons (ApplyIconColumn): the taskbar's own
    // width, and the one we last set.
    double originalIconColumn = 0;
    double appliedIconColumn = 0;
    // Room on either side, beyond the spacing: for a divider or the Recycle
    // Bin, and to move File Explorer (see LayOutButtons).
    float extraLeft = 0, extraRight = 0;
};

[[clang::no_destroy]] inline std::optional<std::list<Button>> g_buttons{std::in_place};

inline Button* FindButton(wux::FrameworkElement const& el) {
    if (!g_buttons) return nullptr;
    for (auto& b : *g_buttons) {
        if (b.element.get() == el) return &b;
    }
    return nullptr;
}

inline void ApplySpacing(wux::FrameworkElement const& el) {
    Button* b = FindButton(el);
    wux::Thickness want{g_buttonMargin + (b ? b->extraLeft : 0), 0,
                        g_buttonMargin + (b ? b->extraRight : 0), 0};
    auto m = el.Margin();
    if (m.Left != want.Left || m.Top != want.Top || m.Right != want.Right ||
        m.Bottom != want.Bottom) {
        el.Margin(want);
    }
}

inline void RestoreLocalValue(wux::FrameworkElement const& el, wux::DependencyProperty const& dp,
                       wf::IInspectable const& value) {
    if (!value || value == wux::DependencyProperty::UnsetValue()) {
        el.ClearValue(dp);
    } else {
        el.SetValue(dp, value);
    }
}

inline float ButtonPivotY(wux::FrameworkElement const& button) {
    return (float)button.ActualHeight() / 2 + g_iconSize / 2;
}

inline void SetScale(wux::FrameworkElement const& el, float s, float tx = 0) {
    auto visual = wuxh::ElementCompositionPreview::GetElementVisual(el);
    visual.CenterPoint({(float)el.ActualWidth() / 2, ButtonPivotY(el), 0});
    visual.Scale({s, s, 1});
    float ty = GetRowOffsetY(wuxm::VisualTreeHelper::GetParent(el));
    wuxh::ElementCompositionPreview::SetIsTranslationEnabled(el, true);
    visual.Properties().InsertVector3(L"Translation", {tx, ty, 0});
    PlaceIndicator(el, s);
}

// The row of icons the pointer is currently over. Its positions are re-read
// every frame (see OnFrame), so it follows apps opening, closing and being
// dragged to a new spot.
struct RowItem {
    winrt::weak_ref<wux::FrameworkElement> element;
    float center = 0;  // horizontal center, in the row's coordinates
    float width = 0;
    float scale = 1.0f;
    float shift = 0;
    bool trash = false;  // the Recycle Bin, which isn't a taskbar button
};

inline std::vector<RowItem> g_row;
inline winrt::weak_ref<wux::UIElement> g_rowParent;

inline bool g_pointerOver = false;
inline bool g_dockExpanded = false;  // icons magnified or about to be (see ClipTaskbars)
inline float g_pointerX = 0;

inline bool g_anchorLeftEnd = true;
inline float g_maxGrowth = 0, g_lastGrowth = 0;

// ---- Hidden taskbar background ----

struct HiddenBackground {
    winrt::weak_ref<wux::FrameworkElement> element;
    double originalOpacity = 1.0;
};

inline std::list<HiddenBackground> g_backgrounds;

inline void HideBackground(wux::FrameworkElement const& el) {
    if (!el) return;
    g_backgrounds.remove_if([](HiddenBackground const& b) { return !b.element.get(); });
    for (auto& b : g_backgrounds) {
        if (b.element.get() == el) return;
    }
    g_backgrounds.push_back({winrt::make_weak(el), el.Opacity()});
    el.Opacity(0);
    Wh_Log(L"Taskbar background hidden: %s", winrt::get_class_name(el).c_str());
}

inline void RestoreHidden(HiddenBackground const& b) {
    if (auto el = b.element.get()) {
        try { el.Opacity(b.originalOpacity); } catch (...) {}
    }
}

inline void RestoreBackgrounds() {
    for (auto& b : g_backgrounds) RestoreHidden(b);
    g_backgrounds.clear();
}


inline wux::FrameworkElement FindTaskbarFrame(wux::FrameworkElement const& el) {
    wux::DependencyObject current = el;
    for (int depth = 0; current && depth < 20; depth++) {
        if (winrt::get_class_name(current) == L"Taskbar.TaskbarFrame") {
            return current.try_as<wux::FrameworkElement>();
        }
        current = wuxm::VisualTreeHelper::GetParent(current);
    }
    return nullptr;
}

// ---- Button highlight ----

struct HiddenHighlight {
    winrt::weak_ref<wux::FrameworkElement> element;
    double originalOpacity = 1.0;
    wux::Visibility originalVisibility = wux::Visibility::Visible;
    int64_t opacityToken = 0, visibilityToken = 0;
};

inline std::vector<HiddenHighlight> g_highlights;

inline void ApplyHighlightHidden(wux::UIElement const& el) {
    if (el.Opacity() != 0) el.Opacity(0);
    if (el.Visibility() != wux::Visibility::Collapsed) el.Visibility(wux::Visibility::Collapsed);
}

inline void HideHighlight(wux::FrameworkElement const& el) {
    g_highlights.erase(std::remove_if(g_highlights.begin(), g_highlights.end(),
                                      [](HiddenHighlight& h) { return !h.element.get(); }),
                       g_highlights.end());
    for (auto& h : g_highlights) {
        if (h.element.get() == el) return;
    }
    auto& h = g_highlights.emplace_back();
    h.element = winrt::make_weak(el);
    h.originalOpacity = el.Opacity();
    h.originalVisibility = el.Visibility();
    auto rehide = [](wux::DependencyObject const& sender, wux::DependencyProperty const&) {
        try {
            if (auto ui = sender.try_as<wux::UIElement>()) ApplyHighlightHidden(ui);
        } catch (...) {}
    };
    h.opacityToken = el.RegisterPropertyChangedCallback(wux::UIElement::OpacityProperty(), rehide);
    h.visibilityToken =
        el.RegisterPropertyChangedCallback(wux::UIElement::VisibilityProperty(), rehide);
    ApplyHighlightHidden(el);
}

inline void RestoreHighlights() {
    for (auto& h : g_highlights) {
        auto el = h.element.get();
        if (!el) continue;
        try {
            el.UnregisterPropertyChangedCallback(wux::UIElement::OpacityProperty(),
                                                 h.opacityToken);
            el.UnregisterPropertyChangedCallback(wux::UIElement::VisibilityProperty(),
                                                 h.visibilityToken);
            el.Opacity(h.originalOpacity);
            el.Visibility(h.originalVisibility);
        } catch (...) {}
    }
    g_highlights.clear();
}

// ---- Icon size ----

struct IconImage {
    winrt::weak_ref<wux::FrameworkElement> element;
    wf::IInspectable originalWidth{nullptr};   // local values, may be UnsetValue
    wf::IInspectable originalHeight{nullptr};
    int64_t widthToken = 0, heightToken = 0;
};

[[clang::no_destroy]] inline std::optional<std::list<IconImage>> g_iconImages{std::in_place};

inline void ApplyIconSize(wux::FrameworkElement const& el) {
    if (el.Width() != g_iconSize) el.Width(g_iconSize);
    if (el.Height() != g_iconSize) el.Height(g_iconSize);
}

inline void TrackIconImage(wux::FrameworkElement const& el) {
    for (auto& i : *g_iconImages) {
        if (i.element.get() == el) return;
    }
    auto& i = g_iconImages->emplace_back();
    i.element = winrt::make_weak(el);
    i.originalWidth = el.ReadLocalValue(wux::FrameworkElement::WidthProperty());
    i.originalHeight = el.ReadLocalValue(wux::FrameworkElement::HeightProperty());
    auto reapply = [](wux::DependencyObject const& sender, wux::DependencyProperty const&) {
        try {
            if (auto fe = sender.try_as<wux::FrameworkElement>()) ApplyIconSize(fe);
        } catch (...) {}
    };
    i.widthToken =
        el.RegisterPropertyChangedCallback(wux::FrameworkElement::WidthProperty(), reapply);
    i.heightToken =
        el.RegisterPropertyChangedCallback(wux::FrameworkElement::HeightProperty(), reapply);
    ApplyIconSize(el);
}

inline void ReleaseIconImage(IconImage const& i) {
    auto el = i.element.get();
    if (!el) return;
    try {
        el.UnregisterPropertyChangedCallback(wux::FrameworkElement::WidthProperty(),
                                             i.widthToken);
        el.UnregisterPropertyChangedCallback(wux::FrameworkElement::HeightProperty(),
                                             i.heightToken);
        RestoreLocalValue(el, wux::FrameworkElement::WidthProperty(), i.originalWidth);
        RestoreLocalValue(el, wux::FrameworkElement::HeightProperty(), i.originalHeight);
    } catch (...) {}
}

inline void RestoreIconImages() {
    for (auto& i : *g_iconImages) ReleaseIconImage(i);
    g_iconImages->clear();
}

// A visible taskbar button and where it sits inside its row.
struct Icon {
    wux::FrameworkElement element{nullptr};
    float left = 0;  // in the row's coordinates
    float top = 0;
    float width = 0;
    float height = 0;
    float slotLeft = 0, slotWidth = 0;
};

inline std::vector<Icon> GetIcons(wux::UIElement const& parent) {
    std::vector<Icon> icons;
    for (auto& b : *g_buttons) {
        auto other = b.element.get();
        if (!other || other.Visibility() != wux::Visibility::Visible ||
            other.ActualWidth() <= 0) {
            continue;
        }
        if (wuxm::VisualTreeHelper::GetParent(other) != parent) continue;
        auto slot = wuxc::Primitives::LayoutInformation::GetLayoutSlot(other);
        auto margin = other.Margin();
        float x = slot.X + (float)margin.Left;
        float y = slot.Y + (float)margin.Top;
        if (y < -other.ActualHeight() || y > other.ActualHeight()) continue;
        icons.push_back({other, x, y, (float)other.ActualWidth(), (float)other.ActualHeight(),
                         slot.X, slot.Width});
    }
    return icons;
}

// ---- Taskbar buttons ----

inline Button& OnButtonAdded(wux::FrameworkElement const& el) {
    for (auto& b : *g_buttons) {
        if (b.element.get() == el) return b;
    }
    auto& b = g_buttons->emplace_back();
    b.element = winrt::make_weak(el);
    b.originalMargin = el.ReadLocalValue(wux::FrameworkElement::MarginProperty());
    b.marginToken = el.RegisterPropertyChangedCallback(
        wux::FrameworkElement::MarginProperty(),
        [](wux::DependencyObject const& sender, wux::DependencyProperty const&) {
            try {
                if (auto fe = sender.try_as<wux::FrameworkElement>()) ApplySpacing(fe);
            } catch (...) {}
        });
    b.sizeChanged = el.SizeChanged(
        winrt::auto_revoke,
        [](wf::IInspectable const&, wux::SizeChangedEventArgs const&) { MarkPlatesDirty(); });
    ApplySpacing(el);
    MarkPlatesDirty();
    return b;
}

inline void ApplyButtonWidth(Button& b, wux::FrameworkElement const& button) {
    auto root = b.root.get();
    if (!root) {
        if (wuxm::VisualTreeHelper::GetChildrenCount(button) == 0) return;
        root = wuxm::VisualTreeHelper::GetChild(button, 0).try_as<wux::FrameworkElement>();
        if (!root) return;
        b.root = winrt::make_weak(root);
        b.originalRootMinWidth = root.ReadLocalValue(wux::FrameworkElement::MinWidthProperty());
    }
    if (root.MinWidth() != g_buttonWidth) root.MinWidth(g_buttonWidth);
}

inline void ApplyIconColumn(Button& b) {
    auto root = b.root.get();
    auto grid = root ? root.try_as<wuxc::Grid>() : nullptr;
    if (!grid) return;
    auto columns = grid.ColumnDefinitions();
    if (columns.Size() == 0) return;
    auto column = columns.GetAt(0);
    auto width = column.Width();
    if (width.GridUnitType != wux::GridUnitType::Pixel) return;

    if (width.Value != b.appliedIconColumn) b.originalIconColumn = width.Value;
    auto padding = grid.Padding();
    double target = g_buttonWidth - padding.Left - padding.Right;
    if (width.Value != target) column.Width({target, wux::GridUnitType::Pixel});
    b.appliedIconColumn = target;
}

inline void ReleaseButton(Button& b) {
    b.sizeChanged.revoke();
    auto el = b.element.get();
    if (!el) return;
    if (b.marginToken) {
        el.UnregisterPropertyChangedCallback(wux::FrameworkElement::MarginProperty(),
                                             b.marginToken);
        b.marginToken = 0;
    }
    RestoreLocalValue(el, wux::FrameworkElement::MarginProperty(), b.originalMargin);
    if (auto root = b.root.get()) {
        if (b.originalIconColumn > 0) {
            if (auto grid = root.try_as<wuxc::Grid>(); grid && grid.ColumnDefinitions().Size()) {
                auto column = grid.ColumnDefinitions().GetAt(0);
                if (column.Width().Value == b.appliedIconColumn) {
                    column.Width({b.originalIconColumn, wux::GridUnitType::Pixel});
                }
            }
        }
        RestoreLocalValue(root, wux::FrameworkElement::MinWidthProperty(),
                          b.originalRootMinWidth);
    }
}

inline std::wstring GetAppIdentifier(wux::FrameworkElement const& button) {
    std::wstring combined;
    auto appendIf = [&](winrt::hstring const& hs) {
        if (!hs.empty()) {
            if (!combined.empty()) combined += L" ";
            combined += hs;
        }
    };
    try { appendIf(wuxa::AutomationProperties::GetAutomationId(button)); } catch (...) {}
    try { appendIf(wuxa::AutomationProperties::GetName(button)); } catch (...) {}
    try { appendIf(wuxa::AutomationProperties::GetHelpText(button)); } catch (...) {}
    try { appendIf(button.Name()); } catch (...) {}
    try {
        auto tip = wuxc::ToolTipService::GetToolTip(button);
        if (tip) {
            auto s = winrt::unbox_value_or<winrt::hstring>(tip, L"");
            appendIf(s);
            if (auto tt = tip.try_as<wuxc::ToolTip>()) {
                auto cs = winrt::unbox_value_or<winrt::hstring>(tt.Content(), L"");
                appendIf(cs);
            }
        }
    } catch (...) {}

    ForEachDescendant(button, 6, [&](wux::FrameworkElement const& el) {
        try { appendIf(wuxa::AutomationProperties::GetAutomationId(el)); } catch (...) {}
        try { appendIf(wuxa::AutomationProperties::GetName(el)); } catch (...) {}
        try { appendIf(el.Name()); } catch (...) {}
        return true;
    });
    return combined;
}

inline bool ShouldFillSquircle(std::wstring const& title) {
    if (title.empty()) return false;
    std::wstring lower = title;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    // Antigravity (ban trang - electron.app.Antigravity) co transparent padding ben trong nen phai lot card trang day du
    bool isAntigravityWhite = (lower.find(L"electron.app.antigravity") != std::wstring::npos) ||
                             (lower.find(L"antigravity") != std::wstring::npos &&
                              lower.find(L"ide") == std::wstring::npos);
    if (isAntigravityWhite) {
        return false;
    }

    // Danh sach cac app co icon phang 4 huong / squircle:
    // Cho phep tran ra (full-bleed 100% squircle), khong lot card de tranh bi double squircle
    static const std::vector<std::wstring> kFillKeywords = {
        L"anydesk",
        L"antigravity ide", // Antigravity IDE la ban DEN -> cho tran vien 100%
        L"google.antigravityide",
        L"deepmind",
        L"blip",
        L"photos",
        L"seelen",
        L"terminal",
        L"windowsterminal",
        L"wt.exe",
        L"powershell",
        L"cmd",
        L"wsl",
        L"whatsapp",
        L"figma",
        L"photoshop",
        L"illustrator"
    };

    for (auto const& kw : kFillKeywords) {
        if (lower.find(kw) != std::wstring::npos) return true;
    }
    return false;
}

inline void ProcessButton(wux::FrameworkElement const& button) {
    bool isStart = IsStartButton(button);
    if (g_hideStartButton && isStart) {
        button.Visibility(wux::Visibility::Collapsed);
        return;
    }

    Button& b = OnButtonAdded(button);
    ApplyButtonWidth(b, button);
    ApplyIconColumn(b);
    bool isExplorer = IsFileExplorer(button);
    std::wstring title = GetAppIdentifier(button);
    IconProfile prof = GetIconProfileForButton(button, title);
    DockLog(L"ProcessButton: id='%s' -> isSquircle=%d, isDark=%d, padMul=%.2f",
           title.c_str(), prof.isSquircle ? 1 : 0, prof.isDark ? 1 : 0, prof.paddingMultiplier);

    auto visit = [&](wux::FrameworkElement const& el) {
        auto name = el.Name();
        if (name == L"Icon") {
            DockLog(L"[icon] class='%s' isExplorer=%d", winrt::get_class_name(el).c_str(), isExplorer ? 1 : 0);
            TrackIconImage(el);
            if (isExplorer) {
                // Đảm bảo không bị lót MacAppBackground từ trước và reset scale về 1.0
                RemoveIconBackground(el);
                if (auto img = el.try_as<wuxc::Image>()) {
                    img.Stretch(wuxm::Stretch::Uniform);
                    img.Width(g_iconSize);
                    img.Height(g_iconSize);
                    if (auto finder = LoadImageFromFile(L"C:\\ProgramData\\Windhawk\\EditorWorkspace\\finder.png", (int)(g_iconSize * 2.0f))) {
                        img.Source(finder);
                    }
                }
                // Giữ nguyên antialiasing siêu mượt của Apple, không gọt hard clip gây răng cưa
            } else if (isStart) {
                if (auto img = el.try_as<wuxc::Image>()) {
                    img.Stretch(wuxm::Stretch::Uniform);
                    img.Width(g_iconSize);
                    img.Height(g_iconSize);
                    if (auto finder = LoadImageFromFile(L"C:\\ProgramData\\Windhawk\\EditorWorkspace\\finder.png", (int)(g_iconSize * 2.0f))) {
                        img.Source(finder);
                    }
                }
            } else {
                EnsureIconBackground(el, prof, title);
                ClipToSquircle(el, g_iconSize);
            }
        } else if (name == L"RunningIndicator") {
            TrackIndicator(el, button);
        } else if ((name == L"BackgroundElement" || name == L"MultiWindowElement") &&
                   el.try_as<wuxc::Border>()) {
            HideHighlight(el);
        } else if (std::wstring_view(name).find(L"Badge") != std::wstring_view::npos ||
                    std::wstring_view(winrt::get_class_name(el)).find(L"Badge") != std::wstring_view::npos) {
            ApplyBadgeRed(el);
            return false;
        }
        return true;
    };
    ForEachDescendant(button, 6, visit);
}

inline void ScanAllBadges() {
    if (!g_buttons) return;
    for (auto& b : *g_buttons) {
        auto btn = b.element.get();
        if (!btn || btn.Visibility() != wux::Visibility::Visible) continue;
        ForEachDescendant(btn, 6, [](wux::FrameworkElement const& el) {
            auto name = el.Name();
            std::wstring_view type = winrt::get_class_name(el);
            if (std::wstring_view(name).find(L"Badge") != std::wstring_view::npos ||
                type.find(L"Badge") != std::wstring_view::npos) {
                if (el.Visibility() == wux::Visibility::Visible) {
                    ApplyBadgeRed(el);
                }
                return false;
            }
            return true;
        });
    }
}

inline void RefreshAllButtonsAndCards() {
    g_iconProfileCache.clear();
    if (!g_buttons) return;
    for (auto& b : *g_buttons) {
        if (auto btn = b.element.get()) {
            ForEachDescendant(btn, 6, [](wux::FrameworkElement const& el) {
                if (el.Name() == L"Icon" || el.Name() == L"IconImage") {
                    RemoveIconBackground(el);
                }
                return true;
            });
            ProcessButton(btn);
        }
    }
}
