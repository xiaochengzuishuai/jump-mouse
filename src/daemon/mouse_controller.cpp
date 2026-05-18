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
    auto c = calculateCenter(hwnd, clientArea);
    if (c.x < -16000 || c.x > 32000 || c.y < -16000 || c.y > 32000) return;
    SetCursorPos(c.x, c.y);
    LOG_DEBUG(std::format("Moved cursor to ({}, {})", c.x, c.y));
}

double MouseController::easeOutCubic(double t) { double u = 1.0 - t; return 1.0 - u*u*u; }

void MouseController::startSmooth(HWND target, bool ca) {
    POINT cur; GetCursorPos(&cur); m_animFrom = cur;
    auto center = calculateCenter(target, ca);
    m_animTo = { center.x, center.y };
    m_animStart = std::chrono::steady_clock::now(); m_animActive = true;
}

bool MouseController::smoothTick() {
    if (!m_animActive) return true;
    auto now = std::chrono::steady_clock::now();
    double t = std::chrono::duration<double,std::milli>(now - m_animStart).count() / m_animDurationMs;
    if (t >= 1.0) { SetCursorPos(m_animTo.x, m_animTo.y); m_animActive = false; return true; }
    double e = easeOutCubic(t);
    SetCursorPos(m_animFrom.x + (LONG)((m_animTo.x - m_animFrom.x)*e),
                 m_animFrom.y + (LONG)((m_animTo.y - m_animFrom.y)*e));
    return false;
}
