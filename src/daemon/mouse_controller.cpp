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
