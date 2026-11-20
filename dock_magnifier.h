#pragma once

#include <windhawk_utils.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>

// Forward declarations of external dependencies
struct RowItem;
extern std::vector<RowItem> g_row;
extern winrt::weak_ref<wux::UIElement> g_rowParent;
extern float g_maxGrowth, g_lastGrowth;
extern bool g_anchorLeftEnd;
extern bool g_pointerOver;
extern bool g_dockExpanded;
extern float g_pointerX;
extern int g_animationMs;
extern float g_radius;
extern float g_scale;

struct Plate;
Plate* GetRowPlate(wux::DependencyObject const& row);
void SetScale(wux::FrameworkElement const& el, float s, float tx);
void SetTrashScale(Plate& p, float s, float tx, float dividerTx);
void SetAppsDividerShift(Plate& p, float tx);
void ScanTaskbars();
void EnsurePlate(wux::FrameworkElement const& button);
void MeasurePlate(Plate& p);
void ScheduleOverWindows(bool autoHide);
void ApplyPlate(Plate& p, float extraLeft, float extra);
struct Icon;
extern std::vector<Icon> GetIcons(wux::UIElement const& parent);

inline winrt::event_token g_frameToken{};
inline bool g_frameRunning = false;
inline std::chrono::steady_clock::time_point g_lastFrame;

inline void StopFrames() {
    if (!g_frameRunning) return;
    wuxm::CompositionTarget::Rendering(g_frameToken);
    g_frameRunning = false;
}

inline void BuildRow(wux::UIElement const& parent) {
    std::vector<RowItem> old = std::move(g_row);
    Plate* oldPlate = GetRowPlate(g_rowParent.get());
    if (g_rowParent.get() != parent) g_maxGrowth = g_lastGrowth = 0;
    g_row.clear();
    g_rowParent = nullptr;

    if (parent) {
        for (auto& icon : GetIcons(parent)) {
            float pitch = std::max(1.0f, icon.width + 2 * g_buttonMargin);
            RowItem item{winrt::make_weak(icon.element), icon.left + icon.width / 2, pitch};
            for (auto& o : old) {
                if (o.element.get() == icon.element) {
                    item.scale = o.scale;
                    item.shift = o.shift;
                    o.element = nullptr;
                    break;
                }
            }
            g_row.push_back(item);
        }
        std::sort(g_row.begin(), g_row.end(),
                  [](RowItem const& a, RowItem const& b) { return a.center < b.center; });
        g_rowParent = winrt::make_weak(parent);
        Plate* p = GetRowPlate(parent);

        if (p && p->trash && !g_row.empty() &&
            p->trash.Visibility() == wux::Visibility::Visible) {
            RowItem item{nullptr, p->trashCenter, g_trashSlot};
            item.trash = true;
            for (auto& o : old) {
                if (o.trash && oldPlate == p) {
                    item.scale = o.scale;
                    item.shift = o.shift;
                    o.trash = false;
                }
            }
            g_row.push_back(item);
        }
    }

    for (auto& o : old) {
        if (auto icon = o.element.get()) SetScale(icon, 1.0f);
        if (o.trash && oldPlate) SetTrashScale(*oldPlate, 1.0f, 0, 0);
    }
    if (oldPlate && oldPlate != GetRowPlate(parent)) SetAppsDividerShift(*oldPlate, 0);
}

inline void OnFrame() {
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - g_lastFrame).count();
    g_lastFrame = now;
    if (dt > 0.1f) dt = 0.1f;

    float alpha = 1.0f;
    if (g_animationMs > 0) {
        float tau = g_animationMs / 1000.0f / 3.0f;
        alpha = 1.0f - std::exp(-dt / tau);
    }

    if (g_platesDirtyFrames > 0) {
        g_platesDirtyFrames--;
        try { ScanTaskbars(); } catch (...) {
            Wh_Log(L"ScanTaskbars failed: 0x%08X", winrt::to_hresult());
        }
        for (auto& b : *g_buttons) {
            if (auto el = b.element.get()) {
                try { EnsurePlate(el); } catch (...) {
                    Wh_Log(L"EnsurePlate failed: 0x%08X", winrt::to_hresult());
                }
            }
        }
        for (auto& p : *g_plates) MeasurePlate(p);
        ScheduleOverWindows(false);
    }

    if (auto rowParent = g_rowParent.get()) BuildRow(rowParent);

    bool settled = g_platesDirtyFrames == 0;
    std::vector<float> scales(g_row.size(), 1.0f);
    for (size_t i = 0; i < g_row.size(); i++) {
        auto& item = g_row[i];

        float target = 1.0f;
        if (g_pointerOver) {
            float d = std::fabs(g_pointerX - item.center);
            if (d < g_radius) {
                float k = (std::cos(d / g_radius * 3.14159265f) + 1.0f) / 2.0f;
                target = 1.0f + (g_scale - 1.0f) * k;
            }
        }

        float s = item.scale + (target - item.scale) * alpha;
        if (std::fabs(target - s) < 0.001f) {
            s = target;
        } else {
            settled = false;
        }
        scales[i] = s;
    }

    bool expanded = g_pointerOver;
    for (float scale : scales) expanded |= scale > 1.0f;
    if (expanded != g_dockExpanded) {
        g_dockExpanded = expanded;
        ScheduleOverWindows(false);
    }

    float total = 0;
    for (size_t i = 0; i < g_row.size(); i++) {
        total += (scales[i] - 1.0f) * g_row[i].width;
    }
    if (g_pointerOver) {
        if (!g_row.empty()) {
            float middle = (g_row.front().center - g_row.front().width / 2 +
                            g_row.back().center + g_row.back().width / 2) / 2;
            g_anchorLeftEnd = g_pointerX > middle;
        }
        g_maxGrowth = std::max(g_maxGrowth, total);
    } else if (g_lastGrowth > 0) {
        g_maxGrowth *= total / g_lastGrowth;
    } else {
        g_maxGrowth = total;
    }
    g_lastGrowth = total;
    float growLeft = g_anchorLeftEnd ? g_maxGrowth / 2 : total - g_maxGrowth / 2;

    auto rowParent = g_rowParent.get();
    Plate* plate = GetRowPlate(rowParent);
    auto beforeAppsDivider = plate ? plate->beforeAppsDivider.get() : nullptr;
    float appsDividerShift = 0;
    float before = 0;
    for (size_t i = 0; i < g_row.size(); i++) {
        auto& item = g_row[i];
        float extra = (scales[i] - 1.0f) * item.width;
        float shift = before + extra / 2 - growLeft;
        float gapShift = before - growLeft;
        before += extra;
        if (beforeAppsDivider && item.element.get() == beforeAppsDivider) {
            appsDividerShift = before - growLeft;
        }

        if (item.trash) {
            if (plate && (scales[i] != item.scale || std::fabs(shift - item.shift) > 0.01f)) {
                SetTrashScale(*plate, scales[i], shift, gapShift);
                item.scale = scales[i];
                item.shift = shift;
            }
            continue;
        }

        auto icon = item.element.get();
        if (!icon) continue;
        if (scales[i] != item.scale || std::fabs(shift - item.shift) > 0.01f) {
            SetScale(icon, scales[i], shift);
            item.scale = scales[i];
            item.shift = shift;
        }
    }
    if (plate) SetAppsDividerShift(*plate, appsDividerShift);

    for (auto& p : *g_plates) {
        bool current = rowParent && p.row.get() == rowParent;
        ApplyPlate(p, current ? growLeft : 0, current ? total : 0);
    }

    if (settled) StopFrames();
}

inline void StartFrames() {
    if (g_frameRunning) return;
    g_lastFrame = std::chrono::steady_clock::now();
    g_frameToken = wuxm::CompositionTarget::Rendering(
        [](wf::IInspectable const&, wf::IInspectable const&) {
            try {
                OnFrame();
            } catch (...) {
                Wh_Log(L"OnFrame error: 0x%08X %s", winrt::to_hresult(),
                       winrt::to_message().c_str());
                StopFrames();
            }
        });
    g_frameRunning = true;
}
