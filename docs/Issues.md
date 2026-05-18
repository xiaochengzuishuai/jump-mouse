# Jump Mouse — Known Issues

> Updated: 2026-05-18

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| v1.0 | 2026-05-16 | Initial: daemon + GUI config tool |
| v1.1 | 2026-05-17 | Restore delay, save-without-close, system tray, exe icons |
| v1.0 | 2026-05-18 | **Jump V1.0 Stable** — Removed cursor personalization and test scripts, fixed resource ID conflicts |

---

## P2 — Medium Priority

### P2-1: Mouse moves to top-left after restoring minimized window — Fixed (v1.1)

**Symptom**: When Alt+Tabbing to a minimized window, GetWindowRect returns invalid coordinates before the restore animation completes, causing the cursor to jump to (0,0).

**Root cause**: GetWindowRect returns (0,0,0,0) or empty rect during window restore.

**Fix**: Added moveDelayMs config option (0-2000ms). Waits before moving to allow the restore animation to finish. Implemented via WM_TIMER, non-blocking.

---

### P2-2: Occasional failure to move after rapid Alt+Tab

**Symptom**: Holding Alt and rapidly pressing Tab multiple times may result in the cursor not moving after Alt release.

**Investigation**:
1. Relax POST_RELEASE_WINDOW_MS to 1000ms
2. Track Tab key press/release in KeyTracker

---

## Resolved

| ID | Issue | Version |
|----|-------|:---:|
| #1 | rc.exe build failure (IDC_STATIC undefined) | v1.0 |
| #2 | Duplicate manifest resource (CVTRES 1100) | v1.0 |
| #3 | rc.exe build failure (CommCtrl.h) | v1.0 |
| #4 | wchar_t to char narrowing warnings | v1.0 |
| #5 | Alt-release timing race condition | v1.0 |
| #6 | Removed cursor personalization / test scripts | v2.0 |
