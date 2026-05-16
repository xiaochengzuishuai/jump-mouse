#pragma once

#include <Windows.h>
#include <chrono>
#include "../common/config_manager.h"
#include "mouse_controller.h"
#include "key_tracker.h"
#include <functional>

class TriggerDetector {
public:
    using MoveAction = std::function<void(HWND)>;

    TriggerDetector(ConfigManager& cfg, KeyTracker& keys, MouseController& mouse);

    void onFocusChange(HWND hwnd);
    void onAltPressed();
    void onAltReleased();
    void setMoveAction(MoveAction action) { m_moveAction = std::move(action); }

private:
    ConfigManager&  m_config;
    KeyTracker&     m_keys;
    MouseController& m_mouse;
    MoveAction      m_moveAction;

    // Set when a valid window gets focus while Alt IS held
    HWND m_candidateHwnd = nullptr;

    // Post-release window: set when Alt released but foreground change
    // hasn't arrived yet (typical for hold-and-cycle Alt+Tab).
    bool m_waitingForTarget = false;
    std::chrono::steady_clock::time_point m_altReleaseTime;
    static constexpr int POST_RELEASE_WINDOW_MS = 600;

    bool isExcluded(HWND hwnd);
    bool shouldTrigger(HWND hwnd);
    void executeAction(HWND target);
};
