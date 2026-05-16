#pragma once

#include <Windows.h>
#include <chrono>

struct WindowCenter { LONG x, y; };

class MouseController {
public:
    WindowCenter calculateCenter(HWND hwnd, bool clientArea) const;

    // Instant move to window center
    void moveInstant(HWND hwnd, bool clientArea);

    // Set animation duration
    void setSmoothDuration(int ms) { m_animDurationMs = ms; }

    // Start smooth animation — first call initializes, subsequent WM_TIMER ticks
    // drive the animation. Returns true when complete.
    void startSmooth(HWND targetHwnd, bool clientArea);
    bool smoothTick();

private:
    POINT m_animFrom = {};
    POINT m_animTo = {};
    std::chrono::steady_clock::time_point m_animStart;
    int m_animDurationMs = 150;
    bool m_animActive = false;

    static double easeOutCubic(double t);
};
