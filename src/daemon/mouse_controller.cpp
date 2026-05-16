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

HCURSOR MouseController::createColorCursor(int size) {
    int andStride = ((size + 15) / 16) * 2;
    std::vector<BYTE> andPlane(andStride * size, 0xFF);
    std::vector<BYTE> xorPlane(size * size * 4, 0);

    int r = size / 2;
    int r2 = r * r;

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int dx = x - r;
            int dy = y - r;
            if (dx * dx + dy * dy <= r2) {
                setBit(andPlane, andStride, x, y, false);
                int off = (y * size + x) * 4;
                xorPlane[off + 0] = 0x00; // B
                xorPlane[off + 1] = 0xFF; // G
                xorPlane[off + 2] = 0xFF; // R
                xorPlane[off + 3] = 0x00;
            }
        }
    }

    return CreateCursor(GetModuleHandleW(nullptr), r, r, size, size,
        andPlane.data(), xorPlane.data());
}

bool MouseController::applyHighlight() {
    if (!m_highlight || m_highlightActive) return false;

    // Save original arrow cursor (CopyImage makes an independent copy)
    m_originalCursor = (HCURSOR)CopyImage(
        LoadCursorW(nullptr, IDC_ARROW), IMAGE_CURSOR, 0, 0, LR_COPYFROMRESOURCE);
    if (!m_originalCursor) {
        LOG_DEBUG("  Could not save original cursor");
        return false;
    }

    HCURSOR hNew = createColorCursor(m_highlightSize);
    if (!hNew) {
        LOG_DEBUG("  Could not create highlight cursor");
        DestroyCursor(m_originalCursor);
        m_originalCursor = nullptr;
        return false;
    }

    if (!SetSystemCursor(hNew, OCR_NORMAL)) {
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
            SetSystemCursor(hRestore, OCR_NORMAL);
        }
        DestroyCursor(m_originalCursor);
        m_originalCursor = nullptr;
    }
    LOG_DEBUG("  Cursor restored to original");
}
