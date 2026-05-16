#include "mouse_controller.h"
#include "../common/adaptation_layer.h"
#include "../common/logger.h"
#include <cmath>
#include <format>

WindowCenter MouseController::calculateCenter(HWND hwnd, bool clientArea) const {
    RECT r = Adaptation::getActualWindowRect(hwnd, clientArea);
    return { (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
}

void MouseController::moveInstant(HWND hwnd, bool clientArea) {
    auto center = calculateCenter(hwnd, clientArea);
    if (center.x < -16000 || center.x > 32000 || center.y < -16000 || center.y > 32000) {
        LOG_WARN("Window center out of bounds, skipping move");
        return;
    }
    SetCursorPos(center.x, center.y);
    LOG_DEBUG(std::format("Moved cursor to ({}, {})", center.x, center.y));
}

double MouseController::easeOutCubic(double t) {
    double u = 1.0 - t;
    return 1.0 - u * u * u;
}

void MouseController::startSmooth(HWND targetHwnd, bool clientArea) {
    POINT cur;
    GetCursorPos(&cur);
    m_animFrom = cur;
    auto center = calculateCenter(targetHwnd, clientArea);
    m_animTo = { center.x, center.y };
    m_animStart = std::chrono::steady_clock::now();
    m_animActive = true;
}

bool MouseController::smoothTick() {
    if (!m_animActive) return true;
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(now - m_animStart).count();
    double t = elapsed / m_animDurationMs;
    if (t >= 1.0) {
        SetCursorPos(m_animTo.x, m_animTo.y);
        m_animActive = false;
        LOG_DEBUG(std::format("Smooth move completed at ({}, {})", m_animTo.x, m_animTo.y));
        return true;
    }
    double eased = easeOutCubic(t);
    LONG x = m_animFrom.x + static_cast<LONG>((m_animTo.x - m_animFrom.x) * eased);
    LONG y = m_animFrom.y + static_cast<LONG>((m_animTo.y - m_animFrom.y) * eased);
    SetCursorPos(x, y);
    return false;
}

// ===================== Cursor Highlight =====================

void MouseController::setBit(std::vector<BYTE>& plane, int stride, int x, int y, bool on) {
    int off = y * stride + x / 8;
    int bit = 7 - (x % 8);
    if (on) plane[off] |= (1U << bit);
    else    plane[off] &= ~(1U << bit);
}

// Shape definition: name, key, system cursor resource ID (nullptr = custom colored)
struct CursorShapeDef { LPCWSTR resId; };

static CursorShapeDef getShapeDef(const std::string& key) {
    if (key == "arrow")   return { IDC_ARROW };
    if (key == "hand")    return { IDC_HAND };
    if (key == "ibeam")   return { IDC_IBEAM };
    if (key == "cross")   return { IDC_CROSS };
    if (key == "sizeall") return { IDC_SIZEALL };
    if (key == "wait")    return { IDC_WAIT };
    return { nullptr }; // custom (circle/square)
}

static bool shapeHit(const std::string& shape, int x, int y, int r) {
    int dx = x - r, dy = y - r;
    if (shape == "circle")  return dx*dx + dy*dy <= r*r;
    if (shape == "square")  return abs(dx) <= r && abs(dy) <= r;
    return false;
}

HCURSOR MouseController::createColorCursor(int size, const std::string& shape, COLORREF rgb) {
    // System cursor: just copy the system cursor
    auto def = getShapeDef(shape);
    if (def.resId) {
        HCURSOR hSys = LoadCursor(nullptr, def.resId);
        return hSys ? CopyCursor(hSys) : nullptr;
    }

    // Custom colored shape
    int r = size / 2;
    int andStride = ((size + 15) / 16) * 2;
    std::vector<BYTE> andBits(andStride * size, 0xFF);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(nullptr);
    BYTE* pColor = nullptr;
    HBITMAP hbmColor = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&pColor, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!hbmColor || !pColor) { if (hbmColor) DeleteObject(hbmColor); return nullptr; }

    int rowBytes = size * 4;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (shapeHit(shape, x, y, r)) {
                setBit(andBits, andStride, x, y, false);
                int off = y * rowBytes + x * 4;
                pColor[off+0] = GetBValue(rgb);
                pColor[off+1] = GetGValue(rgb);
                pColor[off+2] = GetRValue(rgb);
                pColor[off+3] = 0x00;
            }
        }
    }

    HBITMAP hbmMask = CreateBitmap(size, size, 1, 1, andBits.data());
    ICONINFO ii = {}; ii.fIcon = FALSE; ii.xHotspot = r; ii.yHotspot = r;
    ii.hbmMask = hbmMask; ii.hbmColor = hbmColor;
    HCURSOR hCursor = CreateIconIndirect(&ii);
    DeleteObject(hbmMask); DeleteObject(hbmColor);
    return hCursor;
}

HCURSOR createPreviewCursor(int size, const std::string& shape, COLORREF color) {
    return MouseController::createColorCursor(size, shape, color);
}

bool MouseController::applyHighlight() {
    if (!m_highlight || m_highlightActive) return false;

    // Save original arrow cursor (CopyImage makes an independent copy)
    m_originalCursor = (HCURSOR)CopyImage(
        LoadCursor(nullptr, IDC_ARROW), IMAGE_CURSOR, 0, 0, LR_COPYFROMRESOURCE);
    if (!m_originalCursor) {
        LOG_DEBUG("  Could not save original cursor");
        return false;
    }

    HCURSOR hNew = nullptr;
    if (!m_highlightCustomFile.empty()) {
        std::wstring wfile(m_highlightCustomFile.begin(), m_highlightCustomFile.end());
        hNew = (HCURSOR)LoadImageW(nullptr, wfile.c_str(), IMAGE_CURSOR,
            m_highlightSize, m_highlightSize, LR_LOADFROMFILE);
    }
    if (!hNew) {
        hNew = createColorCursor(m_highlightSize, m_highlightShape,
            static_cast<COLORREF>(m_highlightColor));
    }
    if (!hNew) {
        LOG_DEBUG("  Could not create highlight cursor");
        if (m_originalCursor) { DestroyCursor(m_originalCursor); m_originalCursor = nullptr; }
        return false;
    }

    if (!SetSystemCursor(hNew, 32512)) {  // OCR_NORMAL
        LOG_DEBUG("  SetSystemCursor failed (may need elevation)");
        DestroyCursor(hNew);
        DestroyCursor(m_originalCursor);
        m_originalCursor = nullptr;
        return false;
    }

    // hNew ownership transferred to system. m_originalCursor holds saved copy.
    m_highlightActive = true;
    GetCursorPos(&m_highlightPos);
    LOG_DEBUG(std::format("  Highlight applied: yellow circle {}px at ({},{})",
        m_highlightSize, m_highlightPos.x, m_highlightPos.y));
    return true;
}

bool MouseController::pollHighlightRestore() {
    if (!m_highlightActive) return true;

    POINT cur;
    GetCursorPos(&cur);
    int dx = cur.x - m_highlightPos.x;
    int dy = cur.y - m_highlightPos.y;

    if (dx * dx + dy * dy > 4) {
        LOG_DEBUG(std::format("  Mouse moved — restoring cursor (d={},{})", dx, dy));
        forceRestoreCursor();
        return true;
    }
    return false;
}

void MouseController::forceRestoreCursor() {
    if (!m_highlightActive) return;
    m_highlightActive = false;

    if (m_originalCursor) {
        HCURSOR hRestore = CopyCursor(m_originalCursor);
        if (hRestore) {
            SetSystemCursor(hRestore, 32512);
        }
        DestroyCursor(m_originalCursor);
        m_originalCursor = nullptr;
    }
    LOG_DEBUG("  Cursor restored to original");
}
