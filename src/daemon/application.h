#pragma once

#include <Windows.h>
#include <string>
#include <memory>
#include "../common/config_manager.h"
#include "focus_monitor.h"
#include "key_tracker.h"
#include "trigger_detector.h"
#include "mouse_controller.h"

#define TIMER_SMOOTH_MOVE      1
#define TIMER_POLL_WATCH       2
#define TIMER_DELAYED_MOVE     3

class Application {
public:
    Application();
    ~Application();

    int run(HINSTANCE hInstance, const std::wstring& configPath);

private:
    ConfigManager m_config;
    MouseController m_mouse;
    std::unique_ptr<FocusMonitor> m_focusMonitor;
    std::unique_ptr<KeyTracker> m_keyTracker;
    std::unique_ptr<TriggerDetector> m_triggerDetector;

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;

    // Hot-reload: poll config file modification time
    std::wstring m_configPath;
    FILETIME m_lastConfigTime = {};

    // Delayed move state
    HWND  m_delayedTarget = nullptr;
    bool  m_delayedClientArea = false;

    void setupModules();
    void checkConfigReload();
    void executeMove(HWND target);

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static constexpr const wchar_t* WINDOW_CLASS = L"MouseFocusDaemon_Hidden";
    static constexpr const wchar_t* MUTEX_NAME   = L"Global\\MouseFocusScript_Instance";
};

extern Application* g_application;
