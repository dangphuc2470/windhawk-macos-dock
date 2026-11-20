#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <windhawk_api.h>

#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>

namespace wux = winrt::Windows::UI::Xaml;
namespace wuxm = winrt::Windows::UI::Xaml::Media;

#ifndef __IDockBufferByteAccess_INTERFACE_DEFINED__
#define __IDockBufferByteAccess_INTERFACE_DEFINED__
// Raw access to a WriteableBitmap's pixels (IBufferByteAccess).
MIDL_INTERFACE("905a0fef-bc53-11df-8c49-001e4fc686da")
IDockBufferByteAccess : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE Buffer(BYTE** value) = 0;
};
__CRT_UUID_DECL(IDockBufferByteAccess, 0x905a0fef, 0xbc53, 0x11df, 0x8c, 0x49, 0x00, 0x1e, 0x4f,
                0xc6, 0x86, 0xda)
#endif

inline wuxm::ImageSource LoadImageFromFile(const wchar_t* path, int px = 160) {
    static ULONG_PTR s_gdiToken = [] {
        Gdiplus::GdiplusStartupInput input;
        ULONG_PTR token = 0;
        Gdiplus::GdiplusStartup(&token, &input, nullptr);
        return token;
    }();
    if (!s_gdiToken || !path) return nullptr;

    Gdiplus::Bitmap gdiBmp(path);
    if (gdiBmp.GetLastStatus() != Gdiplus::Ok) return nullptr;

    int w = gdiBmp.GetWidth();
    int h = gdiBmp.GetHeight();
    if (w <= 0 || h <= 0) return nullptr;

    // Downscale bang High Quality Bicubic ve kich thuoc retina px de triet tieu 100% rang cua
    int outW = px > 0 ? px : w;
    int outH = px > 0 ? (int)std::lround((double)h * px / w) : h;

    Gdiplus::Bitmap scaledBmp(outW, outH, PixelFormat32bppPARGB);
    {
        Gdiplus::Graphics g(&scaledBmp);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.DrawImage(&gdiBmp, 0, 0, outW, outH);
    }

    Gdiplus::Rect rc(0, 0, outW, outH);
    Gdiplus::BitmapData bmpData{};
    if (scaledBmp.LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &bmpData) != Gdiplus::Ok) {
        return nullptr;
    }

    wuxm::Imaging::WriteableBitmap wb(outW, outH);
    BYTE* pBuf = nullptr;
    if (SUCCEEDED(wb.PixelBuffer().as<IDockBufferByteAccess>()->Buffer(&pBuf))) {
        memcpy(pBuf, bmpData.Scan0, (size_t)outW * outH * 4);
        wb.Invalidate();
    }
    scaledBmp.UnlockBits(&bmpData);
    return wb;
}

inline void DockLog(const wchar_t* format, ...) {
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
    va_end(args);
    OutputDebugStringW(L"[WindhawkDock] ");
    OutputDebugStringW(buffer);
    OutputDebugStringW(L"\n");
    Wh_Log(L"%s", buffer);
}

#include <winrt/Windows.UI.Xaml.Automation.h>
namespace wuxa = winrt::Windows::UI::Xaml::Automation;

inline bool IsFileExplorer(wux::FrameworkElement const& button) {
    if (!button) return false;
    try {
        auto autoId = wuxa::AutomationProperties::GetAutomationId(button);
        if (autoId == L"Appid: Microsoft.Windows.Explorer") return true;
        auto name = wuxa::AutomationProperties::GetName(button);
        if (name == L"File Explorer" || name == L"Windows Explorer") return true;
    } catch (...) {}
    return false;
}

inline bool IsStartButton(wux::FrameworkElement const& button) {
    if (!button) return false;
    return winrt::get_class_name(button) == L"Taskbar.ExperienceToggleButton";
}

template <typename F>
inline void ForEachDescendant(wux::DependencyObject const& parent, int depth, F& f) {
    int count = wuxm::VisualTreeHelper::GetChildrenCount(parent);
    for (int i = 0; i < count; i++) {
        auto child = wuxm::VisualTreeHelper::GetChild(parent, i);
        bool descend = true;
        if (auto el = child.try_as<wux::FrameworkElement>()) descend = f(el);
        if (descend && depth > 1) ForEachDescendant(child, depth - 1, f);
    }
}

template <typename F>
inline void ForEachDescendant(wux::DependencyObject const& parent, int depth, F&& f) {
    ForEachDescendant(parent, depth, f);
}

inline ULONGLONG g_lastAppButtonContextTime = 0;
