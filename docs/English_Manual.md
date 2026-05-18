# Jump Mouse — Automatic Cursor Warp

**Automatically moves the mouse cursor to the center of the newly focused window when Alt+Tabbing.**

[中文文档](../README.md)

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Quick Start](#quick-start)
- [Configuration](#configuration)
- [Building from Source](#building-from-source)
- [Architecture](#architecture)
- [FAQ](#faq)
- [Changelog](#changelog)

---

## Overview

On multi-monitor Windows 10/11 setups, manually moving the mouse to the center of a window after Alt+Tabbing is tedious. Jump Mouse automates this:

1. You press Alt+Tab to switch windows
2. The cursor warps to the new window's center
3. You're ready to work immediately — no hunting for the cursor

**Zero runtime dependencies** — download and run, no additional installs required.

---

## Features

### Core Capabilities

| Feature | Description |
|---------|-------------|
| **Alt+Tab Detection** | Three-phase detection (Alt down → record candidate → Alt up triggers move) |
| **Any Focus Change** | Optional: trigger on any foreground window change |
| **Instant Move** | `SetCursorPos` warp with zero latency |
| **Smooth Animation** | Cubic ease-out interpolation for natural-looking movement |
| **Restore Delay** | Configurable delay for minimized windows to complete their restore animation |
| **System Window Filter** | Auto-filters taskbar, desktop, Alt+Tab switcher, and other non-user windows |
| **Process Exclusion** | Exclude specific processes (e.g. `devenv.exe`) from triggering moves |

### GUI Config Tool

- Native Win32 dialog — fast startup, low memory footprint
- Visual editing for all settings — no manual JSON editing needed
- System tray icon with right-click menu (start/stop/exit)
- Hot-reload: daemon picks up config changes within 2 seconds

### Cross-Version Compatibility

- Windows 10 1809+ and Windows 11 supported
- UWP app windows handled correctly
- PerMonitorV2 DPI awareness — works across mixed-DPI multi-monitor setups
- Both Win10 (`TaskSwitcherWnd`) and Win11 (`TopLevelWindowForOverflowXamlIsland`) Alt+Tab switchers correctly identified

---

## Quick Start

### Download & Run

1. Download the latest release from [GitHub Releases](https://github.com)
2. Extract to any folder — keep all files together
3. Run `jump_mouse_config.exe`
4. Click the **Start** button
5. Alt+Tab to switch windows — the cursor follows automatically

### Autostart

In the GUI config tool, check **Autostart** → click **Save**. The daemon will start with Windows.

### Command Line (Daemon)

```bash
# Use default config.json (same directory as the exe)
jump_mouse_daemon.exe

# Specify a custom config path
jump_mouse_daemon.exe --config "D:\my_config.json"

# Show usage help
jump_mouse_daemon.exe --help
```

---

## Configuration

The configuration file `config.json` can be edited via the GUI or directly.

```jsonc
{
  // Trigger mode: "alt_tab_only" | "any_focus_change"
  "triggerMode": "alt_tab_only",

  // Mouse mode: "instant" | "smooth"
  "mouseMode": "smooth",

  // Smooth animation duration in ms (50-600)
  "smoothDurationMs": 600,

  // Delay before moving for restoring minimized windows (0-2000 ms)
  "moveDelayMs": 100,

  // Target area: "window_rect" (with title bar) | "client_rect" (client area only)
  "targetArea": "window_rect",

  // Master enable switch
  "enabled": true,

  // Excluded process names (case-insensitive)
  "excludedProcesses": [],

  // Excluded window class name substrings
  "excludedClasses": [],

  // Log level: "none" | "error" | "warn" | "info" | "debug"
  "logLevel": "none",

  // Log output file path (empty = no file output)
  "logFile": ""
}
```

### Hot-Reload

After saving config via GUI or modifying `config.json` directly, the daemon automatically reloads changes within 2 seconds — no restart needed.

---

## Building from Source

### Requirements

- Visual Studio 2022 (Community edition is free) with "Desktop development with C++" workload
- Windows 10 SDK 10.0.19041+ (included with VS)
- CMake 3.20+ (included with VS)

### Build Steps

```bash
# Open "x64 Native Tools Command Prompt for VS 2022"

cmake -B build -S . -A x64
cmake --build build --config Release
```

Output in `build/release/`:
```
build/release/
├── jump_mouse_daemon.exe
├── jump_mouse_config.exe
└── config.json
```

### GitHub Actions CI

Push a `v*` tag to GitHub — Actions automatically builds and creates a Release.

---

## Architecture

### Dual Executables

```
jump_mouse_daemon.exe    Background service (no window)
    ├── FocusMonitor      SetWinEventHook for foreground window changes
    ├── KeyTracker        WH_KEYBOARD_LL for Alt key state
    ├── TriggerDetector   Validates Alt+Tab transitions
    └── MouseController   Computes window center + executes instant/smooth moves

jump_mouse_config.exe    GUI configuration tool
    ├── ConfigDialog      Win32 dialog UI (resource-script driven)
    ├── ConfigManager     JSON config read/write (shared with daemon)
    └── DaemonController  Daemon lifecycle (CreateProcess / stop / status)
```

### Core Modules

| Module | File | Purpose |
|--------|------|---------|
| Config Manager | `config_manager.h/cpp` | JSON read/write, validation, hot-reload |
| Adaptation Layer | `adaptation_layer.h/cpp` | Win10/11 differences, DPI, UWP detection |
| Focus Monitor | `focus_monitor.h/cpp` | `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` wrapper |
| Key Tracker | `key_tracker.h/cpp` | `WH_KEYBOARD_LL` low-level keyboard hook |
| Trigger Detector | `trigger_detector.h/cpp` | Alt+Tab three-phase detection core |
| Mouse Controller | `mouse_controller.h/cpp` | Window center calculation + instant/smooth move |
| GUI Dialog | `config_dialog.h/cpp` | Win32 dialog UI, system tray |
| Daemon Controller | `daemon_controller.h/cpp` | CreateProcess start/stop daemon |

### Tech Stack

- **Language**: C++20 (`<filesystem>`, `<chrono>`, `<format>`)
- **UI**: Win32 Dialog (`.rc` resource script)
- **Build**: CMake + MSVC
- **Dependencies**: Zero third-party libraries, Windows SDK only

---

## FAQ

### Q: Daemon starts but Alt+Tab doesn't trigger movement?

1. Check that `"enabled": true` in `config.json`
2. Excluded processes won't trigger moves — try clearing `excludedProcesses`
3. Set `logLevel` to `"debug"` and specify a `logFile` path to investigate

### Q: Mouse moves to wrong location for restored minimized windows?

Increase `moveDelayMs` (e.g. to 300-500ms) to give the window restore animation time to complete.

### Q: Cursor movement is choppy?

- Instant mode has zero delay; smooth mode uses a 10ms frame timer
- Increase `smoothDurationMs` for slower, smoother animation
- Switch `mouseMode` to `"instant"` to skip animation entirely

### Q: Can't start the daemon?

- Check if another instance is already running (Task Manager → `jump_mouse_daemon.exe`)
- Ensure the exe and `config.json` are in the same directory
- Enable debug logging to see error details

### Q: Autostart not working?

- In the GUI: uncheck Autostart → Save → recheck → Save
- Or manually verify `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\JumpMouse`

---

## Project Structure

```
jump-mouse/
├── src/
│   ├── common/                 # Shared modules
│   │   ├── config_manager.h/cpp
│   │   ├── adaptation_layer.h/cpp
│   │   ├── json_util.h
│   │   └── logger.h
│   ├── daemon/                 # Background daemon
│   │   ├── daemon_main.cpp
│   │   ├── application.h/cpp
│   │   ├── focus_monitor.h/cpp
│   │   ├── key_tracker.h/cpp
│   │   ├── trigger_detector.h/cpp
│   │   └── mouse_controller.h/cpp
│   └── config_tool/            # GUI configuration tool
│       ├── config_main.cpp
│       ├── config_dialog.h/cpp
│       └── daemon_controller.h/cpp
├── CMakeLists.txt
├── config.json
├── README.md
└── docs/
    ├── README_zh-CN.md
    ├── README_en.md
    ├── DESIGN_DOC.md
    ├── BUILD_GUIDE.md
    ├── CODE_REVIEW.md
    └── ISSUES.md
```

---

## Changelog

### Jump V1.0 (2026-05-18)

- First stable release
- Core: Alt+Tab detection + cursor warp (instant/smooth)
- Win32 GUI config tool with system tray and hot-reload
- Fixed resource ID conflicts (root cause of UI control issues)
- Removed legacy cursor personalization features
- Static CRT linking — no runtime installs needed

---

## License

MIT License
