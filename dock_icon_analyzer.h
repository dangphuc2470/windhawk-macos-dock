#pragma once

#include <windows.h>
#include <shellapi.h>
#include <windhawk_api.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// macOS Style Dock - Automatic Icon Geometry & Pixel Analyzer Module
// ============================================================================
// Quet va phan tich truc tiep mang pixel ARGB cua icon de:
// 1. Phat hien icon phang 4 huong / squircle -> cho phep tran vien 100%.
// 2. Phat hien icon lom chom -> tu dong lot card squircle phong cach macOS.
// 3. Do luong khoang thut le (transparent padding) ben trong asset de bu scale.
// 4. Tinh toan do sang (luminance) de tu dong chon card trang hoac card toi.
// 5. Luu ket qua vao In-Memory Cache (std::unordered_map) de dat toc do 0ms.

struct RectInt {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct IconProfile {
    bool analyzed = false;
    bool isSquircle = false;          // True: icon da la squircle/phang -> tran vien 100%
    bool isDark = false;              // True: tong mau toi -> card toi, False: card trang
    double paddingMultiplier = 1.0;   // He so bu tru thut le asset (mac dinh 1.0)
    double edgeZoom = 1.0;            // He so zoom tran vien (mac dinh 1.0, squircle co vien tuong phan thi 1.08)
    RectInt bounds;
};

[[clang::no_destroy]] inline std::unordered_map<std::wstring, IconProfile> g_iconProfileCache;

// Phan tich hinh hoc va mau sac truc tiep tu HICON theo thuat toan WinLaunch IconClassifier
inline IconProfile AnalyzeIcon(HICON hIcon, std::wstring const& name = L"") {
    IconProfile profile;
    if (!hIcon) return profile;

    ICONINFO info = {0};
    if (!GetIconInfo(hIcon, &info)) return profile;

    BITMAP bm = {0};
    GetObject(info.hbmColor ? info.hbmColor : info.hbmMask, sizeof(bm), &bm);
    int w = bm.bmWidth;
    int h = info.hbmColor ? bm.bmHeight : (bm.bmHeight / 2);
    if (w <= 0 || h <= 0) {
        if (info.hbmColor) DeleteObject(info.hbmColor);
        if (info.hbmMask) DeleteObject(info.hbmMask);
        return profile;
    }

    BITMAPINFO bi{};
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint32_t> pixels(w * h, 0);
    HDC hdc = GetDC(nullptr);
    GetDIBits(hdc, info.hbmColor ? info.hbmColor : info.hbmMask, 0, h, pixels.data(), &bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);

    if (info.hbmColor) DeleteObject(info.hbmColor);
    if (info.hbmMask) DeleteObject(info.hbmMask);

    int minX = w, minY = h, maxX = -1, maxY = -1, count = 0;
    double luminance = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t p = pixels[y * w + x];
            uint8_t a = (p >> 24) & 0xFF;
            if (a <= 180) continue;
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
            count++;
            uint8_t r = (p >> 16) & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t b = p & 0xFF;
            luminance += 0.299 * r + 0.587 * g + 0.114 * b;
        }
    }

    profile.bounds = {0, 0, w, h};
    auto getA = [&](int px, int py) -> uint8_t {
        if (px < 0 || px >= w || py < 0 || py >= h) return 0;
        return (pixels[py * w + px] >> 24) & 0xFF;
    };

    if (count > 0) {
        int cw = maxX - minX + 1, ch = maxY - minY + 1;
        profile.bounds = {minX, minY, cw, ch};
        int mx = (minX + maxX) / 2, my = (minY + maxY) / 2;
        double aspect = (double)cw / (double)ch;
        profile.isSquircle = aspect >= 0.90 && aspect <= 1.10 && (count / (double)(cw * ch)) > 0.82 &&
            getA(mx, minY) > 180 && getA(mx, maxY) > 180 &&
            getA(minX, my) > 180 && getA(maxX, my) > 180;
        profile.isDark = (luminance / count) < 125.0;
        double padding = (double)cw / (double)w;
        if (padding < 0.80 && padding > 0.40) {
            profile.paddingMultiplier = std::min(1.35, 1.0 / padding);
        }
    }

    std::wstring lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    auto contains = [&](std::wstring_view kw) {
        return lower.find(kw) != std::wstring::npos;
    };

    static const std::vector<std::wstring> terminal = {
        L"terminal", L"powershell", L"pwsh", L"cmd", L"command prompt", L"wsl", L"bash"
    };
    bool isTerm = false;
    for (auto const& t : terminal) {
        if (contains(t)) { isTerm = true; break; }
    }

    if (isTerm) {
        profile.isSquircle = false;
        profile.isDark = true;
        profile.paddingMultiplier = 1.0;
    } else if (contains(L"chatgpt") || contains(L"openai")) {
        profile.isSquircle = false;
        profile.isDark = false;
        profile.paddingMultiplier = 1.0;
    } else if (contains(L"antigravity") && !contains(L"ide")) {
        profile.isSquircle = false;
        profile.isDark = false;
        profile.paddingMultiplier = 1.27;
    } else if (contains(L"google.antigravityide") || contains(L"antigravity ide")) {
        profile.isSquircle = true;
        profile.isDark = true;
        profile.paddingMultiplier = 1.0;
    }

    // Conservative default: physical objects and asymmetric silhouettes
    // retain breathing room. A filled tile needs a stable opaque rim.
    if (profile.isSquircle) {
        int good = 0, total = 0;
        for (int edge = 0; edge < 4; edge++) {
            for (int i = 20; i <= 80; i += 5) {
                int xx = profile.bounds.x + (profile.bounds.width - 1) * i / 100;
                int yy = profile.bounds.y + (profile.bounds.height - 1) * i / 100;
                if (edge == 0) yy = profile.bounds.y + profile.bounds.height / 20;
                if (edge == 1) yy = profile.bounds.y + profile.bounds.height - 1 - profile.bounds.height / 20;
                if (edge == 2) xx = profile.bounds.x + profile.bounds.width / 20;
                if (edge == 3) xx = profile.bounds.x + profile.bounds.width - 1 - profile.bounds.width / 20;
                total++;
                if (getA(xx, yy) > 240) good++;
            }
        }
        profile.isSquircle = (total > 0 && ((double)good / total >= 0.94 || good == total));
    }

    if (profile.isSquircle) {
        int contrastedEdges = 0;
        auto getRGB = [&](int px, int py, int& r, int& g, int& b) {
            if (px < 0 || px >= w || py < 0 || py >= h) { r = g = b = 0; return; }
            uint32_t val = pixels[py * w + px];
            r = (val >> 16) & 0xFF;
            g = (val >> 8) & 0xFF;
            b = val & 0xFF;
        };
        for (int edge = 0; edge < 4; edge++) {
            int differences = 0;
            for (int i = 30; i <= 70; i += 10) {
                int x = profile.bounds.x + (profile.bounds.width - 1) * i / 100;
                int y = profile.bounds.y + (profile.bounds.height - 1) * i / 100;
                int inset = std::max(2, std::min(profile.bounds.width, profile.bounds.height) / 20);
                int ax = x, ay = y, bx = x, by = y;
                if (edge == 0) { ay = profile.bounds.y + 1; by = ay + inset; }
                if (edge == 1) { ay = profile.bounds.y + profile.bounds.height - 2; by = ay - inset; }
                if (edge == 2) { ax = profile.bounds.x + 1; bx = ax + inset; }
                if (edge == 3) { ax = profile.bounds.x + profile.bounds.width - 2; bx = ax - inset; }
                int ra, ga, ba, rb, gb, bb;
                getRGB(ax, ay, ra, ga, ba);
                getRGB(bx, by, rb, gb, bb);
                if (std::abs(ra - rb) + std::abs(ga - gb) + std::abs(ba - bb) > 110) differences++;
            }
            if (differences >= 4) contrastedEdges++;
        }
        if (contrastedEdges >= 3) profile.edgeZoom = 1.08;
    }

    profile.analyzed = true;
    return profile;
}

// Lay profile phan tich icon cho button, tu dong tra cuu cache truoc
template <typename TButton>
inline IconProfile GetIconProfileForButton(TButton const& button, std::wstring const& title) {
    if (title.empty()) return IconProfile{};

    // 1. Kiem tra cache trong bo nho
    auto it = g_iconProfileCache.find(title);
    if (it != g_iconProfileCache.end()) {
        return it->second;
    }

    IconProfile prof;
    std::wstring lower = title;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    // 2. Kiem tra cac app dac biet theo yeu cau truc tiep:
    // A. Terminal / Console: La hinh chu nhat -> BAT BUOC loai ra khoi Squircle, lot card DEN bo tron
    bool isTerminal = (lower.find(L"terminal") != std::wstring::npos) ||
                      (lower.find(L"powershell") != std::wstring::npos) ||
                      (lower.find(L"pwsh") != std::wstring::npos) ||
                      (lower.find(L"cmd") != std::wstring::npos) ||
                      (lower.find(L"command prompt") != std::wstring::npos) ||
                      (lower.find(L"wsl") != std::wstring::npos) ||
                      (lower.find(L"bash") != std::wstring::npos);
    if (isTerminal) {
        prof.isSquircle = false;
        prof.isDark = true;
        prof.paddingMultiplier = 1.0;
        prof.analyzed = true;
        g_iconProfileCache[title] = prof;
        Wh_Log(L"[IconAnalyzer] TERMINAL detected: '%s' -> isSquircle=0, isDark=1", title.c_str());
        return prof;
    }

    // B. ChatGPT: Icon la net xoan oc mau den -> BAT BUOC lot card TRANG de thay duoc net ve
    bool isChatGPT = (lower.find(L"chatgpt") != std::wstring::npos) ||
                     (lower.find(L"openai") != std::wstring::npos);
    if (isChatGPT) {
        prof.isSquircle = false;
        prof.isDark = false;
        prof.paddingMultiplier = 1.0;
        prof.analyzed = true;
        g_iconProfileCache[title] = prof;
        Wh_Log(L"[IconAnalyzer] CHATGPT detected: '%s' -> isSquircle=0, isDark=0", title.c_str());
        return prof;
    }

    // C. Antigravity Trang (electron app): Co khoang thut le -> lot card TRANG, bu padding 1.27x
    bool isAntigravityWhite = (lower.find(L"electron.app.antigravity") != std::wstring::npos) ||
                             (lower.find(L"antigravity") != std::wstring::npos &&
                              lower.find(L"ide") == std::wstring::npos);
    if (isAntigravityWhite) {
        prof.isSquircle = false;
        prof.isDark = false;
        prof.paddingMultiplier = 1.27; // Bu thut le 35px de bung vua khit card
        prof.analyzed = true;
        g_iconProfileCache[title] = prof;
        Wh_Log(L"[IconAnalyzer] ANTIGRAVITY WHITE detected: '%s' -> isSquircle=0, isDark=0, paddingMul=1.27", title.c_str());
        return prof;
    }

    // D. Antigravity IDE Den: Full-bleed squircle den
    bool isAntigravityBlack = (lower.find(L"google.antigravityide") != std::wstring::npos) ||
                              (lower.find(L"antigravity ide") != std::wstring::npos);
    if (isAntigravityBlack) {
        prof.isSquircle = true;
        prof.isDark = true;
        prof.paddingMultiplier = 1.0;
        prof.analyzed = true;
        g_iconProfileCache[title] = prof;
        Wh_Log(L"[IconAnalyzer] ANTIGRAVITY BLACK IDE detected: '%s' -> isSquircle=1, isDark=1", title.c_str());
        return prof;
    }

    // 3. Trich xuat HICON tu file path (.exe / .lnk)
    HICON hIcon = nullptr;
    bool needDestroy = false;

    auto tryExtractIconFromPath = [&](std::wstring const& path) -> bool {
        if (path.empty()) return false;
        HRESULT hr = SHDefExtractIconW(path.c_str(), 0, 0, &hIcon, nullptr, 256);
        if (SUCCEEDED(hr) && hIcon) {
            needDestroy = true;
            return true;
        }
        SHFILEINFOW sfi = {0};
        if (SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON) && sfi.hIcon) {
            hIcon = sfi.hIcon;
            needDestroy = true;
            return true;
        }
        return false;
    };

    // A. Tim duong dan tap tin .exe hoac .lnk ben trong title
    size_t colon = title.find(L":\\");
    while (colon != std::wstring::npos && !hIcon) {
        if (colon > 0 && iswalpha(title[colon - 1])) {
            size_t start = colon - 1;
            size_t exePos = title.find(L".exe", colon);
            size_t lnkPos = title.find(L".lnk", colon);
            size_t endPos = std::wstring::npos;
            if (exePos != std::wstring::npos && (lnkPos == std::wstring::npos || exePos < lnkPos)) {
                endPos = exePos + 4;
            } else if (lnkPos != std::wstring::npos) {
                endPos = lnkPos + 4;
            }
            if (endPos != std::wstring::npos && endPos > start) {
                std::wstring cand = title.substr(start, endPos - start);
                if (tryExtractIconFromPath(cand)) break;
            }
        }
        colon = title.find(L":\\", colon + 2);
    }

    // B. Tim trong thu muc Pinned Shortcuts cua Taskbar neu chua co icon
    if (!hIcon) {
        wchar_t appData[MAX_PATH] = {0};
        if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH)) {
            std::wstring pinnedDir = std::wstring(appData) + L"\\Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar\\";
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW((pinnedDir + L"*.lnk").c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    std::wstring lnkName = fd.cFileName;
                    std::wstring lnkLower = lnkName;
                    std::transform(lnkLower.begin(), lnkLower.end(), lnkLower.begin(), ::towlower);
                    if (lnkLower.length() > 4) lnkLower.resize(lnkLower.length() - 4);
                    if (lnkLower.length() >= 3 && lower.find(lnkLower) != std::wstring::npos) {
                        if (tryExtractIconFromPath(pinnedDir + lnkName)) break;
                    }
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }
        }
    }

    // C. Neu van chua co icon, thu tim tu HWND cua cua so dang chay
    if (!hIcon) {
        struct SearchContext {
            std::wstring query;
            HWND foundHwnd = nullptr;
        } ctx;
        ctx.query = lower;

        EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL {
            auto* c = (SearchContext*)lParam;
            if (!IsWindowVisible(hWnd)) return TRUE;
            wchar_t text[256] = {0};
            GetWindowTextW(hWnd, text, ARRAYSIZE(text));
            if (text[0] == L'\0') return TRUE;

            std::wstring wndTitle = text;
            std::transform(wndTitle.begin(), wndTitle.end(), wndTitle.begin(), ::towlower);

            if (wndTitle.length() >= 3 && (c->query.find(wndTitle) != std::wstring::npos || wndTitle.find(c->query) != std::wstring::npos)) {
                c->foundHwnd = hWnd;
                return FALSE; // Tim thay
            }
            return TRUE;
        }, (LPARAM)&ctx);

        if (ctx.foundHwnd) {
            SendMessageTimeoutW(ctx.foundHwnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 50, (PDWORD_PTR)&hIcon);
            if (!hIcon) SendMessageTimeoutW(ctx.foundHwnd, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 50, (PDWORD_PTR)&hIcon);
            if (!hIcon) hIcon = (HICON)GetClassLongPtrW(ctx.foundHwnd, GCLP_HICON);
            if (!hIcon) hIcon = (HICON)GetClassLongPtrW(ctx.foundHwnd, GCLP_HICONSM);

            if (!hIcon) {
                DWORD pid = 0;
                GetWindowThreadProcessId(ctx.foundHwnd, &pid);
                if (pid) {
                    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                    if (hProc) {
                        wchar_t exePath[MAX_PATH] = {0};
                        DWORD size = MAX_PATH;
                        if (QueryFullProcessImageNameW(hProc, 0, exePath, &size)) {
                            tryExtractIconFromPath(exePath);
                        }
                        CloseHandle(hProc);
                    }
                }
            }
        }
    }

    // 4. Neu co HICON -> quet pixel truc tiep bang thuat toan WinLaunch
    if (hIcon) {
        prof = AnalyzeIcon(hIcon, title);
        if (needDestroy) DestroyIcon(hIcon);
    }

    // 5. Fallback day du tuong thich 1:1 voi WinLaunch
    if (!prof.analyzed) {
        static const std::vector<std::wstring> kSquircleApps = {
            L"anydesk", L"blip", L"photos", L"seelen", L"whatsapp", L"figma", L"photoshop",
            L"windhawk", L"quick share", L"quickshare", L"nearby share", L"nearbyshare",
            L"blender", L"nvidia", L"obs", L"parsec", L"photocraft", L"powertoys",
            L"shadowsnap", L"soundswitch", L"steam", L"stremio", L"tailscale",
            L"topaz", L"topnotify", L"unity", L"wireshark"
        };
        for (auto const& kw : kSquircleApps) {
            if (lower.find(kw) != std::wstring::npos) {
                prof.isSquircle = true;
                break;
            }
        }

        static const std::vector<std::wstring> kDarkApps = {
            L"spotify", L"discord", L"steam", L"obsidian", L"sublime", L"eagle",
            L"windhawk", L"blender", L"nvidia", L"obs", L"powertoys", L"shadowsnap",
            L"soundswitch", L"tailscale", L"topaz", L"unity"
        };
        for (auto const& kw : kDarkApps) {
            if (lower.find(kw) != std::wstring::npos) {
                prof.isDark = true;
                break;
            }
        }
        prof.paddingMultiplier = 1.0;
        prof.analyzed = true;
    }

    // Luu vao cache
    g_iconProfileCache[title] = prof;
    Wh_Log(L"[IconAnalyzer] id='%s' -> isSquircle=%d, isDark=%d, paddingMul=%.2f",
           title.c_str(), prof.isSquircle ? 1 : 0, prof.isDark ? 1 : 0, prof.paddingMultiplier);
    return prof;
}
