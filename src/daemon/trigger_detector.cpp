#include "trigger_detector.h"
#include "../common/adaptation_layer.h"
#include "../common/logger.h"
#include <algorithm>
#include <format>

static std::string w2s(const std::wstring& w) {
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return {};
    std::string s(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), len, nullptr, nullptr);
    return s;
}

TriggerDetector::TriggerDetector(ConfigManager& cfg, KeyTracker& keys, MouseController& mouse)
    : m_config(cfg), m_keys(keys), m_mouse(mouse)
{}

bool TriggerDetector::isExcluded(HWND hwnd) {
    const auto& cfg = m_config.get();
    auto procName = Adaptation::getProcessName(hwnd);
    for (auto& ex : cfg.excludedProcesses) {
        std::string lower = ex;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (procName == lower) return true;
    }
    auto clsName = Adaptation::getWindowClassName(hwnd);
    for (auto& ex : cfg.excludedClasses) {
        auto lowerCls = clsName;
        std::transform(lowerCls.begin(), lowerCls.end(), lowerCls.begin(), ::towlower);
        auto lowerEx = ex;
        std::transform(lowerEx.begin(), lowerEx.end(), lowerEx.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::wstring wLowerEx(lowerEx.begin(), lowerEx.end());
        if (lowerCls.find(wLowerEx) != std::wstring::npos) return true;
    }
    return false;
}

bool TriggerDetector::shouldTrigger(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        LOG_DEBUG("  skip: invalid hwnd");
        return false;
    }
    if (!m_config.get().enabled) {
        LOG_DEBUG("  skip: disabled");
        return false;
    }
    if (Adaptation::isSystemWindow(hwnd)) {
        LOG_DEBUG(std::format("  skip: system window class={}",
            w2s(Adaptation::getWindowClassName(hwnd))));
        return false;
    }
    if (Adaptation::isCloakedWindow(hwnd)) {
        LOG_DEBUG("  skip: cloaked");
        return false;
    }
    if (isExcluded(hwnd)) {
        LOG_DEBUG(std::format("  skip: excluded process={}",
            Adaptation::getProcessName(hwnd)));
        return false;
    }
    return true;
}

void TriggerDetector::onFocusChange(HWND hwnd) {
    const auto& cfg = m_config.get();

    LOG_DEBUG(std::format("FocusChange hwnd=0x{:X} proc={} class={} triggerMode={}",
        reinterpret_cast<uintptr_t>(hwnd),
        Adaptation::getProcessName(hwnd),
        w2s(Adaptation::getWindowClassName(hwnd)),
        cfg.triggerMode));

    // Post-release window check FIRST (before shouldTrigger, because
    // the target window after Alt release is a normal user window).
    if (m_waitingForTarget) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_altReleaseTime).count();
        LOG_DEBUG(std::format("  post-release mode, elapsed={}ms", elapsed));

        if (elapsed < POST_RELEASE_WINDOW_MS) {
            if (!Adaptation::isAltTabSwitcher(hwnd) && shouldTrigger(hwnd)) {
                LOG_DEBUG(std::format("  post-release target: {}",
                    Adaptation::getProcessName(hwnd)));
                m_waitingForTarget = false;
                executeAction(hwnd);
                return;
            }
            // Still waiting for a real window
            return;
        }
        // Window expired
        LOG_DEBUG("  post-release window expired");
        m_waitingForTarget = false;
    }

    // Check if this window should be considered at all
    if (!shouldTrigger(hwnd)) return;

    if (cfg.triggerMode == "any_focus_change") {
        LOG_DEBUG("  any_focus_change -> execute");
        executeAction(hwnd);
        return;
    }

    // alt_tab_only mode: only accept if Alt is physically held
    bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    LOG_DEBUG(std::format("  alt_tab_only mode, AltPhysicallyDown={}", altDown));

    if (altDown) {
        // If this is the switcher itself, ignore it but note we're in a session
        if (Adaptation::isAltTabSwitcher(hwnd)) {
            LOG_DEBUG("  Alt+Tab switcher active, waiting for target...");
            return;
        }
        m_candidateHwnd = hwnd;
        LOG_DEBUG(std::format("  candidate saved: {}",
            Adaptation::getProcessName(hwnd)));
    }
}

void TriggerDetector::onAltPressed() {
    LOG_DEBUG("Alt pressed");
    m_candidateHwnd = nullptr;
    m_waitingForTarget = false;
}

void TriggerDetector::onAltReleased() {
    LOG_DEBUG(std::format("Alt released, candidate=0x{:X}",
        reinterpret_cast<uintptr_t>(m_candidateHwnd)));

    // Priority 1: candidate captured while Alt was held
    if (m_candidateHwnd && shouldTrigger(m_candidateHwnd)) {
        LOG_DEBUG(std::format("  executing candidate: {}",
            Adaptation::getProcessName(m_candidateHwnd)));
        auto target = m_candidateHwnd;
        m_candidateHwnd = nullptr;
        executeAction(target);
        return;
    }
    m_candidateHwnd = nullptr;

    // Priority 2: no candidate yet — the foreground change will arrive
    // shortly after Alt release (the system closes the switcher, then
    // sets the target window as foreground). Enter post-release wait mode.
    LOG_DEBUG("  entering post-release wait mode");
    m_waitingForTarget = true;
    m_altReleaseTime = std::chrono::steady_clock::now();
}

void TriggerDetector::executeAction(HWND target) {
    if (m_moveAction) {
        m_moveAction(target);
    }
}
