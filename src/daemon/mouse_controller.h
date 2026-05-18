#pragma once

#include <Windows.h>
#include <chrono>

struct WindowCenter { LONG x, y; };

class MouseController {
public:
    WindowCenter calculateCenter(HWND hwnd, bool clientArea) const;
    void moveInstant(HWND hwnd, bool clientArea);

    void setSmoothDuration(int ms) { m_animDurationMs = ms; }
    void startSmooth(HWND targetHwnd, bool clientArea);
    bool smoothTick();

private:
    POINT m_animFrom = {}, m_animTo = {};
    std::chrono::steady_clock::time_point m_animStart;
    int m_animDurationMs = 150;
    bool m_animActive = false;
    static double easeOutCubic(double t);
};
