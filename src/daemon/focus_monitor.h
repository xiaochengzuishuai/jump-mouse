#pragma once

#include <Windows.h>
#include <functional>

using FocusChangeCallback = std::function<void(HWND newForeground)>;

class FocusMonitor {
public:
    void start(FocusChangeCallback cb);
    void stop();
    HWND currentForeground() const;

private:
    HWINEVENTHOOK m_hook = nullptr;
    FocusChangeCallback m_callback;

    static void CALLBACK eventProc(
        HWINEVENTHOOK, DWORD event,
        HWND hwnd, LONG idObject, LONG idChild,
        DWORD, DWORD);
};
