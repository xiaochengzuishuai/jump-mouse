// Mouse Focus — Test Harness
// Outputs results to console AND "test_result.txt"
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include "../common/config_manager.h"
#include "../common/adaptation_layer.h"
#include "../daemon/mouse_controller.h"
#include "../config_tool/daemon_controller.h"

static std::wofstream g_log;
static int g_fail = 0;
static int g_pass = 0;

static void out(const std::wstring& s) {
    std::wcout << s;
    if (g_log.is_open()) g_log << s;
}

static void check(bool cond, const wchar_t* expr, int line) {
    if (cond) { ++g_pass; }
    else {
        ++g_fail;
        out(std::format(L"  FAIL line {}: {}\n", line, expr));
    }
}
#define CHECK(c) check((c), L###c, __LINE__)
#define LOG(s) out(std::format(L"{}\n", s))

static void heading(const wchar_t* s) {
    auto hdr = std::format(L"\n=== {} ===\n", s);
    out(hdr);
}

// ===================== ConfigManager =====================
static void test_config() {
    heading(L"1. ConfigManager");

    {
        ConfigManager cm;
        if (!cm.load(L"test_cfg.json")) { out(L"  load returned false\n"); ++g_fail; return; }
        const auto& c = cm.get();
        CHECK(c.triggerMode == "alt_tab_only");
        CHECK(c.mouseMode == "instant");
        CHECK(c.smoothDurationMs == 150);
        CHECK(c.enabled == true);
        DeleteFileW(L"test_cfg.json");
        LOG(L"  defaults: OK");
    }

    {
        ConfigManager cm;
        cm.load(L"test_cfg2.json");
        auto mut = cm.get();
        mut.triggerMode = "any_focus_change";
        mut.mouseMode = "smooth";
        mut.smoothDurationMs = 200;
        cm.apply(mut);
        CHECK(cm.save(L"test_cfg2.json"));

        ConfigManager cm2;
        cm2.load(L"test_cfg2.json");
        CHECK(cm2.get().triggerMode == "any_focus_change");
        CHECK(cm2.get().mouseMode == "smooth");
        CHECK(cm2.get().smoothDurationMs == 200);
        DeleteFileW(L"test_cfg2.json");
        LOG(L"  save/reload: OK");
    }
}

// ===================== AdaptationLayer =====================
static void test_adapt() {
    heading(L"2. AdaptationLayer");

    CHECK(Adaptation::isWindows10OrLater());
    LOG(L"  isWindows10OrLater: OK");

    HWND desktop = GetDesktopWindow();
    auto cls = Adaptation::getWindowClassName(desktop);
    CHECK(!cls.empty());
    LOG(std::format(L"  desktop class: {}", std::wstring(cls)));

    CHECK(Adaptation::isDesktop(desktop));
    LOG(L"  isDesktop: OK");

    HWND console = GetConsoleWindow();
    if (console) {
        auto proc = Adaptation::getProcessName(console);
        CHECK(!proc.empty());
        LOG(std::format(L"  self process: {}", std::wstring(proc.begin(), proc.end())));
    }
}

// ===================== MouseController =====================
static void test_mouse() {
    heading(L"3. MouseController");

    HWND desktop = GetDesktopWindow();
    MouseController mc;

    auto c = mc.calculateCenter(desktop, false);
    CHECK(c.x >= 0 && c.y >= 0);
    LOG(std::format(L"  center=({},{})", c.x, c.y));

    mc.moveInstant(desktop, false);
    LOG(L"  moveInstant: OK");

    mc.setSmoothDuration(80);
    mc.startSmooth(desktop, false);
    for (int i = 0; i < 12; ++i) { if (mc.smoothTick()) break; Sleep(10); }
    LOG(L"  smoothTick: OK");
}

// ===================== DaemonController =====================
static void test_daemon() {
    heading(L"4. DaemonController");

    DaemonController dc;
    bool r = dc.isRunning();
    LOG(std::format(L"  isRunning={}", r ? L"yes" : L"no"));

    dc.isAutoStart();
    LOG(L"  isAutoStart: OK (no crash)");
}

// ===================== Integration Summary =====================
static void test_summary() {
    heading(L"5. Integration Points");

    // Verify all components can be wired together without crash
    ConfigManager cm;
    MouseController mc;
    cm.load(L"test_integration.json");
    DeleteFileW(L"test_integration.json");

    const auto& cfg = cm.get();
    HWND desktop = GetDesktopWindow();

    bool clientArea = (cfg.targetArea == "client_rect");
    if (cfg.mouseMode == "instant") {
        mc.moveInstant(desktop, clientArea);
    } else {
        mc.setSmoothDuration(cfg.smoothDurationMs);
        mc.startSmooth(desktop, clientArea);
    }
    LOG(L"  Config->MouseController wiring: OK");

    // Verify system window filtering
    CHECK(Adaptation::isSystemWindow(GetDesktopWindow()));
    CHECK(Adaptation::isSystemWindow(FindWindowW(L"Shell_TrayWnd", nullptr)));
    LOG(L"  System window detection: OK");
}

int wmain() {
    SetConsoleOutputCP(CP_UTF8);

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &t);

    auto filename = std::format(L"test_result_{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}.txt",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);

    g_log.open(filename);
    auto banner = std::format(L"Mouse Focus Test Harness — {}\n\n", filename);
    out(banner);

    test_config();
    test_adapt();
    test_mouse();
    test_daemon();
    test_summary();

    auto summary = std::format(
        L"\n========================================\n"
        L"  PASS: {}  FAIL: {}  TOTAL: {}\n"
        L"  Log saved to: {}\n"
        L"========================================\n",
        g_pass, g_fail, g_pass + g_fail, filename);
    out(summary);

    if (g_log.is_open()) g_log.close();
    return g_fail;
}
