#include "adaptation_layer.h"
#include <Psapi.h>
#include <algorithm>
#include <cctype>

namespace Adaptation {

bool isWindows11() {
    // Win11 build >= 22000
    using RtlGetVersionFn = LONG(WINAPI*)(POSVERSIONINFOEXW);
    auto ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    auto RtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
        GetProcAddress(ntdll, "RtlGetVersion"));
    if (!RtlGetVersion) return false;
    OSVERSIONINFOEXW os = { sizeof(os) };
    RtlGetVersion(&os);
    return os.dwBuildNumber >= 22000;
}

bool isWindows10OrLater() {
    using RtlGetVersionFn = LONG(WINAPI*)(POSVERSIONINFOEXW);
    auto ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    auto RtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
        GetProcAddress(ntdll, "RtlGetVersion"));
    if (!RtlGetVersion) return false;
    OSVERSIONINFOEXW os = { sizeof(os) };
    RtlGetVersion(&os);
    return os.dwMajorVersion >= 10;
}

std::wstring getWindowClassName(HWND hwnd) {
    wchar_t buf[256] = {};
    GetClassNameW(hwnd, buf, 256);
    return buf;
}

std::string getProcessName(HWND hwnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return {};

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return {};

    wchar_t path[MAX_PATH] = {};
    DWORD len = MAX_PATH;
    if (!QueryFullProcessImageNameW(hProc, 0, path, &len)) {
        CloseHandle(hProc);
        return {};
    }
    CloseHandle(hProc);

    std::wstring fullPath(path, len);
    auto pos = fullPath.find_last_of(L"\\/");
    std::wstring name = (pos != std::wstring::npos) ? fullPath.substr(pos + 1) : fullPath;

    std::string result;
    for (wchar_t c : name) result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

bool isSystemWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return true;
    if (!IsWindowVisible(hwnd)) return true;
    if (isAltTabSwitcher(hwnd)) return true;
    if (isTaskbar(hwnd)) return true;
    if (isDesktop(hwnd)) return true;
    if (isStartMenu(hwnd)) return true;

    // Tooltips, menus, etc.
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        // If it has no owner and is a tool window, likely system
        if (!GetWindow(hwnd, GW_OWNER)) return true;
    }

    return false;
}

bool isAltTabSwitcher(HWND hwnd) {
    auto cls = getWindowClassName(hwnd);
    // Win10 Alt+Tab
    if (cls.find(L"TaskSwitcher") != std::wstring::npos) return true;
    // Win11 Alt+Tab / Snap
    if (cls.find(L"TopLevelWindowForOverflow") != std::wstring::npos) return true;
    if (cls.find(L"XamlIsland") != std::wstring::npos) return true;
    // Generic system popup overlays
    if (cls == L"MultitaskingViewFrame") return true;
    return false;
}

bool isTaskbar(HWND hwnd) {
    auto cls = getWindowClassName(hwnd);
    return cls == L"Shell_TrayWnd"
        || cls == L"Shell_SecondaryTrayWnd";
}

bool isDesktop(HWND hwnd) {
    auto cls = getWindowClassName(hwnd);
    return cls == L"Progman" || cls == L"WorkerW";
}

bool isStartMenu(HWND hwnd) {
    auto cls = getWindowClassName(hwnd);
    return cls.find(L"Windows.UI.Core.CoreWindow") != std::wstring::npos
        || cls == L"ImmersiveLauncher"
        || cls == L"SearchHost";
}

bool isFullscreenWindow(HWND hwnd) {
    RECT r;
    if (!GetWindowRect(hwnd, &r)) return false;
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    return (w >= screenW && h >= screenH);
}

bool isUWPWindow(HWND hwnd) {
    auto cls = getWindowClassName(hwnd);
    return cls == L"ApplicationFrameWindow";
}

bool isCloakedWindow(HWND hwnd) {
    // Windows 8+ DWM cloaking — hidden but not minimized
    using DwmGetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, PVOID, DWORD);
    static auto pfn = reinterpret_cast<DwmGetWindowAttributeFn>(
        GetProcAddress(GetModuleHandleW(L"dwmapi.dll"), "DwmGetWindowAttribute"));
    if (!pfn) return false;
    int cloaked = 0;
    pfn(hwnd, 14 /*DWMWA_CLOAKED*/, &cloaked, sizeof(cloaked));
    return cloaked != 0;
}

RECT getActualWindowRect(HWND hwnd, bool clientArea) {
    RECT r = {};

    if (isUWPWindow(hwnd)) {
        // UWP windows are wrapped in ApplicationFrameWindow;
        // the actual content is a child
        HWND child = FindWindowExW(hwnd, nullptr, L"Windows.UI.Core.CoreWindow", nullptr);
        if (child && IsWindowVisible(child)) {
            hwnd = child;
        }
    }

    if (clientArea) {
        GetClientRect(hwnd, &r);
        POINT tl = { r.left, r.top };
        ClientToScreen(hwnd, &tl);
        r.left = tl.x;
        r.top = tl.y;
        POINT br = { r.right, r.bottom };
        ClientToScreen(hwnd, &br);
        r.right = br.x;
        r.bottom = br.y;
    } else {
        GetWindowRect(hwnd, &r);
    }

    return r;
}

} // namespace Adaptation
