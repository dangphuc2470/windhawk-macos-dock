#pragma once

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include "dock_utils.h"
#include "dock_icon_analyzer.h"

namespace wf = winrt::Windows::Foundation;
namespace wux = winrt::Windows::UI::Xaml;
namespace wuxc = winrt::Windows::UI::Xaml::Controls;
namespace wuxm = winrt::Windows::UI::Xaml::Media;
namespace wuxh = winrt::Windows::UI::Xaml::Hosting;

extern float g_iconSize;

namespace DockIconCard {
    // 0.85 = ti le chuan 85% goc cua user
    constexpr double kInnerIconScale = 0.75;
}

inline bool IsDarkTileApp(std::wstring const& lower) {
    static const std::vector<std::wstring> kDarkApps = {
        L"spotify", L"discord", L"terminal", L"windowsterminal", L"powershell",
        L"pwsh", L"cmd", L"command prompt", L"wsl", L"bash", L"steam",
        L"obsidian", L"sublime", L"windhawk", L"github", L"eagle"
    };
    for (auto const& kw : kDarkApps) {
        if (lower.find(kw) != std::wstring::npos) return true;
    }
    return false;
}

// Khung cung Squircle ben ngoai tu dong cat cac phan vuot ra ngoai
inline void ClipToSquircle(wux::FrameworkElement const& el, float size) {
    if (!el || size <= 0.0f) return;
    float r = size * 0.225f;

    try {
        auto visual = wuxh::ElementCompositionPreview::GetElementVisual(el);
        if (visual) {
            visual.Size({size, size});
            // Khung cung luon giu ti le 1.0 de clip khong bi meo
            visual.Scale({1.0f, 1.0f, 1.0f});
            auto compositor = visual.Compositor();
            if (compositor) {
                auto shape = compositor.CreateRoundedRectangleGeometry();
                shape.CornerRadius({r, r});
                shape.Size({size, size});
                visual.Clip(compositor.CreateGeometricClip(shape));
            }
        }
    } catch (...) {}
}

inline void RemoveIconBackground(wux::FrameworkElement const& iconEl) {
    if (!iconEl) return;
    try {
        if (auto parent = wuxm::VisualTreeHelper::GetParent(iconEl).try_as<wuxc::Panel>()) {
            uint32_t count = parent.Children().Size();
            for (uint32_t i = 0; i < count; i++) {
                if (auto child = parent.Children().GetAt(i).try_as<wux::FrameworkElement>()) {
                    if (child.Name() == L"MacAppBackground") {
                        parent.Children().RemoveAt(i);
                        break;
                    }
                }
            }
        }
        iconEl.RenderTransform(nullptr);
        auto visual = wuxh::ElementCompositionPreview::GetElementVisual(iconEl);
        if (visual) {
            visual.Scale({1.0f, 1.0f, 1.0f});
        }
    } catch (...) {}
}

inline void EnsureIconBackground(wux::FrameworkElement const& iconEl, IconProfile const& prof, std::wstring const& title = L"") {
    using namespace DockIconCard;

    if (prof.isSquircle) {
        // App phang 4 huong / squircle: cho phep tran ra day du 100% squircle, khong lot card
        RemoveIconBackground(iconEl);

        auto configureFillImage = [&](wuxc::Image const& img) {
            img.Stretch(wuxm::Stretch::UniformToFill);
            img.HorizontalAlignment(wux::HorizontalAlignment::Center);
            img.VerticalAlignment(wux::VerticalAlignment::Center);
            img.Width(g_iconSize);
            img.Height(g_iconSize);
        };

        if (auto img = iconEl.try_as<wuxc::Image>()) {
            configureFillImage(img);
        }
        ForEachDescendant(iconEl, 3, [&](wux::FrameworkElement const& child) {
            if (auto img = child.try_as<wuxc::Image>()) {
                configureFillImage(img);
            }
            return true;
        });

        if (prof.edgeZoom > 1.001) {
            wuxm::ScaleTransform st;
            st.ScaleX(prof.edgeZoom);
            st.ScaleY(prof.edgeZoom);
            st.CenterX(g_iconSize / 2.0);
            st.CenterY(g_iconSize / 2.0);
            iconEl.RenderTransform(st);
        } else {
            iconEl.RenderTransform(nullptr);
        }

        // Khung squircle khoa dung kich thuoc g_iconSize va tu dong cat gon 4 goc
        ClipToSquircle(iconEl, g_iconSize);
        return;
    }

    // App co card nen: lot MacAppBackground (trang mac dinh hoac toi theo ket qua pixel analyzer)
    bool isDark = prof.isDark;

    auto parent = wuxm::VisualTreeHelper::GetParent(iconEl).try_as<wuxc::Panel>();
    if (parent) {
        bool hasMatchingBg = false;
        uint32_t count = parent.Children().Size();
        for (uint32_t i = 0; i < count; i++) {
            if (auto child = parent.Children().GetAt(i).try_as<wux::FrameworkElement>()) {
                if (child.Name() == L"MacAppBackground") {
                    auto tag = winrt::unbox_value_or<int>(child.Tag(), -1);
                    if (tag == (isDark ? 1 : 0)) {
                        hasMatchingBg = true;
                    } else {
                        parent.Children().RemoveAt(i);
                    }
                    break;
                }
            }
        }

        if (!hasMatchingBg) {
            wuxc::Border bg;
            bg.Name(L"MacAppBackground");
            bg.Tag(winrt::box_value(isDark ? 1 : 0));
            bg.Width(g_iconSize);
            bg.Height(g_iconSize);
            float r = g_iconSize * 0.225f;
            bg.CornerRadius(wux::CornerRadius{r, r, r, r});

            wuxm::LinearGradientBrush cardBrush;
            cardBrush.StartPoint({0, 0});
            cardBrush.EndPoint({0, 1});

            wuxm::GradientStop s1, s2;
            if (isDark) {
                // macOS Dark tile gradient (#4A4A4E -> #1F1F22)
                s1.Color(winrt::Windows::UI::Color{255, 0x4A, 0x4A, 0x4E});
                s2.Color(winrt::Windows::UI::Color{255, 0x1F, 0x1F, 0x22});
                bg.BorderBrush(wuxm::SolidColorBrush(winrt::Windows::UI::Color{45, 255, 255, 255}));
            } else {
                // macOS Light tile gradient (#FFFFFF -> #E9E9EE)
                s1.Color(winrt::Windows::UI::Color{255, 0xFF, 0xFF, 0xFF});
                s2.Color(winrt::Windows::UI::Color{255, 0xE9, 0xE9, 0xEE});
                bg.BorderBrush(wuxm::SolidColorBrush(winrt::Windows::UI::Color{30, 0, 0, 0}));
            }
            s1.Offset(0.0);
            s2.Offset(1.0);
            cardBrush.GradientStops().Append(s1);
            cardBrush.GradientStops().Append(s2);
            bg.Background(cardBrush);

            bg.BorderThickness(wux::Thickness{0.5, 0.5, 0.5, 0.5});
            bg.HorizontalAlignment(wux::HorizontalAlignment::Center);
            bg.VerticalAlignment(wux::VerticalAlignment::Center);
            bg.IsHitTestVisible(false);

            ClipToSquircle(bg, g_iconSize);
            try {
                auto visual = wuxh::ElementCompositionPreview::GetElementVisual(bg);
                if (auto sprite = visual.try_as<winrt::Windows::UI::Composition::SpriteVisual>()) {
                    auto compositor = sprite.Compositor();
                    if (compositor) {
                        auto shadow = compositor.CreateDropShadow();
                        shadow.BlurRadius(5.0f);
                        shadow.Color(winrt::Windows::UI::Color{50, 0, 0, 0});
                        shadow.Offset({0.0f, 1.5f, 0.0f});
                        sprite.Shadow(shadow);
                    }
                }
            } catch (...) {}

            uint32_t iconIndex = 0;
            if (parent.Children().IndexOf(iconEl, iconIndex)) {
                parent.Children().InsertAt(iconIndex, bg);
            }
        }
    }

    auto configureImage = [&](wuxc::Image const& img) {
        img.Stretch(wuxm::Stretch::Uniform);
        img.HorizontalAlignment(wux::HorizontalAlignment::Center);
        img.VerticalAlignment(wux::VerticalAlignment::Center);
        img.Width(g_iconSize);
        img.Height(g_iconSize);
    };

    if (auto img = iconEl.try_as<wuxc::Image>()) {
        configureImage(img);
    }
    ForEachDescendant(iconEl, 3, [&](wux::FrameworkElement const& child) {
        if (auto img = child.try_as<wuxc::Image>()) {
            configureImage(img);
        }
        return true;
    });

    // Ti le scale luon lay truc tiep tu kInnerIconScale ma user da cau hinh,
    // nhan voi he so bu padding neu asset bi thut le (nhu Antigravity trang)
    double scale = kInnerIconScale * prof.paddingMultiplier;

    wuxm::ScaleTransform st;
    st.ScaleX(scale);
    st.ScaleY(scale);
    st.CenterX(g_iconSize / 2.0);
    st.CenterY(g_iconSize / 2.0);
    iconEl.RenderTransform(st);

    ClipToSquircle(iconEl, g_iconSize);
}
