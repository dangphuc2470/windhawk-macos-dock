#pragma once

#include <windhawk_utils.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <vector>
#include <list>
#include <optional>
#include <algorithm>
#include <numeric>
#include <cmath>

// Forward declarations of functions and external state
struct RowItem;
extern std::vector<RowItem> g_row;
struct Button;
extern Button* FindButton(wux::FrameworkElement const& el);
extern void ApplySpacing(wux::FrameworkElement const& el);
extern void SetScale(wux::FrameworkElement const& el, float s, float tx);
extern wux::FrameworkElement FindTaskbarFrame(wux::FrameworkElement const& el);
struct Icon;
extern std::vector<Icon> GetIcons(wux::UIElement const& parent);

void StartFrames();
void StartTrash();
void UpdateTrashIcons();
struct Plate;
void HookUpTrash(Plate& p);
void UpdateAllTrayPlates();

// ---- Dock plate: a rounded box drawn behind each taskbar's icons ----

constexpr float kMaxBlur = 60;

enum DOCK_EFFECT_PROPERTY_MAPPING : int {
    DOCK_EFFECT_PROPERTY_MAPPING_UNKNOWN = 0,
    DOCK_EFFECT_PROPERTY_MAPPING_DIRECT = 1,
};

MIDL_INTERFACE("2FC57384-A068-44D7-A331-30982FCF7177")
IDockGraphicsEffectD2D1Interop : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetEffectId(GUID* id) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetNamedPropertyMapping(
        LPCWSTR name, UINT* index, DOCK_EFFECT_PROPERTY_MAPPING* mapping) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyCount(UINT* count) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProperty(UINT index, void** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSource(UINT index, void** source) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSourceCount(UINT* count) = 0;
};
__CRT_UUID_DECL(IDockGraphicsEffectD2D1Interop, 0x2fc57384, 0xa068, 0x44d7, 0xa3, 0x31, 0x30,
                0x98, 0x2f, 0xcf, 0x71, 0x77)

constexpr GUID kGaussianBlurId = {
    0x1feb6d69, 0x2fe6, 0x4ac9, {0x8c, 0x58, 0x1d, 0x7f, 0x93, 0xe7, 0xa6, 0xa5}};

class GaussianBlurEffect
    : public winrt::implements<GaussianBlurEffect, wge::IGraphicsEffect,
                               wge::IGraphicsEffectSource, IDockGraphicsEffectD2D1Interop> {
public:
    GaussianBlurEffect(float amount, wge::IGraphicsEffectSource const& source)
        : m_amount(amount), m_source(source) {}

    winrt::hstring Name() { return m_name; }
    void Name(winrt::hstring const& name) { m_name = name; }

    HRESULT STDMETHODCALLTYPE GetEffectId(GUID* id) override {
        *id = kGaussianBlurId;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetNamedPropertyMapping(
        LPCWSTR name, UINT* index, DOCK_EFFECT_PROPERTY_MAPPING* mapping) override {
        if (_wcsicmp(name, L"BlurAmount") != 0) return E_INVALIDARG;
        *index = 0;
        *mapping = DOCK_EFFECT_PROPERTY_MAPPING_DIRECT;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPropertyCount(UINT* count) override {
        *count = 3;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetProperty(UINT index, void** value) override {
        try {
            wf::IInspectable v;
            switch (index) {
                case 0: v = wf::PropertyValue::CreateSingle(m_amount); break;
                case 1: v = wf::PropertyValue::CreateUInt32(1); break;
                case 2: v = wf::PropertyValue::CreateUInt32(1); break;
                default: return E_INVALIDARG;
            }
            *value = winrt::detach_abi(v.as<wf::IPropertyValue>());
            return S_OK;
        } catch (...) {
            return winrt::to_hresult();
        }
    }
    HRESULT STDMETHODCALLTYPE GetSource(UINT index, void** source) override {
        if (index != 0) return E_INVALIDARG;
        winrt::copy_to_abi(m_source, *source);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetSourceCount(UINT* count) override {
        *count = 1;
        return S_OK;
    }

private:
    float m_amount;
    wge::IGraphicsEffectSource m_source;
    winrt::hstring m_name;
};

inline wuc::CompositionRoundedRectangleGeometry SetBlurVisual(wux::UIElement const& host, float amount) {
    if (amount <= 0) {
        wuxh::ElementCompositionPreview::SetElementChildVisual(host, nullptr);
        return nullptr;
    }
    auto hostVisual = wuxh::ElementCompositionPreview::GetElementVisual(host);
    auto compositor = hostVisual.Compositor();

    auto effect = winrt::make<GaussianBlurEffect>(
        amount, wuc::CompositionEffectSourceParameter(L"Backdrop"));
    auto brush = compositor.CreateEffectFactory(effect).CreateBrush();
    brush.SetSourceParameter(L"Backdrop", compositor.CreateBackdropBrush());

    auto sprite = compositor.CreateSpriteVisual();
    sprite.Brush(brush);

    auto size = compositor.CreateExpressionAnimation(L"host.Size");
    size.SetReferenceParameter(L"host", hostVisual);
    sprite.StartAnimation(L"Size", size);
    auto shape = compositor.CreateRoundedRectangleGeometry();
    shape.CornerRadius({g_dockRadius, g_dockRadius});
    shape.StartAnimation(L"Size", size);
    sprite.Clip(compositor.CreateGeometricClip(shape));

    wuxh::ElementCompositionPreview::SetElementChildVisual(host, sprite);
    return shape;
}

#include <winrt/Windows.UI.Xaml.Shapes.h>
namespace wuxs = winrt::Windows::UI::Xaml::Shapes;

constexpr float kCornerReach = 1.52866f;
constexpr float kCorner[16][2] = {
    {kCornerReach, 0},
    {1.08849f, 0}, {0.86841f, 0}, {0.66993f, 0.06550f},
    {0.66993f, 0.06550f}, {0.63149f, 0.07491f}, {0.63149f, 0.07491f},
    {0.37282f, 0.16906f}, {0.16906f, 0.37282f}, {0.07491f, 0.63149f},
    {0.07491f, 0.63149f}, {0.06550f, 0.66993f}, {0.06550f, 0.66993f},
    {0, 0.86841f}, {0, 1.08849f}, {0, kCornerReach},
};

inline float SquircleRadius(float w, float h, float r) {
    return std::clamp(std::min(w, h) / 2 / kCornerReach, 0.0f, r);
}

inline std::vector<wf::Point> Squircle(float x, float y, float w, float h, float rx, float ry) {
    struct Corner { float x, y, inX, inY, outX, outY; };
    Corner corners[] = {{x + w, y, -1, 0, 0, 1}, {x + w, y + h, 0, -1, -1, 0},
                        {x, y + h, 1, 0, 0, -1}, {x, y, 0, 1, 1, 0}};
    std::vector<wf::Point> points;
    for (auto& c : corners) {
        for (int i = 0; i < 16; i++) {
            auto [in, out] = kCorner[i];
            wf::Point point{c.x + (in * c.inX + out * c.outX) * rx,
                            c.y + (in * c.inY + out * c.outY) * ry};
            if (i == 0 && !points.empty()) {
                points.insert(points.end(), {points.back(), point});
            }
            points.push_back(point);
        }
    }
    return points;
}

inline wuxm::PathGeometry SquircleGeometry(std::initializer_list<std::vector<wf::Point>> outlines) {
    wuxm::PathGeometry geometry;
    geometry.FillRule(wuxm::FillRule::EvenOdd);
    for (auto& points : outlines) {
        wuxm::PolyBezierSegment curves;
        for (size_t i = 1; i < points.size(); i++) curves.Points().Append(points[i]);
        wuxm::PathFigure figure;
        figure.StartPoint(points[0]);
        figure.IsClosed(true);
        figure.Segments().Append(curves);
        geometry.Figures().Append(figure);
    }
    return geometry;
}

constexpr int kGlowSteps = 10;
constexpr float kGlowFalloff = 4.5f;
constexpr float kEdgeHighlight = 0.7f;
constexpr float kEdgeShadow = 0.6f;
constexpr float kEdgeHighlightOpacity = 0.55f;
constexpr float kEdgeMinOpacity = 0.18f;
constexpr float kEdgeMaxOpacity = 0.35f;
constexpr float kEdgeMaxOpacityAt = 80.0f;

inline float Lightness(winrt::Windows::UI::Color color) {
    int max = std::max({color.R, color.G, color.B});
    int min = std::min({color.R, color.G, color.B});
    return (max + min) / 510.0f;
}

inline winrt::Windows::UI::Color ShiftLightness(winrt::Windows::UI::Color color, float delta) {
    float r = color.R / 255.0f, g = color.G / 255.0f, b = color.B / 255.0f;
    float max = std::max({r, g, b}), min = std::min({r, g, b});
    float l = (max + min) / 2, h = 0, s = 0;
    if (max != min) {
        float d = max - min;
        s = l > 0.5f ? d / (2 - max - min) : d / (max + min);
        if (max == r) h = (g - b) / d + (g < b ? 6 : 0);
        else if (max == g) h = (b - r) / d + 2;
        else h = (r - g) / d + 4;
        h /= 6;
    }
    l = std::clamp(l + delta, 0.0f, 1.0f);
    if (s == 0) {
        uint8_t gray = (uint8_t)std::lround(l * 255);
        return {color.A, gray, gray, gray};
    }
    auto hue2rgb = [](float p, float q, float t) {
        if (t < 0) t += 1;
        if (t > 1) t -= 1;
        if (t < 1.0f / 6) return p + (q - p) * 6 * t;
        if (t < 1.0f / 2) return q;
        if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
        return p;
    };
    float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
    float p = 2 * l - q;
    return {color.A, (uint8_t)std::lround(hue2rgb(p, q, h + 1.0f / 3) * 255),
            (uint8_t)std::lround(hue2rgb(p, q, h) * 255),
            (uint8_t)std::lround(hue2rgb(p, q, h - 1.0f / 3) * 255)};
}

inline winrt::Windows::UI::Color GlowColor(winrt::Windows::UI::Color fill, float strength) {
    winrt::Windows::UI::Color glow = ShiftLightness(fill, (1 - Lightness(fill)) * kEdgeHighlight);
    glow.A = (uint8_t)std::min(255L, std::lround(fill.A * strength));
    return glow;
}

inline std::array<std::pair<float, float>, kGlowSteps * 2> GlowStops(float reach) {
    std::array<std::pair<float, float>, kGlowSteps * 2> stops;
    float end = std::exp(-kGlowFalloff);
    for (int i = 0; i < kGlowSteps; i++) {
        float t = i / (kGlowSteps - 1.0f);
        float strength = (std::exp(-kGlowFalloff * t * t) - end) / (1 - end);
        stops[i] = {reach * t, strength};
        stops[kGlowSteps * 2 - 1 - i] = {1 - reach * t, strength};
    }
    return stops;
}

inline wuxm::LinearGradientBrush GlowBrush(winrt::Windows::UI::Color color, wf::Point end) {
    wuxm::LinearGradientBrush brush;
    brush.StartPoint({0, 0});
    brush.EndPoint(end);
    for (auto [offset, strength] : GlowStops(0)) {
        winrt::Windows::UI::Color stopColor = color;
        stopColor.A = (uint8_t)std::lround(color.A * strength);
        wuxm::GradientStop stop;
        stop.Color(stopColor);
        brush.GradientStops().Append(stop);
    }
    return brush;
}

struct Plate {
    winrt::weak_ref<wux::UIElement> row;
    winrt::weak_ref<wuxc::Panel> panel;
    wuxc::Grid root{nullptr};
    wuxc::Border blurHost{nullptr};
    wuxc::Border tint{nullptr};
    wuxs::Path fill{nullptr}, edgeInner{nullptr}, edgeOuter{nullptr};
    wuxs::Path topGlow{nullptr}, sideGlow{nullptr};
    wuxm::LinearGradientBrush topGlowBrush{nullptr}, sideGlowBrush{nullptr};
    winrt::weak_ref<wux::FrameworkElement> frame;
    wuc::CompositionRoundedRectangleGeometry blurShape{nullptr};
    float appliedBlur = -1;
    float baseLeft = 0, baseWidth = 0;
    float top = 0;
    float left = -1, width = -1;
    float offsetY = 0;
    float dockBottom = 0;
    wux::FrameworkElement::SizeChanged_revoker rowSizeChanged;
    wux::FrameworkElement::SizeChanged_revoker panelSizeChanged;
    wux::FrameworkElement::ActualThemeChanged_revoker panelThemeChanged;
    wuxc::Grid trash{nullptr};
    wuxc::Image trashImage{nullptr};
    wuxc::Border divider{nullptr};
    wux::UIElement::Tapped_revoker trashTapped;
    wux::UIElement::RightTapped_revoker trashRightTapped;
    wux::UIElement::DragEnter_revoker trashDragEnter;
    wux::UIElement::DragOver_revoker trashDragOver;
    wux::UIElement::Drop_revoker trashDrop;
    winrt::weak_ref<wux::FrameworkElement> beforeTrash;
    float trashCenter = 0;
    int trashState = -2;
    wuxc::Border appsDivider{nullptr};
    winrt::weak_ref<wux::FrameworkElement> beforeAppsDivider;
    float appsDividerShift = 0;
    wux::UIElement::RightTapped_revoker plateRightTapped;
};

[[clang::no_destroy]] inline std::optional<std::list<Plate>> g_plates{std::in_place};

inline Plate* GetRowPlate(wux::DependencyObject const& row) {
    if (!row) return nullptr;
    for (auto& p : *g_plates) {
        if (p.row.get() == row) return &p;
    }
    return nullptr;
}



inline wux::FrameworkElement AppsDividerButton(std::vector<wux::FrameworkElement> const& buttons) {
    size_t i = 0;
    while (i < buttons.size() && winrt::get_class_name(buttons[i]) != L"Taskbar.TaskListButton") {
        i++;
    }
    if (i < buttons.size() && IsFileExplorer(buttons[i])) i++;
    if (i == 0 || i >= buttons.size()) return nullptr;
    return buttons[i - 1];
}

inline void SetAppsDividerShift(Plate& p, float tx) {
    if (!p.appsDivider || std::fabs(tx - p.appsDividerShift) < 0.01f) return;
    p.appsDividerShift = tx;
    auto visual = wuxh::ElementCompositionPreview::GetElementVisual(p.appsDivider);
    wuxh::ElementCompositionPreview::SetIsTranslationEnabled(p.appsDivider, true);
    visual.Properties().InsertVector3(L"Translation", {tx, 0, 0});
}

inline float GetRowOffsetY(wux::DependencyObject const& row) {
    Plate* p = GetRowPlate(row);
    return p ? p->offsetY : 0;
}

inline int g_platesDirtyFrames = 0;

inline void MarkPlatesDirty() {
    g_platesDirtyFrames = 3;
    StartFrames();
}

inline void ShapePlate(Plate& p) {
    if (g_dockPlateStyle != 0) return;
    float w = p.width > 0 ? p.width : p.baseWidth, h = g_dockHeight;
    if (w <= 0 || h <= 0) return;
    float r = SquircleRadius(w, h, g_dockRadius);
    float in = std::max(0.0f, r - 1.0f);
    auto outline = Squircle(0, 0, w, h, r, r);

    if (p.fill) p.fill.Data(SquircleGeometry({outline}));
    if (p.topGlow) p.topGlow.Data(SquircleGeometry({outline}));
    if (p.sideGlow) p.sideGlow.Data(SquircleGeometry({outline}));
    if (p.edgeInner) p.edgeInner.Data(SquircleGeometry({outline, Squircle(0, 1, w, h - 2, r, in)}));
    if (p.edgeOuter) {
        p.edgeOuter.Margin({-1, 0, -1, 0});
        p.edgeOuter.Data(SquircleGeometry({Squircle(0, 0, w + 2, h, r + 1, r), Squircle(1, 0, w, h, r, r)}));
    }

    float size = 10.0f * g_iconSize / 40.0f;
    for (auto [brush, extent] : {std::pair{p.topGlowBrush, h}, std::pair{p.sideGlowBrush, w}}) {
        if (!brush) continue;
        auto stops = brush.GradientStops();
        uint32_t i = 0;
        for (auto [offset, strength] : GlowStops(std::clamp(size / extent, 0.0f, 0.5f))) {
            if (i < stops.Size()) {
                stops.GetAt(i++).Offset(offset);
            }
        }
    }
}

inline void ApplyPlateLook(Plate& p) {
    bool light = g_dockTheme == 2;
    if (g_dockTheme == 0) {
        auto panel = p.panel.get();
        light = panel && panel.ActualTheme() == wux::ElementTheme::Light;
    }
    using winrt::Windows::UI::Color;
    uint8_t alpha = (uint8_t)(g_dockTint * 255 / 100);
    Color tint = light ? Color{alpha, 245, 245, 247} : Color{alpha, 35, 35, 38};
    Color border = light ? Color{50, 255, 255, 255} : Color{45, 255, 255, 255};
    Color line = light ? Color{50, 0, 0, 0} : Color{60, 255, 255, 255};
    for (auto& divider : {p.divider, p.appsDivider}) {
        if (divider) divider.Background(wuxm::SolidColorBrush(line));
    }

    if (g_dockPlateStyle == 0) {
        // macOS Sequoia / Sonoma (Squircle + Inner Glow, Default)
        p.tint.Visibility(wux::Visibility::Collapsed);
        if (p.fill) {
            p.fill.Visibility(wux::Visibility::Visible);
            p.fill.Fill(wuxm::SolidColorBrush(tint));
        }
        if (p.topGlow) p.topGlow.Visibility(wux::Visibility::Visible);
        if (p.sideGlow) p.sideGlow.Visibility(wux::Visibility::Visible);
        if (p.edgeInner) {
            p.edgeInner.Visibility(wux::Visibility::Visible);
            Color lighter = ShiftLightness(tint, (1 - Lightness(tint)) * kEdgeHighlight);
            float t = std::clamp((float)g_dockTint / kEdgeMaxOpacityAt, 0.0f, 1.0f);
            uint8_t edgeAlpha = (uint8_t)std::lround(255 * (kEdgeMinOpacity + (kEdgeMaxOpacity - kEdgeMinOpacity) * t));
            lighter.A = (uint8_t)std::lround(edgeAlpha * kEdgeHighlightOpacity);
            p.edgeInner.Fill(wuxm::SolidColorBrush(lighter));
        }
        if (p.edgeOuter) {
            p.edgeOuter.Visibility(wux::Visibility::Visible);
            Color darker = ShiftLightness(tint, -Lightness(tint) * kEdgeShadow);
            float t = std::clamp((float)g_dockTint / kEdgeMaxOpacityAt, 0.0f, 1.0f);
            darker.A = (uint8_t)std::lround(255 * (kEdgeMinOpacity + (kEdgeMaxOpacity - kEdgeMinOpacity) * t));
            p.edgeOuter.Fill(wuxm::SolidColorBrush(darker));
        }
        Color glowStart = GlowColor(tint, 0.35f);
        p.topGlowBrush = GlowBrush(glowStart, {0, 1});
        p.sideGlowBrush = GlowBrush(glowStart, {1, 0});
        if (p.topGlow) p.topGlow.Fill(p.topGlowBrush);
        if (p.sideGlow) p.sideGlow.Fill(p.sideGlowBrush);
        ShapePlate(p);
    } else {
        // Classic Simple
        p.tint.Visibility(wux::Visibility::Visible);
        p.tint.Background(wuxm::SolidColorBrush(tint));
        p.tint.BorderBrush(wuxm::SolidColorBrush(border));
        p.tint.BorderThickness({1, 1, 1, 1});
        p.tint.CornerRadius({g_dockRadius, g_dockRadius, g_dockRadius, g_dockRadius});
        if (p.fill) p.fill.Visibility(wux::Visibility::Collapsed);
        if (p.topGlow) p.topGlow.Visibility(wux::Visibility::Collapsed);
        if (p.sideGlow) p.sideGlow.Visibility(wux::Visibility::Collapsed);
        if (p.edgeInner) p.edgeInner.Visibility(wux::Visibility::Collapsed);
        if (p.edgeOuter) p.edgeOuter.Visibility(wux::Visibility::Collapsed);
    }

    float blur = g_dockBlur * kMaxBlur / 100;
    if (blur != p.appliedBlur) {
        p.appliedBlur = blur;
        try {
            p.blurShape = SetBlurVisual(p.blurHost, blur);
        } catch (...) {
            p.blurShape = nullptr;
            Wh_Log(L"Dock blur failed: 0x%08X %s", winrt::to_hresult(),
                   winrt::to_message().c_str());
        }
    } else if (p.blurShape) {
        p.blurShape.CornerRadius({g_dockRadius, g_dockRadius});
    }
}

inline void PlaceInPanel(wuxc::Panel const& panel, wux::FrameworkElement const& el) {
    el.HorizontalAlignment(wux::HorizontalAlignment::Left);
    el.VerticalAlignment(wux::VerticalAlignment::Top);
    el.Visibility(wux::Visibility::Collapsed);
    if (panel.try_as<wuxc::Grid>()) {
        wuxc::Grid::SetRowSpan(el, 100);
        wuxc::Grid::SetColumnSpan(el, 100);
    }
}

inline wuxc::Border AddDivider(wuxc::Panel const& panel, wux::UIElement const& row) {
    wuxc::Border divider;
    PlaceInPanel(panel, divider);
    divider.IsHitTestVisible(false);
    divider.Width(1);
    divider.Height(g_iconSize);
    uint32_t index = 0;
    if (!panel.Children().IndexOf(row, index)) return nullptr;
    panel.Children().InsertAt(index + 1, divider);
    return divider;
}

inline void AddTrash(Plate& p, wuxc::Panel const& panel, wux::UIElement const& row) {
    wuxc::Grid trash;
    PlaceInPanel(panel, trash);
    trash.Width(g_iconSize * kTrashIconScale);
    trash.Height(g_iconSize * kTrashIconScale);
    trash.Background(wuxm::SolidColorBrush(winrt::Windows::UI::Color{0, 0, 0, 0}));
    wuxc::ToolTipService::SetToolTip(trash, winrt::box_value(L"Recycle Bin"));
    wuxc::Image image;
    image.Stretch(wuxm::Stretch::Uniform);
    trash.Children().Append(image);

    uint32_t index = 0;
    if (!panel.Children().IndexOf(row, index)) return;
    panel.Children().InsertAt(index + 1, trash);
    auto divider = AddDivider(panel, row);

    p.trash = trash;
    p.trashImage = image;
    p.divider = divider;
    HookUpTrash(p);
    StartTrash();
}

inline bool IsVietnameseUI() {
    LANGID lang = GetUserDefaultUILanguage();
    return PRIMARYLANGID(lang) == LANG_VIETNAMESE;
}

inline wuxc::FontIcon MakeMenuIcon(LPCWSTR glyph) {
    wuxc::FontIcon icon;
    icon.FontFamily(wuxm::FontFamily(L"Segoe Fluent Icons, Segoe MDL2 Assets"));
    icon.Glyph(glyph);
    icon.FontSize(16.0);
    return icon;
}

inline void ShowDockMenu(wux::FrameworkElement const& target, wf::Point const& pt) {
    wuxc::MenuFlyout menu;
    bool vi = IsVietnameseUI();

    // 1. Magnification
    wuxc::MenuFlyoutItem magItem;
    bool isMagnified = (g_scale > 1.0f);
    if (isMagnified) {
        magItem.Text(vi ? L"Tắt phóng to" : L"Disable magnification");
        magItem.Icon(MakeMenuIcon(L"\uE71F"));
    } else {
        magItem.Text(vi ? L"Bật phóng to" : L"Enable magnification");
        magItem.Icon(MakeMenuIcon(L"\uE8A3"));
    }
    magItem.Click([](wf::IInspectable const&, wux::RoutedEventArgs const&) {
        if (g_scale > 1.0f) {
            g_scale = 1.0f;
            Wh_SetIntValue(L"magnification_enabled", 0);
        } else {
            g_scale = 1.6f;
            Wh_SetIntValue(L"magnification_enabled", 1);
        }
        ComputeSizes();
        MarkPlatesDirty();
    });
    menu.Items().Append(magItem);

    // System Tray
    wuxc::MenuFlyoutItem trayItem;
    bool isTrayHidden = (g_transparentTray != 0);
    if (isTrayHidden) {
        trayItem.Text(vi ? L"Hiện khay hệ thống" : L"Show system tray");
        trayItem.Icon(MakeMenuIcon(L"\uE890"));
    } else {
        trayItem.Text(vi ? L"Ẩn khay hệ thống" : L"Hide system tray");
        trayItem.Icon(MakeMenuIcon(L"\uED1A"));
    }
    trayItem.Click([](wf::IInspectable const&, wux::RoutedEventArgs const&) {
        g_transparentTray = (g_transparentTray != 0) ? 0 : 1;
        Wh_SetIntValue(L"transparent_tray", g_transparentTray);
        ApplyAllTrayStealth();
        UpdateAllTrayPlates();
    });
    menu.Items().Append(trayItem);

    wuxc::MenuFlyoutSeparator sep1;
    menu.Items().Append(sep1);

    // Task Manager
    wuxc::MenuFlyoutItem taskMgr;
    taskMgr.Text(vi ? L"Trình quản lý tác vụ" : L"Task Manager");
    taskMgr.Icon(MakeMenuIcon(L"\uE9D9"));
    taskMgr.Click([](wf::IInspectable const&, wux::RoutedEventArgs const&) {
        QueueShellJob([] {
            ShellExecuteW(nullptr, L"open", L"taskmgr.exe", nullptr, nullptr, SW_SHOWNORMAL);
        });
    });
    menu.Items().Append(taskMgr);

    // Taskbar settings
    wuxc::MenuFlyoutItem taskbarSettings;
    taskbarSettings.Text(vi ? L"Cài đặt thanh tác vụ" : L"Taskbar settings");
    taskbarSettings.Icon(MakeMenuIcon(L"\uE713"));
    taskbarSettings.Click([](wf::IInspectable const&, wux::RoutedEventArgs const&) {
        QueueShellJob([] {
            ShellExecuteW(nullptr, L"open", L"ms-settings:taskbar", nullptr, nullptr, SW_SHOWNORMAL);
        });
    });
    menu.Items().Append(taskbarSettings);

    wuxc::MenuFlyoutSeparator sep2;
    menu.Items().Append(sep2);

    // Dock settings
    wuxc::MenuFlyoutItem modSettings;
    modSettings.Text(vi ? L"Cài đặt Dock" : L"Dock settings");
    modSettings.Icon(MakeMenuIcon(L"\uE71D"));
    modSettings.Click([](wf::IInspectable const&, wux::RoutedEventArgs const&) {
        QueueShellJob([] {
            ShellExecuteW(nullptr, L"open", L"windhawk://mod/macos-style-dock-fork", nullptr, nullptr, SW_SHOWNORMAL);
        });
    });
    menu.Items().Append(modSettings);

    auto clearFocusVisual = [](wuxc::Control const& c) {
        c.UseSystemFocusVisuals(false);
        c.IsTabStop(false);
        c.FocusVisualPrimaryThickness({0, 0, 0, 0});
        c.FocusVisualSecondaryThickness({0, 0, 0, 0});
        c.FocusVisualMargin({0, 0, 0, 0});
    };
    clearFocusVisual(magItem);
    clearFocusVisual(trayItem);
    clearFocusVisual(taskMgr);
    clearFocusVisual(taskbarSettings);
    clearFocusVisual(modSettings);

    menu.ShowAt(target, pt);
}

inline void EnsurePlate(wux::FrameworkElement const& button) {
    if (!FindTaskbarFrame(button)) return;
    auto row = wuxm::VisualTreeHelper::GetParent(button).try_as<wux::FrameworkElement>();
    if (!row) return;
    for (auto& p : *g_plates) {
        if (p.row.get() == row) return;
    }
    auto panel = wuxm::VisualTreeHelper::GetParent(row).try_as<wuxc::Panel>();
    if (!panel) {
        Wh_Log(L"The icon row's parent isn't a panel; no dock plate");
        return;
    }

    wuxc::Grid root;
    PlaceInPanel(panel, root);
    root.Background(wuxm::SolidColorBrush(winrt::Windows::UI::Color{0, 0, 0, 0}));
    root.IsHitTestVisible(true);
    wuxc::Border blurHost;
    wuxc::Border tint;
    wuxs::Path fill;
    wuxs::Path topGlow;
    wuxs::Path sideGlow;
    wuxs::Path edgeInner;
    wuxs::Path edgeOuter;

    root.Children().Append(blurHost);
    root.Children().Append(fill);
    root.Children().Append(topGlow);
    root.Children().Append(sideGlow);
    root.Children().Append(edgeInner);
    root.Children().Append(edgeOuter);
    root.Children().Append(tint);

    for (wux::FrameworkElement el : {wux::FrameworkElement(root), wux::FrameworkElement(blurHost),
                                     wux::FrameworkElement(tint), wux::FrameworkElement(fill),
                                     wux::FrameworkElement(topGlow), wux::FrameworkElement(sideGlow),
                                     wux::FrameworkElement(edgeInner), wux::FrameworkElement(edgeOuter)}) {
        el.UseLayoutRounding(false);
    }

    uint32_t index = 0;
    if (!panel.Children().IndexOf(row, index)) index = 0;
    panel.Children().InsertAt(index, root);

    auto& p = g_plates->emplace_back();
    p.row = winrt::make_weak(row.as<wux::UIElement>());
    p.panel = winrt::make_weak(panel);
    p.root = root;
    p.blurHost = blurHost;
    p.tint = tint;
    p.fill = fill;
    p.topGlow = topGlow;
    p.sideGlow = sideGlow;
    p.edgeInner = edgeInner;
    p.edgeOuter = edgeOuter;

    auto onSize = [](wf::IInspectable const&, wux::SizeChangedEventArgs const&) {
        MarkPlatesDirty();
    };
    p.rowSizeChanged = row.SizeChanged(winrt::auto_revoke, onSize);
    p.panelSizeChanged = panel.SizeChanged(winrt::auto_revoke, onSize);
    if (g_showTrash) AddTrash(p, panel, row);
    if (g_appsDivider) p.appsDivider = AddDivider(panel, row);
    ApplyPlateLook(p);
    p.panelThemeChanged = panel.ActualThemeChanged(
        winrt::auto_revoke, [](wux::FrameworkElement const&, wf::IInspectable const&) {
            for (auto& plate : *g_plates) {
                try { ApplyPlateLook(plate); } catch (...) {}
            }
        });
    p.plateRightTapped = root.RightTapped(
        winrt::auto_revoke,
        [](wf::IInspectable const& sender, wuxi::RightTappedRoutedEventArgs const& e) {
            if (GetTickCount64() - g_lastAppButtonContextTime < 600) return;
            e.Handled(true);
            try {
                auto fe = sender.as<wux::FrameworkElement>();
                auto pt = e.GetPosition(fe);
                ShowDockMenu(fe, pt);
            } catch (...) {}
        });
    Wh_Log(L"Dock plate added");
}

inline void ReapplyButton(wux::FrameworkElement const& button) {
    float s = 1.0f, tx = 0;
    for (auto& item : g_row) {
        if (item.element.get() == button) {
            s = item.scale;
            tx = item.shift;
            break;
        }
    }
    SetScale(button, s, tx);
}

inline std::pair<wux::FrameworkElement, wux::FrameworkElement> LayOutButtons(
    Plate& p, std::vector<Icon> const& icons) {
    std::vector<size_t> order(icons.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        if (icons[a].slotLeft != icons[b].slotLeft) return icons[a].slotLeft < icons[b].slotLeft;
        return icons[a].slotWidth < icons[b].slotWidth;
    });

    constexpr size_t kNone = SIZE_MAX;
    size_t firstApp = kNone, explorer = kNone;
    for (size_t i : order) {
        if (winrt::get_class_name(icons[i].element) != L"Taskbar.TaskListButton") continue;
        if (firstApp == kNone) firstApp = i;
        if (explorer == kNone && IsFileExplorer(icons[i].element)) explorer = i;
    }
    bool moveExplorer = explorer != kNone && explorer != firstApp;

    std::vector<float> left(icons.size()), right(icons.size());
    size_t last = order.back();
    if (moveExplorer && last == explorer) last = order[order.size() - 2];
    if (p.trash) right[last] += g_trashSpace;

    wux::FrameworkElement beforeAppsDivider{nullptr};
    float dividerSpace = p.appsDivider ? g_dividerSpace : 0;
    if (moveExplorer) {
        auto& e = icons[explorer];
        float pitch = g_buttonWidth + 2 * g_buttonMargin;
        left[firstApp] += pitch + dividerSpace;
        left[explorer] = icons[firstApp].slotLeft - e.slotLeft;
        right[explorer] = -pitch - left[explorer];
        beforeAppsDivider = e.element;
    } else if (p.appsDivider) {
        std::vector<wux::FrameworkElement> buttons;
        for (size_t i : order) buttons.push_back(icons[i].element);
        beforeAppsDivider = AppsDividerButton(buttons);
        for (size_t i = 0; i < icons.size(); i++) {
            if (icons[i].element == beforeAppsDivider) right[i] += g_dividerSpace;
        }
    }

    bool changed = false;
    for (size_t i = 0; i < icons.size(); i++) {
        Button* b = FindButton(icons[i].element);
        if (!b || (std::fabs(b->extraLeft - left[i]) < 0.5f &&
                   std::fabs(b->extraRight - right[i]) < 0.5f)) {
            continue;
        }
        b->extraLeft = left[i];
        b->extraRight = right[i];
        ApplySpacing(icons[i].element);
        changed = true;
    }
    if (changed) MarkPlatesDirty();
    return {icons[last].element, beforeAppsDivider};
}

inline void MeasurePlate(Plate& p) {
    auto row = p.row.get();
    auto panel = p.panel.get();
    if (!row || !panel || !p.root) return;

    if (!p.frame.get()) {
        if (auto frame = FindTaskbarFrame(row.as<wux::FrameworkElement>())) {
            p.frame = winrt::make_weak(frame);
        }
    }

    auto icons = GetIcons(row);
    if (icons.empty()) {
        for (wux::UIElement el : {wux::UIElement(p.root), wux::UIElement(p.trash),
                                  wux::UIElement(p.divider), wux::UIElement(p.appsDivider)}) {
            if (el) el.Visibility(wux::Visibility::Collapsed);
        }
        return;
    }
    std::sort(icons.begin(), icons.end(),
              [](Icon const& a, Icon const& b) { return a.left < b.left; });

    float minLeft = icons[0].left, maxRight = icons[0].left + icons[0].width;
    float minTop = icons[0].top, maxBottom = icons[0].top + icons[0].height;
    for (auto& icon : icons) {
        minLeft = std::min(minLeft, icon.left);
        maxRight = std::max(maxRight, icon.left + icon.width);
        minTop = std::min(minTop, icon.top);
        maxBottom = std::max(maxBottom, icon.top + icon.height);
    }

    float avgWidth = 0;
    for (auto& icon : icons) avgWidth += icon.width;
    avgWidth /= icons.size();
    float emptyPerSide = std::max(0.0f, (avgWidth - g_iconSize) / 2);
    float padX = g_dockSidePadding - emptyPerSide;
    auto origin = row.TransformToVisual(panel).TransformPoint({0, 0});
    p.baseLeft = origin.X + minLeft - padX;
    p.baseWidth = (maxRight - minLeft) + 2 * padX;

    float height = g_dockHeight;
    p.top = (float)panel.ActualHeight() - g_dockBottomGap - height;
    float iconCenterY = origin.Y + (minTop + maxBottom) / 2;
    float offsetY = p.top + height / 2 - iconCenterY;

    double current = p.root.Height();
    if (std::isnan(current) || std::fabs(current - height) > 0.5) p.root.Height(height);
    p.left = p.width = -1;
    p.root.Visibility(wux::Visibility::Visible);

    auto [beforeTrash, beforeAppsDivider] = LayOutButtons(p, icons);
    p.beforeTrash = winrt::make_weak(beforeTrash);
    p.beforeAppsDivider = beforeAppsDivider ? winrt::make_weak(beforeAppsDivider) : nullptr;
    auto iconOf = [&](wux::FrameworkElement const& el) -> Icon const& {
        for (auto& icon : icons) {
            if (icon.element == el) return icon;
        }
        return icons.back();
    };

    float centerY = p.top + height / 2;
    auto placeDivider = [&](wuxc::Border const& divider, Icon const& icon) {
        float x = icon.left + icon.width + g_buttonMargin + g_dividerSpace / 2;
        divider.Margin({origin.X + x - 0.5f, centerY - g_iconSize / 2, 0, 0});
        divider.Visibility(wux::Visibility::Visible);
    };

    if (p.trash) {
        auto& last = iconOf(beforeTrash);
        p.trashCenter = last.left + last.width + g_buttonMargin + g_dividerSpace + g_trashSlot / 2;
        p.baseWidth += g_trashSpace;

        float trashSize = g_iconSize * kTrashIconScale;
        p.trash.Margin({origin.X + p.trashCenter - trashSize / 2, centerY - trashSize / 2, 0, 0});
        p.trash.Visibility(wux::Visibility::Visible);
        placeDivider(p.divider, last);
        UpdateTrashIcons();
    }

    if (p.appsDivider) {
        p.appsDivider.Visibility(wux::Visibility::Collapsed);
        if (beforeAppsDivider) placeDivider(p.appsDivider, iconOf(beforeAppsDivider));
    }

    p.offsetY = offsetY;
    p.dockBottom = p.top + height - origin.Y;
    for (auto& icon : icons) ReapplyButton(icon.element);
}

inline void ApplyPlate(Plate& p, float extraLeft, float extra) {
    if (!p.root) return;
    float left = p.baseLeft - extraLeft;
    float width = p.baseWidth + extra;
    if (std::fabs(left - p.left) > 0.25f || std::fabs(width - p.width) > 0.25f) {
        p.root.Margin({left, p.top, 0, 0});
        p.root.Width(width);
        p.left = left;
        p.width = width;
        ShapePlate(p);
    }
}

inline void RemovePlates() {
    for (auto& p : *g_plates) {
        p.rowSizeChanged.revoke();
        p.panelSizeChanged.revoke();
        p.panelThemeChanged.revoke();
        if (p.blurHost) {
            try { wuxh::ElementCompositionPreview::SetElementChildVisual(p.blurHost, nullptr); }
            catch (...) {}
        }
        p.trashTapped.revoke();
        p.trashRightTapped.revoke();
        p.trashDragEnter.revoke();
        p.trashDragOver.revoke();
        p.trashDrop.revoke();
        p.plateRightTapped.revoke();
        auto panel = p.panel.get();
        if (!panel) continue;
        for (wux::UIElement el : {wux::UIElement(p.root), wux::UIElement(p.trash),
                                  wux::UIElement(p.divider), wux::UIElement(p.appsDivider)}) {
            uint32_t index;
            if (el && panel.Children().IndexOf(el, index)) panel.Children().RemoveAt(index);
        }
    }
    g_plates->clear();
}

// ---- Dedicated Tray Dock (Tray Island) ----

struct TrayPlate {
    winrt::weak_ref<wux::FrameworkElement> row;
    winrt::weak_ref<wuxc::Panel> panel;
    wuxc::Grid root{nullptr};
    wuxc::Border blurHost{nullptr};
    wuxc::Border tint{nullptr};
    wuxs::Path fill{nullptr}, edgeInner{nullptr}, edgeOuter{nullptr};
    wuxs::Path topGlow{nullptr}, sideGlow{nullptr};
    wuxm::LinearGradientBrush topGlowBrush{nullptr}, sideGlowBrush{nullptr};
    wuc::CompositionRoundedRectangleGeometry blurShape{nullptr};
    float appliedBlur = -1;
    float left = -1, width = -1;
    wux::FrameworkElement::SizeChanged_revoker rowSizeChanged;
    wux::FrameworkElement::ActualThemeChanged_revoker panelThemeChanged;
    wux::UIElement::RightTapped_revoker plateRightTapped;
};

[[clang::no_destroy]] inline std::optional<std::list<TrayPlate>> g_trayPlates{std::in_place};

inline void ShapeTrayPlate(TrayPlate& tp, float w, float h) {
    if (g_dockPlateStyle != 0) return;
    if (w <= 0 || h <= 0) return;
    float r = SquircleRadius(w, h, g_dockRadius);
    float in = std::max(0.0f, r - 1.0f);
    auto outline = Squircle(0, 0, w, h, r, r);

    if (tp.fill) tp.fill.Data(SquircleGeometry({outline}));
    if (tp.topGlow) tp.topGlow.Data(SquircleGeometry({outline}));
    if (tp.sideGlow) tp.sideGlow.Data(SquircleGeometry({outline}));
    if (tp.edgeInner) tp.edgeInner.Data(SquircleGeometry({outline, Squircle(0, 1, w, h - 2, r, in)}));
    if (tp.edgeOuter) {
        tp.edgeOuter.Margin({-1, 0, -1, 0});
        tp.edgeOuter.Data(SquircleGeometry({Squircle(0, 0, w + 2, h, r + 1, r), Squircle(1, 0, w, h, r, r)}));
    }

    float size = 10.0f * g_iconSize / 40.0f;
    for (auto [brush, extent] : {std::pair{tp.topGlowBrush, h}, std::pair{tp.sideGlowBrush, w}}) {
        if (!brush) continue;
        auto stops = brush.GradientStops();
        uint32_t i = 0;
        for (auto [offset, strength] : GlowStops(std::clamp(size / extent, 0.0f, 0.5f))) {
            if (i < stops.Size()) {
                stops.GetAt(i++).Offset(offset);
            }
        }
    }
}

inline void ApplyTrayPlateLook(TrayPlate& tp) {
    bool light = g_dockTheme == 2;
    if (g_dockTheme == 0) {
        auto panel = tp.panel.get();
        light = panel && panel.ActualTheme() == wux::ElementTheme::Light;
    }
    using winrt::Windows::UI::Color;
    uint8_t alpha = (uint8_t)(g_dockTint * 255 / 100);
    Color tint = light ? Color{alpha, 245, 245, 247} : Color{alpha, 35, 35, 38};
    Color border = light ? Color{50, 255, 255, 255} : Color{45, 255, 255, 255};

    if (g_dockPlateStyle == 0) {
        tp.tint.Visibility(wux::Visibility::Collapsed);
        if (tp.fill) {
            tp.fill.Visibility(wux::Visibility::Visible);
            tp.fill.Fill(wuxm::SolidColorBrush(tint));
        }
        if (tp.topGlow) tp.topGlow.Visibility(wux::Visibility::Visible);
        if (tp.sideGlow) tp.sideGlow.Visibility(wux::Visibility::Visible);
        if (tp.edgeInner) {
            tp.edgeInner.Visibility(wux::Visibility::Visible);
            Color lighter = ShiftLightness(tint, (1 - Lightness(tint)) * kEdgeHighlight);
            float t = std::clamp((float)g_dockTint / kEdgeMaxOpacityAt, 0.0f, 1.0f);
            uint8_t edgeAlpha = (uint8_t)std::lround(255 * (kEdgeMinOpacity + (kEdgeMaxOpacity - kEdgeMinOpacity) * t));
            lighter.A = (uint8_t)std::lround(edgeAlpha * kEdgeHighlightOpacity);
            tp.edgeInner.Fill(wuxm::SolidColorBrush(lighter));
        }
        if (tp.edgeOuter) {
            tp.edgeOuter.Visibility(wux::Visibility::Visible);
            Color darker = ShiftLightness(tint, -Lightness(tint) * kEdgeShadow);
            float t = std::clamp((float)g_dockTint / kEdgeMaxOpacityAt, 0.0f, 1.0f);
            darker.A = (uint8_t)std::lround(255 * (kEdgeMinOpacity + (kEdgeMaxOpacity - kEdgeMinOpacity) * t));
            tp.edgeOuter.Fill(wuxm::SolidColorBrush(darker));
        }
        Color glowStart = GlowColor(tint, 0.35f);
        tp.topGlowBrush = GlowBrush(glowStart, {0, 1});
        tp.sideGlowBrush = GlowBrush(glowStart, {1, 0});
        if (tp.topGlow) tp.topGlow.Fill(tp.topGlowBrush);
        if (tp.sideGlow) tp.sideGlow.Fill(tp.sideGlowBrush);
        ShapeTrayPlate(tp, tp.width, g_dockHeight);
    } else {
        tp.tint.Visibility(wux::Visibility::Visible);
        tp.tint.Background(wuxm::SolidColorBrush(tint));
        tp.tint.BorderBrush(wuxm::SolidColorBrush(border));
        tp.tint.BorderThickness({1, 1, 1, 1});
        tp.tint.CornerRadius({g_dockRadius, g_dockRadius, g_dockRadius, g_dockRadius});
        if (tp.fill) tp.fill.Visibility(wux::Visibility::Collapsed);
        if (tp.topGlow) tp.topGlow.Visibility(wux::Visibility::Collapsed);
        if (tp.sideGlow) tp.sideGlow.Visibility(wux::Visibility::Collapsed);
        if (tp.edgeInner) tp.edgeInner.Visibility(wux::Visibility::Collapsed);
        if (tp.edgeOuter) tp.edgeOuter.Visibility(wux::Visibility::Collapsed);
    }

    float blur = g_dockBlur * kMaxBlur / 100;
    if (blur != tp.appliedBlur) {
        tp.appliedBlur = blur;
        try {
            tp.blurShape = SetBlurVisual(tp.blurHost, blur);
        } catch (...) {
            tp.blurShape = nullptr;
        }
    } else if (tp.blurShape) {
        tp.blurShape.CornerRadius({g_dockRadius, g_dockRadius});
    }
}

inline void UpdateTrayPlate(TrayPlate& tp) {
    auto row = tp.row.get();
    auto panel = tp.panel.get();
    if (!row || !panel || !tp.root) return;

    if (g_transparentTray == 1) {
        tp.root.Visibility(wux::Visibility::Collapsed);
        return;
    }

    float rowW = (float)row.ActualWidth();
    if (rowW <= 0) {
        tp.root.Visibility(wux::Visibility::Collapsed);
        return;
    }

    float padLeft = 26.0f;
    float padRight = 20.0f;
    float height = g_dockHeight;
    float width = rowW + padLeft + padRight;

    float bottomMargin = g_dockBottomGap + (height - 48.0f) / 2.0f;
    float rightMargin = g_dockBottomGap + padRight;

    auto currentMargin = row.Margin();
    if (std::fabs(currentMargin.Right - rightMargin) > 0.5f ||
        std::fabs(currentMargin.Bottom - bottomMargin) > 0.5f) {
        row.Margin({0, 0, rightMargin, bottomMargin});
    }

    tp.root.HorizontalAlignment(wux::HorizontalAlignment::Right);
    tp.root.VerticalAlignment(wux::VerticalAlignment::Bottom);
    tp.root.Margin({0, 0, g_dockBottomGap, g_dockBottomGap});
    tp.root.Height(height);
    tp.root.Width(width);
    tp.root.Visibility(wux::Visibility::Visible);

    if (std::fabs(width - tp.width) > 0.25f) {
        tp.width = width;
        ShapeTrayPlate(tp, width, height);
    }
}

inline void UpdateAllTrayPlates() {
    if (!g_trayPlates) return;
    for (auto& tp : *g_trayPlates) {
        try { UpdateTrayPlate(tp); } catch (...) {}
    }
}

inline void ApplyAllTrayPlateLooks() {
    if (!g_trayPlates) return;
    for (auto& tp : *g_trayPlates) {
        try { ApplyTrayPlateLook(tp); } catch (...) {}
    }
}

inline void SetTrayPlateVisibility(wux::FrameworkElement const& row, wux::Visibility vis, float opacity) {
    if (!g_trayPlates) return;
    for (auto& tp : *g_trayPlates) {
        if (tp.row.get() == row && tp.root) {
            tp.root.Visibility(vis);
            tp.root.Opacity(opacity);
        }
    }
}

inline void EnsureTrayPlate(wux::FrameworkElement const& row) {
    if (!row) return;
    if (!g_trayPlates) g_trayPlates.emplace();

    for (auto& tp : *g_trayPlates) {
        if (tp.row.get() == row) {
            UpdateTrayPlate(tp);
            return;
        }
    }

    wuxc::Panel panel{nullptr};
    wux::DependencyObject cur = row;
    while (cur) {
        if (winrt::get_class_name(cur) == L"Taskbar.TaskbarFrame") {
            if (auto p = cur.try_as<wuxc::Panel>()) {
                panel = p;
            } else if (wuxm::VisualTreeHelper::GetChildrenCount(cur) > 0) {
                panel = wuxm::VisualTreeHelper::GetChild(cur, 0).try_as<wuxc::Panel>();
            }
            break;
        }
        cur = wuxm::VisualTreeHelper::GetParent(cur);
    }
    if (!panel && g_plates) {
        for (auto& p : *g_plates) {
            if (auto pl = p.panel.get()) {
                panel = pl;
                break;
            }
        }
    }
    if (!panel) return;

    wuxc::Grid root;
    PlaceInPanel(panel, root);
    root.HorizontalAlignment(wux::HorizontalAlignment::Right);
    root.VerticalAlignment(wux::VerticalAlignment::Bottom);
    root.Background(wuxm::SolidColorBrush(winrt::Windows::UI::Color{0, 0, 0, 0}));
    root.IsHitTestVisible(true);

    wuxc::Border blurHost;
    wuxc::Border tint;
    wuxs::Path fill;
    wuxs::Path topGlow;
    wuxs::Path sideGlow;
    wuxs::Path edgeInner;
    wuxs::Path edgeOuter;

    root.Children().Append(blurHost);
    root.Children().Append(fill);
    root.Children().Append(topGlow);
    root.Children().Append(sideGlow);
    root.Children().Append(edgeInner);
    root.Children().Append(edgeOuter);
    root.Children().Append(tint);

    for (wux::FrameworkElement el : {wux::FrameworkElement(root), wux::FrameworkElement(blurHost),
                                     wux::FrameworkElement(tint), wux::FrameworkElement(fill),
                                     wux::FrameworkElement(topGlow), wux::FrameworkElement(sideGlow),
                                     wux::FrameworkElement(edgeInner), wux::FrameworkElement(edgeOuter)}) {
        el.UseLayoutRounding(false);
    }

    panel.Children().InsertAt(0, root);

    auto& tp = g_trayPlates->emplace_back();
    tp.row = winrt::make_weak(row);
    tp.panel = winrt::make_weak(panel);
    tp.root = root;
    tp.blurHost = blurHost;
    tp.tint = tint;
    tp.fill = fill;
    tp.topGlow = topGlow;
    tp.sideGlow = sideGlow;
    tp.edgeInner = edgeInner;
    tp.edgeOuter = edgeOuter;

    ApplyTrayPlateLook(tp);

    tp.rowSizeChanged = row.SizeChanged(
        winrt::auto_revoke,
        [](wf::IInspectable const&, wux::SizeChangedEventArgs const&) {
            UpdateAllTrayPlates();
        });

    tp.panelThemeChanged = panel.ActualThemeChanged(
        winrt::auto_revoke,
        [](wux::FrameworkElement const&, wf::IInspectable const&) {
            ApplyAllTrayPlateLooks();
        });

    tp.plateRightTapped = root.RightTapped(
        winrt::auto_revoke,
        [](wf::IInspectable const& sender, wuxi::RightTappedRoutedEventArgs const& e) {
            if (GetTickCount64() - g_lastAppButtonContextTime < 600) return;
            e.Handled(true);
            try {
                auto fe = sender.as<wux::FrameworkElement>();
                auto pt = e.GetPosition(fe);
                ShowDockMenu(fe, pt);
            } catch (...) {}
        });

    UpdateTrayPlate(tp);
    Wh_Log(L"Tray dock plate added");
}

inline void RemoveTrayPlates() {
    if (!g_trayPlates) return;
    for (auto& tp : *g_trayPlates) {
        tp.rowSizeChanged.revoke();
        tp.panelThemeChanged.revoke();
        tp.plateRightTapped.revoke();
        if (tp.blurHost) {
            try { wuxh::ElementCompositionPreview::SetElementChildVisual(tp.blurHost, nullptr); }
            catch (...) {}
        }
        auto panel = tp.panel.get();
        if (panel && tp.root) {
            uint32_t index;
            if (panel.Children().IndexOf(tp.root, index)) {
                panel.Children().RemoveAt(index);
            }
        }
    }
    g_trayPlates->clear();
}

