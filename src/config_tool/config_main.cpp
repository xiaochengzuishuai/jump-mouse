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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_UPDOWN_CLASS };
    InitCommonControlsEx(&icc);

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
