#pragma once

#include "dock_utils.h"

// ============================================================================
// macOS Style Dock - Badge Styling Module
// ============================================================================

namespace DockBadge {
    // Chinh do day vien trang: 0.0f = bo outline trang, 1.5f = co outline trang
    constexpr float kOutlineThickness = 0.0f;

    // Chinh vi tri badge:
    // kOffsetX: so duong (+) day sang phai ra ngoai icon, so am (-) keo vao trong
    // kOffsetY: so am (-) day len tren cao, so duong (+) keo xuong duoi
    constexpr float kOffsetX = 1.0f; // Dịch qua trái 2px (từ 3.0f -> 1.0f)
    constexpr float kOffsetY = 4.0f; // Dịch xuống dưới 2px (từ 2.0f -> 4.0f)

    // Kich thuoc badge
    constexpr double kBadgeHeight = 18.0;
    constexpr double kBadgeCircleSize = 18.0;
}

struct TrackedBadge {
    winrt::weak_ref<wux::FrameworkElement> element;
    int64_t bgToken = 0;
    int64_t visToken = 0;
    bool updating = false;
};
inline std::list<TrackedBadge> g_trackedBadges;

inline void RestoreBadges() {
    for (auto& tb : g_trackedBadges) {
        if (auto el = tb.element.get()) {
            if (tb.visToken) {
                try { el.UnregisterPropertyChangedCallback(wux::UIElement::VisibilityProperty(), tb.visToken); } catch (...) {}
            }
            if (tb.bgToken) {
                try {
                    if (auto b = el.try_as<wuxc::Border>()) {
                        b.UnregisterPropertyChangedCallback(wuxc::Border::BackgroundProperty(), tb.bgToken);
                    }
                } catch (...) {}
            }
        }
    }
    g_trackedBadges.clear();
}

inline void ApplyBadgeRed(wux::FrameworkElement const& el) {
    if (!el) return;
    using namespace DockBadge;

    // Track badge property changes to automatically re-enforce red if Windows resets to accent color
    TrackedBadge* trackedPtr = nullptr;
    for (auto& tb : g_trackedBadges) {
        if (tb.element.get() == el) {
            trackedPtr = &tb;
            break;
        }
    }
    if (!trackedPtr) {
        auto& tb = g_trackedBadges.emplace_back();
        tb.element = winrt::make_weak(el);
        trackedPtr = &tb;
        auto onPropChanged = [&tb](wux::DependencyObject const& sender, wux::DependencyProperty const&) {
            if (tb.updating) return;
            if (auto fe = sender.try_as<wux::FrameworkElement>()) {
                ApplyBadgeRed(fe);
            }
        };
        try {
            tb.visToken = el.RegisterPropertyChangedCallback(wux::UIElement::VisibilityProperty(), onPropChanged);
        } catch (...) {}
        if (auto border = el.try_as<wuxc::Border>()) {
            try {
                tb.bgToken = border.RegisterPropertyChangedCallback(wuxc::Border::BackgroundProperty(), onPropChanged);
            } catch (...) {}
        }
    }

    if (trackedPtr) trackedPtr->updating = true;

    winrt::Windows::UI::Color redColor{255, 255, 59, 48}; // macOS red #FF3B30
    wuxm::SolidColorBrush redBrush{redColor};
    wuxm::SolidColorBrush whiteBrush{winrt::Windows::UI::Color{255, 255, 255, 255}};
    wuxm::SolidColorBrush nullBrush{nullptr};

    // Tim TextBlock truoc de biet so chu so (1 so -> hinh tron 18x18, 2+ so -> hinh pill)
    std::wstring badgeText;
    ForEachDescendant(el, 6, [&](wux::FrameworkElement const& fe) {
        if (auto tb = fe.try_as<wuxc::TextBlock>()) {
            badgeText = tb.Text().c_str();
            return false;
        }
        return true;
    });

    double badgeW = kBadgeCircleSize;
    if (badgeText.length() <= 1) {
        // 1 chu so (vi du "9", "1") -> Tron xoe 18x18
        badgeW = kBadgeCircleSize;
    } else if (badgeText.length() == 2) {
        // 2 chu so (vi du "10", "99") -> Pill gon 23x18
        badgeW = 23.0;
    } else {
        // 3 chu so tro len (vi du "99+", "100") -> Pill 28x18
        badgeW = 28.0;
    }

    // Set kich thuoc va can goc tren phai tren chinh element badge
    try {
        el.Width(badgeW);
        el.Height(kBadgeHeight);
        el.Margin(wux::Thickness{0, 0, 0, 0});
        wuxc::Grid::SetColumn(el, 0);
        wuxc::Grid::SetRow(el, 0);
        wuxc::Canvas::SetZIndex(el, 999);
        el.HorizontalAlignment(wux::HorizontalAlignment::Right);
        el.VerticalAlignment(wux::VerticalAlignment::Top);
    } catch (...) {}

    // Don dep bat ky MacBadgeRedBackground cu nao
    if (auto parent = wuxm::VisualTreeHelper::GetParent(el).try_as<wuxc::Panel>()) {
        uint32_t count = parent.Children().Size();
        for (uint32_t i = 0; i < count; i++) {
            if (auto child = parent.Children().GetAt(i).try_as<wux::FrameworkElement>()) {
                if (child.Name() == L"MacBadgeRedBackground") {
                    parent.Children().RemoveAt(i);
                    break;
                }
            }
        }
    }

    bool hasBackgroundShape = false;
    auto style = [&](wux::FrameworkElement const& fe) {
        if (auto border = fe.try_as<wuxc::Border>()) {
            border.Background(redBrush);
            border.CornerRadius(wux::CornerRadius{99, 99, 99, 99}); // Round circle/pill
            border.Width(badgeW);
            border.Height(kBadgeHeight);
            border.MinWidth(badgeW);
            border.MinHeight(kBadgeHeight);
            border.Padding(wux::Thickness{0, 0, 0, 0});
            border.HorizontalAlignment(wux::HorizontalAlignment::Right);
            border.VerticalAlignment(wux::VerticalAlignment::Top);
            border.Margin(wux::Thickness{0, 0, 0, 0});
            if (kOutlineThickness > 0) {
                border.BorderBrush(whiteBrush);
                border.BorderThickness(wux::Thickness{kOutlineThickness, kOutlineThickness, kOutlineThickness, kOutlineThickness});
            } else {
                border.BorderBrush(nullBrush);
                border.BorderThickness(wux::Thickness{0, 0, 0, 0});
            }
            hasBackgroundShape = true;
        } else if (auto shape = fe.try_as<wuxs::Shape>()) {
            shape.Fill(redBrush);
            shape.Width(badgeW);
            shape.Height(kBadgeHeight);
            shape.HorizontalAlignment(wux::HorizontalAlignment::Right);
            shape.VerticalAlignment(wux::VerticalAlignment::Top);
            shape.Margin(wux::Thickness{0, 0, 0, 0});
            if (kOutlineThickness > 0) {
                shape.Stroke(whiteBrush);
                shape.StrokeThickness(kOutlineThickness);
            } else {
                shape.Stroke(nullBrush);
                shape.StrokeThickness(0);
            }
            hasBackgroundShape = true;
        } else if (auto grid = fe.try_as<wuxc::Grid>()) {
            if (!hasBackgroundShape) {
                grid.Background(redBrush);
                grid.CornerRadius(wux::CornerRadius{99, 99, 99, 99});
                grid.Width(badgeW);
                grid.Height(kBadgeHeight);
                grid.HorizontalAlignment(wux::HorizontalAlignment::Right);
                grid.VerticalAlignment(wux::VerticalAlignment::Top);
                grid.Margin(wux::Thickness{0, 0, 0, 0});
                hasBackgroundShape = true;
            }
        } else if (auto panel = fe.try_as<wuxc::Panel>()) {
            if (!hasBackgroundShape) {
                panel.Background(redBrush);
                panel.Width(badgeW);
                panel.Height(kBadgeHeight);
                panel.HorizontalAlignment(wux::HorizontalAlignment::Right);
                panel.VerticalAlignment(wux::VerticalAlignment::Top);
                panel.Margin(wux::Thickness{0, 0, 0, 0});
                hasBackgroundShape = true;
            }
        } else if (auto ctrl = fe.try_as<wuxc::Control>()) {
            if (!hasBackgroundShape) {
                ctrl.Background(redBrush);
                ctrl.CornerRadius(wux::CornerRadius{99, 99, 99, 99});
                ctrl.Width(badgeW);
                ctrl.Height(kBadgeHeight);
                ctrl.HorizontalAlignment(wux::HorizontalAlignment::Right);
                ctrl.VerticalAlignment(wux::VerticalAlignment::Top);
                ctrl.Margin(wux::Thickness{0, 0, 0, 0});
                hasBackgroundShape = true;
            }
            ctrl.Foreground(whiteBrush);
        }
        if (auto tb = fe.try_as<wuxc::TextBlock>()) {
            tb.Foreground(whiteBrush);
            tb.FontSize(11.0);
            tb.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
            tb.TextAlignment(wux::TextAlignment::Center);
            tb.HorizontalAlignment(wux::HorizontalAlignment::Center);
            tb.VerticalAlignment(wux::VerticalAlignment::Center);
            tb.Margin(wux::Thickness{0, 0, 0, 0});
        }
        return true;
    };

    style(el);
    ForEachDescendant(el, 6, style);

    // Dat Translation sang phai (kOffsetX) va can tren cao (kOffsetY), go bo clipping mask
    try {
        auto visual = wuxh::ElementCompositionPreview::GetElementVisual(el);
        visual.Clip(nullptr);
        wuxh::ElementCompositionPreview::SetIsTranslationEnabled(el, true);
        visual.Properties().InsertVector3(L"Translation", {kOffsetX, kOffsetY, 0.0f});
    } catch (...) {}

    if (trackedPtr) trackedPtr->updating = false;
}
