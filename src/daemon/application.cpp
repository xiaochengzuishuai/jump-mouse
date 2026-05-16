#include "application.h"
#include "../common/logger.h"
#include "../common/adaptation_layer.h"
#include <format>

Application* g_application = nullptr;
FocusMonitor* g_focusMonitor = nullptr;  // used by focus_monitor.cpp static callback

Application::Application()
    : m_focusMonitor(std::make_unique<FocusMonitor>())
    , m_keyTracker(std::make_unique<KeyTracker>())
    , m_triggerDetector(std::make_unique<TriggerDetector>(m_config, *m_keyTracker, m_mouse))
{}

Application::~Application() {
    g_application = nullptr;
    g_focusMonitor = nullptr;
}

int Application::run(HINSTANCE hInstance, const std::wstring& configPath) {
    m_hInstance = hInstance;
    m_configPath = configPath;

    if (!m_config.load(configPath)) {
        LOG_WARN("Failed to load config, using defaults");
    }

    // Apply initial log settings
    Logger::instance().setLevel(levelFromString(m_config.get().logLevel));
    if (!m_config.get().logFile.empty()) {
        std::wstring wlog(m_config.get().logFile.begin(), m_config.get().logFile.end());
        Logger::instance().setFile(wlog);
    }

    // Single instance
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        LOG_ERROR("Another instance is already running");
        return 1;
    }

    // Record initial config file time for hot-reload detection
    HANDLE hFile = CreateFileW(configPath.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        GetFileTime(hFile, nullptr, nullptr, &m_lastConfigTime);
        CloseHandle(hFile);
    }

    // Register hidden window class
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = WINDOW_CLASS;
    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(0, WINDOW_CLASS, L"", 0,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, hInstance, nullptr);
    if (!m_hwnd) {
        LOG_ERROR("Failed to create hidden window");
        CloseHandle(hMutex);
        return 1;
    }

    g_application = this;
    setupModules();

    // Poll config for changes every 2 seconds
    SetTimer(m_hwnd, TIMER_POLL_WATCH, 2000, nullptr);

    LOG_INFO("Daemon started");

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    LOG_INFO("Daemon shutting down");
    CloseHandle(hMutex);
    return 0;
}

void Application::setupModules() {
    g_focusMonitor = m_focusMonitor.get();

    m_focusMonitor->start([this](HWND hwnd) {
        if (m_triggerDetector) m_triggerDetector->onFocusChange(hwnd);
    });

    m_keyTracker->start(
        [this]() { if (m_triggerDetector) m_triggerDetector->onAltPressed(); },
        [this]() { if (m_triggerDetector) m_triggerDetector->onAltReleased(); }
    );

    m_triggerDetector->setMoveAction([this](HWND target) {
        executeMove(target);
    });
}

void Application::executeMove(HWND target) {
    const auto& cfg = m_config.get();

    if (cfg.moveDelayMs > 0) {
        // Delay move to let minimized/restoring windows finish animation
        m_delayedTarget = target;
        m_delayedClientArea = (cfg.targetArea == "client_rect");
        SetTimer(m_hwnd, TIMER_DELAYED_MOVE, cfg.moveDelayMs, nullptr);
        LOG_DEBUG(std::format("Move delayed by {}ms", cfg.moveDelayMs));
        return;
    }

    bool clientArea = (cfg.targetArea == "client_rect");
    if (cfg.mouseMode == "instant") {
        m_mouse.moveInstant(target, clientArea);
    } else {
        m_mouse.setSmoothDuration(cfg.smoothDurationMs);
        m_mouse.startSmooth(target, clientArea);
        SetTimer(m_hwnd, TIMER_SMOOTH_MOVE, 10, nullptr);
    }
}

void Application::checkConfigReload() {
    HANDLE hFile = CreateFileW(m_configPath.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    FILETIME ft;
    if (GetFileTime(hFile, nullptr, nullptr, &ft)) {
        if (CompareFileTime(&ft, &m_lastConfigTime) != 0) {
            m_lastConfigTime = ft;
            CloseHandle(hFile);

            // Small delay to let writer finish
            Sleep(100);

            auto path = m_config.loadedPath();
            if (m_config.load(path)) {
                Logger::instance().setLevel(levelFromString(m_config.get().logLevel));
                if (!m_config.get().logFile.empty()) {
                    std::wstring wlog(m_config.get().logFile.begin(), m_config.get().logFile.end());
                    Logger::instance().setFile(wlog);
                }
                LOG_INFO("Config reloaded");
            }
            return;
        }
    }
    CloseHandle(hFile);
}

LRESULT CALLBACK Application::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!g_application) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_TIMER:
        if (wParam == TIMER_SMOOTH_MOVE) {
            bool done = g_application->m_mouse.smoothTick();
            if (done) KillTimer(hwnd, TIMER_SMOOTH_MOVE);
        } else if (wParam == TIMER_DELAYED_MOVE) {
            KillTimer(hwnd, TIMER_DELAYED_MOVE);
            HWND target = g_application->m_delayedTarget;
            bool clientArea = g_application->m_delayedClientArea;
            g_application->m_delayedTarget = nullptr;
            if (target && IsWindow(target)) {
                const auto& cfg = g_application->m_config.get();
                if (cfg.mouseMode == "instant") {
                    g_application->m_mouse.moveInstant(target, clientArea);
                } else {
                    g_application->m_mouse.setSmoothDuration(cfg.smoothDurationMs);
                    g_application->m_mouse.startSmooth(target, clientArea);
                    SetTimer(hwnd, TIMER_SMOOTH_MOVE, 10, nullptr);
                }
            }
        } else if (wParam == TIMER_POLL_WATCH) {
            g_application->checkConfigReload();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
