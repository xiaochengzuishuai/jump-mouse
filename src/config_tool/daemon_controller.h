#pragma once

#include <Windows.h>
#include <string>

class DaemonController {
public:
    bool isRunning() const;

    // Launch the daemon (jump_mouse_daemon.exe in the same directory)
    bool start();
    bool stop();

    bool setAutoStart(bool enable);
    bool isAutoStart() const;

private:
    static constexpr const wchar_t* MUTEX_NAME = L"Global\\JumpMouse_Instance";
    static constexpr const wchar_t* DAEMON_EXE  = L"jump_mouse_daemon.exe";

    std::wstring daemonPath() const;
    std::wstring exeDir() const;
    HWND findDaemonWindow() const;
};
