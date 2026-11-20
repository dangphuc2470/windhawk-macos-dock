// ==WindhawkMod==
// @id              macos-style-dock-fork
// @name            MacOS Style Dock - Fork
// @description     Turns the Windows 11 taskbar into a macOS-style dock with magnifying icons
// @version         1.0
// @author          Zach Vaughn
// @github          https://github.com/zachvdev
// @include         explorer.exe
// @architecture    amd64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -lversion -lgdi32 -lshell32 -lshcore -ladvapi32 -lgdiplus -IC:/ProgramData/Windhawk/EditorWorkspace
// @license         GPL-3.0
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# MacOS Style Dock

Turns the Windows 11 taskbar into a dock with the look and feel of macOS,
while it keeps working the way Windows does. It restyles the real taskbar
rather than replacing it, so pinned apps, jump lists, thumbnails, dragging
files onto apps and rearranging your apps all work as usual. (Unlike
"Taskbar Dock Animation", which magnifies the stock taskbar's icons, and "Mac
Island & Dock", which replaces the taskbar with a separate dock window.)

![The dock in action](https://i.imgur.com/z2GaRW8.gif)

## The look

- **A frosted glass dock.** Your icons sit on a rounded plate of blurred,
  tinted glass with a thin edge, floating just above the bottom of the
  screen, in place of the taskbar's flat background. It's dark, light, or
  follows your Windows theme.
- **Bigger icons, no boxes.** Icons are shown larger, without the square
  highlight Windows draws behind them on hover or for the active app.
- **Sharp at any size.** Icons are loaded at the size they're shown at when
  fully magnified, so they stay crisp as they grow.
- **Running indicators.** A small dot under each running app, just above
  the bottom of the dock.
- **Dividers.** Thin lines separate Start, Task View and File Explorer from
  your other apps, and your apps from the Recycle Bin.
- **A tidier system tray.** The clock, notifications and tray icons on the
  right keep a fixed size and position, with even margins from the screen
  edges, whatever size the dock is.

## How it works

- **Magnification.** Icons grow as the pointer passes over them, smoothly
  falling off with distance, and push their neighbors aside. As on a Mac,
  the dock grows from one end: with the pointer right of its middle, its
  left end stays put and its right end moves, and the other way around. The
  dock shrinks back as soon as the pointer leaves it.
- **Over your windows.** Maximized windows reach down to the top of the
  dock, and magnified icons rise over them. Windows still keeps the dock's
  own strip of the screen free.
- **A fixed order.** Start comes first, then Task View, then File Explorer,
  a divider, and the rest of your apps. Whenever File Explorer is pinned or
  open, it keeps its spot next to the divider and can't be dragged
  elsewhere; your other apps can be rearranged as usual.
- **Jump lists in the right place.** Right-click menus open above the icon
  you clicked.
- **A Recycle Bin** at the right end of the dock, using Windows' own icon,
  which shows whether it's empty or full. Click it to open it, drag files
  onto it to delete them (they can be restored), or right-click it to open
  or empty it.

## Settings

- **Dock Panel:** the size of the icons and dock, and the dock's color
  (System, Dark or Light), tint and blur.
- **Magnification:** how much icons grow, how far from the pointer they
  grow, how long the animation takes, and whether icons magnify over
  maximized windows. With that turned off, the space above the dock is kept
  free for magnified icons instead, so maximized windows get a little less
  of the screen.
- **Dock Items:** the divider after File Explorer, and the Recycle Bin.

## Compatibility

- Windows 11 only, with the taskbar at the bottom of the screen.
- x64 PCs only for now; ARM64 isn't supported yet.
- The "Small taskbar buttons" setting isn't supported.
- Leave Windows' "Automatically hide the taskbar" setting off. While
  "Magnify over windows" is on, the mod turns it on behind the scenes (and
  keeps the taskbar from hiding), and puts it back as it was when the
  setting or the mod is turned off. If the mod can't put it back, for
  example when Windhawk is uninstalled while Explorer isn't running, turn it
  off yourself in Windows' taskbar settings.
- Icons are loaded at high resolution on Windows 11 with the October 2024
  update (KB5044384) or later. On older builds, magnified icons are blurrier.
- Not designed to be used with other taskbar mods. In particular, it sets
  the taskbar's height and icon size, lays out and animates its icons
  itself, and places their right-click menus, so don't combine it with
  "Taskbar height and icon size", "Taskbar Dock Animation", "Taskbar Dock
  Animation Plus", "Mac Island & Dock", "Taskbar Multirow" or "Start button
  always on the left".
- If Windows' Smart App Control is on, it blocks this mod (as it does all
  Windhawk mods), because the mod is compiled on your PC and isn't signed.

## Credits

The taskbar height code is based on m417z's "Taskbar height and icon size"
mod. The way it handles dragging icons and places right-click menus follows
his "Taskbar Multirow" mod, and the way it keeps the taskbar shown follows
his "Taskbar auto-hide when maximized" mod (all GPL-3.0).
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- dock:
  - plateStyle: sequoia
    $name: Dock plate style
    $description: Visual appearance of the glass plate.
    $options:
    - sequoia: macOS Sequoia / Sonoma (Squircle + Inner Glow, Default)
    - classic: Classic Simple (Smooth Rounded Glass)
  - size: 55
    $name: Size (0-100)
    $description: Size of the icons and the dock. (Default = 55)
  - color: auto
    $name: Color
    $description: The color of the dock panel.
    $options:
    - auto: System
    - dark: Dark
    - light: Light
  - tint: 20
    $name: Tint (0-100)
    $description: How opaque the dock panel is. (Default = 20)
  - blur: 35
    $name: Blur (0-100)
    $description: How blurred the dock panel is. (Default = 35)
  $name: Dock Panel
- magnification:
  - amount: 60
    $name: Amount (0-100)
    $description: How much the icons grow when hovered. (Default = 60)
  - range: 55
    $name: Range (0-100)
    $description: How far from the pointer icons grow. (Default = 55)
  - smoothing: 100
    $name: Smoothing (ms)
    $description: Duration of animation transitions. (Default = 100)
  - overWindows: true
    $name: Magnify over windows
    $description: Maximized windows reach down to the dock, and magnified icons rise over them.
  $name: Magnification
- items:
  - hideStartButton: true
    $name: Hide Start button
    $description: Start with Finder first on the dock as on macOS. Use Win key for Start. (Default = on)
  - appsDivider: true
    $name: Divider after File Explorer
    $description: A divider between Start, Task View and File Explorer, and the rest of your apps.
  - recycleBin: true
    $name: Recycle Bin
    $description: Show the Recycle Bin at the right end of the dock, after a divider.
  - explorerRightClickWinX: true
    $name: Right click File Explorer opens Win+X
    $description: Right-clicking File Explorer opens the Windows Start button context menu (Win+X menu).
  - activeIndicatorColor: auto
    $name: Active app indicator color
    $description: Color of the indicator under the active running app.
    $options:
    - auto: Adaptive (White on dark background, Theme color on light background)
    - theme: Windows Theme Accent Color
    - white: Always Bright White
  - transparentTray: stealth
    $name: Stealth system tray (Ẩn khay hệ thống)
    $description: Make clock, network, sound and tray icons transparent while keeping them fully clickable.
    $options:
    - stealth: Always stealth (Ẩn hoàn toàn, click bình thường)
    - hover: Show on hover (Hiện khi rê chuột)
    - visible: Normal visible (Hiện bình thường)
  $name: Dock Items
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <shellapi.h>
#include <shellscalingapi.h>
#include <sddl.h>
#include <shlobj.h>
#include <gdiplus.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <mutex>
#include <numeric>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <winrt/Windows.UI.Text.h>

namespace wadt = winrt::Windows::ApplicationModel::DataTransfer;
namespace wf = winrt::Windows::Foundation;
namespace wux = winrt::Windows::UI::Xaml;
namespace wuxc = winrt::Windows::UI::Xaml::Controls;
namespace wuxh = winrt::Windows::UI::Xaml::Hosting;
namespace wuxi = winrt::Windows::UI::Xaml::Input;
namespace wuxm = winrt::Windows::UI::Xaml::Media;
namespace wuxs = winrt::Windows::UI::Xaml::Shapes;
namespace wuc = winrt::Windows::UI::Composition;
namespace wge = winrt::Windows::Graphics::Effects;

#include "C:/ProgramData/Windhawk/EditorWorkspace/dock_settings.h"

#include "C:/ProgramData/Windhawk/EditorWorkspace/dock_buttons.h"
#include "C:/ProgramData/Windhawk/EditorWorkspace/dock_plate.h"
#include "C:/ProgramData/Windhawk/EditorWorkspace/dock_recycle_bin.h"
#include "C:/ProgramData/Windhawk/EditorWorkspace/dock_indicators.h"
#include "C:/ProgramData/Windhawk/EditorWorkspace/dock_magnifier.h"

#include "C:/ProgramData/Windhawk/EditorWorkspace/dock_taskbars.h"

// ---------------------------------------------------------------------------
// Reaching the taskbar's elements from its own code
// ---------------------------------------------------------------------------
// The taskbar's button and frame classes are XAML elements. In a hook on one
// of their methods, the element is the interface at pThis + 3 pointers (the
// same as in "Taskbar Labels").

#include "C:/ProgramData/Windhawk/EditorWorkspace/dock_over_windows.h"
#include "C:/ProgramData/Windhawk/EditorWorkspace/dock_hooks.h"

// ---------------------------------------------------------------------------
// Windhawk entry points
// ---------------------------------------------------------------------------

void LoadSettings() {
    auto style = WindhawkUtils::StringSetting::make(L"dock.plateStyle");
    g_dockPlateStyle = wcscmp(style, L"classic") == 0 ? 1 : 0;
    g_hideStartButton = Wh_GetIntSetting(L"items.hideStartButton");
    g_dockSize = std::clamp(Wh_GetIntSetting(L"dock.size"), 0, 100);
    auto color = WindhawkUtils::StringSetting::make(L"dock.color");
    g_dockTheme = wcscmp(color, L"dark") == 0 ? 1 : wcscmp(color, L"light") == 0 ? 2 : 0;
    g_dockTint = std::clamp(Wh_GetIntSetting(L"dock.tint"), 0, 100);
    g_dockBlur = std::clamp(Wh_GetIntSetting(L"dock.blur"), 0, 100);
    // Amount 0-100 -> the icon grows to 1.0x-2.0x.
    // g_scale = 1.0f + std::clamp(Wh_GetIntSetting(L"magnification.amount"), 0, 100) / 100.0f;
    int userMag = Wh_GetIntValue(L"magnification_enabled", -1);
    g_scale = (userMag == 1) ? 1.6f : 1.0f;
    g_magnificationRange = std::clamp(Wh_GetIntSetting(L"magnification.range"), 0, 100);
    g_animationMs = std::clamp(Wh_GetIntSetting(L"magnification.smoothing"), 0, 1000);
    g_magnifyOverWindows = Wh_GetIntSetting(L"magnification.overWindows");
    g_appsDivider = Wh_GetIntSetting(L"items.appsDivider");
    g_showTrash = Wh_GetIntSetting(L"items.recycleBin");
    g_explorerRightClickWinX = Wh_GetIntSetting(L"items.explorerRightClickWinX");
    auto indColor = WindhawkUtils::StringSetting::make(L"items.activeIndicatorColor");
    g_activeIndicatorColor = wcscmp(indColor, L"theme") == 0 ? 1 : wcscmp(indColor, L"white") == 0 ? 2 : 0;
    int userTray = Wh_GetIntValue(L"transparent_tray", -1);
    if (userTray != -1) {
        g_transparentTray = userTray;
    } else {
        auto trayMode = WindhawkUtils::StringSetting::make(L"items.transparentTray");
        g_transparentTray = wcscmp(trayMode, L"visible") == 0 ? 0 : wcscmp(trayMode, L"hover") == 0 ? 2 : 1;
    }

    ComputeSizes();
    Wh_Log(L"Sizes: icon %.0f (loaded at %.0f), dock %.0f, taskbar %d, button margin %.1f",
           g_iconSize, g_iconLoadSize, g_dockHeight, g_taskbarHeight, g_buttonMargin);
}

BOOL Wh_ModInit() {
    LoadSettings();
    g_managingAutoHide = Wh_GetIntValue(UserValueName(L"managingAutoHide").c_str(), 0);
    g_autoHideBefore = Wh_GetIntValue(UserValueName(L"autoHideBefore").c_str(), 0);
    g_shellJobWork = CreateThreadpoolWork(RunShellJob, nullptr, nullptr);

    if (!HookTaskbarDllSymbols()) Wh_Log(L"taskbar.dll hooks failed; height won't change");
    HookTaskbarModules();

    if (HMODULE kb = GetModuleHandle(L"kernelbase.dll")) {
        if (auto p = GetProcAddress(kb, "LoadLibraryExW")) {
            WindhawkUtils::SetFunctionHook((LoadLibraryExW_t)p, LoadLibraryExW_Hook,
                                           &LoadLibraryExW_Original);
        }
    }
    return TRUE;
}

void Wh_ModAfterInit() {
    // Besides the height, this makes the taskbar update all its buttons, which
    // runs the hooks that find them (and the taskbars they're in).
    if (g_taskbarHeight > 0) ApplyTaskbarHeight();
    ApplyMagnifyOverWindows();
    RefreshAllButtonsAndCards();
}

// Runs before the hooks are removed, which the taskbar needs to go back to its
// normal height and icon size.
void Wh_ModBeforeUninit() {
    // The hooks stay in place until after this; stop them from applying the
    // dock's changes again.
    g_unloading = true;

    // Undo the changes to the taskbar's elements first, so that when it goes
    // back to its normal size below, it sizes its icons itself, with nothing
    // of ours left to override that.
    if (HWND tb = FindTaskbar()) {
        RunFromWindowThread(tb, [](void*) { ResetAll(); }, nullptr);
    }
    ApplyAutoHide();  // back off, if the mod turned it on
    UnregisterClass(kWindowClass, ModuleInstance());
    // Shell jobs first: they end by checking the Recycle Bin.
    for (PTP_WORK* work : {&g_shellJobWork, &g_trashWork}) {
        if (!*work) continue;
        WaitForThreadpoolWorkCallbacks(*work, FALSE);
        CloseThreadpoolWork(*work);
        *work = nullptr;
    }

    if (g_taskbarHeight > 0) {
        g_taskbarHeight = 0;
        g_iconLoadSize = 0;
        ApplyTaskbarHeight();
    }
}

void Wh_ModSettingsChanged() {
    int oldTaskbarHeight = g_taskbarHeight;
    float oldIconLoadSize = g_iconLoadSize;
    // The frame loop and the hooks read the settings on the taskbar thread, so
    // they're loaded there too. Scale settings are read every frame; the
    // plates (with the Recycle Bin) are made again on the next frame, and the
    // spacing and icon sizes need a refresh.
    HWND tb = FindTaskbar();
    bool ran = tb && RunFromWindowThread(tb, [](void*) {
            LoadSettings();
            try { RemovePlates(); } catch (...) {}
            try { StopTrash(); } catch (...) {}
            for (auto& b : *g_buttons) {
                if (auto el = b.element.get()) {
                    try {
                        ApplySpacing(el);
                        ApplyButtonWidth(b, el);
                        ApplyIconColumn(b);
                    } catch (...) {}
                }
            }
            for (auto& i : *g_iconImages) {
                if (auto el = i.element.get()) {
                    try { ApplyIconSize(el); } catch (...) {}
                }
            }
            g_row.clear();  // rebuilt with the new spacing on next hover
            g_rowParent = nullptr;
            MarkPlatesDirty();
            try { ApplyAllTrayStealth(); } catch (...) {}
            try { UpdateAllTrayPlates(); } catch (...) {}
            try { ApplyAllTrayPlateLooks(); } catch (...) {}
        }, nullptr);
    if (!ran) LoadSettings();
    if (g_taskbarHeight != oldTaskbarHeight || g_iconLoadSize != oldIconLoadSize) {
        ApplyTaskbarHeight();
    }
    ApplyMagnifyOverWindows();  // the dock's height may have changed too
}
