#pragma once

#include <windhawk_utils.h>
#include <windows.h>
#include <regex>
#include <charconv>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Input.h>

namespace wuxa = winrt::Windows::UI::Xaml::Automation;

// Forward declarations of external dependencies
extern int g_taskbarHeight;
extern float g_iconLoadSize;
extern float g_iconSize;
struct Button;
extern Button* FindButton(wux::FrameworkElement const& el);
void ProcessButton(wux::FrameworkElement const& button);
void NoteTaskbarFrame(wux::FrameworkElement const& frame);
wux::FrameworkElement FindTaskbarFrame(wux::FrameworkElement const& el);
bool IsFileExplorer(wux::FrameworkElement const& button);
void MarkPlatesDirty();
void OnTaskbarPointerMoved(wux::FrameworkElement const& frame, wuxi::PointerRoutedEventArgs const& e);
void OnTaskbarPointerExited();
HWND FindTaskbar();

// External declarations from dock_over_windows.h
extern TrayUI__Hide_t TrayUI__Hide_Original;
void WINAPI TrayUI__Hide_Hook(void* pThis);
extern CSecondaryTray__AutoHide_t CSecondaryTray__AutoHide_Original;
void WINAPI CSecondaryTray__AutoHide_Hook(void* pThis, bool param1);

// Set while the mod unloads, so the hooks stop touching the taskbar.
inline std::atomic<bool> g_unloading = false;

inline wux::FrameworkElement ElementFromThis(void* pThis) {
    winrt::Windows::Foundation::IUnknown unknown;
    winrt::copy_from_abi(unknown, (void**)pThis + 3);
    return unknown.try_as<wux::FrameworkElement>();
}

inline void OnTaskListButtonUpdated(void* pThis) {
    if (g_unloading) return;
    try {
        if (auto button = ElementFromThis(pThis)) {
            DockLog(L"[hook] button class: %s | name: %s",
                winrt::get_class_name(button).c_str(),
                wuxa::AutomationProperties::GetName(button).c_str());
            ProcessButton(button);
            NoteTaskbarFrame(FindTaskbarFrame(button));
        }
    } catch (...) {
        DockLog(L"Button update failed: 0x%08X", winrt::to_hresult());
    }
}

using TaskbarFrame_OnPointer_t = int(WINAPI*)(void* pThis, void* pArgs);

inline wux::FrameworkElement TaskbarFrameFromThis(void* pThis) {
    wux::FrameworkElement element{nullptr};
    ((::IUnknown*)pThis)->QueryInterface(winrt::guid_of<wux::FrameworkElement>(),
                                         winrt::put_abi(element));
    if (!element || winrt::get_class_name(element) != L"Taskbar.TaskbarFrame") return nullptr;
    return element;
}

inline TaskbarFrame_OnPointer_t TaskbarFrame_OnPointerMoved_Original;
inline int WINAPI TaskbarFrame_OnPointerMoved_Hook(void* pThis, void* pArgs) {
    int result = TaskbarFrame_OnPointerMoved_Original(pThis, pArgs);
    if (g_unloading) return result;
    try {
        if (auto frame = TaskbarFrameFromThis(pThis)) {
            wuxi::PointerRoutedEventArgs args{nullptr};
            ((::IUnknown*)pArgs)->QueryInterface(
                winrt::guid_of<wuxi::PointerRoutedEventArgs>(), winrt::put_abi(args));
            if (args) OnTaskbarPointerMoved(frame, args);
        }
    } catch (...) {}
    return result;
}

inline TaskbarFrame_OnPointer_t TaskbarFrame_OnPointerExited_Original;
inline int WINAPI TaskbarFrame_OnPointerExited_Hook(void* pThis, void* pArgs) {
    int result = TaskbarFrame_OnPointerExited_Original(pThis, pArgs);
    if (g_unloading) return result;
    try {
        if (TaskbarFrameFromThis(pThis)) OnTaskbarPointerExited();
    } catch (...) {}
    return result;
}

inline bool g_placingJumpList = false;
inline bool g_hasJumpListButton = false;
inline wf::Rect g_jumpListButton{};

template <typename F>
void OpenJumpListFor(wux::UIElement const& sender, F original) {
    g_hasJumpListButton = false;
    if (!g_unloading) {
        try {
            auto button = sender.try_as<wux::FrameworkElement>();
            auto root = button ? button.XamlRoot() : nullptr;
            auto content = root ? root.Content() : nullptr;
            if (content && FindButton(button)) {
                g_jumpListButton = button.TransformToVisual(content).TransformBounds(
                    {0, 0, (float)button.ActualWidth(), (float)button.ActualHeight()});
                g_hasJumpListButton = true;
            }
        } catch (...) {}
    }
    original();
    g_hasJumpListButton = false;
}

using VoidMethod_t = void(WINAPI*)(void* pThis);

inline void TriggerWinXMenu() {
    DockLog(L"[Dock] TriggerWinXMenu: queueing Win+X hotkey asynchronously");
    QueueShellJob([] {
        Sleep(80);
        keybd_event(VK_LWIN, 0, 0, 0);
        keybd_event('X', 0, 0, 0);
        keybd_event('X', 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
    });
}

using TaskbarResources_OnTaskListButtonContextRequested_t =
    void(WINAPI*)(void* pThis, wux::UIElement const& sender, void* args);
inline TaskbarResources_OnTaskListButtonContextRequested_t
    TaskbarResources_OnTaskListButtonContextRequested_Original;
inline void WINAPI TaskbarResources_OnTaskListButtonContextRequested_Hook(
    void* pThis, wux::UIElement const& sender, void* args) {
    g_lastAppButtonContextTime = GetTickCount64();
    if (!g_unloading && g_explorerRightClickWinX) {
        if (auto fe = sender.try_as<wux::FrameworkElement>()) {
            if (IsFileExplorer(fe)) {
                bool isModifierPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0 || 
                                         (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                if (!isModifierPressed) {
                    TriggerWinXMenu();
                    return;
                }
            }
        }
    }
    OpenJumpListFor(sender, [&] {
        TaskbarResources_OnTaskListButtonContextRequested_Original(pThis, sender, args);
    });
}

using TaskListButtonHandlers_HandleContextRequested_t =
    void(WINAPI*)(wux::UIElement const& sender, void* args);
inline TaskListButtonHandlers_HandleContextRequested_t
    TaskListButtonHandlers_HandleContextRequested_Original;
inline void WINAPI TaskListButtonHandlers_HandleContextRequested_Hook(wux::UIElement const& sender,
                                                               void* args) {
    g_lastAppButtonContextTime = GetTickCount64();
    if (!g_unloading && g_explorerRightClickWinX) {
        if (auto fe = sender.try_as<wux::FrameworkElement>()) {
            if (IsFileExplorer(fe)) {
                bool isModifierPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0 || 
                                         (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                if (!isModifierPressed) {
                    TriggerWinXMenu();
                    return;
                }
            }
        }
    }
    OpenJumpListFor(sender, [&] {
        TaskListButtonHandlers_HandleContextRequested_Original(sender, args);
    });
}

using CTaskListWnd_ComputeJumpViewPosition_t =
    HRESULT(WINAPI*)(void* pThis, void* taskBtnGroup, int param2, wf::Point* point,
                     wux::HorizontalAlignment* horizontal, wux::VerticalAlignment* vertical);
inline CTaskListWnd_ComputeJumpViewPosition_t CTaskListWnd_ComputeJumpViewPosition_Original;
inline HRESULT WINAPI CTaskListWnd_ComputeJumpViewPosition_Hook(void* pThis, void* taskBtnGroup,
                                                         int param2, wf::Point* point,
                                                         wux::HorizontalAlignment* horizontal,
                                                         wux::VerticalAlignment* vertical) {
    g_placingJumpList = true;
    HRESULT hr = CTaskListWnd_ComputeJumpViewPosition_Original(pThis, taskBtnGroup, param2,
                                                               point, horizontal, vertical);
    g_placingJumpList = false;
    return hr;
}

using ToRawPixelsInDesktopCoordinates_t = RECT(WINAPI*)(wf::Rect const& rect, UINT dpi,
                                                         HWND hWnd);
inline ToRawPixelsInDesktopCoordinates_t ToRawPixelsInDesktopCoordinates_Original;
inline RECT WINAPI ToRawPixelsInDesktopCoordinates_Hook(wf::Rect const& rect, UINT dpi, HWND hWnd) {
    if (g_placingJumpList && g_hasJumpListButton && !g_unloading) {
        wf::Rect moved = rect;
        moved.X = g_jumpListButton.X;
        moved.Width = g_jumpListButton.Width;
        return ToRawPixelsInDesktopCoordinates_Original(moved, dpi, hWnd);
    }
    return ToRawPixelsInDesktopCoordinates_Original(rect, dpi, hWnd);
}

inline winrt::weak_ref<wux::FrameworkElement> g_blockedDrag;

inline bool IsBlockedDrag(void* pThis) {
    try {
        auto blocked = g_blockedDrag.get();
        return blocked && ElementFromThis(pThis) == blocked;
    } catch (...) {
        return false;
    }
}

using TaskListButton_DragPoint_t = void(WINAPI*)(void* pThis, wf::Point point);
using TaskListButton_OnDragCompletedGesture_t = void(WINAPI*)(void* pThis);
inline TaskListButton_DragPoint_t TaskListButton_OnDragStartedGesture_Original;
inline void WINAPI TaskListButton_OnDragStartedGesture_Hook(void* pThis, wf::Point point) {
    if (!g_unloading) {
        try {
            auto button = ElementFromThis(pThis);
            if (button && IsFileExplorer(button)) {
                g_blockedDrag = winrt::make_weak(button);
                return;
            }
        } catch (...) {}
    }
    TaskListButton_OnDragStartedGesture_Original(pThis, point);
}

inline TaskListButton_DragPoint_t TaskListButton_UpdateDrag_Original;
inline void WINAPI TaskListButton_UpdateDrag_Hook(void* pThis, wf::Point point) {
    if (IsBlockedDrag(pThis)) return;
    TaskListButton_UpdateDrag_Original(pThis, point);
}

inline TaskListButton_OnDragCompletedGesture_t TaskListButton_OnDragCompletedGesture_Original;
inline void WINAPI TaskListButton_OnDragCompletedGesture_Hook(void* pThis) {
    if (IsBlockedDrag(pThis)) {
        g_blockedDrag = nullptr;
        return;
    }
    TaskListButton_OnDragCompletedGesture_Original(pThis);
    if (!g_unloading) MarkPlatesDirty();
}

inline std::atomic<bool> g_applyingHeight = false;

inline bool OverrideHeight(int enumTaskbarSize) {
    return g_taskbarHeight > 0 && (enumTaskbarSize == 1 || enumTaskbarSize == 2);
}

using VoidMethod_t = void(WINAPI*)(void* pThis);
using SetDouble_t = void(WINAPI*)(void* pThis, double value);

using TaskbarConfiguration_GetFrameSize_t = double(WINAPI*)(int enumTaskbarSize);
inline TaskbarConfiguration_GetFrameSize_t TaskbarConfiguration_GetFrameSize_Original;
inline double WINAPI TaskbarConfiguration_GetFrameSize_Hook(int enumTaskbarSize) {
    if (OverrideHeight(enumTaskbarSize)) return g_taskbarHeight;
    return TaskbarConfiguration_GetFrameSize_Original(enumTaskbarSize);
}

using SystemTray_GetFrameSize_t = double(WINAPI*)(void* pThis, int enumTaskbarSize);
inline SystemTray_GetFrameSize_t SystemTrayController_GetFrameSize_Original;
inline double WINAPI SystemTrayController_GetFrameSize_Hook(void* pThis, int enumTaskbarSize) {
    if (OverrideHeight(enumTaskbarSize)) return g_taskbarHeight;
    return SystemTrayController_GetFrameSize_Original(pThis, enumTaskbarSize);
}

inline SystemTray_GetFrameSize_t SystemTraySecondaryController_GetFrameSize_Original;
inline double WINAPI SystemTraySecondaryController_GetFrameSize_Hook(void* pThis, int enumTaskbarSize) {
    if (OverrideHeight(enumTaskbarSize)) return g_taskbarHeight;
    return SystemTraySecondaryController_GetFrameSize_Original(pThis, enumTaskbarSize);
}

inline thread_local bool g_inTrayUpdateFrameSize = false;

inline VoidMethod_t SystemTrayController_UpdateFrameSize_Original;
inline void WINAPI SystemTrayController_UpdateFrameSize_Hook(void* pThis) {
    g_inTrayUpdateFrameSize = true;
    SystemTrayController_UpdateFrameSize_Original(pThis);
    g_inTrayUpdateFrameSize = false;
}

inline VoidMethod_t SystemTraySecondaryController_UpdateFrameSize_Original;
inline void WINAPI SystemTraySecondaryController_UpdateFrameSize_Hook(void* pThis) {
    g_inTrayUpdateFrameSize = true;
    SystemTraySecondaryController_UpdateFrameSize_Original(pThis);
    g_inTrayUpdateFrameSize = false;
}

inline SetDouble_t SystemTrayFrame_Height_Original;
inline void WINAPI SystemTrayFrame_Height_Hook(void* pThis, double value) {
    if (g_inTrayUpdateFrameSize && g_taskbarHeight > 0) value = g_taskbarHeight;
    SystemTrayFrame_Height_Original(pThis, value);
}

inline SetDouble_t TaskbarFrame_MaxHeight_Original;

inline SetDouble_t TaskbarFrame_Height_Original;
inline void WINAPI TaskbarFrame_Height_Hook(void* pThis, double value) {
    if (TaskbarFrame_MaxHeight_Original) {
        TaskbarFrame_MaxHeight_Original(pThis, std::numeric_limits<double>::infinity());
    }
    TaskbarFrame_Height_Original(pThis, value);
}

inline void* TaskbarController_OnGroupingModeChanged_Address;

inline LONG GetTaskbarFrameOffset() {
    static LONG offset = []() -> LONG {
        const BYTE* p = (const BYTE*)TaskbarController_OnGroupingModeChanged_Address;
        if (p && p[0] == 0x48 && p[1] == 0x83 && p[2] == 0xEC &&
            (p[4] == 0x48 || p[4] == 0x4C) && p[5] == 0x8B && (p[6] & 0xC0) == 0x80) {
            LONG o = *(LONG*)(p + 7);
            if (o > 0 && o <= 0xFFFF) return o;
        }
        Wh_Log(L"Taskbar frame offset not found");
        return 0;
    }();
    return offset;
}

inline VoidMethod_t TaskbarController_UpdateFrameHeight_Original;
inline void WINAPI TaskbarController_UpdateFrameHeight_Hook(void* pThis) {
    wux::FrameworkElement frame{nullptr};
    try {
        LONG offset = GetTaskbarFrameOffset();
        void* taskbarFrame = offset ? *(void**)((BYTE*)pThis + offset) : nullptr;
        if (taskbarFrame) {
            ((::IUnknown**)taskbarFrame)[1]->QueryInterface(
                winrt::guid_of<wux::FrameworkElement>(), winrt::put_abi(frame));
        }
        if (frame) frame.MaxHeight(std::numeric_limits<double>::infinity());
    } catch (...) {}

    TaskbarController_UpdateFrameHeight_Original(pThis);

    if (!frame) return;
    if (!g_unloading) {
        try { NoteTaskbarFrame(frame); } catch (...) {}
    }

    try {
        auto grid = wuxm::VisualTreeHelper::GetParent(frame).try_as<wux::FrameworkElement>();
        if (grid) {
            double height = frame.Height();
            double gridHeight = grid.Height();
            if (gridHeight > 0 && gridHeight != height) grid.Height(height);
        }
    } catch (...) {}
}

inline thread_local bool g_inTaskbarFrameGetMetrics = false;

using GetIconHeightInViewPixels_t = double(WINAPI*)(void* pThis);
inline GetIconHeightInViewPixels_t TaskbarConfiguration_GetIconHeightInViewPixels_Original;
inline double WINAPI TaskbarConfiguration_GetIconHeightInViewPixels_Hook(void* pThis) {
    double height = TaskbarConfiguration_GetIconHeightInViewPixels_Original(pThis);
    if (g_iconLoadSize <= 0 || g_inTaskbarFrameGetMetrics || height <= 16) return height;
    return g_iconLoadSize;
}

using TaskbarFrame_GetMetrics_t = void*(WINAPI*)(void* pThis, void* metrics);
inline TaskbarFrame_GetMetrics_t TaskbarFrame_GetMetrics_Original;
inline void* WINAPI TaskbarFrame_GetMetrics_Hook(void* pThis, void* metrics) {
    g_inTaskbarFrameGetMetrics = true;
    void* ret = TaskbarFrame_GetMetrics_Original(pThis, metrics);
    g_inTaskbarFrameGetMetrics = false;
    return ret;
}

inline void* TaskListButton_IconHeight_Address;

inline size_t GetIconHeightOffset() {
    static size_t offset = []() -> size_t {
        BYTE* p = (BYTE*)TaskListButton_IconHeight_Address;
        std::regex regex(R"(movsd xmm\d+, qword ptr \[rcx\+0x([0-9a-f]+)\])",
                         std::regex_constants::icase);
        for (int i = 0; p && i < 30; i++) {
            WH_DISASM_RESULT result;
            if (!Wh_Disasm(p, &result)) break;
            p += result.length;
            std::string_view text = result.text;
            if (text == "ret") break;
            std::match_results<std::string_view::const_iterator> match;
            if (std::regex_match(text.begin(), text.end(), match, regex)) {
                size_t o = 0;
                std::from_chars(&*match[1].first, &*match[1].first + match[1].length(), o, 16);
                Wh_Log(L"Icon height offset: 0x%zX", o);
                return o <= 0xFFFF ? o : 0;
            }
        }
        Wh_Log(L"Icon height offset not found; icon resolution unchanged");
        return 0;
    }();
    return offset;
}

template <typename F>
void WithStockIconHeight(void* pThis, F original) {
    size_t offset = g_iconLoadSize > 0 ? GetIconHeightOffset() : 0;
    if (!offset) return original();
    double* iconHeight = (double*)((BYTE*)pThis + offset);
    double saved = *iconHeight;
    *iconHeight = kStockIconSize;
    original();
    *iconHeight = saved;
}

#define STOCK_ICON_HEIGHT_HOOK(name)                                              \
    inline VoidMethod_t TaskListButton_##name##_Original;                                \
    inline void WINAPI TaskListButton_##name##_Hook(void* pThis) {                       \
        WithStockIconHeight(pThis, [=] { TaskListButton_##name##_Original(pThis); }); \
    }

STOCK_ICON_HEIGHT_HOOK(UpdateMultiWindowClip)
STOCK_ICON_HEIGHT_HOOK(CreateMultiWindowClip)

inline VoidMethod_t TaskListButton_UpdateBadge_Original;
inline void WINAPI TaskListButton_UpdateBadge_Hook(void* pThis) {
    WithStockIconHeight(pThis, [=] { TaskListButton_UpdateBadge_Original(pThis); });
    OnTaskListButtonUpdated(pThis);
}

inline VoidMethod_t TaskListButton_UpdateButtonPadding_Original;
inline void WINAPI TaskListButton_UpdateButtonPadding_Hook(void* pThis) {
    WithStockIconHeight(pThis, [=] { TaskListButton_UpdateButtonPadding_Original(pThis); });
    OnTaskListButtonUpdated(pThis);
}

inline VoidMethod_t TaskListButton_UpdateVisualStates_Original;
inline void WINAPI TaskListButton_UpdateVisualStates_Hook(void* pThis) {
    WithStockIconHeight(pThis, [=] { TaskListButton_UpdateVisualStates_Original(pThis); });
    OnTaskListButtonUpdated(pThis);
}

inline VoidMethod_t TaskListButton_UpdateIconColumnDefinition_Original;
inline void WINAPI TaskListButton_UpdateIconColumnDefinition_Hook(void* pThis) {
    TaskListButton_UpdateIconColumnDefinition_Original(pThis);
    OnTaskListButtonUpdated(pThis);
}

using TaskListButton_OverlayIcon_t = void(WINAPI*)(void* pThis, void* stream);
inline TaskListButton_OverlayIcon_t TaskListButton_OverlayIcon_Original;
inline void WINAPI TaskListButton_OverlayIcon_Hook(void* pThis, void* stream) {
    WithStockIconHeight(pThis, [=] { TaskListButton_OverlayIcon_Original(pThis, stream); });
}

inline SetDouble_t ExperienceToggleButton_IconHeight_Original;
inline void WINAPI ExperienceToggleButton_IconHeight_Hook(void* pThis, double height) {
    if (g_iconLoadSize > 0 && height > 16) height = g_iconSize;
    ExperienceToggleButton_IconHeight_Original(pThis, height);
}

using GetDouble_t = double(WINAPI*)(void* pThis);
inline GetDouble_t ExperienceToggleButton_IconHeight_get;
inline thread_local bool g_settingToggleIconHeight = false;

inline void SetToggleIconHeight(void* pThis, double height) {
    g_settingToggleIconHeight = true;
    ExperienceToggleButton_IconHeight_Original(pThis, height);
    g_settingToggleIconHeight = false;
}

inline VoidMethod_t ExperienceToggleButton_UpdateButtonPadding_Original;
inline void WINAPI ExperienceToggleButton_UpdateButtonPadding_Hook(void* pThis) {
    if (g_settingToggleIconHeight) return;
    double height = kStockIconSize;
    if (g_iconLoadSize > 0 && ExperienceToggleButton_IconHeight_get &&
        ExperienceToggleButton_IconHeight_Original) {
        height = ExperienceToggleButton_IconHeight_get(pThis);
    }
    bool swap = height > 16 && height != kStockIconSize;
    if (swap) SetToggleIconHeight(pThis, kStockIconSize);
    ExperienceToggleButton_UpdateButtonPadding_Original(pThis);
    if (swap) SetToggleIconHeight(pThis, height);
}

inline SetDouble_t TaskbarComponentHost_IconHeight_Original;
inline void WINAPI TaskbarComponentHost_IconHeight_Hook(void* pThis, double height) {
    if (g_iconLoadSize > 0 && height > 16) height = g_iconSize;
    TaskbarComponentHost_IconHeight_Original(pThis, height);
}

inline VoidMethod_t TrayUI__StuckTrayChange_Original;

using TrayUI__HandleSettingChange_t = void(WINAPI*)(void*, void*, void*, void*, void*);
inline TrayUI__HandleSettingChange_t TrayUI__HandleSettingChange_Original;
inline void WINAPI TrayUI__HandleSettingChange_Hook(void* pThis, void* p1, void* p2, void* p3,
                                             void* p4) {
    TrayUI__HandleSettingChange_Original(pThis, p1, p2, p3, p4);
    if (g_applyingHeight) TrayUI__StuckTrayChange_Original(pThis);
}

inline void ApplyTaskbarHeight() {
    HWND tb = FindTaskbar();
    if (!tb) return;
    g_applyingHeight = true;
    SendMessage(tb, WM_SETTINGCHANGE, SPI_SETLOGICALDPIOVERRIDE, 0);
    g_applyingHeight = false;

    if (HWND rebar = FindWindowEx(tb, nullptr, L"ReBarWindow32", nullptr)) {
        if (HWND taskSw = FindWindowEx(rebar, nullptr, L"MSTaskSwWClass", nullptr)) {
            SendMessage(taskSw, 0x452, 3, 0);
        }
    }
}

inline bool HookTaskbarDllSymbols() {
    HMODULE module = LoadLibraryEx(L"taskbar.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) return false;

    WindhawkUtils::SYMBOL_HOOK taskbarDllHooks[] = {
        {
            {LR"(public: void __cdecl TrayUI::_StuckTrayChange(void))"},
            &TrayUI__StuckTrayChange_Original,
        },
        {
            {LR"(public: void __cdecl TrayUI::_HandleSettingChange(struct HWND__ *,unsigned int,unsigned __int64,__int64))"},
            &TrayUI__HandleSettingChange_Original,
            TrayUI__HandleSettingChange_Hook,
        },
        {
            {LR"(public: void __cdecl TrayUI::_Hide(void))"},
            &TrayUI__Hide_Original,
            TrayUI__Hide_Hook,
            true,
        },
        {
            {LR"(private: void __cdecl CSecondaryTray::_AutoHide(bool))"},
            &CSecondaryTray__AutoHide_Original,
            CSecondaryTray__AutoHide_Hook,
            true,
        },
        {
            {LR"(protected: long __cdecl CTaskListWnd::_ComputeJumpViewPosition(struct ITaskBtnGroup *,int,struct Windows::Foundation::Point &,enum Windows::UI::Xaml::HorizontalAlignment &,enum Windows::UI::Xaml::VerticalAlignment &)const )"},
            &CTaskListWnd_ComputeJumpViewPosition_Original,
            CTaskListWnd_ComputeJumpViewPosition_Hook,
            true,
        },
        {
            {LR"(struct tagRECT __cdecl ToRawPixelsInDesktopCoordinates(struct winrt::Windows::Foundation::Rect const &,unsigned int,struct HWND__ *))"},
            &ToRawPixelsInDesktopCoordinates_Original,
            ToRawPixelsInDesktopCoordinates_Hook,
            true,
        },
    };
    return WindhawkUtils::HookSymbols(module, taskbarDllHooks, ARRAYSIZE(taskbarDllHooks));
}

using COMBINED_SH = WindhawkUtils::SYMBOL_HOOK;

inline std::vector<COMBINED_SH> GetTrayHooks() {
    WindhawkUtils::SYMBOL_HOOK trayHooks[] = {
        {
            {LR"(private: double __cdecl winrt::SystemTray::implementation::SystemTrayController::GetFrameSize(enum winrt::WindowsUdk::UI::Shell::TaskbarSize))"},
            &SystemTrayController_GetFrameSize_Original,
            SystemTrayController_GetFrameSize_Hook,
            true,
        },
        {
            {LR"(private: double __cdecl winrt::SystemTray::implementation::SystemTraySecondaryController::GetFrameSize(enum winrt::WindowsUdk::UI::Shell::TaskbarSize))"},
            &SystemTraySecondaryController_GetFrameSize_Original,
            SystemTraySecondaryController_GetFrameSize_Hook,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::SystemTray::implementation::SystemTrayController::UpdateFrameSize(void))"},
            &SystemTrayController_UpdateFrameSize_Original,
            SystemTrayController_UpdateFrameSize_Hook,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::SystemTray::implementation::SystemTraySecondaryController::UpdateFrameSize(void))"},
            &SystemTraySecondaryController_UpdateFrameSize_Original,
            SystemTraySecondaryController_UpdateFrameSize_Hook,
            true,
        },
        {
            {LR"(public: __cdecl winrt::impl::consume_Windows_UI_Xaml_IFrameworkElement<struct winrt::SystemTray::SystemTrayFrame>::Height(double)const )"},
            &SystemTrayFrame_Height_Original,
            SystemTrayFrame_Height_Hook,
            true,
        },
    };
    return {std::make_move_iterator(std::begin(trayHooks)),
            std::make_move_iterator(std::end(trayHooks))};
}

inline bool HookTaskbarViewSymbols(HMODULE module, bool withTray) {
    WindhawkUtils::SYMBOL_HOOK taskbarViewHooks[] = {
        {
            {LR"(public: static double __cdecl winrt::Taskbar::implementation::TaskbarConfiguration::GetFrameSize(enum winrt::WindowsUdk::UI::Shell::TaskbarSize))"},
            &TaskbarConfiguration_GetFrameSize_Original,
            TaskbarConfiguration_GetFrameSize_Hook,
            true,
        },
        {
            {LR"(public: __cdecl winrt::impl::consume_Windows_UI_Xaml_IFrameworkElement<struct winrt::Taskbar::implementation::TaskbarFrame>::MaxHeight(double)const )"},
            &TaskbarFrame_MaxHeight_Original,
            nullptr,
            true,
        },
        {
            {LR"(public: __cdecl winrt::impl::consume_Windows_UI_Xaml_IFrameworkElement<struct winrt::Taskbar::implementation::TaskbarFrame>::Height(double)const )"},
            &TaskbarFrame_Height_Original,
            TaskbarFrame_Height_Hook,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskbarController::OnGroupingModeChanged(void))"},
            &TaskbarController_OnGroupingModeChanged_Address,
            nullptr,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskbarController::UpdateFrameHeight(void))"},
            &TaskbarController_UpdateFrameHeight_Original,
            TaskbarController_UpdateFrameHeight_Hook,
            true,
        },
        {
            {LR"(public: virtual int __cdecl winrt::impl::produce<struct winrt::Taskbar::implementation::TaskbarFrame,struct winrt::Windows::UI::Xaml::Controls::IControlOverrides>::OnPointerMoved(void *))"},
            &TaskbarFrame_OnPointerMoved_Original,
            TaskbarFrame_OnPointerMoved_Hook,
            true,
        },
        {
            {LR"(public: virtual int __cdecl winrt::impl::produce<struct winrt::Taskbar::implementation::TaskbarFrame,struct winrt::Windows::UI::Xaml::Controls::IControlOverrides>::OnPointerExited(void *))"},
            &TaskbarFrame_OnPointerExited_Original,
            TaskbarFrame_OnPointerExited_Hook,
            true,
        },
        {
            {LR"(public: void __cdecl winrt::Taskbar::implementation::TaskbarResources::OnTaskListButtonContextRequested(struct winrt::Windows::UI::Xaml::UIElement const &,struct winrt::Windows::UI::Xaml::Input::ContextRequestedEventArgs const &))"},
            &TaskbarResources_OnTaskListButtonContextRequested_Original,
            TaskbarResources_OnTaskListButtonContextRequested_Hook,
            true,
        },
        {
            {LR"(public: static void __cdecl winrt::Taskbar::implementation::TaskListButtonHandlers::HandleContextRequested(struct winrt::Windows::UI::Xaml::UIElement const &,struct winrt::Windows::UI::Xaml::Input::ContextRequestedEventArgs const &))"},
            &TaskListButtonHandlers_HandleContextRequested_Original,
            TaskListButtonHandlers_HandleContextRequested_Hook,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskListButton::OnDragStartedGesture(struct winrt::Windows::Foundation::Point))"},
            &TaskListButton_OnDragStartedGesture_Original,
            TaskListButton_OnDragStartedGesture_Hook,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskListButton::UpdateDrag(struct winrt::Windows::Foundation::Point))"},
            &TaskListButton_UpdateDrag_Original,
            TaskListButton_UpdateDrag_Hook,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskListButton::OnDragCompletedGesture(void))"},
            &TaskListButton_OnDragCompletedGesture_Original,
            TaskListButton_OnDragCompletedGesture_Hook,
            true,
        },
        {
            {LR"(public: double __cdecl winrt::Taskbar::implementation::TaskbarConfiguration::GetIconHeightInViewPixels(void))"},
            &TaskbarConfiguration_GetIconHeightInViewPixels_Original,
            TaskbarConfiguration_GetIconHeightInViewPixels_Hook,
            true,
        },
        {
            {LR"(public: struct winrt::Taskbar::implementation::TaskbarFrameMetrics __cdecl winrt::Taskbar::implementation::TaskbarFrame::GetMetrics(void)const )"},
            &TaskbarFrame_GetMetrics_Original,
            TaskbarFrame_GetMetrics_Hook,
            true,
        },
        {
            {LR"(public: void __cdecl winrt::Taskbar::implementation::TaskListButton::IconHeight(double))"},
            &TaskListButton_IconHeight_Address,
            nullptr,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskListButton::UpdateButtonPadding(void))"},
            &TaskListButton_UpdateButtonPadding_Original,
            TaskListButton_UpdateButtonPadding_Hook,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskListButton::UpdateVisualStates(void))"},
            &TaskListButton_UpdateVisualStates_Original,
            TaskListButton_UpdateVisualStates_Hook,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskListButton::UpdateIconColumnDefinition(void))"},
            &TaskListButton_UpdateIconColumnDefinition_Original,
            TaskListButton_UpdateIconColumnDefinition_Hook,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskListButton::UpdateMultiWindowClip(void))"},
            &TaskListButton_UpdateMultiWindowClip_Original,
            TaskListButton_UpdateMultiWindowClip_Hook,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskListButton::CreateMultiWindowClip(void))"},
            &TaskListButton_CreateMultiWindowClip_Original,
            TaskListButton_CreateMultiWindowClip_Hook,
            true,
        },
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskListButton::UpdateBadge(void))"},
            &TaskListButton_UpdateBadge_Original,
            TaskListButton_UpdateBadge_Hook,
            true,
        },
        {
            {LR"(public: void __cdecl winrt::Taskbar::implementation::TaskListButton::OverlayIcon(struct winrt::Windows::Storage::Streams::IRandomAccessStream const &))"},
            &TaskListButton_OverlayIcon_Original,
            TaskListButton_OverlayIcon_Hook,
            true,
        },
        {
            {LR"(public: double __cdecl winrt::Taskbar::implementation::ExperienceToggleButton::IconHeight(void)const )"},
            &ExperienceToggleButton_IconHeight_get,
            nullptr,
            true,
        },
        {
            {LR"(public: void __cdecl winrt::Taskbar::implementation::ExperienceToggleButton::IconHeight(double))"},
            &ExperienceToggleButton_IconHeight_Original,
            ExperienceToggleButton_IconHeight_Hook,
            true,
        },
        {
            {LR"(protected: virtual void __cdecl winrt::Taskbar::implementation::ExperienceToggleButton::UpdateButtonPadding(void))"},
            &ExperienceToggleButton_UpdateButtonPadding_Original,
            ExperienceToggleButton_UpdateButtonPadding_Hook,
            true,
        },
        {
            {LR"(public: void __cdecl winrt::Microsoft::Windows::Taskbar::implementation::TaskbarComponentHost::IconHeight(double))"},
            &TaskbarComponentHost_IconHeight_Original,
            TaskbarComponentHost_IconHeight_Hook,
            true,
        },
    };

    std::vector<COMBINED_SH> allHooks(std::make_move_iterator(std::begin(taskbarViewHooks)),
                                      std::make_move_iterator(std::end(taskbarViewHooks)));
    if (withTray) {
        for (auto& hook : GetTrayHooks()) allHooks.push_back(std::move(hook));
    }
    if (!WindhawkUtils::HookSymbols(module, allHooks.data(), allHooks.size())) return false;
    if (TaskListButton_IconHeight_Address) GetIconHeightOffset();
    return true;
}

inline bool HookTraySymbols(HMODULE module) {
    auto systemTrayHooks = GetTrayHooks();
    return WindhawkUtils::HookSymbols(module, systemTrayHooks.data(), systemTrayHooks.size());
}

inline bool TaskbarViewHasTray(HMODULE module) {
    HRSRC res = FindResource(module, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
    HGLOBAL global = res ? LoadResource(module, res) : nullptr;
    void* data = global ? LockResource(global) : nullptr;
    VS_FIXEDFILEINFO* info = nullptr;
    UINT size = 0;
    if (!data || !VerQueryValue(data, L"\\", (void**)&info, &size) || !size) return false;
    return HIWORD(info->dwFileVersionMS) < 2604;
}

inline std::atomic<bool> g_taskbarViewHooked = false;
inline std::atomic<bool> g_trayHooked = false;

inline bool HookTaskbarModules() {
    bool hooked = false;
    if (HMODULE view = GetModuleHandle(L"Taskbar.View.dll");
        view && !g_taskbarViewHooked.exchange(true)) {
        bool withTray = !GetModuleHandle(L"SystemTray.dll") && TaskbarViewHasTray(view) &&
                        !g_trayHooked.exchange(true);
        HookTaskbarViewSymbols(view, withTray);
        hooked = true;
    }
    if (HMODULE tray = GetModuleHandle(L"SystemTray.dll");
        tray && !g_trayHooked.exchange(true)) {
        HookTraySymbols(tray);
        hooked = true;
    }
    return hooked;
}

using LoadLibraryExW_t = decltype(&LoadLibraryExW);
inline LoadLibraryExW_t LoadLibraryExW_Original;
inline HMODULE WINAPI LoadLibraryExW_Hook(LPCWSTR name, HANDLE file, DWORD flags) {
    HMODULE module = LoadLibraryExW_Original(name, file, flags);
    if (module && (!g_taskbarViewHooked || !g_trayHooked) && HookTaskbarModules()) {
        Wh_ApplyHookOperations();
    }
    return module;
}
