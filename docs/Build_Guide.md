# Jump Mouse — Build Guide

> Platform: Windows 10 / Windows 11
> Compiler: MSVC (Visual Studio 2022)
> C++ Standard: C++20

---

## 1. Prerequisites

Install Visual Studio 2022 Community (free) with the following workload:

```
Desktop development with C++
  - MSVC v143 - VS 2022 C++ x64/x86 build tools
  - Windows 11 SDK (10.0.22621.0 or higher)
  - C++ CMake tools for Windows
```

---

## 2. Quick Build (3 Steps)

Open **x64 Native Tools Command Prompt for VS 2022**, then:

```cmd
cd /d E:\path\to\jump-mouse
cmake -B build -S . -A x64
cmake --build build --config Release --parallel
```

---

## 3. Output

```
build/release/
├── jump_mouse_daemon.exe
├── jump_mouse_config.exe
└── config.json
```

---

## 4. Build Options

| Option | Recommendation |
|--------|---------------|
| VS 2022 IDE | File → Open → CMake... → select CMakeLists.txt → Build All |
| VS Code + CMake Tools | Install Microsoft C/C++ and CMake Tools extensions → Open folder → F7 |
| GitHub Actions | Push v* tag → auto-build + Release |

---

## 5. Common Issues

### cmake: command not found

Open **x64 Native Tools Command Prompt for VS 2022** instead of regular cmd. This sets up the PATH for cl.exe, cmake.exe, and MSBuild.exe.

### fatal error C1083: Cannot open Windows.h

Install the "Windows 11 SDK" component in Visual Studio Installer.

### rc.exe build failure

Ensure you are using the "Visual Studio 17 2022" generator and x64 architecture. Do not use MinGW or Clang.

### Another instance is already running

A daemon instance is already running. Stop it via the GUI config tool or Task Manager:

```cmd
taskkill /f /im jump_mouse_daemon.exe
```

---

## 6. One-Click Build Script

Create `build.bat` in the project root:

```bat
@echo off
cmake -B build -S . -A x64
cmake --build build --config Release --parallel
echo Build complete. Output: build\release\
pause
```

---

> Document version: v1.0 | Last updated: 2026-05-18
