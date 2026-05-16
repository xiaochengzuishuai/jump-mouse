// Mouse Focus Config Tool — Win32 GUI with System Tray
#include "config_dialog.h"
#include <Windows.h>
#include <CommCtrl.h>
#include <filesystem>
#pragma comment(lib, "comctl32.lib")

static std::wstring getExeDir() {
    wchar_t path[MAX_PATH]; GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::filesystem::path p(path); return p.parent_path().wstring();
}
static std::wstring resolveConfigPath() {
    int argc = 0; auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring path = getExeDir() + L"\\config.json";
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if ((arg == L"--config" || arg == L"-c") && i + 1 < argc) path = argv[++i];
    }
    LocalFree(argv); return path;
}

// Custom canvas window for cursor preview
static LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));

        HCURSOR hCur = (HCURSOR)GetPropW(hwnd, L"CURSOR");
        if (hCur) {
            ICONINFO ii = {};
            if (GetIconInfo(hCur, &ii)) {
                BITMAP bm = {}; int curW = 32, curH = 32;
                if (ii.hbmColor && GetObjectW(ii.hbmColor, sizeof(bm), &bm))
                    { curW = bm.bmWidth; curH = bm.bmHeight; }
                else if (ii.hbmMask && GetObjectW(ii.hbmMask, sizeof(bm), &bm))
                    { curW = bm.bmWidth; curH = bm.bmHeight / 2; }
                int pad = 6, aw = rc.right - rc.left - pad*2, ah = rc.bottom - rc.top - pad*2;
                float s = 3.0f; if (curW > 0) s = min(s, (float)aw / curW);
                if (curH > 0) s = min(s, (float)ah / curH);
                int dw = (int)(curW * s), dh = (int)(curH * s);
                int x = (rc.right - rc.left - dw) / 2, y = (rc.bottom - rc.top - dh) / 2;
                DrawIconEx(hdc, x, y, hCur, dw, dh, 0, nullptr, DI_NORMAL);
                if (ii.hbmMask) DeleteObject(ii.hbmMask);
                if (ii.hbmColor) DeleteObject(ii.hbmColor);
            }
        }
        FrameRect(hdc, &rc, (HBRUSH)GetStockObject(GRAY_BRUSH));
        EndPaint(hwnd, &ps); return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_UPDOWN_CLASS };
    InitCommonControlsEx(&icc);

    // Register custom canvas class
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = CanvasWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"CursorCanvas";
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return 1;

    auto configPath = resolveConfigPath();
    ConfigDialog dlg(hInstance, configPath);
    HWND hwnd = dlg.create(nullptr);
    if (!hwnd) return 1;

    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    return 0;
}
