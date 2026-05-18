# Jump Mouse — Design Document (V1.0)

> Version: Jump V1.0 (Stable) | Date: 2026-05-18 | Status: First Stable Release

---

## 1. Overview

**Goal**: On Windows 10/11 multi-monitor setups, automatically move the mouse cursor to the center of the newly focused window when the user Alt+Tabs.

**Architecture**: Dual executables
- `jump_mouse_daemon.exe` — Headless background daemon for window monitoring and cursor control
- `jump_mouse_config.exe` — Native Win32 GUI config tool, WYSIWYG settings editing

**Core constraints**:
- Daemon runs headless; config tool is a standalone GUI
- All behavior configurable via GUI
- Adaptive to Win10/Win11 differences
- Zero external runtime dependencies (Windows SDK + standard library only)

---

## 2. Architecture

### 2.1 High-Level Architecture

Two executables sharing a single config.json. The daemon reads config at startup and hot-reloads on changes. The GUI tool reads/writes config.json and controls the daemon lifecycle via CreateProcess / window messages.

### 2.2 Module Responsibilities

| Module | Responsibility | Used By |
|--------|---------------|---------|
| ConfigManager | JSON config read/write, validation, defaults | daemon + config tool |
| FocusMonitor | Monitor foreground window changes via SetWinEventHook | daemon only |
| KeyTracker | Track Alt key press/release via WH_KEYBOARD_LL | daemon only |
| TriggerDetector | Determine if window switch was triggered by Alt+Tab | daemon only |
| MouseController | Calculate window center + move cursor (instant/smooth) | daemon only |
| AdaptationLayer | Win10/11 differences, DPI, UWP detection | daemon + config tool |
| Logger | Optional debug logging | daemon + config tool |
| DaemonController | Start/stop/query daemon process | config tool only |
| ConfigDialog | Win32 dialog UI layout and logic | config tool only |

---

## 3. Core Flows

### 3.1 Alt+Tab Detection (default mode)

Three-phase detection:

1. **Alt pressed** — mark state Alt=true
2. **Foreground window change while Alt=true** — record HWND as candidate (filtering out system windows and the Alt+Tab switcher itself)
3. **Alt released** — if a candidate window exists and is still valid, execute mouse move to its center

System windows filtered: Desktop (Progman/WorkerW), Taskbar (Shell_TrayWnd), Alt+Tab switcher (TaskSwitcherWnd / TopLevelWindowForOverflowXamlIsland), Start menu (Windows.UI.Core.CoreWindow), invisible/invalid windows.

### 3.2 Mouse Movement

Two modes:
- **Instant**: SetCursorPos() to window center directly
- **Smooth**: Cubic ease-out interpolation driven by 10ms timer, non-blocking message pump

Optional moveDelayMs: waits before moving to allow minimized windows to complete their restore animation.

---

## 4. Configuration Schema

```jsonc
{
  "version": 1,
  "triggerMode": "alt_tab_only",
  "mouseMode": "smooth",
  "smoothDurationMs": 600,
  "moveDelayMs": 100,
  "enabled": true,
  "targetArea": "window_rect",
  "excludedProcesses": [],
  "excludedClasses": [],
  "logLevel": "none",
  "logFile": ""
}
```

---

## 5. Windows Version Adaptation

| Feature | Win10 | Win11 | Strategy |
|---------|-------|-------|----------|
| Alt+Tab window class | TaskSwitcherWnd | TopLevelWindowForOverflowXamlIsland | Fuzzy class name matching |
| DPI scaling | PerMonitorV2 | Same | Embedded manifest |
| UWP apps | ApplicationFrameWindow wrapper | Same | Get actual child window rect |

---

## 6. Smooth Animation Algorithm

Cubic ease-out curve (fast start, slow end):

```
t = elapsed / duration
eased = 1 - (1-t)^3
x(t) = from.x + (to.x - from.x) * eased
y(t) = from.y + (to.y - from.y) * eased
```

10ms timer drives each frame. Non-blocking message pump (SetTimer + WM_TIMER). New target interrupts current animation.

---

## 7. System Window Filtering

Windows that never trigger movement:

| Window | Detection |
|--------|-----------|
| Desktop (Progman/WorkerW) | Class name Progman / WorkerW |
| Taskbar | Class name Shell_TrayWnd |
| Alt+Tab switcher | Class name contains TaskSwitcher / TopLevelWindowForOverflow |
| Start menu (Win10) | Class name Windows.UI.Core.CoreWindow |
| Null HWND | hwnd == nullptr or !IsWindow(hwnd) |
| Invisible window | !IsWindowVisible(hwnd) |

---

## 8. Security and Edge Cases

| Scenario | Handling |
|----------|----------|
| Secure desktop (UAC) | Low-privilege hooks disabled across sessions |
| Fullscreen games | Configurable process exclusion |
| Multi-user sessions | Per-session independent instances |
| Unexpected exit | Hooks cleaned up by OS |

---

## 9. Build and Usage

```bash
cmake -B build -S .
cmake --build build --config Release
```

Output: `build/release/jump_mouse_daemon.exe`, `build/release/jump_mouse_config.exe`

### Usage

```bash
jump_mouse_daemon.exe
jump_mouse_daemon.exe --config "D:\my_config.json"
jump_mouse_config.exe
```

Single instance enforced via named mutex `Global\JumpMouse_Instance`.

---

## 10. Extension Points

1. Win+Tab support — reuse Win key tracking logic
2. Per-monitor binding — only trigger for windows on the current monitor
3. Custom animation curves — support multiple easing styles
4. Multi-language GUI — English/Chinese toggle
