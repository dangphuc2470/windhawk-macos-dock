#pragma once

// ============================================================================
// macOS Style Dock - Recycle Bin Module
// ============================================================================

#include "dock_utils.h"


// Windows' own Recycle Bin icon, `px` pixels square.
inline wuxm::ImageSource LoadStockTrashIcon(bool full, int px) {
    SHSTOCKICONINFO info{sizeof(info)};
    HICON icon = nullptr;
    if (FAILED(SHGetStockIconInfo(full ? SIID_RECYCLERFULL : SIID_RECYCLER, SHGSI_ICONLOCATION,
                                  &info)) ||
        SHDefExtractIconW(info.szPath, info.iIcon, 0, &icon, nullptr, px) != S_OK || !icon) {
        Wh_Log(L"[trash] couldn't load Windows' icon");
        return nullptr;
    }

    // Its pixels, top row first, in the BGRA order XAML uses.
    ICONINFO ii{};
    BITMAP bm{};
    std::vector<BYTE> pixels;
    if (GetIconInfo(icon, &ii)) {
        if (ii.hbmColor && GetObject(ii.hbmColor, sizeof(bm), &bm)) {
            BITMAPINFO bi{};
            bi.bmiHeader = {sizeof(BITMAPINFOHEADER), bm.bmWidth, -bm.bmHeight, 1, 32, BI_RGB};
            pixels.resize((size_t)bm.bmWidth * bm.bmHeight * 4);
            HDC dc = GetDC(nullptr);
            if (GetDIBits(dc, ii.hbmColor, 0, bm.bmHeight, pixels.data(), &bi, DIB_RGB_COLORS) !=
                bm.bmHeight) {
                pixels.clear();
            }
            ReleaseDC(nullptr, dc);
        }
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
    }
    DestroyIcon(icon);
    if (pixels.empty()) return nullptr;

    // XAML wants the colors premultiplied by their transparency.
    for (size_t i = 0; i < pixels.size(); i += 4) {
        BYTE a = pixels[i + 3];
        for (int c = 0; c < 3; c++) pixels[i + c] = (BYTE)(pixels[i + c] * a / 255);
    }
    wuxm::Imaging::WriteableBitmap bitmap(bm.bmWidth, bm.bmHeight);
    BYTE* data = nullptr;
    winrt::check_hresult(bitmap.PixelBuffer().as<IDockBufferByteAccess>()->Buffer(&data));
    memcpy(data, pixels.data(), pixels.size());
    bitmap.Invalidate();
    return bitmap;
}

// The icons, loaded once and shared by every taskbar's Recycle Bin.
[[clang::no_destroy]] inline wuxm::ImageSource g_trashIcons[2] = {nullptr, nullptr};

// Windows' icon for the Recycle Bin's state, loaded at the size it's shown at
// fully magnified.
inline wuxm::ImageSource TrashIcon(bool full, wux::UIElement const& el) {
    auto& icon = g_trashIcons[full];
    if (icon) return icon;
    double dpiScale = 1;
    try {
        if (auto root = el.XamlRoot()) dpiScale = root.RasterizationScale();
    } catch (...) {}
    int px = (int)std::ceil(g_iconLoadSize * kTrashIconScale * dpiScale);

    std::wstring customPath = full ? L"C:\\ProgramData\\Windhawk\\EditorWorkspace\\trash_full.png"
                                   : L"C:\\ProgramData\\Windhawk\\EditorWorkspace\\trash_empty.png";
    if (auto custom = LoadImageFromFile(customPath.c_str(), px)) {
        return icon = custom;
    }
    return icon = LoadStockTrashIcon(full, px);
}

// Whether the Recycle Bin has anything in it: -1 = not known yet, 0 = empty,
// 1 = full. Checked whenever Windows reports a change to it, on a thread pool
// thread since it can take a moment with many drives.
inline std::atomic<int> g_trashFull = -1;
inline std::atomic<bool> g_trashChecking = false;
inline PTP_WORK g_trashWork = nullptr;
inline ULONG g_trashNotify = 0;  // the SHChangeNotifyRegister registration

inline void CALLBACK CheckRecycleBin(PTP_CALLBACK_INSTANCE, PVOID, PTP_WORK) {
    HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    SHQUERYRBINFO info{sizeof(info)};
    if (SUCCEEDED(SHQueryRecycleBinW(nullptr, &info))) g_trashFull = info.i64NumItems > 0;
    if (SUCCEEDED(co)) CoUninitialize();
    g_trashChecking = false;
    if (HWND w = g_modWindow) PostMessage(w, kTrashCheckedMessage, 0, 0);  // show it
}

inline void CheckRecycleBinSoon() {
    if (g_trashWork && !g_trashChecking.exchange(true)) SubmitThreadpoolWork(g_trashWork);
}

// Shows each Recycle Bin's icon for the latest known state.
inline void UpdateTrashIcons() {
    int full = g_trashFull;
    for (auto& p : *g_plates) {
        if (!p.trashImage || p.trashState == full) continue;
        p.trashState = full;
        try {
            p.trashImage.Source(TrashIcon(full == 1, p.trashImage));
        } catch (...) {
            Wh_Log(L"[trash] icon failed: 0x%08X", winrt::to_hresult());
        }
    }
}

// Asks Windows to report changes to the Recycle Bin to the mod's window,
// which then checks it again (see ModWindowProc).
inline void StartTrash() {
    if (!g_trashWork) g_trashWork = CreateThreadpoolWork(CheckRecycleBin, nullptr, nullptr);
    HWND w = EnsureModWindow();  // first, as it's sent the result
    CheckRecycleBinSoon();
    if (g_trashNotify) return;
    PIDLIST_ABSOLUTE bin;
    if (!w || FAILED(SHGetFolderLocation(nullptr, CSIDL_BITBUCKET, nullptr, 0, &bin))) return;
    SHChangeNotifyEntry entry{bin, TRUE};
    g_trashNotify = SHChangeNotifyRegister(w, SHCNRF_ShellLevel, SHCNE_ALLEVENTS,
                                           kTrashChangedMessage, 1, &entry);
    CoTaskMemFree(bin);
    if (!g_trashNotify) Wh_Log(L"[trash] can't watch the Recycle Bin for changes");
}

// Stops watching it and lets go of the icons. The check that may still be
// running is waited for in Wh_ModBeforeUninit, off the taskbar's thread.
inline void StopTrash() {
    if (g_trashNotify) {
        SHChangeNotifyDeregister(g_trashNotify);
        g_trashNotify = 0;
    }
    g_trashIcons[0] = g_trashIcons[1] = nullptr;
}

inline void OpenRecycleBin() {
    ShellExecuteW(nullptr, L"open", L"shell:RecycleBinFolder", nullptr, nullptr, SW_SHOWNORMAL);
}

// Moves files to the Recycle Bin, so they can be restored; Windows warns
// first if one is too big for it and would be deleted for good. `paths` is a
// list of paths, each ending in a null character. Run as a shell job.
inline void RecycleFiles(std::wstring const& paths) {
    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = paths.c_str();  // its own final null ends the list
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_WANTNUKEWARNING;
    if (int error = SHFileOperationW(&op)) Wh_Log(L"[trash] recycling failed: %d", error);
    CheckRecycleBinSoon();
}

// Asks first, like emptying it anywhere else in Windows. Run as a shell job.
inline void EmptyRecycleBin() {
    SHEmptyRecycleBinW(nullptr, nullptr, 0);
    CheckRecycleBinSoon();
}

// Right-click: Open, and Empty Recycle Bin (unavailable when it's empty).
inline void ShowTrashMenu(wux::FrameworkElement const& target) {
    wuxc::MenuFlyoutItem open;
    open.Text(L"Open");
    open.Click([](wf::IInspectable const&, wux::RoutedEventArgs const&) { OpenRecycleBin(); });
    wuxc::MenuFlyoutItem empty;
    empty.Text(L"Empty Recycle Bin");
    empty.IsEnabled(g_trashFull != 0);
    empty.Click([](wf::IInspectable const&, wux::RoutedEventArgs const&) {
        QueueShellJob(EmptyRecycleBin);
    });
    wuxc::MenuFlyout menu;
    menu.Items().Append(open);
    menu.Items().Append(empty);
    menu.ShowAt(target);
}

// Files dragged over the Recycle Bin: offer to move them there.
inline void AcceptTrashDrag(wux::DragEventArgs const& e) {
    if (!e.DataView().Contains(wadt::StandardDataFormats::StorageItems())) return;
    e.AcceptedOperation(wadt::DataPackageOperation::Move);
    if (auto ui = e.DragUIOverride()) ui.Caption(L"Move to Recycle Bin");
    e.Handled(true);
}

// Files dropped on the Recycle Bin: their paths are read (which Windows does
// asynchronously), then they're recycled, both in a shell job, so the mod
// waits for them before it unloads.
inline void OnTrashDrop(wux::DragEventArgs const& e) {
    e.Handled(true);
    e.AcceptedOperation(wadt::DataPackageOperation::None);
    if (!e.DataView().Contains(wadt::StandardDataFormats::StorageItems())) return;
    QueueShellJob([items = winrt::make_agile(e.DataView().GetStorageItemsAsync())] {
        std::wstring paths;
        try {
            for (auto const& item : items.get().get()) {
                auto path = item.Path();
                if (path.empty()) continue;  // not a file on disk
                paths.append(path.c_str(), path.size());
                paths.push_back(L'\0');
            }
        } catch (...) {
            Wh_Log(L"[trash] reading the dropped files failed: 0x%08X", winrt::to_hresult());
            return;
        }
        if (!paths.empty()) RecycleFiles(paths);
    });
}

inline void HookUpTrash(Plate& p) {
    p.trash.AllowDrop(true);
    p.trashTapped = p.trash.Tapped(
        winrt::auto_revoke, [](wf::IInspectable const&, wuxi::TappedRoutedEventArgs const& e) {
            e.Handled(true);
            OpenRecycleBin();
        });
    p.trashRightTapped = p.trash.RightTapped(
        winrt::auto_revoke,
        [](wf::IInspectable const& sender, wuxi::RightTappedRoutedEventArgs const& e) {
            e.Handled(true);
            try { ShowTrashMenu(sender.as<wux::FrameworkElement>()); } catch (...) {
                Wh_Log(L"[trash] menu failed: 0x%08X", winrt::to_hresult());
            }
        });
    auto accept = [](wf::IInspectable const&, wux::DragEventArgs const& e) {
        try { AcceptTrashDrag(e); } catch (...) {}
    };
    p.trashDragEnter = p.trash.DragEnter(winrt::auto_revoke, accept);
    p.trashDragOver = p.trash.DragOver(winrt::auto_revoke, accept);
    p.trashDrop = p.trash.Drop(
        winrt::auto_revoke, [](wf::IInspectable const&, wux::DragEventArgs const& e) {
            try { OnTrashDrop(e); } catch (...) {
                Wh_Log(L"[trash] drop failed: 0x%08X", winrt::to_hresult());
            }
        });
}

// Magnifies the Recycle Bin like a button (growing upward from its bottom
// edge) and shifts it and the divider sideways.
inline void SetTrashScale(Plate& p, float s, float tx, float dividerTx) {
    if (!p.trash) return;
    auto visual = wuxh::ElementCompositionPreview::GetElementVisual(p.trash);
    // Its pivot is on the same line as the buttons': the bottom of an icon.
    float size = g_iconSize * kTrashIconScale;
    visual.CenterPoint({size / 2, size / 2 + g_iconSize / 2, 0});
    visual.Scale({s, s, 1});
    wuxh::ElementCompositionPreview::SetIsTranslationEnabled(p.trash, true);
    visual.Properties().InsertVector3(L"Translation", {tx, 0, 0});
    auto divider = wuxh::ElementCompositionPreview::GetElementVisual(p.divider);
    wuxh::ElementCompositionPreview::SetIsTranslationEnabled(p.divider, true);
    divider.Properties().InsertVector3(L"Translation", {dividerTx, 0, 0});
}
