// Mouse Focus Daemon — background process
// Listens for Alt+Tab window switches and moves cursor to window center.
//
// Usage:
//   mouse_focus_daemon.exe                    — run with config.json in exe directory
//   mouse_focus_daemon.exe --config <path>    — run with custom config path
//   mouse_focus_daemon.exe --help             — show usage

#include "application.h"
#include "../common/logger.h"
#include <string>
#include <filesystem>

static std::wstring getExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::filesystem::path p(path);
    return p.parent_path().wstring();
}

static void printUsage() {
    MessageBoxW(nullptr,
        L"Mouse Focus Daemon\n\n"
        L"Usage:\n"
        L"  mouse_focus_daemon.exe [options]\n\n"
        L"Options:\n"
        L"  --config <path>   Specify config file path\n"
        L"  --help            Show this message\n\n"
        L"The daemon runs as a background process with no window.\n"
        L"Use mouse_focus_config.exe to change settings.",
        L"Mouse Focus Daemon", MB_OK | MB_ICONINFORMATION);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::wstring configPath = getExeDir() + L"\\config.json";
    bool showHelp = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--config" || arg == L"-c") {
            if (i + 1 < argc) {
                configPath = argv[++i];
            }
        } else if (arg == L"--help" || arg == L"-h") {
            showHelp = true;
        }
    }
    LocalFree(argv);

    if (showHelp) {
        printUsage();
        return 0;
    }

    Application app;
    return app.run(hInstance, configPath);
}
