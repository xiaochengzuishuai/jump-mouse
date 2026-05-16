#pragma once

#include <Windows.h>
#include <string>

class DaemonController {
public:
    bool isRunning() const;

    // Launch the daemon (mouse_focus_daemon.exe in the same directory)
    bool start();
    bool stop();

    bool setAutoStart(bool enable);
    bool isAutoStart() const;

private:
    static constexpr const wchar_t* MUTEX_NAME = L"Global\\MouseFocusScript_Instance";
    static constexpr const wchar_t* DAEMON_EXE  = L"mouse_focus_daemon.exe";

    std::wstring daemonPath() const;
    std::wstring exeDir() const;
    HWND findDaemonWindow() const;
};
