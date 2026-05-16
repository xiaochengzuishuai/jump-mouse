#include "focus_monitor.h"
#include "../common/logger.h"

void FocusMonitor::start(FocusChangeCallback cb) {
    m_callback = std::move(cb);
    m_hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr,
        eventProc,
        0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );
    if (!m_hook) {
        LOG_ERROR("Failed to set foreground event hook");
    } else {
        LOG_INFO("FocusMonitor started");
    }
}

void FocusMonitor::stop() {
    if (m_hook) {
        UnhookWinEvent(m_hook);
        m_hook = nullptr;
    }
    m_callback = nullptr;
    LOG_INFO("FocusMonitor stopped");
}

HWND FocusMonitor::currentForeground() const {
    return GetForegroundWindow();
}

void CALLBACK FocusMonitor::eventProc(
    HWINEVENTHOOK, DWORD, HWND hwnd,
    LONG idObject, LONG idChild,
    DWORD, DWORD)
{
    // Only care about window-level events
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;
    if (!hwnd || !IsWindow(hwnd)) return;

    // The hook was registered per-instance; we need to access the instance.
    // For simplicity, use a global accessor.
    // This is set by Application during initialization.
    extern FocusMonitor* g_focusMonitor;
    if (g_focusMonitor && g_focusMonitor->m_callback) {
        g_focusMonitor->m_callback(hwnd);
    }
}
