#pragma once

#include <Windows.h>
#include <chrono>
#include <vector>

struct WindowCenter { LONG x, y; };

class MouseController {
public:
    WindowCenter calculateCenter(HWND hwnd, bool clientArea) const;

    void moveInstant(HWND hwnd, bool clientArea);

    void setSmoothDuration(int ms) { m_animDurationMs = ms; }
    void startSmooth(HWND targetHwnd, bool clientArea);
    bool smoothTick();

    // ---- Cursor highlight ----
    void enableHighlight(bool on) { m_highlight = on; }
    void setHighlightSize(int px) { m_highlightSize = px; }
    bool isHighlightActive() const { return m_highlightActive; }

    // Apply yellow enlarged cursor at current position; returns true if applied
    bool applyHighlight();

    // Poll: if mouse moved away from highlight position, restore original cursor
    // Returns true if restored
    bool pollHighlightRestore();

    // Force restore original cursor (e.g., on shutdown)
    void forceRestoreCursor();

    ~MouseController() { forceRestoreCursor(); }

private:
    POINT m_animFrom = {};
    POINT m_animTo = {};
    std::chrono::steady_clock::time_point m_animStart;
    int m_animDurationMs = 150;
    bool m_animActive = false;

    static double easeOutCubic(double t);

    // Highlight state
    bool m_highlight = false;
    int m_highlightSize = 48;
    bool m_highlightActive = false;
    POINT m_highlightPos = {};
    HCURSOR m_originalCursor = nullptr;

    static HCURSOR createColorCursor(int size);
    static void setBit(std::vector<BYTE>& plane, int stride, int x, int y, bool on);
};
