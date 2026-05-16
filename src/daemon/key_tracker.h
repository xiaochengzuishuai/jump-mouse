#pragma once

#include <Windows.h>
#include <functional>
#include <chrono>
#include <atomic>

class KeyTracker {
public:
    using AltKeyCallback = std::function<void()>;

    void start(AltKeyCallback onPress, AltKeyCallback onRelease);
    void stop();
    bool isAltDown() const { return m_altDown.load(); }

private:
    HHOOK m_hook = nullptr;
    std::atomic<bool> m_altDown{false};
    std::chrono::steady_clock::time_point m_altUpTime;
    AltKeyCallback m_onPress;
    AltKeyCallback m_onRelease;

    static LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
};

extern KeyTracker* g_keyTracker;
