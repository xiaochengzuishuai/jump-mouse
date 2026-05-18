# Jump Mouse — Code Review (V1.0)

> Date: 2026-05-18 | Scope: Core functionality

---

## Checklist

| Item | Status | Notes |
|------|:---:|------|
| Alt+Tab Detection | OK | Three-phase (Alt down, record candidate, Alt up triggers move) |
| Minimized Window Restore Delay | OK | moveDelayMs delay then re-fetch window position |
| Smooth Animation | OK | Cubic ease-out, 10ms timer-driven |
| Instant Move | OK | SetCursorPos direct warp |
| System Window Filter | OK | TaskSwitcherWnd, Shell_TrayWnd, Progman, etc. |
| UWP Window Support | OK | ApplicationFrameWindow wrapper detection |
| Config Hot-Reload | OK | Poll file mtime every 2 seconds |
| Single Instance | OK | Named mutex Global\JumpMouse_Instance |
| System Tray | OK | Right-click menu: Open/Toggle/Exit |
| DPI Awareness | OK | Embedded manifest declaring PerMonitorV2 |
| Win10/Win11 Compat | OK | Fuzzy class name matching |
| Zero Dependencies | OK | Custom json_util.h |

---

## Known Limitations

| Issue | Severity | Notes |
|-------|:---:|------|
| WH_KEYBOARD_LL timeout | Low | Win10 enforces a timeout on low-level hooks |
| Fullscreen games | Low | No auto-detection; add process to exclusion list manually |

> **Conclusion**: Core functionality is stable and well-tested. Win10/Win11 compatibility verified.
