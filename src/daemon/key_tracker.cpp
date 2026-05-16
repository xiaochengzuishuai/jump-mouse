#include "key_tracker.h"
#include "../common/logger.h"

KeyTracker* g_keyTracker = nullptr;

void KeyTracker::start(AltKeyCallback onPress, AltKeyCallback onRelease) {
    m_onPress = std::move(onPress);
    m_onRelease = std::move(onRelease);
    g_keyTracker = this;
    m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc,
        GetModuleHandleW(nullptr), 0);
    if (!m_hook) {
        LOG_ERROR("Failed to set keyboard hook");
    } else {
        LOG_INFO("KeyTracker started");
    }
}

void KeyTracker::stop() {
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    m_onPress = nullptr;
    m_onRelease = nullptr;
    g_keyTracker = nullptr;
    LOG_INFO("KeyTracker stopped");
}

LRESULT CALLBACK KeyTracker::keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && g_keyTracker) {
        auto& p = *reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool prev = g_keyTracker->m_altDown.load();

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (p.vkCode == VK_MENU || p.vkCode == VK_LMENU || p.vkCode == VK_RMENU) {
                if (!prev) {
                    g_keyTracker->m_altDown.store(true);
                    if (g_keyTracker->m_onPress) g_keyTracker->m_onPress();
                }
            }
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            if (p.vkCode == VK_MENU || p.vkCode == VK_LMENU || p.vkCode == VK_RMENU) {
                g_keyTracker->m_altDown.store(false);
                g_keyTracker->m_altUpTime = std::chrono::steady_clock::now();
                if (prev && g_keyTracker->m_onRelease) {
                    g_keyTracker->m_onRelease();
                }
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
