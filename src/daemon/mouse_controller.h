#pragma once

#include <Windows.h>
#include <chrono>
#include <vector>
#include <string>

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
    void setHighlightShape(const std::string& s) { m_highlightShape = s; }
    void setHighlightColor(int c) { m_highlightColor = c; }
    void setHighlightCustomFile(const std::string& f) { m_highlightCustomFile = f; }
    bool isHighlightActive() const { return m_highlightActive; }

    bool applyHighlight();
    bool pollHighlightRestore();
    void forceRestoreCursor();

    static HCURSOR createColorCursor(int size, const std::string& shape, COLORREF rgb);

    ~MouseController() { forceRestoreCursor(); }

private:
    POINT m_animFrom = {}, m_animTo = {};
    std::chrono::steady_clock::time_point m_animStart;
    int m_animDurationMs = 150;
    bool m_animActive = false;
    static double easeOutCubic(double t);

    bool m_highlight = false;
    std::string m_highlightShape = "arrow";
    int m_highlightColor = 0x0000FFFF;
    int m_highlightSize = 48;
    std::string m_highlightCustomFile;
    bool m_highlightActive = false;
    POINT m_highlightPos = {};

    // Registry backup (current user cursor setting, not system default)
    std::wstring m_originalArrowPath;

    // Temp .cur file for custom colored shapes
    std::wstring m_tempCurFile;

    static void setBit(std::vector<BYTE>& plane, int stride, int x, int y, bool on);
    static bool shapeHit(const std::string& shape, int x, int y, int r);
    std::wstring resolveCursorPath() const;
    bool writeCurFile(const std::wstring& path);
    static std::wstring registryCursorPath(const wchar_t* name);
    static void setRegistryCursorPath(const wchar_t* name, const std::wstring& path);
    static void notifyCursorChange();
};

HCURSOR createPreviewCursor(int size, const std::string& shape, COLORREF color);
